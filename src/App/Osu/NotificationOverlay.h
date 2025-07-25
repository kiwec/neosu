#pragma once
#include "CBaseUIButton.h"
#include "KeyboardEvent.h"
#include "OsuScreen.h"

class ToastElement : public CBaseUIButton {
   public:
    ToastElement(const UString& text, Color borderColor_arg);
    ~ToastElement() override { ; }

    void draw() override;
    void onClicked(bool left = true, bool right = false) override;

    std::vector<UString> lines;
    Color borderColor;
    f32 alpha = 0.f;
    f32 height = 0.f;
    f64 creationTime;
};

class NotificationOverlayKeyListener {
   public:
    virtual ~NotificationOverlayKeyListener() { ; }
    virtual void onKey(KeyboardEvent &e) = 0;
};

class NotificationOverlay : public OsuScreen {
   public:
    NotificationOverlay();
    ~NotificationOverlay() override { ; }

    void mouse_update(bool *propagate_clicks) override;
    void draw() override;

    void onKeyDown(KeyboardEvent &e) override;
    void onKeyUp(KeyboardEvent &e) override;
    void onChar(KeyboardEvent &e) override;

    using ToastClickCallback = SA::delegate<void()>;
    void addToast(const UString& text, Color borderColor = 0xffdd0000, const ToastClickCallback& callback = {});

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

    void drawNotificationText(NOTIFICATION &n);
    void drawNotificationBackground(NOTIFICATION &n);

    std::vector<ToastElement *> toasts;

    NOTIFICATION notification1;
    NOTIFICATION notification2;

    bool bWaitForKey;
    bool bWaitForKeyDisallowsLeftClick;
    bool bConsumeNextChar;
    NotificationOverlayKeyListener *keyListener;
};
