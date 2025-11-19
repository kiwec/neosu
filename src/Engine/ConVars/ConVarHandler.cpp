// Copyright (c) 2011, PG & 2025, WH & 2025, kiwec, All rights reserved.
#include "ConVarHandler.h"
#include "ConVar.h"

#include "AsyncIOHandler.h"
#include "Logging.h"
#include "Engine.h"
#include "SString.h"
#include "SyncOnce.h"

#include "misc_bin.h"

#include "fmt/chrono.h"

#include <algorithm>
#include <unordered_set>

// singleton init
std::unique_ptr<ConVarHandler> cvars{std::make_unique<ConVarHandler>()};

// private static
std::vector<ConVar *> &ConVarHandler::getConVarArray_int() {
    static std::vector<ConVar *> vConVarArray;

    static Sync::once_flag reserved;
    Sync::call_once(reserved, []() { vConVarArray.reserve(1024); });

    return vConVarArray;
}

sv_unordered_map<ConVar *> &ConVarHandler::getConVarMap_int() {
    static sv_unordered_map<ConVar *> vConVarMap;

    static Sync::once_flag reserved;
    Sync::call_once(reserved, []() { vConVarMap.reserve(1024); });

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
        std::string errormsg = "ENGINE: ConVar \"";
        errormsg.append(name);
        errormsg.append("\" does not exist...");
        Logger::logRaw("{:s}", errormsg);
        engine->showMessageWarning("Engine Error", errormsg.c_str());
    }

    if(!warnIfNotFound)
        return nullptr;
    else
        return &_emptyDummyConVar;
}

std::vector<ConVar *> ConVarHandler::getConVarByLetter(std::string_view letters) const {
    std::unordered_set<std::string_view> matchingConVarNames;
    std::vector<ConVar *> matchingConVars;
    {
        if(letters.length() < 1) return matchingConVars;

        const std::vector<ConVar *> &convars = ConVarHandler::getConVarArray();

        // first try matching exactly
        for(auto convar : convars) {
            if(convar->isFlagSet(cv::HIDDEN)) continue;

            const std::string_view name = convar->getName();
            if(name.find(letters) != std::string::npos) {
                if(letters.length() > 1) matchingConVarNames.insert(name);

                matchingConVars.push_back(convar);
            }
        }

        // then try matching substrings
        if(letters.length() > 1) {
            for(auto convar : convars) {
                if(convar->isFlagSet(cv::HIDDEN)) continue;
                const std::string_view name = convar->getName();

                if(name.find(letters) != std::string::npos) {
                    if(!matchingConVarNames.contains(name)) {
                        matchingConVarNames.insert(name);
                        matchingConVars.push_back(convar);
                    }
                }
            }
        }

        // (results should be displayed in vector order)
    }
    return matchingConVars;
}

std::string ConVarHandler::flagsToString(uint8_t flags) {
    if(flags == 0) {
        return "no flags";
    }

    static constexpr const auto flagStringPairArray = std::array{
        std::pair{cv::CLIENT, "client"},       std::pair{cv::SERVER, "server"},     std::pair{cv::SKINS, "skins"},
        std::pair{cv::PROTECTED, "protected"}, std::pair{cv::GAMEPLAY, "gameplay"}, std::pair{cv::HIDDEN, "hidden"},
        std::pair{cv::NOSAVE, "nosave"},       std::pair{cv::NOLOAD, "noload"}};

    std::string string;
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

    for(auto *cv : ConVarHandler::getConVarArray()) {
        if(!cv->isProtected() || cv->isDefault()) continue;

        list.push_back(cv);
    }

    return list;
}

ConVarHandler::CVSubmittableCriteriaFunc ConVarHandler::areAllCvarsSubmittableExtraCheck{nullptr};

bool ConVarHandler::areAllCvarsSubmittable() {
    if(!this->getNonSubmittableCvars().empty()) return false;

    if(!!areAllCvarsSubmittableExtraCheck) {
        return areAllCvarsSubmittableExtraCheck();
    }

    return true;
}

void ConVarHandler::invalidateAllProtectedCaches() {
    for(auto *cv : ConVarHandler::getConVarArray()) {
        if(cv->isFlagSet(cv::PROTECTED)) cv->invalidateCache();
    }
}

void ConVarHandler::resetServerCvars() {
    for(auto *cv : ConVarHandler::getConVarArray()) {
        cv->hasServerValue.store(false, std::memory_order_release);
        cv->setServerProtected(CvarProtection::DEFAULT);
        cv->invalidateCache();
    }
}

void ConVarHandler::resetSkinCvars() {
    for(auto *cv : ConVarHandler::getConVarArray()) {
        cv->hasSkinValue.store(false, std::memory_order_release);
        cv->invalidateCache();
    }
}

bool ConVarHandler::removeServerValue(std::string_view cvarName) {
    ConVar *cvarToChange = ConVarHandler::getConVar_int(cvarName);
    if(!cvarToChange) return false;
    cvarToChange->hasServerValue.store(false, std::memory_order_release);
    cvarToChange->invalidateCache();
    return true;
}

