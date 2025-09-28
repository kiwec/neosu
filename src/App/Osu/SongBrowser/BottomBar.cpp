// Copyright (c) 2025, kiwec, All rights reserved.
#include "BottomBar.h"

#include "AnimationHandler.h"
#include "ConVar.h"
#include "Engine.h"
#include "LoudnessCalcThread.h"
#include "MapCalcThread.h"
#include "Mouse.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "ScoreConverterThread.h"
#include "Skin.h"
#include "SkinImage.h"
#include "SongBrowser.h"
#include "UserCard.h"

enum { MODE = 0, MODS = 1, RANDOM = 2, OPTIONS = 3 };
static McRect btns[4];
static f32 hovers[4] = {0.f, 0.f, 0.f, 0.f};
static i32 hovered_btn = -1;

void press_bottombar_button(i32 btn_index) {
    if(hovered_btn != btn_index) {
        hovers[btn_index] = 1.f;
        anim->moveLinear(&hovers[btn_index], 0.0f, 0.1f, 0.05f, true);
    }

    switch(btn_index) {
        case MODE:
            osu->getSongBrowser()->onSelectionMode();
            break;
        case MODS:
            osu->getSongBrowser()->onSelectionMods();
            break;
        case RANDOM:
            osu->getSongBrowser()->onSelectionRandom();
            break;
        case OPTIONS:
            osu->getSongBrowser()->onSelectionOptions();
            break;
        default:
            abort();
    }
}

f32 bottombar_get_min_height() { return SongBrowser::getUIScale() * 101.f; }

f32 get_bottombar_height() {
    f32 max = 0.f;
    for(auto& btn : btns) {
        if(btn.getHeight() > max) {
            max = btn.getHeight();
        }
    }

    return std::max(bottombar_get_min_height(), max);
}

void update_bottombar(bool* propagate_clicks) {
    static bool mouse_was_down = false;

    auto mousePos = mouse->getPos();
    bool clicked = !mouse_was_down && mouse->isLeftDown();
    mouse_was_down = mouse->isLeftDown();

    auto screen = osu->getVirtScreenSize();
    bool is_widescreen = (screen.x / screen.y) > (4.f / 3.f);
    auto mode_img = osu->getSkin()->selectionModeOver;
    btns[MODE].setSize(SongBrowser::getSkinDimensions(mode_img));
    btns[MODE].setPos({Osu::getUIScale(is_widescreen ? 140.0f : 120.0f), screen.y - btns[MODE].getHeight()});

    auto mods_img = osu->getSkin()->selectionModsOver;
    btns[MODS].setSize(SongBrowser::getSkinDimensions(mods_img));
    btns[MODS].setPos({btns[MODE].getX() + SongBrowser::getUIScale(92.5f), screen.y - btns[MODS].getHeight()});

    auto random_img = osu->getSkin()->selectionRandomOver;
    btns[RANDOM].setSize(SongBrowser::getSkinDimensions(random_img));
    btns[RANDOM].setPos({btns[MODS].getX() + SongBrowser::getUIScale(77.5f), screen.y - btns[RANDOM].getHeight()});

    auto options_img = osu->getSkin()->selectionOptionsOver;
    btns[OPTIONS].setSize(SongBrowser::getSkinDimensions(options_img));
    btns[OPTIONS].setPos({btns[RANDOM].getX() + SongBrowser::getUIScale(77.5f), screen.y - btns[OPTIONS].getHeight()});

    osu->getUserButton()->setSize(SongBrowser::getUIScale(320.f), SongBrowser::getUIScale(75.f));
    osu->getUserButton()->setPos(btns[OPTIONS].getX() + SongBrowser::getUIScale(160.f),
                                 osu->getVirtScreenHeight() - osu->getUserButton()->getSize().y);
    osu->getUserButton()->mouse_update(propagate_clicks);

    // Yes, the order looks whack. That's the correct order.
    i32 new_hover = -1;
    if(btns[OPTIONS].contains(mousePos)) {
        new_hover = OPTIONS;
    } else if(btns[MODE].contains(mousePos)) {
        new_hover = MODE;
    } else if(btns[MODS].contains(mousePos)) {
        new_hover = MODS;
    } else if(btns[RANDOM].contains(mousePos)) {
        new_hover = RANDOM;
    } else {
        clicked = false;
    }

    if(hovered_btn != new_hover) {
        if(hovered_btn != -1) {
            anim->moveLinear(&hovers[hovered_btn], 0.0f, hovers[hovered_btn] * 0.1f, true);
        }
        if(new_hover != -1) {
            anim->moveLinear(&hovers[new_hover], 1.f, 0.1f, true);
        }

        hovered_btn = new_hover;
    }

    if(clicked && *propagate_clicks) {
        *propagate_clicks = false;
        press_bottombar_button(hovered_btn);
    }

    // TODO @kiwec: do hovers make sound?
}

