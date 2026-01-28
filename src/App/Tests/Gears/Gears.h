// Copyright (c) 2026, WH, All rights reserved.
#pragma once

#ifndef GEARS_TEST_H
#define GEARS_TEST_H

#include "App.h"
#include "MouseListener.h"

#include <memory>

struct CBaseUIEventCtx;

namespace mc::tests {

class GearsButton;

class Gears : public App, public MouseListener {
    NOCOPY_NOMOVE(Gears)
   public:
    Gears();
    ~Gears() override;

    void draw() override;
    void update() override;

    void onResolutionChanged(vec2 newResolution) override;
    void onDPIChanged() override;

    void onFocusGained() override;
    void onFocusLost() override;

    void onMinimized() override;
    void onRestored() override;

    [[nodiscard]] bool isInGameplay() const override;
    [[nodiscard]] bool isInUnpausedGameplay() const override;

    void stealFocus() override;

    bool onShutdown() override;

    // may return null!
    [[nodiscard]] Sound *getSound(ActionSound action) const override;

    void showNotification(const NotificationInfo &notif) override;

   public:
    // keyboard
    void onKeyDown(KeyboardEvent &e) override;
    void onKeyUp(KeyboardEvent &e) override;
    void onChar(KeyboardEvent &e) override;

    // mouse
    void onButtonChange(ButtonEvent event) override;
    void onWheelVertical(int delta) override;
    void onWheelHorizontal(int delta) override;

    // UI
    std::unique_ptr<GearsButton> m_testButton;
};
}  // namespace mc::tests

#endif
