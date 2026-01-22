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

enum class CvarEditor : uint8_t { CLIENT, SERVER, SKIN };
enum class CvarProtection : uint8_t { DEFAULT, PROTECTED, UNPROTECTED };

class ConVar {
    // convenience for "tricking" clangd/intellisense into allowing us to use a namespace for ConVarHandler in ConVarDefs.h
#ifndef DEFINE_CONVARS
    friend class ConVarHandler;
#endif

   public:
    enum class CONVAR_TYPE : uint8_t { BOOL, INT, FLOAT, STRING };

    // callback typedefs using Kryukov delegates
    using VoidCB = SA::delegate<void()>;
    using StringCB = SA::delegate<void(std::string_view)>;
    using StringChangeCB = SA::delegate<void(std::string_view, std::string_view)>;
    using FloatCB = SA::delegate<void(float)>;
    using DoubleCB = SA::delegate<void(double)>;
    using FloatChangeCB = SA::delegate<void(float, float)>;
    using DoubleChangeCB = SA::delegate<void(double, double)>;

    // polymorphic callback storage
    using ExecCallback = std::variant<std::monostate,  // empty
                                      VoidCB,          // void()
                                      StringCB,        // void(std::string_view)
                                      FloatCB,         // void(float)
                                      DoubleCB         // void(double)
                                      >;

    using ChangeCB = std::variant<std::monostate,  // empty
                                  StringChangeCB,  // void(std::string_view, std::string_view)
                                  FloatChangeCB,   // void(float, float)
                                  DoubleChangeCB   // void(double, double)
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
    explicit ConVar(const char *name, uint8_t flags, Callback &&callback)
        requires cb_invocable<Callback> || cb_invocable<Callback, std::string_view> || cb_invocable<Callback, float> ||
                     cb_invocable<Callback, double>
        : sName(name), sHelpString("") {
        this->initCallback(flags, std::forward<Callback>(callback));
        this->addConVar();
    }

    template <typename Callback>
    explicit ConVar(const char *name, uint8_t flags, const char *helpString, Callback &&callback)
        requires cb_invocable<Callback> || cb_invocable<Callback, std::string_view> || cb_invocable<Callback, float> ||
                     cb_invocable<Callback, double>
        : sName(name), sHelpString(helpString) {
        this->initCallback(flags, std::forward<Callback>(callback));
        this->addConVar();
    }

    // value constructors handle all types uniformly
    template <typename T>
    explicit ConVar(const char *name, T &&defaultValue, uint8_t flags, const char *helpString = "")
        requires(!std::is_same_v<std::decay_t<T>, const char *>)
        : sName(name), sHelpString(helpString) {
        this->initValue(std::forward<T>(defaultValue), flags, nullptr);
        this->addConVar();
    }

    template <typename T, typename Callback>
    explicit ConVar(const char *name, T &&defaultValue, uint8_t flags, const char *helpString, Callback &&callback)
        requires(!std::is_same_v<std::decay_t<T>, const char *>) &&
                    (cb_invocable<Callback> || cb_invocable<Callback, std::string_view> ||
                     cb_invocable<Callback, float> || cb_invocable<Callback, double> ||
                     cb_invocable<Callback, std::string_view, std::string_view> ||
                     cb_invocable<Callback, float, float> || cb_invocable<Callback, double, double>)
        : sName(name), sHelpString(helpString) {
        this->initValue(std::forward<T>(defaultValue), flags, std::forward<Callback>(callback));
        this->addConVar();
    }

