// Copyright (c) 2026, WH, All rights reserved.
#include "TestRunner.h"

#include "TestApps.h"

#include "Engine.h"
#include "Mouse.h"
#include "Graphics.h"
#include "Font.h"
#include "Logging.h"
#include "KeyBindings.h"
#include "UString.h"

#include <cstring>

namespace mc::tests {

TestRunner::TestRunner(std::optional<std::string> testName) {
    mouse->addListener(this);

    if(testName.has_value() && !testName->empty()) {
        for(const auto &entry : kTestApps) {
            if(testName.value() == entry.name) {
                launchTest(entry.name);
                return;
            }
        }
        debugLog("unknown test app '{}', showing selection screen", testName.value());
    }
}

TestRunner::~TestRunner() {
    m_activeTest.reset();
    mouse->removeListener(this);
}

void TestRunner::launchTest(const char *name) {
    m_activeTest.reset();

    for(const auto &entry : kTestApps) {
        if(std::strcmp(name, entry.name) == 0) {
            debugLog("launching test: {}", name);
            m_activeTest.reset(entry.create());
            return;
        }
    }
}

void TestRunner::returnToMenu() {
    m_activeTest.reset();
    m_iHoveredIndex = -1;
}

void TestRunner::draw() {
    if(m_activeTest) {
        m_activeTest->draw();
        return;
    }

    // selection screen
    McFont *font = engine->getDefaultFont();
    if(!font) return;

    const float lineHeight = font->getHeight() * 1.5f;
    const float startX = 50.f;
    const float startY = 50.f + font->getHeight();

    // title
    g->setColor(0xffffffff);
    g->pushTransform();
    g->translate(startX, startY);
    g->drawString(font, "test apps (ESC to return here)");
    g->popTransform();

    // list entries
    for(size_t i = 0; i < kTestApps.size(); i++) {
        const float y = startY + lineHeight * (float)(i + 1);

        if((int)i == m_iHoveredIndex) {
            g->setColor(0x40ffffff);
            g->fillRect((int)(startX - 5.f), (int)(y - font->getHeight()),
                        (int)(font->getStringWidth(kTestApps[i].name) + 10.f), (int)(font->getHeight() + 4.f));
        }

        g->setColor((int)i == m_iHoveredIndex ? 0xff00ffff : 0xffcccccc);
        g->pushTransform();
        g->translate(startX, y);
        g->drawString(font, kTestApps[i].name);
        g->popTransform();
    }
}

void TestRunner::update() {
    if(m_activeTest) {
        m_activeTest->update();
        return;
    }

    // hit test for selection screen
    McFont *font = engine->getDefaultFont();
    if(!font) return;

    const float lineHeight = font->getHeight() * 1.5f;
    const float startX = 50.f;
    const float startY = 50.f + font->getHeight();

    vec2 mousePos = mouse->getPos();
    m_iHoveredIndex = -1;

    for(size_t i = 0; i < kTestApps.size(); i++) {
        const float y = startY + lineHeight * (float)(i + 1);
        const float left = startX - 5.f;
        const float top = y - font->getHeight();
        const float width = font->getStringWidth(kTestApps[i].name) + 10.f;
        const float height = font->getHeight() + 4.f;

        if(mousePos.x >= left && mousePos.x <= left + width && mousePos.y >= top && mousePos.y <= top + height) {
            m_iHoveredIndex = (int)i;
            break;
        }
    }
}

void TestRunner::onKeyDown(KeyboardEvent &e) {
    if(m_activeTest) {
        if(e.getScanCode() == KEY_ESCAPE) {
            returnToMenu();
            e.consume();
            return;
        }
        m_activeTest->onKeyDown(e);
        return;
    }
}

void TestRunner::onKeyUp(KeyboardEvent &e) {
    if(m_activeTest) {
        m_activeTest->onKeyUp(e);
    }
}

void TestRunner::onChar(KeyboardEvent &e) {
    if(m_activeTest) {
        m_activeTest->onChar(e);
    }
}

void TestRunner::onButtonChange(ButtonEvent event) {
    if(m_activeTest) return;

    // selection screen click
    if(event.down && flags::has<MouseButtonFlags::MF_LEFT>(event.btn) && m_iHoveredIndex >= 0 &&
       m_iHoveredIndex < (int)kTestApps.size()) {
        launchTest(kTestApps[m_iHoveredIndex].name);
    }
}

void TestRunner::onWheelVertical(int delta) {
    if(m_activeTest) return;
    (void)delta;
}

void TestRunner::onWheelHorizontal(int delta) {
    if(m_activeTest) return;
    (void)delta;
}

void TestRunner::onResolutionChanged(vec2 newResolution) {
    if(m_activeTest) m_activeTest->onResolutionChanged(newResolution);
}

void TestRunner::onDPIChanged() {
    if(m_activeTest) m_activeTest->onDPIChanged();
}

void TestRunner::onFocusGained() {
    if(m_activeTest) m_activeTest->onFocusGained();
}

void TestRunner::onFocusLost() {
    if(m_activeTest) m_activeTest->onFocusLost();
}

void TestRunner::onMinimized() {
    if(m_activeTest) m_activeTest->onMinimized();
}

void TestRunner::onRestored() {
    if(m_activeTest) m_activeTest->onRestored();
}

void TestRunner::stealFocus() {
    if(m_activeTest) m_activeTest->stealFocus();
}

bool TestRunner::onShutdown() {
    if(m_activeTest) return m_activeTest->onShutdown();
    return true;
}

bool TestRunner::isInGameplay() const {
    if(m_activeTest) return m_activeTest->isInGameplay();
    return false;
}

bool TestRunner::isInUnpausedGameplay() const {
    if(m_activeTest) return m_activeTest->isInUnpausedGameplay();
    return false;
}

Sound *TestRunner::getSound(ActionSound action) const {
    if(m_activeTest) return m_activeTest->getSound(action);
    return nullptr;
}

void TestRunner::showNotification(const NotificationInfo &info) {
    if(m_activeTest) m_activeTest->showNotification(info);
}

}  // namespace mc::tests
