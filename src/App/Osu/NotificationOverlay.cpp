// Copyright (c) 2016, PG, All rights reserved.
#include "NotificationOverlay.h"

#include <utility>

#include "AnimationHandler.h"
#include "ConVar.h"
#include "Engine.h"
#include "Environment.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "Osu.h"
#include "PauseMenu.h"
#include "ResourceManager.h"

NotificationOverlay::NotificationOverlay() : OsuScreen() {
    this->bWaitForKey = false;
    this->bWaitForKeyDisallowsLeftClick = false;
    this->bConsumeNextChar = false;
    this->keyListener = nullptr;
}

static const f64 TOAST_WIDTH = 350.0;
static const f64 TOAST_INNER_X_MARGIN = 5.0;
static const f64 TOAST_INNER_Y_MARGIN = 5.0;
static const f64 TOAST_OUTER_Y_MARGIN = 10.0;
static const f64 TOAST_SCREEN_BOTTOM_MARGIN = 20.0;
static const f64 TOAST_SCREEN_RIGHT_MARGIN = 10.0;

ToastElement::ToastElement(const UString &text, Color borderColor_arg, ToastElement::TYPE type)
    : CBaseUIButton(0, 0, 0, 0, "", "") {
    this->grabs_clicks = true;

    // TODO: animations
    // TODO: ui scaling

    this->type = type;
    this->borderColor = borderColor_arg;
    this->creationTime = engine->getTime();

    const auto font = resourceManager->getFont("FONT_DEFAULT");
    this->lines = font->wrap(text, TOAST_WIDTH - TOAST_INNER_X_MARGIN * 2.0);
    this->setSize(TOAST_WIDTH, (font->getHeight() * 1.5 * this->lines.size()) + (TOAST_INNER_Y_MARGIN * 2.0));
}

void ToastElement::onClicked(bool left, bool right) {
    // Set creationTime to -10 so toast is deleted in NotificationOverlay::mouse_update
    this->creationTime = -10.0;

    CBaseUIButton::onClicked(left, right);
}

void ToastElement::draw() {
    const auto font = resourceManager->getFont("FONT_DEFAULT");

    f32 alpha = 0.9;
    alpha *= std::max(0.0, (this->creationTime + 9.5) - engine->getTime());

    // background
    g->setColor(this->isMouseInside() ? 0xff222222 : 0xff111111);
    g->setAlpha(alpha);
    g->fillRect(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y);

    // border
    g->setColor(this->isMouseInside() ? rgb(255, 255, 255) : this->borderColor);
    g->setAlpha(alpha);
    g->drawRect(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y);

    // text
    f64 y = this->vPos.y;
    for(const auto &line : this->lines) {
        y += (font->getHeight() * 1.5);
        g->setColor(0xffffffff);
        g->setAlpha(alpha);
        g->pushTransform();
        g->translate(this->vPos.x + TOAST_INNER_X_MARGIN, y);
        g->drawString(font, line);
        g->popTransform();
    }
}

void NotificationOverlay::mouse_update(bool *propagate_clicks) {
    bool chat_toasts_visible = cv::notify_during_gameplay.getBool();
    chat_toasts_visible |= !osu->isInPlayMode();
    chat_toasts_visible |= osu->pauseMenu->isVisible();

    bool a_toast_is_hovered = false;
    Vector2 screen{Osu::g_vInternalResolution};
    f64 bottom_y = screen.y - TOAST_SCREEN_BOTTOM_MARGIN;
    for(auto t : this->toasts) {
        if(t->type == ToastElement::TYPE::CHAT && !chat_toasts_visible) continue;

        bottom_y -= TOAST_OUTER_Y_MARGIN + t->getSize().y;
        t->setPos(screen.x - (TOAST_SCREEN_RIGHT_MARGIN + TOAST_WIDTH), bottom_y);
        t->mouse_update(propagate_clicks);
        a_toast_is_hovered |= t->isMouseInside();
    }

    // Delay toast disappearance
    for(auto t : this->toasts) {
        bool delay_toast = t->type == ToastElement::TYPE::PERMANENT;
        delay_toast |= t->type == ToastElement::TYPE::CHAT && !chat_toasts_visible;
        delay_toast |= a_toast_is_hovered;
        delay_toast |= !env->hasFocus();

        if(delay_toast) {
            t->creationTime += engine->getFrameTime();
        }
    }

    f64 current_time = engine->getTime();
    for(auto it = this->toasts.begin(); it != this->toasts.end(); it++) {
        auto toast = *it;
        if(toast->creationTime + 10.0 < current_time) {
            this->toasts.erase(it);
            delete toast;
            return;
        }
    }
}

void NotificationOverlay::draw() {
    bool chat_toasts_visible = cv::notify_during_gameplay.getBool();
    chat_toasts_visible |= !osu->isInPlayMode();
    chat_toasts_visible |= osu->pauseMenu->isVisible();

    for(auto t : this->toasts) {
        if(t->type == ToastElement::TYPE::CHAT && !chat_toasts_visible) continue;

        t->draw();
    }

    if(!this->isVisible()) return;

    if(this->bWaitForKey) {
        g->setColor(0x22ffffff);
        g->setAlpha((this->notification1.backgroundAnim / 0.5f) * 0.13f);
        g->fillRect(0, 0, osu->getScreenWidth(), osu->getScreenHeight());
    }

    this->drawNotificationBackground(this->notification2);
    this->drawNotificationBackground(this->notification1);
    this->drawNotificationText(this->notification2);
    this->drawNotificationText(this->notification1);
}

