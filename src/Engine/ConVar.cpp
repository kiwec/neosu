// Copyright (c) 2011, PG, All rights reserved.
#include "ConVar.h"

#include "Bancho.h"
#include "BanchoUsers.h"
#include "BeatmapInterface.h"
#include "CBaseUILabel.h"
#include "Console.h"
#include "Database.h"
#include "Engine.h"
#include "ModSelector.h"
#include "Osu.h"
#include "Profiler.h"
#include "RichPresence.h"
#include "SongBrowser/LoudnessCalcThread.h"
#include "SoundEngine.h"
#include "SString.h"
#include "SpectatorScreen.h"
#include "UpdateHandler.h"
#include "Logging.h"

#include "misc_bin.h"

#include "fmt/chrono.h"

#include <algorithm>
#include <unordered_set>

void ConVar::addConVar(ConVar *c) {
    const std::string_view cname_str = c->getName();

    // osu_ prefix is deprecated.
    // If you really need it, you'll also need to edit Console::execConfigFile to whitelist it there.
    assert(!(cname_str.starts_with("osu_") && !cname_str.starts_with("osu_folder")) &&
           "osu_ ConVar prefix is deprecated.");

    auto &convar_map = ConVarHandler::getConVarMap_int();

    // No duplicate ConVar names allowed
    assert(!convar_map.contains(cname_str) && "no duplicate ConVar names allowed.");

    convar_map.emplace(cname_str, c);
    ConVarHandler::getConVarArray_int().push_back(c);
}

ConVarString ConVar::getFancyDefaultValue() {
    switch(this->getType()) {
        case ConVar::CONVAR_TYPE::CONVAR_TYPE_BOOL:
            return this->dDefaultValue == 0 ? "false" : "true";
        case ConVar::CONVAR_TYPE::CONVAR_TYPE_INT:
            return std::to_string((int)this->dDefaultValue);
        case ConVar::CONVAR_TYPE::CONVAR_TYPE_FLOAT:
            return std::to_string(this->dDefaultValue);
        case ConVar::CONVAR_TYPE::CONVAR_TYPE_STRING: {
            ConVarString out = "\"";
            out.append(this->sDefaultValue);
            out.append("\"");
            return out;
        }
    }

    return "unreachable";
}

ConVarString ConVar::typeToString(CONVAR_TYPE type) {
    switch(type) {
        case ConVar::CONVAR_TYPE::CONVAR_TYPE_BOOL:
            return "bool";
        case ConVar::CONVAR_TYPE::CONVAR_TYPE_INT:
            return "int";
        case ConVar::CONVAR_TYPE::CONVAR_TYPE_FLOAT:
            return "float";
        case ConVar::CONVAR_TYPE::CONVAR_TYPE_STRING:
            return "string";
    }

    return "";
}

void ConVar::exec() {
    if(auto *cb = std::get_if<NativeConVarCallback>(&this->callback)) (*cb)();
}

void ConVar::execArgs(std::string_view args) {
    if(auto *cb = std::get_if<NativeConVarCallbackArgs>(&this->callback)) (*cb)(args);
}

void ConVar::execFloat(float args) {
    if(auto *cb = std::get_if<NativeConVarCallbackFloat>(&this->callback)) (*cb)(args);
}

double ConVar::getDouble() const {
    if(this->isFlagSet(cv::SERVER) && this->hasServerValue.load(std::memory_order_acquire)) {
        return this->dServerValue.load(std::memory_order_acquire);
    }

    if(this->isFlagSet(cv::SKINS) && this->hasSkinValue.load(std::memory_order_acquire)) {
        return this->dSkinValue.load(std::memory_order_acquire);
    }

    if(this->isProtected() && BanchoState::is_in_a_multi_room()) {
        return this->dDefaultValue;
    }

    return this->dClientValue.load(std::memory_order_acquire);
}

