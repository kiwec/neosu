// Copyright (c) 2011, PG & 2025, WH & 2025, kiwec, All rights reserved.
#ifndef CONVAR_H
#define CONVAR_H

#include <atomic>
#include <string>
#include <variant>
#include <type_traits>
#include <memory>

#include "Delegate.h"
#include "templates.h"
#include "UString.h"

#ifndef DEFINE_CONVARS
#include "ConVarDefs.h"
#endif

// use a more compact string representation for each ConVar object, instead of UString
using ConVarString = std::string;
using std::string_view_literals::operator""sv;

namespace cv {
enum CvarFlags : uint8_t {
    // Modifiable by clients
    CLIENT = (1 << 0),

    // Modifiable by servers, OR by offline clients
    SERVER = (1 << 1),

    // Modifiable by skins
    // TODO: assert() CLIENT is set
    SKINS = (1 << 2),

    // Scores won't submit if modified
    PROTECTED = (1 << 3),

    // Scores won't submit if modified during gameplay
    GAMEPLAY = (1 << 4),

    // Hidden from console suggestions (e.g. for passwords or deprecated cvars)
    HIDDEN = (1 << 5),

    // Don't save this cvar to configs
    NOSAVE = (1 << 6),

    // Don't load this cvar from configs
    NOLOAD = (1 << 7),

    // Mark the variable as intended for use only inside engine code
    // NOTE: This is intended to be used without any other flags
    CONSTANT = HIDDEN | NOLOAD | NOSAVE,
};
}

class ConVarHandler;

class ConVar {
    friend class ConVarHandler;

   public:
    enum class CONVAR_TYPE : uint8_t { CONVAR_TYPE_BOOL, CONVAR_TYPE_INT, CONVAR_TYPE_FLOAT, CONVAR_TYPE_STRING };
    enum class CvarEditor : uint8_t { CLIENT, SERVER, SKIN };
    enum class ProtectionPolicy : uint8_t { DEFAULT, PROTECTED, UNPROTECTED };

    // callback typedefs using Kryukov delegates
    using NativeConVarCallback = SA::delegate<void()>;
    using NativeConVarCallbackArgs = SA::delegate<void(std::string_view)>;
    using NativeConVarChangeCallback = SA::delegate<void(std::string_view, std::string_view)>;
    using NativeConVarCallbackFloat = SA::delegate<void(float)>;
    using NativeConVarChangeCallbackFloat = SA::delegate<void(float, float)>;

    // polymorphic callback storage
    using ExecutionCallback = std::variant<std::monostate,            // empty
                                           NativeConVarCallback,      // void()
                                           NativeConVarCallbackArgs,  // void(std::string_view)
                                           NativeConVarCallbackFloat  // void(float)
                                           >;

    using ChangeCallback = std::variant<std::monostate,                  // empty
                                        NativeConVarChangeCallback,      // void(std::string_view, std::string_view)
                                        NativeConVarChangeCallbackFloat  // void(float, float)
                                        >;

   private:
    // type detection helper
    template <typename T>
    static constexpr CONVAR_TYPE getTypeFor() {
        if constexpr(std::is_same_v<std::decay_t<T>, bool>)
            return CONVAR_TYPE::CONVAR_TYPE_BOOL;
        else if constexpr(std::is_integral_v<std::decay_t<T>>)
            return CONVAR_TYPE::CONVAR_TYPE_INT;
        else if constexpr(std::is_floating_point_v<std::decay_t<T>>)
            return CONVAR_TYPE::CONVAR_TYPE_FLOAT;
        else
            return CONVAR_TYPE::CONVAR_TYPE_STRING;
    }

    static void addConVar(ConVar *);

   public:
    static ConVarString typeToString(CONVAR_TYPE type);

   public:
    // command-only constructor
    explicit ConVar(std::string_view name) {
        this->sName = this->sDefaultValue = name;
        this->type = CONVAR_TYPE::CONVAR_TYPE_STRING;
        this->iFlags = cv::NOSAVE;
        this->addConVar(this);
    };

