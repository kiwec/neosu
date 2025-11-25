// Copyright (c) 2011, PG & 2025, WH & 2025, kiwec, All rights reserved.
#pragma once
#include "BaseEnvironment.h"
#include "templates.h"

#include <vector>
#include <string>
#include <string_view>
#include <memory>

using std::string_view_literals::operator""sv;

class ConVar;

class ConVarHandler {
    NOCOPY_NOMOVE(ConVarHandler)
   public:
    static std::string flagsToString(uint8_t flags);

   public:
    struct ConVarBuiltins;

    ConVarHandler();
    ~ConVarHandler() = default;

    [[nodiscard]] forceinline const std::vector<ConVar *> &getConVarArray() { return this->vConVarArray; }
    [[nodiscard]] forceinline const sv_unordered_map<ConVar *> &getConVarMap() { return this->vConVarMap; }
    [[nodiscard]] forceinline const ConVar *getConVar(std::string_view name) {
        return static_cast<const ConVar *>(getConVar_int(name));
    }

    [[nodiscard]] forceinline size_t getNumConVars() { return getConVarArray().size(); }
    [[nodiscard]] size_t getTotalMemUsageBytes();

    [[nodiscard]] ConVar *getConVarByName(std::string_view name, bool warnIfNotFound = true) const;
    [[nodiscard]] std::vector<ConVar *> getConVarByLetter(std::string_view letters) const;

    [[nodiscard]] std::vector<ConVar *> getNonSubmittableCvars() const;
    bool areAllCvarsSubmittable();

    // HACKHACK: terrible API (currently necessary for making caching work 100% reliably)
    void invalidateAllProtectedCaches();

    void resetServerCvars();
    void resetSkinCvars();

    bool removeServerValue(std::string_view cvarName);

    // extra check run during areAllCvarsSubmittable
    using CVSubmittableCriteriaFunc = bool (*)();
    void setCVSubmittableCheckFunc(CVSubmittableCriteriaFunc func);

   private:
    friend class ConVar;

    CVSubmittableCriteriaFunc areAllCvarsSubmittableExtraCheck{nullptr};
    std::vector<ConVar *> vConVarArray;
    sv_unordered_map<ConVar *> vConVarMap;

    [[nodiscard]] ConVar *getConVar_int(std::string_view name) const;
};

extern ConVarHandler &cvars();
