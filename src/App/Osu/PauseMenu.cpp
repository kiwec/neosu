// Copyright (c) 2016, PG, All rights reserved.
#include "PauseMenu.h"

#include <cmath>
#include <utility>

#include "AnimationHandler.h"
#include "Bancho.h"
#include "BeatmapInterface.h"
#include "CBaseUIContainer.h"
#include "Chat.h"
#include "OsuConVars.h"
#include "MakeDelegateWrapper.h"
#include "Engine.h"
#include "BanchoNetworking.h"
#include "HUD.h"
#include "Keyboard.h"
#include "ModSelector.h"
#include "OptionsMenu.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "RichPresence.h"
#include "Skin.h"
#include "SoundEngine.h"
#include "UIPauseMenuButton.h"

PauseMenu::PauseMenu() : OsuScreen() {
    this->bScheduledVisibility = false;
    this->bScheduledVisibilityChange = false;

    this->selectedButton = nullptr;
    this->fWarningArrowsAnimStartTime = 0.0f;
    this->fWarningArrowsAnimAlpha = 0.0f;
    this->fWarningArrowsAnimX = 0.0f;
    this->fWarningArrowsAnimY = 0.0f;
    this->bInitialWarningArrowFlyIn = true;

    this->bContinueEnabled = true;
    this->bClick1Down = false;
    this->bClick2Down = false;

    this->fDimAnim = 0.0f;

    this->setSize(osu->getVirtScreenWidth(), osu->getVirtScreenHeight());

    UIPauseMenuButton *continueButton =
        this->addButton([]() -> Image * { return osu->getSkin()->i_pause_continue; }, "Resume");
    UIPauseMenuButton *retryButton =
        this->addButton([]() -> Image * { return osu->getSkin()->i_pause_retry; }, "Retry");
    UIPauseMenuButton *backButton = this->addButton([]() -> Image * { return osu->getSkin()->i_pause_back; }, "Quit");

    continueButton->setClickCallback(SA::MakeDelegate<&PauseMenu::onContinueClicked>(this));
    retryButton->setClickCallback(SA::MakeDelegate<&PauseMenu::onRetryClicked>(this));
    backButton->setClickCallback(SA::MakeDelegate<&PauseMenu::onBackClicked>(this));

    this->updateLayout();
}

void PauseMenu::draw() {
    const bool isAnimating = anim::isAnimating(&this->fDimAnim);
    if(!this->bVisible && !isAnimating) return;

    // draw dim
    if(cv::pause_dim_background.getBool()) {
        g->setColor(argb(this->fDimAnim * cv::pause_dim_alpha.getFloat(), 0.078f, 0.078f, 0.078f));
        g->fillRect(0, 0, osu->getVirtScreenWidth(), osu->getVirtScreenHeight());
    }

    // draw background image
    if((this->bVisible || isAnimating)) {
        Image *image = nullptr;
        if(this->bContinueEnabled)
            image = osu->getSkin()->i_pause_overlay;
        else
            image = osu->getSkin()->i_fail_bg;

        if(image != MISSING_TEXTURE) {
            const float scale = Osu::getImageScaleToFillResolution(image, osu->getVirtScreenSize());
            const vec2 centerTrans = (osu->getVirtScreenSize() / 2.f);

            g->setColor(argb(this->fDimAnim, 1.0f, 1.0f, 1.0f));
            g->pushTransform();
            {
                g->scale(scale, scale);
                g->translate((int)centerTrans.x, (int)centerTrans.y);
                g->drawImage(image);
            }
            g->popTransform();
        }
    }

    // draw buttons
    for(auto &button : this->buttons) {
        button->setAlpha(1.0f - (1.0f - this->fDimAnim) * (1.0f - this->fDimAnim) * (1.0f - this->fDimAnim));
    }
    OsuScreen::draw();

    // draw selection arrows
    if(this->selectedButton != nullptr) {
        const Color arrowColor = argb(255, 0, 114, 255);
        float animation = std::fmod((float)(engine->getTime() - this->fWarningArrowsAnimStartTime) * 3.2f, 2.0f);
        if(animation > 1.0f) animation = 2.0f - animation;

        animation = -animation * (animation - 2);  // quad out
        const float offset = osu->getUIScale(20.0f + 45.0f * animation);

        g->setColor(Color(arrowColor).setA(this->fWarningArrowsAnimAlpha * this->fDimAnim));

        osu->getHUD()->drawWarningArrow(vec2(this->fWarningArrowsAnimX, this->fWarningArrowsAnimY) +
                                            vec2(0, this->selectedButton->getSize().y / 2) - vec2(offset, 0),
                                        false, false);
        osu->getHUD()->drawWarningArrow(
            vec2(osu->getVirtScreenWidth() - this->fWarningArrowsAnimX, this->fWarningArrowsAnimY) +
                vec2(0, this->selectedButton->getSize().y / 2) + vec2(offset, 0),
            true, false);
    }
}