    // callback-only constructors (no value)
    template <typename Callback>
    explicit ConVar(std::string_view name, uint8_t flags, Callback callback)
        requires std::is_invocable_v<Callback> || std::is_invocable_v<Callback, std::string_view> ||
                 std::is_invocable_v<Callback, float>
    {
        this->initCallback(name, flags, ""sv, callback);
        this->addConVar(this);
    }

    template <typename Callback>
    explicit ConVar(std::string_view name, uint8_t flags, std::string_view helpString, Callback callback)
        requires std::is_invocable_v<Callback> || std::is_invocable_v<Callback, std::string_view> ||
                 std::is_invocable_v<Callback, float>
    {
        this->initCallback(name, flags, helpString, callback);
        this->addConVar(this);
    }

    // value constructors handle all types uniformly
    template <typename T>
    explicit ConVar(std::string_view name, T defaultValue, uint8_t flags, std::string_view helpString = ""sv)
        requires(!std::is_same_v<std::decay_t<T>, const char *>)
    {
        this->initValue(name, defaultValue, flags, helpString, nullptr);
        this->addConVar(this);
    }

    template <typename T, typename Callback>
    explicit ConVar(std::string_view name, T defaultValue, uint8_t flags, std::string_view helpString,
                    Callback callback)
        requires(!std::is_same_v<std::decay_t<T>, const char *>) &&
                (std::is_invocable_v<Callback> || std::is_invocable_v<Callback, std::string_view> ||
                 std::is_invocable_v<Callback, float> ||
                 std::is_invocable_v<Callback, std::string_view, std::string_view> ||
                 std::is_invocable_v<Callback, float, float>)
    {
        this->initValue(name, defaultValue, flags, helpString, callback);
        this->addConVar(this);
    }

    template <typename T, typename Callback>
    explicit ConVar(std::string_view name, T defaultValue, uint8_t flags, Callback callback)
        requires(!std::is_same_v<std::decay_t<T>, const char *>) &&
                (std::is_invocable_v<Callback> || std::is_invocable_v<Callback, std::string_view> ||
                 std::is_invocable_v<Callback, float> ||
                 std::is_invocable_v<Callback, std::string_view, std::string_view> ||
                 std::is_invocable_v<Callback, float, float>)
    {
        this->initValue(name, defaultValue, flags, ""sv, callback);
        this->addConVar(this);
    }

    // const char* specializations for string convars
    explicit ConVar(std::string_view name, std::string_view defaultValue, uint8_t flags,
                    std::string_view helpString = ""sv) {
        this->initValue(name, defaultValue, flags, helpString, nullptr);
        this->addConVar(this);
    }

    template <typename Callback>
    explicit ConVar(std::string_view name, std::string_view defaultValue, uint8_t flags, std::string_view helpString,
                    Callback callback)
        requires(std::is_invocable_v<Callback> || std::is_invocable_v<Callback, std::string_view> ||
                 std::is_invocable_v<Callback, float> ||
                 std::is_invocable_v<Callback, std::string_view, std::string_view> ||
                 std::is_invocable_v<Callback, float, float>)
    {
        this->initValue(name, defaultValue, flags, helpString, callback);
        this->addConVar(this);
    }

    template <typename Callback>
    explicit ConVar(std::string_view name, std::string_view defaultValue, uint8_t flags, Callback callback)
        requires(std::is_invocable_v<Callback> || std::is_invocable_v<Callback, std::string_view> ||
                 std::is_invocable_v<Callback, float> ||
                 std::is_invocable_v<Callback, std::string_view, std::string_view> ||
                 std::is_invocable_v<Callback, float, float>)
    {
        this->initValue(name, defaultValue, flags, ""sv, callback);
        this->addConVar(this);
    }

    // callbacks
    void exec();
    void execArgs(std::string_view args);
    void execFloat(float args);

    template <typename T>
    void setValue(T &&value, bool doCallback = true, CvarEditor editor = CvarEditor::CLIENT) {
        if(editor == CvarEditor::CLIENT && !this->isFlagSet(cv::CLIENT)) return;
        if(editor == CvarEditor::SKIN && !this->isFlagSet(cv::SKINS)) return;
        if(editor == CvarEditor::SERVER && !this->isFlagSet(cv::SERVER)) return;

        bool can_set_value = true;
        if(this->isFlagSet(cv::GAMEPLAY)) {
            can_set_value &= this->onSetValueGameplay(editor);
        }

        if(can_set_value) {
            this->setValueInt(std::forward<T>(value), doCallback, editor);
        }
    }

