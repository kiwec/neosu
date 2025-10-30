// Copyright (c) 2011, PG & 2025, WH & 2025, kiwec, All rights reserved.
#include "ConVar.h"
#include "ConVarHandler.h"

#include "File.h"
#include "Logging.h"

// TODO: remove the need for these here
#include "Osu.h"
#include "Bancho.h"

// set by app, shared across all convars, called when a protected convar changes
ConVar::CVVoidCB ConVar::onSetValueProtectedCallback{};

void ConVar::setOnSetValueProtectedCallback(const CVVoidCB &callback) {
    ConVar::onSetValueProtectedCallback = callback;
}

void ConVar::addConVar() {
    std::string_view name = this->getName();

    // osu_ prefix is deprecated.
    // If you really need it, you'll also need to edit Console::execConfigFile to whitelist it there.
    assert(!(name.starts_with("osu_") && !name.starts_with("osu_folder")) && "osu_ ConVar prefix is deprecated.");

    auto &convar_map = ConVarHandler::getConVarMap_int();

    // No duplicate ConVar names allowed
    assert(!convar_map.contains(name) && "no duplicate ConVar names allowed.");

    convar_map.emplace(name, this);
    ConVarHandler::getConVarArray_int().push_back(this);
}

std::string ConVar::getFancyDefaultValue() {
    switch(this->getType()) {
        case CONVAR_TYPE::BOOL:
            return this->dDefaultValue == 0 ? "false" : "true";
        case CONVAR_TYPE::INT:
            return std::to_string((int)this->dDefaultValue);
        case CONVAR_TYPE::FLOAT:
            return std::to_string(this->dDefaultValue);
        case CONVAR_TYPE::STRING: {
            std::string out = "\"";
            out.append(this->sDefaultValue);
            out.append("\"");
            return out;
        }
    }

    return "unreachable";
}

std::string ConVar::typeToString(CONVAR_TYPE type) {
    switch(type) {
        case CONVAR_TYPE::BOOL:
            return "bool";
        case CONVAR_TYPE::INT:
            return "int";
        case CONVAR_TYPE::FLOAT:
            return "float";
        case CONVAR_TYPE::STRING:
            return "string";
    }

    return "";
}

void ConVar::exec() {
    if(auto *cb = std::get_if<CVVoidCB>(&this->callback)) (*cb)();
}

void ConVar::execArgs(std::string_view args) {
    if(auto *cb = std::get_if<CVStringCB>(&this->callback)) (*cb)(args);
}

void ConVar::execFloat(float args) {
    if(auto *cb = std::get_if<CVFloatCB>(&this->callback)) (*cb)(args);
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

const std::string &ConVar::getString() const {
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