size_t ConVarHandler::getTotalMemUsageBytes() {
    static size_t ret = 0;
    if(ret > 0 && !engine->throttledShouldRun(60)) return ret;
    ret = 0;

    for(auto *cv : ConVarHandler::getConVarArray()) {
        ret += strlen(cv->sName);
        ret += strlen(cv->sHelpString);
        ret += cv->sDefaultValue.size();
        ret += sizeof(cv->dDefaultValue);
        ret += sizeof(cv->dClientValue);
        ret += cv->sClientValue.size();
        ret += sizeof(cv->dSkinValue);
        ret += cv->sSkinValue.size();
        ret += sizeof(cv->dServerValue);
        ret += cv->sServerValue.size();
        ret += cv->sCachedReturnedString.size();
        ret += sizeof(cv->callback);
        ret += sizeof(cv->changeCallback);
        ret += sizeof(cv->serverProtectionPolicy);
        ret += sizeof(cv->type);
        ret += sizeof(cv->iFlags);
        ret += sizeof(cv->bCanHaveValue);
        ret += sizeof(cv->hasServerValue);
        ret += sizeof(cv->hasSkinValue);
        ret += sizeof(cv->bUseCachedDouble);
        ret += sizeof(cv->bUseCachedString);
    }
    return ret;
}

//*****************************//
//	ConVarHandler ConCommands  //
//*****************************//

struct ConVarHandler::ConVarBuiltins {
    static void find(std::string_view args);
    static void help(std::string_view args);
    static void listcommands(void);
    static void dumpcommands(void);
    static void echo(std::string_view args);
};

void ConVarHandler::ConVarBuiltins::find(std::string_view args) {
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

void ConVarHandler::ConVarBuiltins::help(std::string_view args) {
    SString::trim_inplace(args);

    if(args.length() < 1) {
        Logger::logRaw("Usage:  help <cvarname>");
        Logger::logRaw("To get a list of all available commands, type \"listcommands\".");
        return;
    }

    const std::vector<ConVar *> matches = cvars->getConVarByLetter(args);

    if(matches.size() < 1) {
        Logger::logRaw("ConVar {:s} does not exist.", args);
        return;
    }

    // use closest match
    size_t index = 0;
    for(size_t i = 0; i < matches.size(); i++) {
        if(matches[i]->getName() == args) {
            index = i;
            break;
        }
    }
    ConVar *match = matches[index];

    std::string_view helpstring = match->getHelpstring();
    if(helpstring.length() < 1) {
        Logger::logRaw("ConVar {:s} does not have a helpstring.", match->getName());
        return;
    }

    std::string thelog{match->getName()};
    {
        if(match->canHaveValue()) {
            const auto &cv_str = match->getString();
            const auto &default_str = match->getDefaultString();
            thelog.append(fmt::format(" = {:s} ( def. \"{:s}\" , ", cv_str, default_str));
            thelog.append(ConVar::typeToString(match->getType()));
            thelog.append(", ");
            thelog.append(ConVarHandler::flagsToString(match->getFlags()));
            thelog.append(" )");
        }

        thelog.append(" - ");
        thelog.append(helpstring);
    }
    Logger::logRaw("{:s}", thelog);
}

void ConVarHandler::ConVarBuiltins::listcommands(void) {
    Logger::logRaw("----------------------------------------------");
    {
        std::vector<ConVar *> convars = cvars->getConVarArray();
        std::ranges::sort(convars, {}, [](const ConVar *v) { return v->getName(); });

        for(auto &convar : convars) {
            if(convar->isFlagSet(cv::HIDDEN)) continue;

            ConVar *var = convar;

            std::string tstring{var->getName()};
            {
                if(var->canHaveValue()) {
                    const auto &var_str = var->getString();
                    const auto &default_str = var->getDefaultString();
                    tstring.append(fmt::format(" = {:s} ( def. \"{:s}\" , ", var_str, default_str));
                    tstring.append(ConVar::typeToString(var->getType()));
                    tstring.append(", ");
                    tstring.append(ConVarHandler::flagsToString(var->getFlags()));
                    tstring.append(" )");
                }

                if(std::string_view{var->getHelpstring()}.length() > 0) {
                    tstring.append(" - ");
                    tstring.append(var->getHelpstring());
                }
            }
            Logger::logRaw("{:s}", tstring);
        }
    }
    Logger::logRaw("----------------------------------------------");
}

void ConVarHandler::ConVarBuiltins::dumpcommands(void) {
    // in assets/misc/convar_template.html
    std::string html_template{reinterpret_cast<const char *>(convar_template),
                              static_cast<size_t>(convar_template_size())};

    std::vector<ConVar *> convars = cvars->getConVarArray();
    std::ranges::sort(convars, {}, [](const ConVar *v) { return v->getName(); });

    std::string html = R"(<section class="variables">)";
    for(auto var : convars) {
        // only doing this because of some stupid spurious warning with LTO
#define STRIF_(FLAG__, flag__) var->isFlagSet(cv::FLAG__) ? "<span class=\"flag " #flag__ "\">" #FLAG__ "</span>" : ""
        const std::string flags = fmt::format("\n{}{}{}{}{}\n",              //
                                              STRIF_(CLIENT, client),        //
                                              STRIF_(SKINS, skins),          //
                                              STRIF_(SERVER, server),        //
                                              STRIF_(PROTECTED, protected),  //
                                              STRIF_(GAMEPLAY, gameplay));   //
#undef STRIF_

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

    io->write(MCENGINE_DATA_DIR "variables.htm", html_template, [](bool success) -> void {
        if(success) {
            Logger::logRaw("ConVars dumped to variables.htm");
        } else {
            Logger::logRaw("Failed to dump ConVars to variables.htm");
        }
    });
}

void ConVarHandler::ConVarBuiltins::echo(std::string_view args) {
    if(args.length() > 0) {
        Logger::logRaw("{:s}", args);
    }
}

#undef CONVARDEFS_H
#define DEFINE_CONVARS

#include "ConVarDefs.h"