void PauseMenu::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible) return;

    // hide retry button in multiplayer
    this->buttons[1]->setVisible(!BanchoState::is_playing_a_multi_map());

    // update and focus handling
    OsuScreen::mouse_update(propagate_clicks);

    if(this->bScheduledVisibilityChange) {
        this->bScheduledVisibilityChange = false;
        this->setVisible(this->bScheduledVisibility);
    }

    if(anim::isAnimating(&this->fWarningArrowsAnimX)) this->fWarningArrowsAnimStartTime = engine->getTime();
}

void PauseMenu::onContinueClicked() {
    if(!this->bContinueEnabled) return;
    if(anim::isAnimating(&this->fDimAnim)) return;

    soundEngine->play(osu->getSkin()->s_click_pause_continue);
    osu->getMapInterface()->pause();

    this->scheduleVisibilityChange(false);
}

void PauseMenu::onRetryClicked() {
    if(BanchoState::is_playing_a_multi_map()) return;  // sanity
    if(anim::isAnimating(&this->fDimAnim)) return;

    soundEngine->play(osu->getSkin()->s_click_pause_retry);
    osu->getMapInterface()->restart();

    this->scheduleVisibilityChange(false);
}

void PauseMenu::onBackClicked() {
    if(anim::isAnimating(&this->fDimAnim)) return;

    soundEngine->play(osu->getSkin()->s_click_pause_back);
    osu->getMapInterface()->stop(true);

    this->scheduleVisibilityChange(false);
}

void PauseMenu::onSelectionChange() {
    if(this->selectedButton != nullptr) {
        if(this->bInitialWarningArrowFlyIn) {
            this->bInitialWarningArrowFlyIn = false;

            this->fWarningArrowsAnimY = this->selectedButton->getPos().y;
            this->fWarningArrowsAnimX = this->selectedButton->getPos().x - osu->getUIScale(170.0f);

            anim::moveLinear(&this->fWarningArrowsAnimAlpha, 1.0f, 0.3f);
            anim::moveQuadIn(&this->fWarningArrowsAnimX, this->selectedButton->getPos().x, 0.3f);
        } else
            this->fWarningArrowsAnimX = this->selectedButton->getPos().x;

        anim::moveQuadOut(&this->fWarningArrowsAnimY, this->selectedButton->getPos().y, 0.1f);
    }
}