const ConVarString &ConVar::getString() const {
    if(this->isFlagSet(cv::SERVER) && this->hasServerValue.load(std::memory_order_acquire)) {
        return this->sServerValue;
    }

    if(this->isFlagSet(cv::SKINS) && this->hasSkinValue.load(std::memory_order_acquire)) {
        return this->sSkinValue;
    }

    if(this->isProtected() && BanchoState::is_in_a_multi_room()) {
        return this->sDefaultValue;
    }

    return this->sClientValue;
}

void ConVar::setDefaultDouble(double defaultValue) {
    this->dDefaultValue = defaultValue;
    this->sDefaultValue = fmt::format("{:g}", defaultValue);
}

void ConVar::setDefaultString(std::string_view defaultValue) {
    this->sDefaultValue = defaultValue;

    // also try to parse default float from the default string
    const double f = std::strtod(this->sDefaultValue.c_str(), nullptr);
    if(f != 0.0) {
        this->dDefaultValue = f;
    }
}

bool ConVar::onSetValueGameplay(CvarEditor editor) {
    if(!osu) return true;  // osu may have been destroyed while shutting down
    // Only SERVER can edit GAMEPLAY cvars during multiplayer matches
    if(BanchoState::is_playing_a_multi_map() && editor != CvarEditor::SERVER) {
        debugLog("Can't edit {} while in a multiplayer match.", this->sName);
        return false;
    }

    // Regardless of the editor, changing GAMEPLAY cvars in the middle of a map
    // will result in an invalid replay. Set it as cheated so the score isn't saved.
    if(osu->isInPlayMode()) {
        debugLog("{} affects gameplay: won't submit score.", this->sName);
    }
    osu->getScore()->setCheated();

    return true;
}

void ConVar::onSetValueProtected(std::string_view oldValue, std::string_view newValue) {
    if(!osu) return;
    if(oldValue == newValue) return;
    osu->getMapInterface()->is_submittable = false;
}

//********************************//
//  ConVarHandler Implementation  //
//********************************//

// singleton init
std::unique_ptr<ConVarHandler> cvars{std::make_unique<ConVarHandler>()};

// private static
std::vector<ConVar *> &ConVarHandler::getConVarArray_int() {
    static std::vector<ConVar *> vConVarArray;

    static std::once_flag reserved;
    std::call_once(reserved, []() { vConVarArray.reserve(1024); });

    return vConVarArray;
}

sv_unordered_map<ConVar *> &ConVarHandler::getConVarMap_int() {
    static sv_unordered_map<ConVar *> vConVarMap;

    static std::once_flag reserved;
    std::call_once(reserved, []() { vConVarMap.reserve(1024); });

    return vConVarMap;
}

ConVar *ConVarHandler::getConVar_int(std::string_view name) {
    const auto &cvarMap = getConVarMap();
    auto it = cvarMap.find(name);
    if(it != cvarMap.end()) return it->second;
    return nullptr;
}

// public
ConVar *ConVarHandler::getConVarByName(std::string_view name, bool warnIfNotFound) const {
    static ConVar _emptyDummyConVar(
        "emptyDummyConVar", 42.0f, cv::CLIENT,
        "this placeholder convar is returned by cvars->getConVarByName() if no matching convar is found");

    ConVar *found = ConVarHandler::getConVar_int(name);
    if(found) return found;

    if(warnIfNotFound) {
        ConVarString errormsg = ConVarString("ENGINE: ConVar \"");
        errormsg.append(name);
        errormsg.append("\" does not exist...");
        Logger::logRaw("{:s}", errormsg.c_str());
        engine->showMessageWarning("Engine Error", errormsg.c_str());
    }

    if(!warnIfNotFound)
        return nullptr;
    else
        return &_emptyDummyConVar;
}

