// Copyright (c) 2011, PG & 2025, WH & 2025, kiwec, All rights reserved.
#ifndef CONVAR_H
#define CONVAR_H

#include <atomic>
#include <cassert>
#include <string>
#include <variant>
#include <type_traits>

#include "Delegate.h"
#include "UString.h"

#ifndef DEFINE_CONVARS
#include "ConVarDefs.h"
#endif

// use a more compact string representation for each ConVar object, instead of UString
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

class ConVar {
    // convenience for "tricking" clangd/intellisense into allowing us to use a namespace for ConVarHandler in ConVarDefs.h
#ifndef DEFINE_CONVARS
    friend class ConVarHandler;
#endif

   public:
    enum class CONVAR_TYPE : uint8_t { BOOL, INT, FLOAT, STRING };
    enum class CvarEditor : uint8_t { CLIENT, SERVER, SKIN };
    enum class ProtectionPolicy : uint8_t { DEFAULT, PROTECTED, UNPROTECTED };

    // callback typedefs using Kryukov delegates
    using CVVoidCB = SA::delegate<void()>;
    using CVStringCB = SA::delegate<void(std::string_view)>;
    using CVStringChangeCB = SA::delegate<void(std::string_view, std::string_view)>;
    using CVFloatCB = SA::delegate<void(float)>;
    using CBFloatChangeCB = SA::delegate<void(float, float)>;

    // polymorphic callback storage
    using ExecCallback = std::variant<std::monostate,  // empty
                                      CVVoidCB,        // void()
                                      CVStringCB,      // void(std::string_view)
                                      CVFloatCB        // void(float)
                                      >;

    using ChangeCB = std::variant<std::monostate,    // empty
                                  CVStringChangeCB,  // void(std::string_view, std::string_view)
                                  CBFloatChangeCB    // void(float, float)
                                  >;

    template <typename... Args>
    static inline constexpr bool cb_invocable = std::is_invocable_v<Args...>;

   private:
    // type detection helper
    template <typename T>
    static constexpr CONVAR_TYPE getTypeFor() {
        if constexpr(std::is_same_v<std::decay_t<T>, bool>)
            return CONVAR_TYPE::BOOL;
        else if constexpr(std::is_integral_v<std::decay_t<T>>)
            return CONVAR_TYPE::INT;
        else if constexpr(std::is_floating_point_v<std::decay_t<T>>)
            return CONVAR_TYPE::FLOAT;
        else
            return CONVAR_TYPE::STRING;
    }

    void addConVar();

   public:
    static std::string typeToString(CONVAR_TYPE type);

   public:
    // command-only constructor
    explicit ConVar(const char *name, uint8_t flags = cv::CLIENT) : sName(name), sHelpString(""), sDefaultValue(name) {
        this->type = CONVAR_TYPE::STRING;
        this->iFlags = cv::NOSAVE | flags;
        this->addConVar();
    };

    // callback-only constructors (no value)
    template <typename Callback>
    explicit ConVar(const char *name, uint8_t flags, Callback callback)
        requires cb_invocable<Callback> || cb_invocable<Callback, std::string_view> || cb_invocable<Callback, float>
        : sName(name), sHelpString("") {
        this->initCallback(flags, callback);
        this->addConVar();
    }

    template <typename Callback>
    explicit ConVar(const char *name, uint8_t flags, const char *helpString, Callback callback)
        requires cb_invocable<Callback> || cb_invocable<Callback, std::string_view> || cb_invocable<Callback, float>
        : sName(name), sHelpString(helpString) {
        this->initCallback(flags, callback);
        this->addConVar();
    }

    // value constructors handle all types uniformly
    template <typename T>
    explicit ConVar(const char *name, T defaultValue, uint8_t flags, const char *helpString = "")
        requires(!std::is_same_v<std::decay_t<T>, const char *>)
        : sName(name), sHelpString(helpString) {
        this->initValue(defaultValue, flags, nullptr);
        this->addConVar();
    }

