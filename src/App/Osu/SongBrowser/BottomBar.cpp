// Copyright (c) 2025, kiwec, All rights reserved.
#include "BottomBar.h"

#include "AnimationHandler.h"
#include "OsuConVars.h"
#include "Engine.h"
#include "LoudnessCalcThread.h"
#include "DBRecalculator.h"
#include "Mouse.h"
#include "OptionsMenu.h"
#include "UIBackButton.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "Skin.h"
#include "SkinImage.h"
#include "SongBrowser.h"
#include "UI.h"
#include "UserCard.h"
#include "Font.h"

namespace BottomBar {
using namespace neosu::sbr;

static McRect btns[4];
static f32 hovers[4] = {0.f, 0.f, 0.f, 0.f};
static Button hovered_btn = NONE;

void press_button(Button btn_index) {
    if(hovered_btn != btn_index) {
        hovers[btn_index] = 1.f;
        anim::moveLinear(&hovers[btn_index], 0.0f, 0.1f, 0.05f, true);
    }

    switch(btn_index) {
        case MODE:
            return g_songbrowser->onSelectionMode();
        case MODS:
            return g_songbrowser->onSelectionMods();
        case RANDOM:
            return g_songbrowser->onSelectionRandom();
        case OPTIONS:
            return g_songbrowser->onSelectionOptions();
        default:
            std::unreachable();
            break;
    }
    std::unreachable();
}

f32 get_min_height() { return SongBrowser::getUIScale() * 101.f; }

f32 get_height() {
    f32 max = 0.f;
    for(auto& btn : btns) {
        if(btn.getHeight() > max) {
            max = btn.getHeight();
        }
    }

    return std::max(get_min_height(), max);
}

void update() {
    static bool mouse_was_down = false;

    auto mousePos = mouse->getPos();
    bool clicked = !mouse_was_down && mouse->isLeftDown();
    mouse_was_down = mouse->isLeftDown();

    auto screen = osu->getVirtScreenSize();
    bool is_widescreen = (screen.x / screen.y) > (4.f / 3.f);
    auto mode_img = osu->getSkin()->i_sel_mode_over;
    btns[MODE].setSize(SongBrowser::getSkinDimensions(mode_img));
    btns[MODE].setPos({Osu::getUIScale(is_widescreen ? 140.0f : 120.0f), screen.y - btns[MODE].getHeight()});

    auto mods_img = osu->getSkin()->i_sel_mods_over;
    btns[MODS].setSize(SongBrowser::getSkinDimensions(mods_img));
    btns[MODS].setPos({btns[MODE].getX() + SongBrowser::getUIScale(92.5f), screen.y - btns[MODS].getHeight()});

    auto random_img = osu->getSkin()->i_sel_random_over;
    btns[RANDOM].setSize(SongBrowser::getSkinDimensions(random_img));
    btns[RANDOM].setPos({btns[MODS].getX() + SongBrowser::getUIScale(77.5f), screen.y - btns[RANDOM].getHeight()});

    auto options_img = osu->getSkin()->i_sel_options_over;
    btns[OPTIONS].setSize(SongBrowser::getSkinDimensions(options_img));
    btns[OPTIONS].setPos({btns[RANDOM].getX() + SongBrowser::getUIScale(77.5f), screen.y - btns[OPTIONS].getHeight()});

    osu->getUserButton()->setSize(SongBrowser::getUIScale(320.f), SongBrowser::getUIScale(75.f));
    osu->getUserButton()->setPos(btns[OPTIONS].getX() + SongBrowser::getUIScale(160.f),
                                 osu->getVirtScreenHeight() - osu->getUserButton()->getSize().y);
    osu->getUserButton()->update();

    // Yes, the order looks whack. That's the correct order.
    Button new_hover = NONE;
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
        if(hovered_btn != NONE) {
            anim::moveLinear(&hovers[hovered_btn], 0.0f, hovers[hovered_btn] * 0.1f, true);
        }
        if(new_hover != NONE) {
            anim::moveLinear(&hovers[new_hover], 1.f, 0.1f, true);
        }

        hovered_btn = new_hover;
    }

    if(clicked && mouse->propagate_clicks) {
        mouse->propagate_clicks = false;
        press_button(hovered_btn);
    }

    // TODO @kiwec: do hovers make sound?
}