std::vector<ConVar *> ConVarHandler::getConVarByLetter(std::string_view letters) const {
    std::unordered_set<std::string> matchingConVarNames;
    std::vector<ConVar *> matchingConVars;
    {
        if(letters.length() < 1) return matchingConVars;

        const std::vector<ConVar *> &convars = this->getConVarArray();

        // first try matching exactly
        for(auto convar : convars) {
            if(convar->isFlagSet(cv::HIDDEN)) continue;

            const ConVarString &name = convar->getName();
            if(name.find(letters) != std::string::npos) {
                if(letters.length() > 1) matchingConVarNames.insert(name);

                matchingConVars.push_back(convar);
            }
        }

        // then try matching substrings
        if(letters.length() > 1) {
            for(auto convar : convars) {
                if(convar->isFlagSet(cv::HIDDEN)) continue;

                if(convar->getName().find(letters) != std::string::npos) {
                    const ConVarString &stdName = convar->getName();
                    if(!matchingConVarNames.contains(stdName)) {
                        matchingConVarNames.insert(stdName);
                        matchingConVars.push_back(convar);
                    }
                }
            }
        }

        // (results should be displayed in vector order)
    }
    return matchingConVars;
}

ConVarString ConVarHandler::flagsToString(uint8_t flags) {
    if(flags == 0) {
        return "no flags";
    }

    static constexpr const auto flagStringPairArray = std::array{
        std::pair{cv::CLIENT, "client"},       std::pair{cv::SERVER, "server"},     std::pair{cv::SKINS, "skins"},
        std::pair{cv::PROTECTED, "protected"}, std::pair{cv::GAMEPLAY, "gameplay"}, std::pair{cv::HIDDEN, "hidden"},
        std::pair{cv::NOSAVE, "nosave"},       std::pair{cv::NOLOAD, "noload"}};

    ConVarString string;
    for(bool first = true; const auto &[flag, str] : flagStringPairArray) {
        if((flags & flag) == flag) {
            if(!first) {
                string.append(" ");
            }
            first = false;
            string.append(str);
        }
    }

    return string;
}

std::vector<ConVar *> ConVarHandler::getNonSubmittableCvars() const {
    std::vector<ConVar *> list;

    for(const auto &cv : ConVarHandler::getConVarArray()) {
        if(!cv->isProtected()) continue;

        if(cv->getString() != cv->getDefaultString()) {
            list.push_back(cv);
        }
    }

    return list;
}

bool ConVarHandler::areAllCvarsSubmittable() {
    for(const auto &cv : ConVarHandler::getConVarArray()) {
        if(!cv->isProtected()) continue;

        if(cv->getString() != cv->getDefaultString()) {
            return false;
        }
    }

    // Also check for non-vanilla mod combinations here while we're at it
    if(osu != nullptr) {
        // We don't want to submit target scores, even though it's allowed in multiplayer
        if(osu->getModTarget()) return false;

        if(osu->getModEZ() && osu->getModHR()) return false;

        if(!cv::sv_allow_speed_override.getBool()) {
            f32 speed = cv::speed_override.getFloat();
            if(speed != -1.f && speed != 0.75 && speed != 1.0 && speed != 1.5) return false;
        }
    }

    return true;
}

void ConVarHandler::resetServerCvars() {
    for(const auto &cv : ConVarHandler::getConVarArray()) {
        cv->hasServerValue.store(false, std::memory_order_release);
        cv->serverProtectionPolicy.store(ConVar::ProtectionPolicy::DEFAULT, std::memory_order_release);
    }
}

void ConVarHandler::resetSkinCvars() {
    for(const auto &cv : ConVarHandler::getConVarArray()) {
        cv->hasSkinValue.store(false, std::memory_order_release);
    }
}

//*****************************//
//	ConVarHandler ConCommands  //
//*****************************//