    template <typename T, typename Callback>
    explicit ConVar(const char *name, T &&defaultValue, uint8_t flags, Callback &&callback)
        requires(!std::is_same_v<std::decay_t<T>, const char *>) &&
                    (cb_invocable<Callback> || cb_invocable<Callback, std::string_view> ||
                     cb_invocable<Callback, float> || cb_invocable<Callback, double> ||
                     cb_invocable<Callback, std::string_view, std::string_view> ||
                     cb_invocable<Callback, float, float> || cb_invocable<Callback, double, double>)
        : sName(name), sHelpString("") {
        this->initValue(std::forward<T>(defaultValue), flags, std::forward<Callback>(callback));
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
                    Callback &&callback)
        requires(cb_invocable<Callback> || cb_invocable<Callback, std::string_view> || cb_invocable<Callback, float> ||
                 cb_invocable<Callback, double> || cb_invocable<Callback, std::string_view, std::string_view> ||
                 cb_invocable<Callback, float, float> || cb_invocable<Callback, double, double>)
        : sName(name), sHelpString(helpString) {
        this->initValue(defaultValue, flags, std::forward<Callback>(callback));
        this->addConVar();
    }

    template <typename Callback>
    explicit ConVar(const char *name, std::string_view defaultValue, uint8_t flags, Callback &&callback)
        requires(cb_invocable<Callback> || cb_invocable<Callback, std::string_view> || cb_invocable<Callback, float> ||
                 cb_invocable<Callback, double> || cb_invocable<Callback, std::string_view, std::string_view> ||
                 cb_invocable<Callback, float, float> || cb_invocable<Callback, double, double>)
        : sName(name), sHelpString("") {
        this->initValue(defaultValue, flags, std::forward<Callback>(callback));
        this->addConVar();
    }

    // callbacks
    void exec();
    void execArgs(std::string_view args);
    void execFloat(float args);
    void execDouble(double args);

    template <typename T>
    void setValue(T &&value, bool doCallback = true, CvarEditor editor = CvarEditor::CLIENT) {
        // if(!this->bCanHaveValue) return; // ignore command convars
        // STUPID: even command convars need to run through all of this validation logic to run callbacks (like find)
        if(editor == CvarEditor::CLIENT && !this->isFlagSet(cv::CLIENT)) return;
        if(editor == CvarEditor::SKIN && !this->isFlagSet(cv::SKINS)) return;
        if(editor == CvarEditor::SERVER && !this->isFlagSet(cv::SERVER)) return;

        // if:
        // (NOT gameplay flag) OR ((NOT onSetValueGameplayCallback is set) OR (onSetValueGameplayCallback returns true))
        if(!this->isFlagSet(cv::GAMEPLAY) || (unlikely(!ConVar::onSetValueGameplayCallback) ||
                                              likely(ConVar::onSetValueGameplayCallback(this->sName, editor)))) {
            this->setValueInt(std::forward<T>(value), doCallback, editor);
        }
    }