void draw() {
    g->pushTransform();
    {
        f32 bar_height = get_min_height();
        Image* img = osu->getSkin()->i_songselect_bot;
        g->setColor(0xffffffff);
        g->scale((f32)osu->getVirtScreenWidth() / (f32)img->getWidth(), bar_height / (f32)img->getHeight());
        g->translate(0, osu->getVirtScreenHeight() - bar_height);
        g->drawImage(img, AnchorPoint::TOP_LEFT);
    }
    g->popTransform();

    // don't double-draw the back button
    if(!ui->getOptionsMenu()->isVisible()) {
        g_songbrowser->backButton->draw();
    }

    // Draw the user card under selection elements, which can cover it for fancy effects
    // (we don't match stable perfectly, but close enough)
    osu->getUserButton()->draw();

    // Careful, these buttons are often used as overlays
    // eg. selection-mode usually covers the whole screen, drawing topbar, bottom right osu cookie etc
    SkinImage* base_imgs[4] = {osu->getSkin()->i_sel_mode, osu->getSkin()->i_sel_mods, osu->getSkin()->i_sel_random,
                               osu->getSkin()->i_sel_options};
    for(i32 i = 0; i < 4; i++) {
        if(base_imgs[i] == nullptr) continue;

        f32 scale = SongBrowser::getSkinScale(base_imgs[i]);
        g->setColor(0xffffffff);
        base_imgs[i]->drawRaw(vec2(btns[i].getX(), osu->getVirtScreenHeight()), scale, AnchorPoint::BOTTOM_LEFT);
    }

    // Ok, and now for the hover images... which are drawn in a weird order, same as update_bottombar()
    auto random_img = osu->getSkin()->i_sel_random_over;
    f32 random_scale = SongBrowser::getSkinScale(random_img);
    g->setColor(Color(0xffffffff).setA(hovers[RANDOM]));
    random_img->drawRaw(vec2(btns[RANDOM].getX(), osu->getVirtScreenHeight()), random_scale, AnchorPoint::BOTTOM_LEFT);

    auto mods_img = osu->getSkin()->i_sel_mods_over;
    f32 mods_scale = SongBrowser::getSkinScale(mods_img);
    g->setColor(Color(0xffffffff).setA(hovers[MODS]));
    mods_img->drawRaw(vec2(btns[MODS].getX(), osu->getVirtScreenHeight()), mods_scale, AnchorPoint::BOTTOM_LEFT);

    auto mode_img = osu->getSkin()->i_sel_mode_over;
    f32 mode_scale = SongBrowser::getSkinScale(mode_img);
    g->setColor(Color(0xffffffff).setA(hovers[MODE]));
    mode_img->drawRaw(vec2(btns[MODE].getX(), osu->getVirtScreenHeight()), mode_scale, AnchorPoint::BOTTOM_LEFT);

    auto options_img = osu->getSkin()->i_sel_options_over;
    f32 options_scale = SongBrowser::getSkinScale(options_img);
    g->setColor(Color(0xffffffff).setA(hovers[OPTIONS]));
    options_img->drawRaw(vec2(btns[OPTIONS].getX(), osu->getVirtScreenHeight()), options_scale,
                         AnchorPoint::BOTTOM_LEFT);

    // mode-osu-small (often used as overlay)
    auto mos_img = osu->getSkin()->i_mode_osu_small;
    if(mos_img != nullptr) {
        f32 mos_scale = SongBrowser::getSkinScale(mos_img);
        g->setBlendMode(DrawBlendMode::BLEND_MODE_ADDITIVE);
        g->setColor(0xffffffff);
        mos_img->drawRaw(vec2(btns[MODE].getX() + (btns[MODS].getX() - btns[MODE].getX()) / 2.f,
                              osu->getVirtScreenHeight() - SongBrowser::getUIScale(56.f)),
                         mos_scale, AnchorPoint::CENTER);
        g->setBlendMode(DrawBlendMode::BLEND_MODE_ALPHA);
    }

    // background task busy notification
    // XXX: move this to permanent toasts
    McFont* font = engine->getDefaultFont();
    i32 calcx = osu->getUserButton()->getPos().x + osu->getUserButton()->getSize().x + 20;
    i32 calcy = osu->getUserButton()->getPos().y + 30;
    if(DBRecalculator::get_maps_total() > 0) {
        UString msg = fmt::format("Calculating stars ({}/{}) ...", DBRecalculator::get_maps_processed(),
                                      DBRecalculator::get_maps_total());
        g->setColor(0xff333333);
        g->pushTransform();
        g->translate(calcx, calcy);
        g->drawString(font, msg);
        g->popTransform();
        calcy += font->getHeight() + 10;
    }
    if(cv::normalize_loudness.getBool() && VolNormalization::get_total() > 0 &&
       VolNormalization::get_computed() < VolNormalization::get_total()) {
        UString msg = fmt::format("Computing loudness ({}/{}) ...", VolNormalization::get_computed(),
                                      VolNormalization::get_total());
        g->setColor(0xff333333);
        g->pushTransform();
        g->translate(calcx, calcy);
        g->drawString(font, msg);
        g->popTransform();
        calcy += font->getHeight() + 10;
    }
    const auto calc_total = DBRecalculator::get_scores_total();
    const auto calc_computed = DBRecalculator::get_scores_processed();
    if(calc_total > 0 && calc_computed < calc_total) {
        UString msg = fmt::format("Converting scores ({}/{}) ...", calc_computed, calc_total);
        g->setColor(0xff333333);
        g->pushTransform();
        g->translate(calcx, calcy);
        g->drawString(font, msg);
        g->popTransform();
        calcy += font->getHeight() + 10;
    }
}

// TODO @kiwec: default icon for mode-osu-small
}  // namespace BottomBar