static void _find(std::string_view args) {
    if(args.length() < 1) {
        Logger::logRaw("Usage:  find <string>");
        return;
    }

    const std::vector<ConVar *> &convars = cvars->getConVarArray();

    std::vector<ConVar *> matchingConVars;
    for(auto convar : convars) {
        if(convar->isFlagSet(cv::HIDDEN)) continue;

        const std::string_view name = convar->getName();
        if(name.find(args) != std::string::npos) matchingConVars.push_back(convar);
    }

    if(matchingConVars.size() > 0) {
        std::ranges::sort(matchingConVars, {}, [](const ConVar *v) { return v->getName(); });
    }

    if(matchingConVars.size() < 1) {
        Logger::logRaw("No commands found containing {:s}.", args);
        return;
    }

    Logger::logRaw("----------------------------------------------");
    {
        std::string thelog = "[ find : ";
        thelog.append(args);
        thelog.append(" ]");
        Logger::logRaw("{:s}", thelog);

        for(auto &matchingConVar : matchingConVars) {
            Logger::logRaw("{:s}", matchingConVar->getName());
        }
    }
    Logger::logRaw("----------------------------------------------");
}

static void _help(std::string_view args) {
    ConVarString trimmedArgs{args};
    SString::trim(&trimmedArgs);

    if(trimmedArgs.length() < 1) {
        Logger::logRaw("Usage:  help <cvarname>");
        Logger::logRaw("To get a list of all available commands, type \"listcommands\".");
        return;
    }

    const std::vector<ConVar *> matches = cvars->getConVarByLetter(trimmedArgs);

    if(matches.size() < 1) {
        Logger::logRaw("ConVar {:s} does not exist.", trimmedArgs);
        return;
    }

    // use closest match
    size_t index = 0;
    for(size_t i = 0; i < matches.size(); i++) {
        if(matches[i]->getName() == trimmedArgs) {
            index = i;
            break;
        }
    }
    ConVar *match = matches[index];

    if(match->getHelpstring().length() < 1) {
        Logger::logRaw("ConVar {:s} does not have a helpstring.", match->getName());
        return;
    }

    ConVarString thelog{match->getName()};
    {
        if(match->hasValue()) {
            const auto &cv_str = match->getString();
            const auto &default_str = match->getDefaultString();
            thelog.append(fmt::format(" = {:s} ( def. \"{:s}\" , ", cv_str.c_str(), default_str.c_str()));
            thelog.append(ConVar::typeToString(match->getType()));
            thelog.append(", ");
            thelog.append(ConVarHandler::flagsToString(match->getFlags()).c_str());
            thelog.append(" )");
        }

        thelog.append(" - ");
        thelog.append(match->getHelpstring().c_str());
    }
    Logger::logRaw("{:s}", thelog);
}

static void _listcommands(void) {
    Logger::logRaw("----------------------------------------------");
    {
        std::vector<ConVar *> convars = cvars->getConVarArray();
        std::ranges::sort(convars, {}, [](const ConVar *v) { return v->getName(); });

        for(auto &convar : convars) {
            if(convar->isFlagSet(cv::HIDDEN)) continue;

            ConVar *var = convar;

            ConVarString tstring{var->getName()};
            {
                if(var->hasValue()) {
                    const auto &var_str = var->getString();
                    const auto &default_str = var->getDefaultString();
                    tstring.append(fmt::format(" = {:s} ( def. \"{:s}\" , ", var_str.c_str(), default_str.c_str()));
                    tstring.append(ConVar::typeToString(var->getType()));
                    tstring.append(", ");
                    tstring.append(ConVarHandler::flagsToString(var->getFlags()).c_str());
                    tstring.append(" )");
                }

                if(var->getHelpstring().length() > 0) {
                    tstring.append(" - ");
                    tstring.append(var->getHelpstring().c_str());
                }
            }
            Logger::logRaw("{:s}", tstring);
        }
    }
    Logger::logRaw("----------------------------------------------");
}