void PauseMenu::onKeyDown(KeyboardEvent &e) {
    OsuScreen::onKeyDown(e);  // only used for options menu
    if(!this->bVisible || e.isConsumed()) return;

    if(e == cv::LEFT_CLICK.getVal<SCANCODE>() || e == cv::RIGHT_CLICK.getVal<SCANCODE>() ||
       e == cv::LEFT_CLICK_2.getVal<SCANCODE>() || e == cv::RIGHT_CLICK_2.getVal<SCANCODE>()) {
        bool fireButtonClick = false;
        if((e == cv::LEFT_CLICK.getVal<SCANCODE>() || e == cv::LEFT_CLICK_2.getVal<SCANCODE>()) && !this->bClick1Down) {
            this->bClick1Down = true;
            fireButtonClick = true;
        }
        if((e == cv::RIGHT_CLICK.getVal<SCANCODE>() || e == cv::RIGHT_CLICK_2.getVal<SCANCODE>()) &&
           !this->bClick2Down) {
            this->bClick2Down = true;
            fireButtonClick = true;
        }
        if(fireButtonClick) {
            for(auto &button : this->buttons) {
                if(button->isMouseInside()) {
                    button->click();
                    break;
                }
            }
        }
    }

    // handle arrow keys selection
    if(this->buttons.size() > 0) {
        if(!keyboard->isAltDown() && e == KEY_DOWN) {
            UIPauseMenuButton *nextSelectedButton = this->buttons[0];

            // get first visible button
            for(auto &button : this->buttons) {
                if(!button->isVisible()) continue;

                nextSelectedButton = button;
                break;
            }

            // next selection logic
            bool next = false;
            for(auto &button : this->buttons) {
                if(!button->isVisible()) continue;

                if(next) {
                    nextSelectedButton = button;
                    break;
                }
                if(this->selectedButton == button) next = true;
            }
            this->selectedButton = nextSelectedButton;
            this->onSelectionChange();
        }

        if(!keyboard->isAltDown() && e == KEY_UP) {
            UIPauseMenuButton *nextSelectedButton = this->buttons.back();

            // get first visible button
            for(int i = this->buttons.size() - 1; i >= 0; i--) {
                if(!this->buttons[i]->isVisible()) continue;

                nextSelectedButton = this->buttons[i];
                break;
            }

            // next selection logic
            bool next = false;
            for(int i = this->buttons.size() - 1; i >= 0; i--) {
                if(!this->buttons[i]->isVisible()) continue;

                if(next) {
                    nextSelectedButton = this->buttons[i];
                    break;
                }
                if(this->selectedButton == this->buttons[i]) next = true;
            }
            this->selectedButton = nextSelectedButton;
            this->onSelectionChange();
        }

        if(this->selectedButton != nullptr && (e == KEY_ENTER || e == KEY_NUMPAD_ENTER)) this->selectedButton->click();
    }

    // consume ALL events, except for a few special binds which are allowed through (e.g. for unpause or changing the
    // local offset in Osu.cpp)
    if(e != KEY_ESCAPE && e != cv::GAME_PAUSE.getVal<SCANCODE>() && e != cv::INCREASE_LOCAL_OFFSET.getVal<SCANCODE>() &&
       e != cv::DECREASE_LOCAL_OFFSET.getVal<SCANCODE>())
        e.consume();
}

void PauseMenu::onKeyUp(KeyboardEvent &e) {
    if(e == cv::LEFT_CLICK.getVal<SCANCODE>() || e == cv::LEFT_CLICK_2.getVal<SCANCODE>()) this->bClick1Down = false;

    if(e == cv::RIGHT_CLICK.getVal<SCANCODE>() || e == cv::RIGHT_CLICK_2.getVal<SCANCODE>()) this->bClick2Down = false;
}

void PauseMenu::onChar(KeyboardEvent &e) {
    if(!this->bVisible) return;

    e.consume();
}

void PauseMenu::scheduleVisibilityChange(bool visible) {
    if(visible != this->bVisible) {
        this->bScheduledVisibilityChange = true;
        this->bScheduledVisibility = visible;
    }

    // HACKHACK:
    if(!visible) this->setContinueEnabled(true);
}

void PauseMenu::updateLayout() {
    const float height = (osu->getVirtScreenHeight() / (float)this->buttons.size());
    const float half = (this->buttons.size() - 1) / 2.0f;

    float maxWidth = 0.0f;
    float maxHeight = 0.0f;
    for(auto &button : this->buttons) {
        Image *img = button->getImage();
        if(img == nullptr) img = MISSING_TEXTURE;

        const float scale = osu->getUIScale(256) / (411.0f * (osu->getSkin()->i_pause_continue.scale()));

        button->setBaseScale(scale, scale);
        button->setSize(img->getWidth() * scale, img->getHeight() * scale);

        if(button->getSize().x > maxWidth) maxWidth = button->getSize().x;
        if(button->getSize().y > maxHeight) maxHeight = button->getSize().y;
    }

    for(int i = 0; i < this->buttons.size(); i++) {
        vec2 newPos =
            vec2(osu->getVirtScreenWidth() / 2.0f - maxWidth / 2, (i + 1) * height - height / 2.0f - maxHeight / 2.0f);

        const float pinch = std::max(0.0f, (height / 2.0f - maxHeight / 2.0f));
        if((float)i < half)
            newPos.y += pinch;
        else if((float)i > half)
            newPos.y -= pinch;

        this->buttons[i]->setPos(newPos);
        this->buttons[i]->setSize(maxWidth, maxHeight);
    }

    this->onSelectionChange();
}

