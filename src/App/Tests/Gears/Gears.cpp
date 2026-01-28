// Copyright (c) 2026, WH, All rights reserved.
#include "Gears.h"

#include "Engine.h"
#include "Mouse.h"
#include "ResourceManager.h"
#include "ConVar.h"
#include "Logging.h"
#include "Graphics.h"
#include "Font.h"
#include "UString.h"
#include "Image.h"
#include "VertexArrayObject.h"

#include "CBaseUIButton.h"

namespace mc::tests {
class GearsButton : public CBaseUIButton {
    NOCOPY_NOMOVE(GearsButton)
   public:
    GearsButton(float xPos, float yPos, float xSize, float ySize, UString name, UString text)
        : CBaseUIButton(xPos, yPos, xSize, ySize, std::move(name), std::move(text)) {}
    ~GearsButton() override = default;

    void onMouseInside() override { debugLog(""); }
    void onMouseOutside() override { debugLog(""); }
    void onMouseDownInside(bool left = true, bool right = false) override {
        debugLog("left: {} right: {}", left, right);
    }
    void onMouseDownOutside(bool left = true, bool right = false) override {
        debugLog("left: {} right: {}", left, right);
    }
    void onMouseUpInside(bool left = true, bool right = false) override { debugLog("left: {} right: {}", left, right); }
    void onMouseUpOutside(bool left = true, bool right = false) override {
        debugLog("left: {} right: {}", left, right);
    }
};

Gears::Gears() {
    debugLog("");

    m_testButton = std::make_unique<GearsButton>(300.f, 600.f, 200.f, 25.f, "GearsButton", "GearsButton");

    // load resource
    resourceManager->loadImage("ic_music_48dp.png", "TESTIMAGE");

    // engine overrides
    cv::debug_mouse.setValue(1.0f);

    mouse->addListener(this);
}

Gears::~Gears() {
    debugLog("");
    mouse->removeListener(this);
}

void Gears::draw() {
    McFont *testFont = engine->getDefaultFont();

    // test general drawing
    g->setColor(0xffff0000);
    int blockSize = 100;
    g->fillRect(engine->getScreenWidth() / 2 - blockSize / 2 + std::sin(engine->getTime() * 3) * 100,
                engine->getScreenHeight() / 2 - blockSize / 2 + std::sin(engine->getTime() * 3 * 1.5f) * 100, blockSize,
                blockSize);

    // test font texture atlas
    g->setColor(0xffffffff);
    g->pushTransform();
    g->translate(100, 100);
    testFont->drawTextureAtlas();
    g->popTransform();

    // test image
    Image *testImage = resourceManager->getImage("TESTIMAGE");
    g->setColor(0xffffffff);
    g->pushTransform();
    g->translate(testImage->getWidth() / 2 + 50, testImage->getHeight() / 2 + 100);
    g->drawImage(testImage);
    g->popTransform();

    // test button
    m_testButton->draw();

    // test text
    UString testText = "It's working!";
    g->push3DScene(McRect(800, 300, testFont->getStringWidth(testText), testFont->getHeight()));
    {
        g->rotate3DScene(0, engine->getTime() * 200, 0);
        g->pushTransform();
        {
            g->translate(800, 300 + testFont->getHeight());
            g->drawString(testFont, testText);
        }
        g->popTransform();
    }
    g->pop3DScene();

    // test vao rgb triangle
    const float triangleSizeMultiplier = 125.0f;
    g->pushTransform();
    {
        g->translate(engine->getScreenWidth() * 0.75f - triangleSizeMultiplier * 0.5f,
                     engine->getScreenHeight() * 0.75f - triangleSizeMultiplier * 0.5f);

        VertexArrayObject vao;
        {
            vao.addVertex(-0.5f * triangleSizeMultiplier, 0.5f * triangleSizeMultiplier, 0.0f);
            vao.addColor(0xffff0000);

            vao.addVertex(0.0f, -0.5f * triangleSizeMultiplier, 0.0f);
            vao.addColor(0xff00ff00);

            vao.addVertex(0.5f * triangleSizeMultiplier, 0.5f * triangleSizeMultiplier, 0.0f);
            vao.addColor(0xff0000ff);
        }
        g->drawVAO(&vao);
    }
    g->popTransform();
}

void Gears::update() {
    CBaseUIEventCtx c;
    m_testButton->update(c);
}

void Gears::onResolutionChanged(vec2 newResolution) { debugLog("{}", newResolution); }

void Gears::onDPIChanged() { debugLog("{}"); }

bool Gears::isInGameplay() const {
    // debugLog("");
    return false;
}
bool Gears::isInUnpausedGameplay() const {
    // debugLog("");
    return false;
}

void Gears::stealFocus() { debugLog(""); }

bool Gears::onShutdown() {
    debugLog("");
    return true;
}

Sound *Gears::getSound(ActionSound action) const {
    debugLog("{}", static_cast<size_t>(action));
    return nullptr;
}

void Gears::showNotification(const NotificationInfo &notif) {
    debugLog("text: {} color: {} duration: {} class: {} preset: {} cb: {:p}", notif.text, notif.custom_color.v,
             notif.duration, static_cast<size_t>(notif.nclass), static_cast<size_t>(notif.preset),
             fmt::ptr(&notif.callback));
}

void Gears::onFocusGained() { debugLog(""); }

void Gears::onFocusLost() { debugLog(""); }

void Gears::onMinimized() { debugLog(""); }

void Gears::onRestored() { debugLog(""); }

void Gears::onKeyDown(KeyboardEvent &e) { debugLog("keyDown: {}", e.getScanCode()); }

void Gears::onKeyUp(KeyboardEvent &e) { debugLog("keyUp: {}", e.getScanCode()); }

void Gears::onChar(KeyboardEvent &e) {
    const char16_t charray[]{e.getCharCode(), u'\0'};
    debugLog("charCode: {}", UString{&charray[0]});
}

void Gears::onButtonChange(ButtonEvent event) {
    debugLog("button: {} down: {} timestamp: {}", static_cast<size_t>(event.btn), event.down, event.timestamp);
}
void Gears::onWheelVertical(int delta) { debugLog("{}", delta); }
void Gears::onWheelHorizontal(int delta) { debugLog("{}", delta); }

}  // namespace mc::tests