static void _dumpcommands(void) {
    // in assets/misc/convar_template.html
    ConVarString html_template{reinterpret_cast<const char *>(convar_template),
                               static_cast<size_t>(convar_template_size())};

    std::vector<ConVar *> convars = cvars->getConVarArray();
    std::ranges::sort(convars, {}, [](const ConVar *v) { return v->getName(); });

    ConVarString html = R"(<section class="variables">)";
    for(auto var : convars) {
        ConVarString flags;
        if(var->isFlagSet(cv::CLIENT)) flags.append("<span class=\"flag client\">CLIENT</span>");
        if(var->isFlagSet(cv::SKINS)) flags.append("<span class=\"flag skins\">SKINS</span>");
        if(var->isFlagSet(cv::SERVER)) flags.append("<span class=\"flag server\">SERVER</span>");
        if(var->isFlagSet(cv::PROTECTED)) flags.append("<span class=\"flag protected\">PROTECTED</span>");
        if(var->isFlagSet(cv::GAMEPLAY)) flags.append("<span class=\"flag gameplay\">GAMEPLAY</span>");

        html.append(fmt::format(R"(<div>
    <cv-header>
        <cv-name>{}</cv-name>
        <cv-default>{}</cv-default>
    </cv-header>
    <cv-description>{}</cv-description>
    <cv-flags>{}</cv-flags>
</div>)",
                                var->getName(), var->getFancyDefaultValue(), var->getHelpstring(), flags));
    }
    html.append(R"(</section>)");

    html.append(fmt::format(R"(<p style="text-align:center">
        This page was generated on {:%Y-%m-%d} for neosu v{:.2f}.<br>
        Use the <code>dumpcommands</code> command to regenerate it yourself.
    </p>)",
                            fmt::gmtime(std::time(nullptr)), cv::version.getDouble()));

    ConVarString marker = "{{CONVARS_HERE}}";
    size_t pos = html_template.find(marker);
    html_template.replace(pos, marker.length(), html);

    FILE *file = fopen("variables.htm", "w");
    if(file == nullptr) {
        Logger::logRaw("Failed to open variables.htm for writing");
        return;
    }

    fwrite(html_template.c_str(), html_template.size(), 1, file);
    fflush(file);
    fclose(file);

    Logger::logRaw("ConVars dumped to variables.htm");
}

void _exec(std::string_view args) { Console::execConfigFile(std::string{args.data(), args.length()}); }

void _echo(std::string_view args) {
    if(args.length() > 0) {
        Logger::logRaw("{:s}", args);
    }
}

void _vprof(float newValue) {
    const bool enable = !!static_cast<int>(newValue);

    if(enable != g_profCurrentProfile.isEnabled()) {
        if(enable)
            g_profCurrentProfile.start();
        else
            g_profCurrentProfile.stop();
    }
}

void _osuOptionsSliderQualityWrapper(float newValue) {
    float value = std::lerp(1.0f, 2.5f, 1.0f - newValue);
    cv::slider_curve_points_separation.setValue(value);
};

void spectate_by_username(std::string_view username) {
    auto user = BANCHO::User::find_user(UString{username.data(), static_cast<int>(username.length())});
    if(user == nullptr) {
        debugLog("Couldn't find user \"{:s}\"!", username);
        return;
    }

    debugLog("Spectating {:s} (user {:d})...", username, user->user_id);
    start_spectating(user->user_id);
}

void _osu_songbrowser_search_hardcoded_filter(std::string_view /*oldValue*/, std::string_view newValue) {
    if(newValue.length() == 1 && SString::whitespace_only(newValue))
        cv::songbrowser_search_hardcoded_filter.setValue("");
}

void loudness_cb(std::string_view /*oldValue*/, std::string_view /*newValue*/) {
    // Restart loudness calc.
    VolNormalization::abort();
    if(db && cv::normalize_loudness.getBool()) {
        VolNormalization::start_calc(db->loudness_to_calc);
    }
}

void _save(void) { db ? db->save() : (void)0; }

void _update(void) { (osu && osu->getUpdateHandler()) ? osu->getUpdateHandler()->checkForUpdates(true) : (void)0; }
#undef CONVARDEFS_H
#define DEFINE_CONVARS

#include "ConVarDefs.h"