void PauseMenu::onResolutionChange(vec2 newResolution) {
    this->setSize(newResolution);
    this->updateLayout();
}

CBaseUIContainer *PauseMenu::setVisible(bool visible) {
    const bool wasVisible = this->bVisible;
    this->bVisible = visible;

    const bool can_pause = !cv::mod_no_pausing.getBool();
    if(can_pause) {
        if(visible) {
            if(!osu->getScore()->isDead()) {
                soundEngine->play(osu->getSkin()->s_pause_loop);
            }
        } else {
            soundEngine->stop(osu->getSkin()->s_fail);
            soundEngine->stop(osu->getSkin()->s_pause_loop);
        }
    }

    if(!osu->isInPlayMode()) return this;  // sanity

    this->setContinueEnabled(!osu->getMapInterface()->hasFailed());

    if(can_pause) {
        if(visible) {
            if(this->bContinueEnabled) {
                RichPresence::setBanchoStatus("Taking a break", Action::PAUSED);

                if(!BanchoState::spectators.empty()) {
                    Packet packet;
                    packet.id = OUTP_SPECTATE_FRAMES;
                    packet.write<i32>(0);
                    packet.write<u16>(0);
                    packet.write<u8>((u8)LiveReplayAction::PAUSE);
                    packet.write<ScoreFrame>(ScoreFrame::get());
                    packet.write<u16>(osu->getMapInterface()->spectator_sequence++);
                    BANCHO::Net::send_packet(packet);
                }
            } else {
                RichPresence::setBanchoStatus("Failed", Action::SUBMITTING);
            }
        } else {
            RichPresence::onPlayStart();

            if(!BanchoState::spectators.empty()) {
                Packet packet;
                packet.id = OUTP_SPECTATE_FRAMES;
                packet.write<i32>(0);
                packet.write<u16>(0);
                packet.write<u8>((u8)LiveReplayAction::UNPAUSE);
                packet.write<ScoreFrame>(ScoreFrame::get());
                packet.write<u16>(osu->getMapInterface()->spectator_sequence++);
                BANCHO::Net::send_packet(packet);
            }
        }
    }

    // HACKHACK: force disable mod selection screen in case it was open and the beatmap ended/failed
    osu->getModSelector()->setVisible(false);

    // reset
    this->selectedButton = nullptr;
    this->bInitialWarningArrowFlyIn = true;
    this->fWarningArrowsAnimAlpha = 0.0f;
    this->bScheduledVisibility = visible;
    this->bScheduledVisibilityChange = false;

    if(this->bVisible) this->updateLayout();

    osu->updateConfineCursor();
    osu->updateWindowsKeyDisable();

    if(this->bVisible != wasVisible) {
        anim::moveQuadOut(
            &this->fDimAnim, (this->bVisible ? 1.0f : 0.0f),
            cv::pause_anim_duration.getFloat() * (this->bVisible ? 1.0f - this->fDimAnim : this->fDimAnim), true);
    }
    osu->getChat()->updateVisibility();
    return this;
}

void PauseMenu::setContinueEnabled(bool continueEnabled) {
    this->bContinueEnabled = continueEnabled;
    if(this->buttons.size() > 0) this->buttons[0]->setVisible(this->bContinueEnabled);
}

UIPauseMenuButton *PauseMenu::addButton(std::function<Image *()> getImageFunc, UString btn_name) {
    auto *button = new UIPauseMenuButton(std::move(getImageFunc), 0, 0, 0, 0, std::move(btn_name));
    this->addBaseUIElement(button);
    this->buttons.push_back(button);
    return button;
}