    // generic callback setter that auto-detects callback type
    template <typename Callback>
    void setCallback(Callback &&callback)
        requires(std::is_invocable_v<Callback> || std::is_invocable_v<Callback, std::string_view> ||
                 std::is_invocable_v<Callback, float> ||
                 std::is_invocable_v<Callback, std::string_view, std::string_view> ||
                 std::is_invocable_v<Callback, float, float>)
    {
        if constexpr(std::is_invocable_v<Callback>)
            this->callback = NativeConVarCallback(std::forward<Callback>(callback));
        else if constexpr(std::is_invocable_v<Callback, std::string_view>)
            this->callback = NativeConVarCallbackArgs(std::forward<Callback>(callback));
        else if constexpr(std::is_invocable_v<Callback, float>)
            this->callback = NativeConVarCallbackFloat(std::forward<Callback>(callback));
        else if constexpr(std::is_invocable_v<Callback, std::string_view, std::string_view>)
            this->changeCallback = NativeConVarChangeCallback(std::forward<Callback>(callback));
        else if constexpr(std::is_invocable_v<Callback, float, float>)
            this->changeCallback = NativeConVarChangeCallbackFloat(std::forward<Callback>(callback));
        else
            static_assert(Env::always_false_v<Callback>, "Unsupported callback signature");
    }

    // get
    [[nodiscard]] inline float getDefaultFloat() const { return static_cast<float>(this->dDefaultValue); }
    [[nodiscard]] inline double getDefaultDouble() const { return this->dDefaultValue; }
    [[nodiscard]] inline const ConVarString &getDefaultString() const { return this->sDefaultValue; }

    void setDefaultDouble(double defaultValue);
    void setDefaultString(std::string_view defaultValue);

    ConVarString getFancyDefaultValue();

    [[nodiscard]] double getDouble() const;
    [[nodiscard]] const ConVarString &getString() const;

    template <typename T = int>
    [[nodiscard]] inline auto getVal() const {
        return static_cast<T>(this->getDouble());
    }

    [[nodiscard]] inline int getInt() const { return this->getVal<int>(); }
    [[nodiscard]] inline bool getBool() const { return !!this->getVal<int>(); }
    [[nodiscard]] inline bool get() const { return this->getBool(); }
    [[nodiscard]] inline float getFloat() const { return this->getVal<float>(); }

    [[nodiscard]] inline const ConVarString &getHelpstring() const { return this->sHelpString; }
    [[nodiscard]] inline const ConVarString &getName() const { return this->sName; }
    [[nodiscard]] inline CONVAR_TYPE getType() const { return this->type; }
    [[nodiscard]] inline uint8_t getFlags() const { return this->iFlags; }

    [[nodiscard]] inline bool hasValue() const { return this->bHasValue; }

    [[nodiscard]] inline bool hasCallbackArgs() const {
        return std::holds_alternative<NativeConVarCallbackArgs>(this->callback) ||
               !std::holds_alternative<std::monostate>(this->changeCallback);
    }

    [[nodiscard]] inline bool isFlagSet(uint8_t flag) const { return (bool)((this->iFlags & flag) == flag); }

    void setServerProtected(ProtectionPolicy policy) {
        this->serverProtectionPolicy.store(policy, std::memory_order_release);
    }

    [[nodiscard]] inline bool isProtected() const {
        switch(this->serverProtectionPolicy.load(std::memory_order_acquire)) {
            case ProtectionPolicy::DEFAULT:
                return this->isFlagSet(cv::PROTECTED);
            case ProtectionPolicy::PROTECTED:
                return true;
            case ProtectionPolicy::UNPROTECTED:
            default:
                return false;
        }
    }

   private:
    // invalidates replay, returns true if value change should be allowed
    [[nodiscard]] bool onSetValueGameplay(CvarEditor editor);

