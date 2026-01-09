#include "LoadingScreen.h"

#include "ConVar.h"
#include "Engine.h"
#include "Font.h"
#include "KeyBindings.h"
#include "Graphics.h"
#include "Osu.h"
#include "OsuConVarDefs.h"
#include "Skin.h"
#include "UI.h"

LoadingScreen::LoadingScreen(UIOverlay* parent, std::function<f32()> get_progress_fn, std::function<void()> cancel_fn) {
    assert(parent != nullptr);
    this->parent = parent;
    this->cancel_fn = cancel_fn;
    this->get_progress_fn = get_progress_fn;
}

CBaseUIContainer* LoadingScreen::setVisible(bool visible) {
    UIOverlay::setVisible(visible);
    if(visible) return this;

    ui->setScreen(this->parent);
    delete this;
    return nullptr;
}

void LoadingScreen::draw() {
    // background
    g->setColor(0xff000000);
    g->fillRect(0, 0, osu->getVirtScreenWidth(), osu->getVirtScreenHeight());

    // progress message
    g->setColor(0xffffffff);
    UString loadingMessage = UString::format("Loading ... (%i %%)", (int)(this->get_progress_fn() * 100.0f));
    g->pushTransform();
    {
        g->translate((int)(osu->getVirtScreenWidth() / 2 - osu->getSubTitleFont()->getStringWidth(loadingMessage) / 2),
                     osu->getVirtScreenHeight() - 15);
        g->drawString(osu->getSubTitleFont(), loadingMessage);
    }
    g->popTransform();

    // spinner
    const float scale = Osu::getImageScale(osu->getSkin()->i_beatmap_import_spinner, 100);
    g->pushTransform();
    {
        g->rotate(engine->getTime() * 180, 0, 0, 1);
        g->scale(scale, scale);
        g->translate(osu->getVirtScreenWidth() / 2, osu->getVirtScreenHeight() / 2);
        g->drawImage(osu->getSkin()->i_beatmap_import_spinner);
    }
    g->popTransform();
}

void LoadingScreen::onKeyDown(KeyboardEvent& e) {
    if(e.isConsumed()) return;

    if(e == KEY_ESCAPE || e == cv::GAME_PAUSE.getVal<SCANCODE>()) {
        e.consume();
        this->cancel_fn();
    }
}