void draw_bottombar() {
    g->pushTransform();
    {
        f32 bar_height = bottombar_get_min_height();
        Image* img = osu->getSkin()->songSelectBottom;
        g->setColor(0xffffffff);
        g->scale((f32)osu->getVirtScreenWidth() / (f32)img->getWidth(), bar_height / (f32)img->getHeight());
        g->translate(0, osu->getVirtScreenHeight() - bar_height);
        g->drawImage(img, AnchorPoint::TOP_LEFT);
    }
    g->popTransform();

    osu->getSongBrowser()->backButton->draw();

    // Draw the user card under selection elements, which can cover it for fancy effects
    // (we don't match stable perfectly, but close enough)
    osu->getUserButton()->draw();

    // Careful, these buttons are often used as overlays
    // eg. selection-mode usually covers the whole screen, drawing topbar, bottom right osu cookie etc
    SkinImage* base_imgs[4] = {osu->getSkin()->selectionMode, osu->getSkin()->selectionMods,
                               osu->getSkin()->selectionRandom, osu->getSkin()->selectionOptions};
    for(i32 i = 0; i < 4; i++) {
        if(base_imgs[i] == nullptr) continue;

        f32 scale = SongBrowser::getSkinScale(base_imgs[i]);
        g->setColor(0xffffffff);
        base_imgs[i]->drawRaw(vec2(btns[i].getX(), osu->getVirtScreenHeight()), scale, AnchorPoint::BOTTOM_LEFT);
    }

    // Ok, and now for the hover images... which are drawn in a weird order, same as update_bottombar()
    auto random_img = osu->getSkin()->selectionRandomOver;
    f32 random_scale = SongBrowser::getSkinScale(random_img);
    g->setColor(Color(0xffffffff).setA(hovers[RANDOM]));
    random_img->drawRaw(vec2(btns[RANDOM].getX(), osu->getVirtScreenHeight()), random_scale, AnchorPoint::BOTTOM_LEFT);

    auto mods_img = osu->getSkin()->selectionModsOver;
    f32 mods_scale = SongBrowser::getSkinScale(mods_img);
    g->setColor(Color(0xffffffff).setA(hovers[MODS]));
    mods_img->drawRaw(vec2(btns[MODS].getX(), osu->getVirtScreenHeight()), mods_scale, AnchorPoint::BOTTOM_LEFT);

    auto mode_img = osu->getSkin()->selectionModeOver;
    f32 mode_scale = SongBrowser::getSkinScale(mode_img);
    g->setColor(Color(0xffffffff).setA(hovers[MODE]));
    mode_img->drawRaw(vec2(btns[MODE].getX(), osu->getVirtScreenHeight()), mode_scale, AnchorPoint::BOTTOM_LEFT);

    auto options_img = osu->getSkin()->selectionOptionsOver;
    f32 options_scale = SongBrowser::getSkinScale(options_img);
    g->setColor(Color(0xffffffff).setA(hovers[OPTIONS]));
    options_img->drawRaw(vec2(btns[OPTIONS].getX(), osu->getVirtScreenHeight()), options_scale,
                         AnchorPoint::BOTTOM_LEFT);

    // mode-osu-small (often used as overlay)
    auto mos_img = osu->getSkin()->mode_osu_small;
    if(mos_img != nullptr) {
        f32 mos_scale = SongBrowser::getSkinScale(mos_img);
        g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_ADDITIVE);
        g->setColor(0xffffffff);
        mos_img->drawRaw(vec2(btns[MODE].getX() + (btns[MODS].getX() - btns[MODE].getX()) / 2.f,
                              osu->getVirtScreenHeight() - SongBrowser::getUIScale(56.f)),
                         mos_scale, AnchorPoint::CENTER);
        g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_ALPHA);
    }

    // background task busy notification
    // XXX: move this to permanent toasts
    McFont* font = resourceManager->getFont("FONT_DEFAULT");
    i32 calcx = osu->getUserButton()->getPos().x + osu->getUserButton()->getSize().x + 20;
    i32 calcy = osu->getUserButton()->getPos().y + 30;
    if(MapCalcThread::get_total() > 0) {
        UString msg =
            UString::format("Calculating stars (%i/%i) ...", MapCalcThread::get_computed(), MapCalcThread::get_total());
        g->setColor(0xff333333);
        g->pushTransform();
        g->translate(calcx, calcy);
        g->drawString(font, msg);
        g->popTransform();
        calcy += font->getHeight() + 10;
    }
    if(cv::normalize_loudness.getBool() && VolNormalization::get_total() > 0 &&
       VolNormalization::get_computed() < VolNormalization::get_total()) {
        UString msg = UString::format("Computing loudness (%i/%i) ...", VolNormalization::get_computed(),
                                      VolNormalization::get_total());
        g->setColor(0xff333333);
        g->pushTransform();
        g->translate(calcx, calcy);
        g->drawString(font, msg);
        g->popTransform();
        calcy += font->getHeight() + 10;
    }
    if(sct_total.load() > 0 && sct_computed.load() < sct_total.load()) {
        UString msg = UString::format("Converting scores (%i/%i) ...", sct_computed.load(), sct_total.load());
        g->setColor(0xff333333);
        g->pushTransform();
        g->translate(calcx, calcy);
        g->drawString(font, msg);
        g->popTransform();
        calcy += font->getHeight() + 10;
    }
}

// TODO @kiwec: default icon for mode-osu-small
