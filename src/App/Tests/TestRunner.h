// Copyright (c) 2026, WH, All rights reserved.
#pragma once

#ifndef TESTRUNNER_H
#define TESTRUNNER_H

#include "App.h"
#include "MouseListener.h"

#include <memory>
#include <optional>
#include <string>

namespace mc::tests {

class TestRunner : public App, public MouseListener {
    NOCOPY_NOMOVE(TestRunner)
   public:
    explicit TestRunner(std::optional<std::string> testName);
    ~TestRunner() override;

    void draw() override;
    void update() override;

    void onKeyDown(KeyboardEvent &e) override;
    void onKeyUp(KeyboardEvent &e) override;
    void onChar(KeyboardEvent &e) override;

    void onButtonChange(ButtonEvent event) override;
    void onWheelVertical(int delta) override;
    void onWheelHorizontal(int delta) override;

    void onResolutionChanged(vec2 newResolution) override;
    void onDPIChanged() override;

    void onFocusGained() override;
    void onFocusLost() override;

    void onMinimized() override;
    void onRestored() override;

    void stealFocus() override;
    bool onShutdown() override;

    [[nodiscard]] bool isInGameplay() const override;
    [[nodiscard]] bool isInUnpausedGameplay() const override;

    [[nodiscard]] Sound *getSound(ActionSound action) const override;
    void showNotification(const NotificationInfo &info) override;

   private:
    void launchTest(const char *name);
    void returnToMenu();

    std::unique_ptr<App> m_activeTest;
    int m_iHoveredIndex{-1};
};

}  // namespace mc::tests

#endif
