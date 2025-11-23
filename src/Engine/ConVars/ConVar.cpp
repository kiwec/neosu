// Copyright (c) 2011, PG & 2025, WH & 2025, kiwec, All rights reserved.
#include "ConVar.h"
#include "ConVarHandler.h"

#include "File.h"

// set by app, shared across all convars, called when a protected convar changes
ConVar::VoidCB ConVar::onSetValueProtectedCallback{};
void ConVar::setOnSetValueProtectedCallback(const VoidCB &callback) { ConVar::onSetValueProtectedCallback = callback; }

// ditto
ConVar::ProtectedCVGetCB ConVar::onGetValueProtectedCallback{nullptr};
void ConVar::setOnGetValueProtectedCallback(ProtectedCVGetCB func) { ConVar::onGetValueProtectedCallback = func; }

// ditto
ConVar::GameplayCVChangeCB ConVar::onSetValueGameplayCallback{nullptr};
void ConVar::setOnSetValueGameplayCallback(GameplayCVChangeCB func) { ConVar::onSetValueGameplayCallback = func; }

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
    if(auto *cb = std::get_if<VoidCB>(&this->callback)) (*cb)();
}

void ConVar::execArgs(std::string_view args) {
    if(auto *cb = std::get_if<StringCB>(&this->callback)) (*cb)(args);
}

void ConVar::execFloat(float args) {
    if(auto *cb = std::get_if<FloatCB>(&this->callback)) (*cb)(args);
}

void ConVar::execDouble(double args) {
    if(auto *cb = std::get_if<DoubleCB>(&this->callback)) (*cb)(args);
}

double ConVar::getDoubleInt() const {
    if(this->isFlagSet(cv::SERVER) && this->hasServerValue.load(std::memory_order_acquire)) {
        this->dCachedReturnedDouble = this->dServerValue.load(std::memory_order_acquire);
    } else if(this->isFlagSet(cv::SKINS) && this->hasSkinValue.load(std::memory_order_acquire)) {
        this->dCachedReturnedDouble = this->dSkinValue.load(std::memory_order_acquire);
    } else if(this->isProtected() &&
              (likely(!!ConVar::onGetValueProtectedCallback) && !ConVar::onGetValueProtectedCallback(this->sName))) {
        // FIXME: this is unreliable since onGetValueProtectedCallback might change arbitrarily,
        // need to invalidate cached state when that happens
        // currently relying on a cvars->invalidateAllProtectedCaches "loophole" (see Bancho.cpp),
        // so the API user needs to know the implementation details or else they'll keep getting default values :)
        this->dCachedReturnedDouble = this->dDefaultValue;
    } else {
        this->dCachedReturnedDouble = this->dClientValue.load(std::memory_order_acquire);
    }

    this->bUseCachedDouble.store(true, std::memory_order_release);
    return this->dCachedReturnedDouble;
}

const std::string &ConVar::getStringInt() const {
    if(this->isFlagSet(cv::SERVER) && this->hasServerValue.load(std::memory_order_acquire)) {
        this->sCachedReturnedString = this->sServerValue;
    } else if(this->isFlagSet(cv::SKINS) && this->hasSkinValue.load(std::memory_order_acquire)) {
        this->sCachedReturnedString = this->sSkinValue;
    } else if(this->isProtected() &&
              (likely(!!ConVar::onGetValueProtectedCallback) && !ConVar::onGetValueProtectedCallback(this->sName))) {
        this->sCachedReturnedString = this->sDefaultValue;
    } else {
        this->sCachedReturnedString = this->sClientValue;
    }

    this->bUseCachedString.store(true, std::memory_order_release);
    return this->sCachedReturnedString;
}

void ConVar::setDefaultDouble(double defaultValue) {
    this->dDefaultValue = defaultValue;
    this->sDefaultValue = fmt::format("{:g}", defaultValue);

    // FIXME: continued hacks from the protected value returning default value issue
    if(this->isFlagSet(cv::PROTECTED)) {
        this->invalidateCache();
    }
}

void ConVar::setDefaultString(std::string_view defaultValue) {
    this->sDefaultValue = defaultValue;

    // also try to parse default float from the default string
    const double f = std::strtod(this->sDefaultValue.c_str(), nullptr);
    if(f != 0.0) {
        this->dDefaultValue = f;
    }

    if(this->isFlagSet(cv::PROTECTED)) {
        this->invalidateCache();
    }
}