    // generic callback setter that auto-detects callback type
    template <typename Callback>
    void setCallback(Callback &&callback)
        requires(cb_invocable<Callback> || cb_invocable<Callback, std::string_view> || cb_invocable<Callback, float> ||
                 cb_invocable<Callback, double> || cb_invocable<Callback, std::string_view, std::string_view> ||
                 cb_invocable<Callback, float, float> || cb_invocable<Callback, double, double>)
    {
        if constexpr(cb_invocable<Callback>)
            this->callback.template emplace<VoidCB>(std::forward<Callback>(callback));
        else if constexpr(cb_invocable<Callback, std::string_view>)
            this->callback.template emplace<StringCB>(std::forward<Callback>(callback));
        else if constexpr(cb_invocable<Callback, float>)
            this->callback.template emplace<FloatCB>(std::forward<Callback>(callback));
        else if constexpr(cb_invocable<Callback, double>)
            this->callback.template emplace<DoubleCB>(std::forward<Callback>(callback));
        else if constexpr(cb_invocable<Callback, std::string_view, std::string_view>)
            this->changeCallback.template emplace<StringChangeCB>(std::forward<Callback>(callback));
        else if constexpr(cb_invocable<Callback, float, float>)
            this->changeCallback.template emplace<FloatChangeCB>(std::forward<Callback>(callback));
        else if constexpr(cb_invocable<Callback, double, double>)
            this->changeCallback.template emplace<DoubleChangeCB>(std::forward<Callback>(callback));
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

    [[nodiscard]] inline double getDouble() const {
        if(likely(this->bUseCachedDouble.load(std::memory_order_relaxed))) {
            return this->dCachedReturnedDouble.load(std::memory_order_relaxed);
        }
        return this->getDoubleInt();
    }
    [[nodiscard]] inline const std::string &getString() const {
        if(likely(this->bUseCachedString.load(std::memory_order_relaxed))) {
            return *(this->sCachedReturnedString.load(std::memory_order_relaxed));
        }
        return this->getStringInt();
    }

    template <typename T = int>
    [[nodiscard]] forceinline T getVal() const {
        return static_cast<T>(this->getDouble());
    }

    [[nodiscard]] forceinline int getInt() const { return static_cast<int>(this->getDouble()); }
    [[nodiscard]] forceinline bool getBool() const { return !!static_cast<int>(this->getDouble()); }
    [[nodiscard]] forceinline bool get() const { return !!static_cast<int>(this->getDouble()); }
    [[nodiscard]] forceinline float getFloat() const { return static_cast<float>(this->getDouble()); }

    [[nodiscard]] forceinline const char *getHelpstring() const { return this->sHelpString; }
    [[nodiscard]] forceinline const char *getName() const { return this->sName; }
    [[nodiscard]] forceinline CONVAR_TYPE getType() const { return this->type; }
    [[nodiscard]] forceinline uint8_t getFlags() const { return this->iFlags; }

    [[nodiscard]] forceinline bool canHaveValue() const { return this->bCanHaveValue; }

    [[nodiscard]] inline bool hasAnyCallbacks() const {
        return !std::holds_alternative<std::monostate>(this->callback) ||
               !std::holds_alternative<std::monostate>(this->changeCallback);
    }

    [[nodiscard]] inline bool hasAnyNonVoidCallback() const {
        return std::holds_alternative<StringCB>(this->callback) || std::holds_alternative<FloatCB>(this->callback) ||
               std::holds_alternative<DoubleCB>(this->callback) ||
               !std::holds_alternative<std::monostate>(this->changeCallback);
    }

    [[nodiscard]] inline bool hasVoidCallback() const { return std::holds_alternative<VoidCB>(this->callback); }

    [[nodiscard]] inline bool hasSingleArgCallback() const {
        return std::holds_alternative<StringCB>(this->callback) || std::holds_alternative<FloatCB>(this->callback) ||
               std::holds_alternative<DoubleCB>(this->callback);
    }

    [[nodiscard]] inline bool hasChangeCallback() const {
        return !std::holds_alternative<std::monostate>(this->changeCallback);
    }

    [[nodiscard]] inline bool isFlagSet(uint8_t flag) const { return ((this->iFlags & flag) == flag); }
    [[nodiscard]] inline bool isDefault() const {
        return this->getString() == this->getDefaultString() && this->getDouble() == this->getDefaultDouble();
    }

    void setServerProtected(CvarProtection policy) {
        this->serverProtectionPolicy.store(policy, std::memory_order_release);
        this->invalidateCache();
    }

    [[nodiscard]] inline bool isProtected() const {
        switch(this->serverProtectionPolicy.load(std::memory_order_acquire)) {
            case CvarProtection::DEFAULT:
                return this->isFlagSet(cv::PROTECTED);
            case CvarProtection::PROTECTED:
                return true;
            case CvarProtection::UNPROTECTED:
            default:
                return false;
        }
    }

    // shared callbacks, app-defined
    static void setOnSetValueProtectedCallback(const VoidCB &callback);

    using ProtectedCVGetCB = bool (*)(const char *cvarname);
    static void setOnGetValueProtectedCallback(ProtectedCVGetCB func);

    using GameplayCVChangeCB = bool (*)(const char *cvarname, CvarEditor setterkind);
    static void setOnSetValueGameplayCallback(GameplayCVChangeCB func);

   private:
    // unified init for callback-only convars
    template <typename Callback>
    void initCallback(uint8_t flags, Callback &&callback) {
        this->iFlags = flags | cv::NOSAVE;

        if constexpr(cb_invocable<Callback>) {
            this->callback.template emplace<VoidCB>(std::forward<Callback>(callback));
            this->type = CONVAR_TYPE::STRING;
        } else if constexpr(cb_invocable<Callback, std::string_view>) {
            this->callback.template emplace<StringCB>(std::forward<Callback>(callback));
            this->type = CONVAR_TYPE::STRING;
        } else if constexpr(cb_invocable<Callback, float>) {
            this->callback.template emplace<FloatCB>(std::forward<Callback>(callback));
            this->type = CONVAR_TYPE::INT;
        } else if constexpr(cb_invocable<Callback, double>) {
            this->callback.template emplace<DoubleCB>(std::forward<Callback>(callback));
            this->type = CONVAR_TYPE::INT;
        }
    }

    // unified init for value convars
    template <typename T, typename Callback>
    void initValue(T &&defaultValue, uint8_t flags, Callback &&callback) {
        this->bCanHaveValue = true;
        this->iFlags = flags;
        this->type = getTypeFor<T>();

        if constexpr((std::is_convertible_v<std::decay_t<T>, double> || std::is_convertible_v<std::decay_t<T>, float> ||
                      std::is_same_v<T, bool>) &&
                     !std::is_same_v<std::decay_t<T>, UString> && !std::is_same_v<std::decay_t<T>, std::string_view> &&
                     !std::is_same_v<std::decay_t<T>, const char *>) {
            // T is double-like
            this->setDefaultDouble(static_cast<double>(std::forward<T>(defaultValue)));
        } else {
            // T is string-like
            this->setDefaultString(std::forward<T>(defaultValue));
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
                this->callback.template emplace<VoidCB>(std::forward<Callback>(callback));
            else if constexpr(cb_invocable<Callback, std::string_view>)
                this->callback.template emplace<StringCB>(std::forward<Callback>(callback));
            else if constexpr(cb_invocable<Callback, float>)
                this->callback.template emplace<FloatCB>(std::forward<Callback>(callback));
            else if constexpr(cb_invocable<Callback, double>)
                this->callback.template emplace<DoubleCB>(std::forward<Callback>(callback));
            else if constexpr(cb_invocable<Callback, std::string_view, std::string_view>)
                this->changeCallback.template emplace<StringChangeCB>(std::forward<Callback>(callback));
            else if constexpr(cb_invocable<Callback, float, float>)
                this->changeCallback.template emplace<FloatChangeCB>(std::forward<Callback>(callback));
            else if constexpr(cb_invocable<Callback, double, double>)
                this->changeCallback.template emplace<DoubleChangeCB>(std::forward<Callback>(callback));
        }
    }

    // no flag checking, setValue (user-accessible) already does that
    template <typename T>
    void setValueInt(T &&value, bool doCallback, CvarEditor editor) {
        // determine double and string representations depending on whether setValue("string") or setValue(double) was
        // called
        const auto [newDouble, newString] = [&]() -> std::pair<double, std::string> {
            if constexpr(((std::is_convertible_v<std::decay_t<T>, double> ||
                           std::is_convertible_v<std::decay_t<T>, float>) &&
                          !std::is_same_v<std::decay_t<T>, UString>) &&
                         !std::is_same_v<std::decay_t<T>, std::string_view> &&
                         !std::is_same_v<std::decay_t<T>, const char *>) {
                const auto f = static_cast<double>(std::forward<T>(value));
                return std::make_pair(f, fmt::format("{:g}", f));
            } else if constexpr(std::is_same_v<T, bool>) {
                const auto f = static_cast<double>(std::forward<T>(value) ? 1. : 0.);
                return std::make_pair(f, f > 0 ? "true" : "false");
            } else if constexpr(std::is_same_v<std::decay_t<T>, UString>) {
                const std::string s{std::forward<T>(value).toUtf8()};
                double dbl{};
                const auto [ptr, err] = std::from_chars(s.data(), s.data() + s.size(), dbl);
                if(err != std::errc()) return std::make_pair(this->dDefaultValue, s);
                return std::make_pair(dbl, s);
            } else {
                const std::string s{std::forward<T>(value)};
                double dbl{};
                const auto [ptr, err] = std::from_chars(s.data(), s.data() + s.size(), dbl);
                if(err != std::errc()) return std::make_pair(this->dDefaultValue, s);
                return std::make_pair(dbl, s);
            }
        }();

        // backup old values, for passing into callbacks
        double oldDouble{this->getDoubleInt()};
        std::string oldString;
        if(doCallback) {
            oldString = this->getStringInt();
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

        this->invalidateCache();

        // run protected value change cb
        if(this->isProtected() && oldDouble != newDouble && likely(!!ConVar::onSetValueProtectedCallback)) {
            ConVar::onSetValueProtectedCallback();
        }

        if(doCallback) {
            // handle possible execution callbacks
            if(!std::holds_alternative<std::monostate>(this->callback)) {
                std::visit(
                    [&](auto &&callback) {
                        using CBType = std::decay_t<decltype(callback)>;
                        if constexpr(std::is_same_v<CBType, VoidCB>)
                            callback();
                        else if constexpr(std::is_same_v<CBType, StringCB>)
                            callback(newString);
                        else if constexpr(std::is_same_v<CBType, FloatCB>)
                            callback(static_cast<float>(newDouble));
                        else if constexpr(std::is_same_v<CBType, DoubleCB>)
                            callback(newDouble);
                    },
                    this->callback);
            }

            // handle possible change callbacks
            if(!std::holds_alternative<std::monostate>(this->changeCallback)) {
                std::visit(
                    [&](auto &&callback) {
                        using CBType = std::decay_t<decltype(callback)>;
                        if constexpr(std::is_same_v<CBType, StringChangeCB>)
                            callback(oldString, newString);
                        else if constexpr(std::is_same_v<CBType, FloatChangeCB>)
                            callback(static_cast<float>(oldDouble), static_cast<float>(newDouble));
                        else if constexpr(std::is_same_v<CBType, DoubleChangeCB>)
                            callback(oldDouble, newDouble);
                    },
                    this->changeCallback);
            }
        }
    }

    [[nodiscard]] double getDoubleInt() const;
    [[nodiscard]] const std::string &getStringInt() const;

    inline void invalidateCache() {
        // invalidate cache, after we stored new values
        this->bUseCachedDouble.store(false, std::memory_order_release);
        this->bUseCachedString.store(false, std::memory_order_release);
    }

   private:
    // static callbacks are shared across all convars
    // to call when a convar with PROTECTED flag has been changed
    static VoidCB onSetValueProtectedCallback;

    // to call when a PROTECTED convar has getString or getValue called on it
    // if the callback returns FALSE, the default value will be returned instead
    // TODO/LOOK INTO: this is only called if it doesn't have a skin or server value, is that cheeseable?
    static ProtectedCVGetCB onGetValueProtectedCallback;

    // to call when a GAMEPLAY convar is being changed
    // if the callback returns FALSE, the convar won't be changed
    static GameplayCVChangeCB onSetValueGameplayCallback;

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

    // just return cached values to avoid checking flags, unless something changed
    mutable std::atomic<const std::string *> sCachedReturnedString{&sDefaultValue};
    mutable std::atomic<double> dCachedReturnedDouble{0.};

    // callback storage (allow having 1 "change" callback and 1 single value (or void) callback)
    ExecCallback callback{std::monostate()};
    ChangeCB changeCallback{std::monostate()};

    std::atomic<CvarProtection> serverProtectionPolicy{CvarProtection::DEFAULT};

    CONVAR_TYPE type{CONVAR_TYPE::FLOAT};
    uint8_t iFlags{0};

    bool bCanHaveValue{false};
    std::atomic<bool> hasServerValue{false};
    std::atomic<bool> hasSkinValue{false};

    mutable std::atomic<bool> bUseCachedDouble{false};
    mutable std::atomic<bool> bUseCachedString{false};
};

#endif