void NotificationOverlay::drawNotificationText(NotificationOverlay::NOTIFICATION &n) {
    McFont *font = osu->getSubTitleFont();
    int height = font->getHeight() * 2;
    int stringWidth = font->getStringWidth(n.text);

    g->pushTransform();
    {
        g->setColor(0xff000000);
        g->setAlpha(n.alpha);
        g->translate((int)(osu->getScreenWidth() / 2 - stringWidth / 2 + 1),
                     (int)(osu->getScreenHeight() / 2 + font->getHeight() / 2 + n.fallAnim * height * 0.15f + 1));
        g->drawString(font, n.text);

        g->setColor(n.textColor);
        g->setAlpha(n.alpha);
        g->translate(-1, -1);
        g->drawString(font, n.text);
    }
    g->popTransform();
}

void NotificationOverlay::drawNotificationBackground(NotificationOverlay::NOTIFICATION &n) {
    McFont *font = osu->getSubTitleFont();
    int height = font->getHeight() * 2 * n.backgroundAnim;

    g->setColor(0xff000000);
    g->setAlpha(n.alpha * 0.75f);
    g->fillRect(0, osu->getScreenHeight() / 2 - height / 2, osu->getScreenWidth(), height);
}

void NotificationOverlay::onKeyDown(KeyboardEvent &e) {
    if(!this->isVisible()) return;

    // escape always stops waiting for a key
    if(e.getKeyCode() == KEY_ESCAPE) {
        if(this->bWaitForKey) e.consume();

        this->stopWaitingForKey();
    }

    // key binding logic
    if(this->bWaitForKey) {
        // HACKHACK: prevent left mouse click bindings if relevant
        if(Env::cfg(OS::WINDOWS) && this->bWaitForKeyDisallowsLeftClick &&
           e.getKeyCode() == 0x01)  // 0x01 == VK_LBUTTON
            this->stopWaitingForKey();
        else {
            this->stopWaitingForKey(true);

            debugLog("keyCode = {:d}\n", e.getKeyCode());

            if(this->keyListener != nullptr) this->keyListener->onKey(e);
        }

        e.consume();
    }

    if(this->bWaitForKey) e.consume();
}

void NotificationOverlay::onKeyUp(KeyboardEvent &e) {
    if(!this->isVisible()) return;

    if(this->bWaitForKey) e.consume();
}

void NotificationOverlay::onChar(KeyboardEvent &e) {
    if(this->bWaitForKey || this->bConsumeNextChar) e.consume();

    this->bConsumeNextChar = false;
}

void NotificationOverlay::addNotification(UString text, Color textColor, bool waitForKey, float duration) {
    const float notificationDuration = (duration < 0.0f ? cv::notification_duration.getFloat() : duration);

    // swap effect
    if(this->isVisible()) {
        this->notification2.text = this->notification1.text;
        this->notification2.textColor = 0xffffffff;

        this->notification2.time = 0.0f;
        this->notification2.alpha = 0.5f;
        this->notification2.backgroundAnim = 1.0f;
        this->notification2.fallAnim = 0.0f;

        anim->deleteExistingAnimation(&this->notification1.alpha);

        anim->moveQuadIn(&this->notification2.fallAnim, 1.0f, 0.2f, 0.0f, true);
        anim->moveQuadIn(&this->notification2.alpha, 0.0f, 0.2f, 0.0f, true);
    }

    // build new notification
    this->bWaitForKey = waitForKey;
    this->bConsumeNextChar = this->bWaitForKey;

    float fadeOutTime = 0.4f;

    this->notification1.text = std::move(text);
    this->notification1.textColor = textColor;

    if(!waitForKey)
        this->notification1.time = engine->getTime() + notificationDuration + fadeOutTime;
    else
        this->notification1.time = 0.0f;

    this->notification1.alpha = 0.0f;
    this->notification1.backgroundAnim = 0.5f;
    this->notification1.fallAnim = 0.0f;

    // animations
    if(this->isVisible())
        this->notification1.alpha = 1.0f;
    else
        anim->moveLinear(&this->notification1.alpha, 1.0f, 0.075f, true);

    if(!waitForKey) anim->moveQuadOut(&this->notification1.alpha, 0.0f, fadeOutTime, notificationDuration, false);

    anim->moveQuadOut(&this->notification1.backgroundAnim, 1.0f, 0.15f, 0.0f, true);
}

void NotificationOverlay::addToast(const UString &text, Color borderColor, const ToastClickCallback &callback,
                                   ToastElement::TYPE type) {
    auto toast = new ToastElement(text, borderColor, type);
    if(!callback.isNull()) {
        toast->setClickCallback(callback);
    }
    this->toasts.push_back(toast);
}

void NotificationOverlay::stopWaitingForKey(bool stillConsumeNextChar) {
    this->bWaitForKey = false;
    this->bWaitForKeyDisallowsLeftClick = false;
    this->bConsumeNextChar = stillConsumeNextChar;
}

bool NotificationOverlay::isVisible() {
    return engine->getTime() < this->notification1.time || engine->getTime() < this->notification2.time ||
           this->bWaitForKey;
}
