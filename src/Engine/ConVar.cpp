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
#include "SpectatorScreen.h"
#include "UpdateHandler.h"

#include <algorithm>
#include <fmt/chrono.h>
#include <unordered_set>

void ConVar::addConVar(ConVar *c) {
    const std::string &cname_str = c->getName();

    // osu_ prefix is deprecated.
    // If you really need it, you'll also need to edit Console::execConfigFile to whitelist it there.
    assert(!(cname_str.starts_with("osu_") && !cname_str.starts_with("osu_folder")) &&
           "osu_ ConVar prefix is deprecated.");

    // No duplicate ConVar names allowed
    assert(!ConVarHandler::getConVarMap().contains(cname_str) && "no duplicate ConVar names allowed.");

    ConVarHandler::getConVarArray_int().push_back(c);
    ConVarHandler::getConVarMap_int()[cname_str] = c;
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

void ConVar::execArgs(const UString &args) {
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

void ConVar::setDefaultString(const std::string_view &defaultValue) {
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
        debugLog("Can't edit {} while in a multiplayer match.\n", this->sName);
        return false;
    }

    // Regardless of the editor, changing GAMEPLAY cvars in the middle of a map
    // will result in an invalid replay. Set it as cheated so the score isn't saved.
    if(osu->isInPlayMode()) {
        debugLog("{} affects gameplay: won't submit score.\n", this->sName);
    }
    osu->getScore()->setCheated();

    return true;
}

void ConVar::onSetValueProtected(const std::string &oldValue, const std::string &newValue) {
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

std::unordered_map<ConVarString, ConVar *> &ConVarHandler::getConVarMap_int() {
    static std::unordered_map<ConVarString, ConVar *> vConVarMap;

    static std::once_flag reserved;
    std::call_once(reserved, []() { vConVarMap.reserve(1024); });

    return vConVarMap;
}

ConVar *ConVarHandler::getConVar_int(const ConVarString &name) {
    const auto &cvarMap = getConVarMap();
    if(cvarMap.contains(name)) return cvarMap.at(name);
    return nullptr;
}

// public
ConVar *ConVarHandler::getConVarByName(const ConVarString &name, bool warnIfNotFound) const {
    static ConVar _emptyDummyConVar(
        "emptyDummyConVar", 42.0f, cv::CLIENT,
        "this placeholder convar is returned by cvars->getConVarByName() if no matching convar is found");

    ConVar *found = ConVarHandler::getConVar_int(name);
    if(found) return found;

    if(warnIfNotFound) {
        ConVarString errormsg = ConVarString("ENGINE: ConVar \"");
        errormsg.append(name);
        errormsg.append("\" does not exist...\n");
        Engine::logRaw("{:s}", errormsg.c_str());
        engine->showMessageWarning("Engine Error", errormsg.c_str());
    }

    if(!warnIfNotFound)
        return nullptr;
    else
        return &_emptyDummyConVar;
}

std::vector<ConVar *> ConVarHandler::getConVarByLetter(const ConVarString &letters) const {
    std::unordered_set<std::string> matchingConVarNames;
    std::vector<ConVar *> matchingConVars;
    {
        if(letters.length() < 1) return matchingConVars;

        const std::vector<ConVar *> &convars = this->getConVarArray();

        // first try matching exactly
        for(auto convar : convars) {
            if(convar->isFlagSet(cv::HIDDEN)) continue;

            if(convar->getName().find(letters) != std::string::npos) {
                if(letters.length() > 1) matchingConVarNames.insert(convar->getName());

                matchingConVars.push_back(convar);
            }
        }

        // then try matching substrings
        if(letters.length() > 1) {
            for(auto convar : convars) {
                if(convar->isFlagSet(cv::HIDDEN)) continue;

                if(convar->getName().find(letters) != std::string::npos) {
                    const ConVarString &stdName = convar->getName();
                    if(matchingConVarNames.find(stdName) == matchingConVarNames.end()) {
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

    ConVarString string;

    if((flags & cv::CLIENT) == cv::CLIENT) string.append(" client");
    if((flags & cv::SERVER) == cv::SERVER) string.append(" server");
    if((flags & cv::SKINS) == cv::SKINS) string.append(" skins");
    if((flags & cv::PROTECTED) == cv::PROTECTED) string.append(" protected");
    if((flags & cv::GAMEPLAY) == cv::GAMEPLAY) string.append(" gameplay");
    if((flags & cv::HIDDEN) == cv::HIDDEN) string.append(" hidden");
    if((flags & cv::NOSAVE) == cv::NOSAVE) string.append(" nosave");
    if((flags & cv::NOLOAD) == cv::NOLOAD) string.append(" noload");

    string.pop_back();  // remove leading space

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

static void _find(const UString &args) {
    if(args.length() < 1) {
        Engine::logRaw("Usage:  find <string>\n");
        return;
    }

    const std::vector<ConVar *> &convars = cvars->getConVarArray();

    std::vector<ConVar *> matchingConVars;
    for(auto convar : convars) {
        if(convar->isFlagSet(cv::HIDDEN)) continue;

        const ConVarString &name = convar->getName();
        if(name.find(args.toUtf8()) != std::string::npos) matchingConVars.push_back(convar);
    }

    if(matchingConVars.size() > 0) {
        std::ranges::sort(matchingConVars, {}, [](const ConVar *v) { return v->getName(); });
    }

    if(matchingConVars.size() < 1) {
        Engine::logRaw("No commands found containing {:s}.\n", args);
        return;
    }

    Engine::logRaw("----------------------------------------------\n");
    {
        UString thelog = "[ find : ";
        thelog.append(args);
        thelog.append(" ]\n");
        Engine::logRaw("{:s}", thelog.toUtf8());

        for(auto &matchingConVar : matchingConVars) {
            Engine::logRaw("{:s}\n", matchingConVar->getName());
        }
    }
    Engine::logRaw("----------------------------------------------\n");
}

static void _help(const UString &args) {
    std::string trimmedArgs{args.trim().utf8View()};

    if(trimmedArgs.length() < 1) {
        Engine::logRaw("Usage:  help <cvarname>\n To get a list of all available commands, type \"listcommands\".\n");
        return;
    }

    const std::vector<ConVar *> matches = cvars->getConVarByLetter(trimmedArgs);

    if(matches.size() < 1) {
        Engine::logRaw("ConVar {:s} does not exist.\n", trimmedArgs);
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
        Engine::logRaw("ConVar {:s} does not have a helpstring.\n", match->getName());
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
    Engine::logRaw("{:s}\n", thelog);
}

static void _listcommands(void) {
    Engine::logRaw("----------------------------------------------\n");
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

                tstring.append("\n");
            }
            Engine::logRaw("{:s}", tstring);
        }
    }
    Engine::logRaw("----------------------------------------------\n");
}

static void _dumpcommands(void) {
    // XXX: move this into assets/
    std::string html_template = R"(<!DOCTYPE html>
<html>
<head>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>neosu | variables</title>
    <style>
        html, body {
            margin: 0;
            padding: 0;
            font-family: sans-serif;
            line-height: 1.5em;
            background-color: #000;
            color: #eee;
        }

        body {
            display: flex;
            flex-wrap: wrap;
            margin: 0 auto;
            justify-content: center;
        }

        main {
            margin: auto;
            max-width: 768px;
        }

        h3 {
            margin: 0;
        }

        section.variables {
            display: block;
            margin-bottom: 2em;
        }

        section.variables > div {
            border: 1px solid #333;
            border-radius: 5px;
            margin-bottom: 1em;
            padding: 1em;
            background-color: #111;
        }

        cv-header {
            display: flex;
            justify-content: space-between;
            align-items: baseline;
            margin-bottom: 0.5em;
        }

        cv-name {
            font-family: monospace;
            font-weight: bold;
            color: #88b0ff;
            font-size: 1.1em;
        }

        cv-default {
            font-family: monospace;
            color: #aaa;
            font-size: 0.9em;
        }

        cv-description {
            display: block;
            color: #ccc;
            margin-bottom: 0.5em;
            font-size: 0.95em;
        }

        cv-flags {
            display: flex;
            flex-wrap: wrap;
            gap: 0.5em;
        }

        .flag {
            padding: 0.2em 0.5em;
            border-radius: 3px;
            font-size: 0.8em;
            font-weight: bold;
            text-transform: uppercase;
        }

        .flag.gameplay {
            background-color: #8b2635;
            color: #fff;
        }

        .flag.protected {
            background-color: #b8860b;
            color: #fff;
        }

        .flag.client {
            background-color: #2d4a88;
            color: #fff;
        }

        .flag.skins {
            background-color: #2d4a22;
            color: #fff;
        }

        .flag.server {
            background-color: #5c1765;
            color: #fff;
        }

        a { color: #88b0ff; }
        a:hover { color: #b4cdff; }

        section.legend {
            display: flex;
            flex-direction: column;
            gap: 0.5em;

            margin-bottom: 2em;
            padding: 1em;
            background-color: #111;
            border-radius: 5px;
            border: 1px solid #333;
        }

        .legend > div {
            display: flex;
            gap: 0.5em;
        }
        .legend > div > div {
            text-align: right;
            min-width: 6em;
        }
        .legend div span {
            flex-grow: 1;
            text-align: left;
        }
    </style>
</head>
<body>
    <main>
        <h1>neosu variables</h1>
        <section class="legend">
            <h3>Availability</h3>
            <div>
                <div><span class="flag client">CLIENT</span></div>
                <span>Modifiable by clients</span>
            </div>
            <div>
                <div><span class="flag skins">SKINS</span></div>
                <span>Modifiable by skins</span>
            </div>
            <div>
                <div><span class="flag server">SERVER</span></div>
                <span>Modifiable by servers (+ saved in replays)</span>
            </div>
            <h3 style="margin-top:0.5em">Behavior</h3>
            <div>
                <div><span class="flag protected">PROTECTED</span></div>
                <span>Scores won't submit if modified, and will be forced to default in multiplayer</span>
            </div>
            <div>
                <div><span class="flag gameplay">GAMEPLAY</span></div>
                <span>Scores won't submit if modified during gameplay</span>
            </div>
        </section>

        {{CONVARS_HERE}}
    </main>
</body>
</html>)";

    std::vector<ConVar *> convars = cvars->getConVarArray();
    std::ranges::sort(convars, {}, [](const ConVar *v) { return v->getName(); });

    std::string html = R"(<section class="variables">)";
    for(auto var : convars) {
        std::string flags;
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

    std::string marker = "{{CONVARS_HERE}}";
    size_t pos = html_template.find(marker);
    html_template.replace(pos, marker.length(), html);

    FILE *file = fopen("variables.htm", "w");
    if(file == nullptr) {
        Engine::logRaw("Failed to open variables.htm for writing\n");
        return;
    }

    fwrite(html_template.c_str(), html_template.size(), 1, file);
    fflush(file);
    fclose(file);

    Engine::logRaw("ConVars dumped to variables.htm\n");
}

void _exec(const UString &args) { Console::execConfigFile(args.toUtf8()); }

void _echo(const UString &args) {
    if(args.length() > 0) {
        Engine::logRaw("{:s}\n", args.toUtf8());
    }
}

void _volume(const UString & /*oldValue*/, const UString &newValue) {
    if(!soundEngine) return;
    soundEngine->setMasterVolume(newValue.toFloat());
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

void spectate_by_username(const UString &username) {
    auto user = BANCHO::User::find_user(username);
    if(user == nullptr) {
        debugLog("Couldn't find user \"{:s}\"!", username.toUtf8());
        return;
    }

    debugLog("Spectating {:s} (user {:d})...\n", username.toUtf8(), user->user_id);
    start_spectating(user->user_id);
}

void _osu_songbrowser_search_hardcoded_filter(const UString & /*oldValue*/, const UString &newValue) {
    if(newValue.length() == 1 && newValue.isWhitespaceOnly()) cv::songbrowser_search_hardcoded_filter.setValue("");
}

void loudness_cb(const UString & /*oldValue*/, const UString & /*newValue*/) {
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