    template <typename T, typename Callback>
    explicit ConVar(const char *name, T defaultValue, uint8_t flags, const char *helpString, Callback callback)
        requires(!std::is_same_v<std::decay_t<T>, const char *>) &&
                    (cb_invocable<Callback> || cb_invocable<Callback, std::string_view> ||
                     cb_invocable<Callback, float> || cb_invocable<Callback, std::string_view, std::string_view> ||
                     cb_invocable<Callback, float, float>)
        : sName(name), sHelpString(helpString) {
        this->initValue(defaultValue, flags, callback);
        this->addConVar();
    }

    template <typename T, typename Callback>
    explicit ConVar(const char *name, T defaultValue, uint8_t flags, Callback callback)
        requires(!std::is_same_v<std::decay_t<T>, const char *>) &&
                    (cb_invocable<Callback> || cb_invocable<Callback, std::string_view> ||
                     cb_invocable<Callback, float> || cb_invocable<Callback, std::string_view, std::string_view> ||
                     cb_invocable<Callback, float, float>)
        : sName(name), sHelpString("") {
        this->initValue(defaultValue, flags, callback);
        this->addConVar();
    }

    // const char* specializations for string convars
    explicit ConVar(const char *name, std::string_view defaultValue, uint8_t flags, const char *helpString = "")
        : sName(name), sHelpString(helpString) {
        this->initValue(defaultValue, flags, nullptr);
        this->addConVar();
    }

    template <typename Callback>
    explicit ConVar(const char *name, std::string_view defaultValue, uint8_t flags, const char *helpString,
                    Callback callback)
        requires(cb_invocable<Callback> || cb_invocable<Callback, std::string_view> || cb_invocable<Callback, float> ||
                 cb_invocable<Callback, std::string_view, std::string_view> || cb_invocable<Callback, float, float>)
        : sName(name), sHelpString(helpString) {
        this->initValue(defaultValue, flags, callback);
        this->addConVar();
    }

    template <typename Callback>
    explicit ConVar(const char *name, std::string_view defaultValue, uint8_t flags, Callback callback)
        requires(cb_invocable<Callback> || cb_invocable<Callback, std::string_view> || cb_invocable<Callback, float> ||
                 cb_invocable<Callback, std::string_view, std::string_view> || cb_invocable<Callback, float, float>)
        : sName(name), sHelpString("") {
        this->initValue(defaultValue, flags, callback);
        this->addConVar();
    }

    // callbacks
    void exec();
    void execArgs(std::string_view args);
    void execFloat(float args);