    // prevents score submission
    void onSetValueProtected(std::string_view oldValue, std::string_view newValue);

    // unified init for callback-only convars
    template <typename Callback>
    void initCallback(std::string_view name, uint8_t flags, std::string_view helpString, Callback callback) {
        this->iFlags = flags | cv::NOSAVE;
        this->sName = name;
        this->sHelpString = helpString;

        if constexpr(std::is_invocable_v<Callback>) {
            this->callback = NativeConVarCallback(callback);
            this->type = CONVAR_TYPE::CONVAR_TYPE_STRING;
        } else if constexpr(std::is_invocable_v<Callback, std::string_view>) {
            this->callback = NativeConVarCallbackArgs(callback);
            this->type = CONVAR_TYPE::CONVAR_TYPE_STRING;
        } else if constexpr(std::is_invocable_v<Callback, float>) {
            this->callback = NativeConVarCallbackFloat(callback);
            this->type = CONVAR_TYPE::CONVAR_TYPE_INT;
        }
    }

    // unified init for value convars
    template <typename T, typename Callback>
    void initValue(std::string_view name, const T &defaultValue, uint8_t flags, std::string_view helpString,
                   Callback callback) {
        this->bValueSettable = true;
        this->iFlags = flags;
        this->sName = name;
        this->sHelpString = helpString;
        this->type = getTypeFor<T>();

        if constexpr(std::is_convertible_v<std::decay_t<T>, double> && !std::is_same_v<std::decay_t<T>, UString> &&
                     !std::is_same_v<std::decay_t<T>, std::string_view> &&
                     !std::is_same_v<std::decay_t<T>, const char *>) {
            // T is double-like
            this->setDefaultDouble(static_cast<double>(defaultValue));
        } else {
            // T is string-like
            this->setDefaultString(defaultValue);
        }

        this->sClientValue = this->sDefaultValue;
        this->sSkinValue = this->sDefaultValue;
        this->sServerValue = this->sDefaultValue;

        this->dClientValue.store(this->dDefaultValue, std::memory_order_relaxed);
        this->dSkinValue.store(this->dDefaultValue, std::memory_order_relaxed);
        this->dServerValue.store(this->dDefaultValue, std::memory_order_relaxed);

        // set callback if provided
        if constexpr(!std::is_same_v<Callback, std::nullptr_t>) {
            if constexpr(std::is_invocable_v<Callback>)
                this->callback = NativeConVarCallback(callback);
            else if constexpr(std::is_invocable_v<Callback, std::string_view>)
                this->callback = NativeConVarCallbackArgs(callback);
            else if constexpr(std::is_invocable_v<Callback, float>)
                this->callback = NativeConVarCallbackFloat(callback);
            else if constexpr(std::is_invocable_v<Callback, std::string_view, std::string_view>)
                this->changeCallback = NativeConVarChangeCallback(callback);
            else if constexpr(std::is_invocable_v<Callback, float, float>)
                this->changeCallback = NativeConVarChangeCallbackFloat(callback);
        }
    }

