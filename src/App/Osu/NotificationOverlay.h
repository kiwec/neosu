#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "CBaseUIButton.h"
#include "KeyboardEvent.h"
#include "OsuScreen.h"

#define CHAT_TOAST 0xff8a2be2
#define INFO_TOAST 0xffffdd00
#define ERROR_TOAST 0xffdd0000
#define SUCCESS_TOAST 0xff00ff00
#define STATUS_TOAST 0xff003bff

class ToastElement final : public CBaseUIButton {
    NOCOPY_NOMOVE(ToastElement)
   public:
    enum class TYPE : uint8_t { PERMANENT, SYSTEM, CHAT };

    ToastElement(const UString &text, Color borderColor_arg, TYPE type);
    ~ToastElement() override = default;

    void draw() override;
    void onClicked(bool left = true, bool right = false) override;

    std::vector<UString> lines;

    f64 creationTime;
    f32 height = 0.f;

    Color borderColor;
    TYPE type;
};

class NotificationOverlayKeyListener {
    NOCOPY_NOMOVE(NotificationOverlayKeyListener)
   public:
    NotificationOverlayKeyListener() = default;
    virtual ~NotificationOverlayKeyListener() = default;
    virtual void onKey(KeyboardEvent &e) = 0;
};

class NotificationOverlay final : public OsuScreen {
    NOCOPY_NOMOVE(NotificationOverlay)
   public:
    NotificationOverlay();
    ~NotificationOverlay() override;

    void mouse_update(bool *propagate_clicks) override;
    void draw() override;

    void onKeyDown(KeyboardEvent &e) override;
    void onKeyUp(KeyboardEvent &e) override;
    void onChar(KeyboardEvent &e) override;

    using ToastClickCallback = std::function<void()>;
    void addToast(const UString &text, Color borderColor, const ToastClickCallback &callback = {},
                  ToastElement::TYPE type = ToastElement::TYPE::SYSTEM);
    void addNotification(UString text, Color textColor = 0xffffffff, bool waitForKey = false, float duration = -1.0f);
    void setDisallowWaitForKeyLeftClick(bool disallowWaitForKeyLeftClick) {
        this->bWaitForKeyDisallowsLeftClick = disallowWaitForKeyLeftClick;
    }

    void stopWaitingForKey(bool stillConsumeNextChar = false);

    void addKeyListener(NotificationOverlayKeyListener *keyListener) { this->keyListener = keyListener; }

    bool isVisible() override;

    inline bool isWaitingForKey() { return this->bWaitForKey || this->bConsumeNextChar; }

   private:
    struct NOTIFICATION {
        UString text = "";
        Color textColor = argb(255, 255, 255, 255);

        float time = 0.f;
        float alpha = 0.f;
        float backgroundAnim = 0.f;
        float fallAnim = 0.f;
    };

    void drawNotificationText(const NOTIFICATION &n);
    void drawNotificationBackground(const NOTIFICATION &n);

    std::vector<std::unique_ptr<ToastElement>> toasts;

    NOTIFICATION notification1;
    NOTIFICATION notification2;
    NotificationOverlayKeyListener *keyListener;

    bool bWaitForKey;
    bool bWaitForKeyDisallowsLeftClick;
    bool bConsumeNextChar;
};
