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

    ConVarHandler() = default;
    ~ConVarHandler() = default;

    [[nodiscard]] static forceinline const std::vector<ConVar *> &getConVarArray() {
        return static_cast<const std::vector<ConVar *> &>(getConVarArray_int());
    }
    [[nodiscard]] static forceinline const sv_unordered_map<ConVar *> &getConVarMap() {
        return static_cast<const sv_unordered_map<ConVar *> &>(getConVarMap_int());
    }
    [[nodiscard]] static forceinline const ConVar *getConVar(std::string_view name) {
        return static_cast<const ConVar *>(getConVar_int(name));
    }

    [[nodiscard]] static forceinline size_t getNumConVars() { return getConVarArray().size(); }

    [[nodiscard]] ConVar *getConVarByName(std::string_view name, bool warnIfNotFound = true) const;
    [[nodiscard]] std::vector<ConVar *> getConVarByLetter(std::string_view letters) const;

    [[nodiscard]] std::vector<ConVar *> getNonSubmittableCvars() const;
    bool areAllCvarsSubmittable();

    void resetServerCvars();
    void resetSkinCvars();

    bool removeServerValue(std::string_view cvarName);

    // extra check run during areAllCvarsSubmittable
    using CVSubmittableCriteriaFunc = bool (*)();
    static inline void setCVSubmittableCheckFunc(CVSubmittableCriteriaFunc func) {
        ConVarHandler::areAllCvarsSubmittableExtraCheck = func;
    }

   private:
    friend class ConVar;

    static CVSubmittableCriteriaFunc areAllCvarsSubmittableExtraCheck;

    [[nodiscard]] static std::vector<ConVar *> &getConVarArray_int();
    [[nodiscard]] static sv_unordered_map<ConVar *> &getConVarMap_int();
    [[nodiscard]] static ConVar *getConVar_int(std::string_view name);
};

extern std::unique_ptr<ConVarHandler> cvars;