    // no flag checking, setValue (user-accessible) already does that
    template <typename T>
    void setValueInt(T &&value, bool doCallback, CvarEditor editor) {
        // determine double and string representations depending on whether setValue("string") or setValue(double) was
        // called
        const auto [newDouble, newString] = [&]() -> std::pair<double, std::string> {
            if constexpr(std::is_convertible_v<std::decay_t<T>, double> && !std::is_same_v<std::decay_t<T>, UString> &&
                         !std::is_same_v<std::decay_t<T>, std::string_view> &&
                         !std::is_same_v<std::decay_t<T>, const char *>) {
                const auto f = static_cast<double>(value);
                return std::make_pair(f, fmt::format("{:g}", f));
            } else if constexpr(std::is_same_v<std::decay_t<T>, UString>) {
                const UString s = std::forward<T>(value);
                const double f = !s.isEmpty() ? s.toDouble() : 0.;
                return std::make_pair(f, std::string{s.toUtf8()});
            } else {
                const ConVarString s{std::forward<T>(value)};
                const double f = !s.empty() ? std::strtod(s.c_str(), nullptr) : 0.;
                return std::make_pair(f, s);
            }
        }();

        // backup old values, for passing into callbacks
        double oldDouble{0.0};
        ConVarString oldString;
        if(doCallback) {
            oldDouble = this->getDouble();
            oldString = this->getString();
        }

        // set new values (if values are settable)
        if(this->bValueSettable) {
            this->bHasValue = true;
            switch(editor) {
                case CvarEditor::CLIENT: {
                    this->dClientValue.store(newDouble, std::memory_order_release);
                    this->sClientValue = newString;
                    break;
                }
                case CvarEditor::SKIN: {
                    this->dSkinValue.store(newDouble, std::memory_order_release);
                    this->sSkinValue = newString;
                    this->hasSkinValue.store(true, std::memory_order_release);
                    break;
                }
                case CvarEditor::SERVER: {
                    this->dServerValue.store(newDouble, std::memory_order_release);
                    this->sServerValue = newString;
                    this->hasServerValue.store(true, std::memory_order_release);
                    break;
                }
            }
        }

        // prevent score submission if the cvar was protected
        if(this->isProtected()) {
            this->onSetValueProtected(oldString, newString);
        }

        if(doCallback) {
            // handle possible execution callbacks
            if(!std::holds_alternative<std::monostate>(this->callback)) {
                std::visit(
                    [&](auto &&callback) {
                        using CallbackType = std::decay_t<decltype(callback)>;
                        if constexpr(std::is_same_v<CallbackType, NativeConVarCallback>)
                            callback();
                        else if constexpr(std::is_same_v<CallbackType, NativeConVarCallbackArgs>)
                            callback(newString);
                        else if constexpr(std::is_same_v<CallbackType, NativeConVarCallbackFloat>)
                            callback(static_cast<float>(newDouble));
                    },
                    this->callback);
            }

            // handle possible change callbacks
            if(!std::holds_alternative<std::monostate>(this->changeCallback)) {
                std::visit(
                    [&](auto &&callback) {
                        using CallbackType = std::decay_t<decltype(callback)>;
                        if constexpr(std::is_same_v<CallbackType, NativeConVarChangeCallback>)
                            callback(oldString, newString);
                        else if constexpr(std::is_same_v<CallbackType, NativeConVarChangeCallbackFloat>)
                            callback(static_cast<float>(oldDouble), static_cast<float>(newDouble));
                    },
                    this->changeCallback);
            }
        }
    }

   public:
    std::atomic<bool> hasServerValue{false};

   private:
    ConVarString sName;
    ConVarString sHelpString;
    double dDefaultValue{0.0};
    ConVarString sDefaultValue{};

    std::atomic<double> dClientValue{0.0};
    ConVarString sClientValue{};

    std::atomic<bool> hasSkinValue{false};
    std::atomic<double> dSkinValue{0.0};
    ConVarString sSkinValue{};

    std::atomic<double> dServerValue{0.0};
    ConVarString sServerValue{};
    std::atomic<ProtectionPolicy> serverProtectionPolicy{ProtectionPolicy::DEFAULT};

    // callback storage (allow having 1 "change" callback and 1 single value (or void) callback)
    ExecutionCallback callback{std::monostate()};
    ChangeCallback changeCallback{std::monostate()};

    CONVAR_TYPE type{CONVAR_TYPE::CONVAR_TYPE_FLOAT};
    uint8_t iFlags{0};

    bool bHasValue{false};
    bool bValueSettable{false};  // command convars can't have values stored in them
};

//*******************//
//  Searching/Lists  //
//*******************//

class ConVarHandler {
    NOCOPY_NOMOVE(ConVarHandler)
   public:
    static ConVarString flagsToString(uint8_t flags);

   public:
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

   private:
    friend class ConVar;

    [[nodiscard]] static std::vector<ConVar *> &getConVarArray_int();
    [[nodiscard]] static sv_unordered_map<ConVar *> &getConVarMap_int();
    [[nodiscard]] static ConVar *getConVar_int(std::string_view name);
};

extern std::unique_ptr<ConVarHandler> cvars;

#endif