    template <typename T>
    void setValue(T &&value, bool doCallback = true, CvarEditor editor = CvarEditor::CLIENT) {
        if(!this->bHasValue) return; // ignore command convars
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
        requires(cb_invocable<Callback> || cb_invocable<Callback, std::string_view> || cb_invocable<Callback, float> ||
                 cb_invocable<Callback, std::string_view, std::string_view> || cb_invocable<Callback, float, float>)
    {
        if constexpr(cb_invocable<Callback>)
            this->callback = CVVoidCB(std::forward<Callback>(callback));
        else if constexpr(cb_invocable<Callback, std::string_view>)
            this->callback = CVStringCB(std::forward<Callback>(callback));
        else if constexpr(cb_invocable<Callback, float>)
            this->callback = CVFloatCB(std::forward<Callback>(callback));
        else if constexpr(cb_invocable<Callback, std::string_view, std::string_view>)
            this->changeCallback = CVStringChangeCB(std::forward<Callback>(callback));
        else if constexpr(cb_invocable<Callback, float, float>)
            this->changeCallback = CBFloatChangeCB(std::forward<Callback>(callback));
        else
            static_assert(Env::always_false_v<Callback>, "Unsupported callback signature");
    }

    inline void removeCallback() { this->callback = std::monostate(); }
    inline void removeChangeCallback() { this->changeCallback = std::monostate(); }
    inline void removeAllCallbacks() {
        this->removeCallback();
        this->removeChangeCallback();
    }

    // get
    [[nodiscard]] inline float getDefaultFloat() const { return static_cast<float>(this->dDefaultValue); }
    [[nodiscard]] inline double getDefaultDouble() const { return this->dDefaultValue; }
    [[nodiscard]] inline const std::string &getDefaultString() const { return this->sDefaultValue; }

    void setDefaultDouble(double defaultValue);
    void setDefaultString(std::string_view defaultValue);

    std::string getFancyDefaultValue();

    [[nodiscard]] double getDouble() const;
    [[nodiscard]] const std::string &getString() const;

    template <typename T = int>
    [[nodiscard]] inline auto getVal() const {
        return static_cast<T>(this->getDouble());
    }

    [[nodiscard]] inline int getInt() const { return this->getVal<int>(); }
    [[nodiscard]] inline bool getBool() const { return !!this->getVal<int>(); }
    [[nodiscard]] inline bool get() const { return this->getBool(); }
    [[nodiscard]] inline float getFloat() const { return this->getVal<float>(); }

    [[nodiscard]] inline const char *getHelpstring() const { return this->sHelpString; }
    [[nodiscard]] inline const char *getName() const { return this->sName; }
    [[nodiscard]] inline CONVAR_TYPE getType() const { return this->type; }
    [[nodiscard]] inline uint8_t getFlags() const { return this->iFlags; }

    [[nodiscard]] inline bool hasValue() const { return this->bHasValue; }

    [[nodiscard]] inline bool hasAnyCallbacks() const {
        return !std::holds_alternative<std::monostate>(this->callback) ||
               !std::holds_alternative<std::monostate>(this->changeCallback);
    }

    [[nodiscard]] inline bool hasAnyNonVoidCallback() const {
        return std::holds_alternative<CVStringCB>(this->callback) ||
               std::holds_alternative<CVFloatCB>(this->callback) ||
               !std::holds_alternative<std::monostate>(this->changeCallback);
    }

    [[nodiscard]] inline bool hasVoidCallback() const { return std::holds_alternative<CVVoidCB>(this->callback); }

    [[nodiscard]] inline bool hasSingleArgCallback() const {
        return std::holds_alternative<CVStringCB>(this->callback) || std::holds_alternative<CVFloatCB>(this->callback);
    }

    [[nodiscard]] inline bool hasChangeCallback() const {
        return !std::holds_alternative<std::monostate>(this->changeCallback);
    }

    [[nodiscard]] inline bool isFlagSet(uint8_t flag) const { return ((this->iFlags & flag) == flag); }
    [[nodiscard]] inline bool isDefault() const { return this->getString() == this->getDefaultString(); }

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

    // shared callback, app-defined
    static void setOnSetValueProtectedCallback(const CVVoidCB &callback);

   private:
    // invalidates replay, returns true if value change should be allowed
    [[nodiscard]] bool onSetValueGameplay(CvarEditor editor);

    // unified init for callback-only convars
    template <typename Callback>
    void initCallback(uint8_t flags, Callback callback) {
        this->iFlags = flags | cv::NOSAVE;

        if constexpr(cb_invocable<Callback>) {
            this->callback = CVVoidCB(callback);
            this->type = CONVAR_TYPE::STRING;
        } else if constexpr(cb_invocable<Callback, std::string_view>) {
            this->callback = CVStringCB(callback);
            this->type = CONVAR_TYPE::STRING;
        } else if constexpr(cb_invocable<Callback, float>) {
            this->callback = CVFloatCB(callback);
            this->type = CONVAR_TYPE::INT;
        }
    }

    // unified init for value convars
    template <typename T, typename Callback>
    void initValue(const T &defaultValue, uint8_t flags, Callback callback) {
        this->bHasValue = true;
        this->iFlags = flags;
        this->type = getTypeFor<T>();

        if constexpr((std::is_convertible_v<std::decay_t<T>, double> || std::is_same_v<T, bool>) &&
                     !std::is_same_v<std::decay_t<T>, UString> && !std::is_same_v<std::decay_t<T>, std::string_view> &&
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
            if constexpr(cb_invocable<Callback>)
                this->callback = CVVoidCB(callback);
            else if constexpr(cb_invocable<Callback, std::string_view>)
                this->callback = CVStringCB(callback);
            else if constexpr(cb_invocable<Callback, float>)
                this->callback = CVFloatCB(callback);
            else if constexpr(cb_invocable<Callback, std::string_view, std::string_view>)
                this->changeCallback = CVStringChangeCB(callback);
            else if constexpr(cb_invocable<Callback, float, float>)
                this->changeCallback = CBFloatChangeCB(callback);
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
            } else if constexpr(std::is_same_v<T, bool>) {
                const auto f = static_cast<double>(value ? 1. : 0.);
                return std::make_pair(f, value ? "true" : "false");
            } else if constexpr(std::is_same_v<std::decay_t<T>, UString>) {
                const UString s = std::forward<T>(value);
                const double f = !s.isEmpty() ? s.toDouble() : 0.;
                return std::make_pair(f, std::string{s.toUtf8()});
            } else {
                const std::string s{std::forward<T>(value)};
                const double f = !s.empty() ? std::strtod(s.c_str(), nullptr) : 0.;
                return std::make_pair(f, s);
            }
        }();

        // backup old values, for passing into callbacks
        double oldDouble{this->getDouble()};
        std::string oldString;
        if(doCallback) {
            oldString = this->getString();
        }

        // set new values
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

        // run protected value change cb
        if(this->isProtected() && oldDouble != newDouble && likely(!ConVar::onSetValueProtectedCallback.isNull())) {
            ConVar::onSetValueProtectedCallback();
        }

        if(doCallback) {
            // handle possible execution callbacks
            if(!std::holds_alternative<std::monostate>(this->callback)) {
                std::visit(
                    [&](auto &&callback) {
                        using CBType = std::decay_t<decltype(callback)>;
                        if constexpr(std::is_same_v<CBType, CVVoidCB>)
                            callback();
                        else if constexpr(std::is_same_v<CBType, CVStringCB>)
                            callback(newString);
                        else if constexpr(std::is_same_v<CBType, CVFloatCB>)
                            callback(static_cast<float>(newDouble));
                    },
                    this->callback);
            }

            // handle possible change callbacks
            if(!std::holds_alternative<std::monostate>(this->changeCallback)) {
                std::visit(
                    [&](auto &&callback) {
                        using CBType = std::decay_t<decltype(callback)>;
                        if constexpr(std::is_same_v<CBType, CVStringChangeCB>)
                            callback(oldString, newString);
                        else if constexpr(std::is_same_v<CBType, CBFloatChangeCB>)
                            callback(static_cast<float>(oldDouble), static_cast<float>(newDouble));
                    },
                    this->changeCallback);
            }
        }
    }

   private:
    // shared across all convars
    static CVVoidCB onSetValueProtectedCallback;

    const char *sName;
    const char *sHelpString;
    std::string sDefaultValue{};
    double dDefaultValue{0.0};

    std::atomic<double> dClientValue{0.0};
    std::string sClientValue{};

    std::atomic<double> dSkinValue{0.0};
    std::string sSkinValue{};

    std::atomic<double> dServerValue{0.0};
    std::string sServerValue{};

    // callback storage (allow having 1 "change" callback and 1 single value (or void) callback)
    ExecCallback callback{std::monostate()};
    ChangeCB changeCallback{std::monostate()};

    std::atomic<ProtectionPolicy> serverProtectionPolicy{ProtectionPolicy::DEFAULT};

    CONVAR_TYPE type{CONVAR_TYPE::FLOAT};
    uint8_t iFlags{0};

    bool bHasValue{false};
    std::atomic<bool> hasServerValue{false};
    std::atomic<bool> hasSkinValue{false};
};

#endif
