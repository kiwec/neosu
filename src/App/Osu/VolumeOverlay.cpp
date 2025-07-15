#include "VolumeOverlay.h"

#include "AnimationHandler.h"
#include "Beatmap.h"
#include "CBaseUIContainer.h"
#include "Changelog.h"
#include "Chat.h"
#include "ConVar.h"
#include "Engine.h"
#include "KeyBindings.h"
#include "Keyboard.h"
#include "ModSelector.h"
#include "Mouse.h"
#include "OptionsMenu.h"
#include "Osu.h"
#include "PauseMenu.h"
#include "RankingScreen.h"
#include "Skin.h"
#include "SongBrowser/SongBrowser.h"
#include "Sound.h"
#include "SoundEngine.h"
#include "UIContextMenu.h"
#include "UIVolumeSlider.h"

VolumeOverlay::VolumeOverlay() : OsuScreen() {
    cv::volume_master.setCallback(fastdelegate::MakeDelegate(this, &VolumeOverlay::onMasterVolumeChange));
    cv::volume_effects.setCallback(fastdelegate::MakeDelegate(this, &VolumeOverlay::onEffectVolumeChange));
    cv::volume_music.setCallback(fastdelegate::MakeDelegate(this, &VolumeOverlay::onMusicVolumeChange));
    cv::hud_volume_size_multiplier.setCallback(fastdelegate::MakeDelegate(this, &VolumeOverlay::updateLayout));

    this->fVolumeChangeTime = 0.0f;
    this->fVolumeChangeFade = 1.0f;
    this->fLastVolume = cv::volume_master.getFloat();
    this->volumeSliderOverlayContainer = new CBaseUIContainer();

    this->volumeMaster = new UIVolumeSlider(0, 0, 450, 75, "");
    this->volumeMaster->setType(UIVolumeSlider::TYPE::MASTER);
    this->volumeMaster->setBlockSize(this->volumeMaster->getSize().y + 7, this->volumeMaster->getSize().y);
    this->volumeMaster->setAllowMouseWheel(false);
    this->volumeMaster->setAnimated(false);
    this->volumeMaster->setSelected(true);
    this->volumeSliderOverlayContainer->addBaseUIElement(this->volumeMaster);
    this->volumeEffects = new UIVolumeSlider(0, 0, this->volumeMaster->getSize().x, this->volumeMaster->getSize().y / 1.5f, "");
    this->volumeEffects->setType(UIVolumeSlider::TYPE::EFFECTS);
    this->volumeEffects->setBlockSize((this->volumeMaster->getSize().y + 7) / 1.5f, this->volumeMaster->getSize().y / 1.5f);
    this->volumeEffects->setAllowMouseWheel(false);
    this->volumeEffects->setKeyDelta(cv::volume_change_interval.getFloat());
    this->volumeEffects->setAnimated(false);
    this->volumeSliderOverlayContainer->addBaseUIElement(this->volumeEffects);
    this->volumeMusic = new UIVolumeSlider(0, 0, this->volumeMaster->getSize().x, this->volumeMaster->getSize().y / 1.5f, "");
    this->volumeMusic->setType(UIVolumeSlider::TYPE::MUSIC);
    this->volumeMusic->setBlockSize((this->volumeMaster->getSize().y + 7) / 1.5f, this->volumeMaster->getSize().y / 1.5f);
    this->volumeMusic->setAllowMouseWheel(false);
    this->volumeMusic->setKeyDelta(cv::volume_change_interval.getFloat());
    this->volumeMusic->setAnimated(false);
    this->volumeSliderOverlayContainer->addBaseUIElement(this->volumeMusic);

    soundEngine->setVolume(cv::volume_master.getFloat());
    this->updateLayout();
}

void VolumeOverlay::animate() {
    const bool active = this->fVolumeChangeTime > engine->getTime();
    this->fVolumeChangeTime = engine->getTime() + cv::hud_volume_duration.getFloat() + 0.2f;

    if(!active) {
        this->fVolumeChangeFade = 0.0f;
        anim->moveQuadOut(&this->fVolumeChangeFade, 1.0f, 0.15f, true);
    } else
        anim->moveQuadOut(&this->fVolumeChangeFade, 1.0f, 0.1f * (1.0f - this->fVolumeChangeFade), true);

    anim->moveQuadOut(&this->fVolumeChangeFade, 0.0f, 0.20f, cv::hud_volume_duration.getFloat(), false);
    anim->moveQuadOut(&this->fLastVolume, cv::volume_master.getFloat(), 0.15f, 0.0f, true);
}

void VolumeOverlay::draw() {
    if(!this->isVisible()) return;

    const float dpiScale = Osu::getUIScale();
    const float sizeMultiplier = cv::hud_volume_size_multiplier.getFloat() * dpiScale;

    if(this->fVolumeChangeFade != 1.0f) {
        g->push3DScene(McRect(this->volumeMaster->getPos().x, this->volumeMaster->getPos().y, this->volumeMaster->getSize().x,
                              (osu->getScreenHeight() - this->volumeMaster->getPos().y) * 2.05f));
        g->rotate3DScene(-(1.0f - this->fVolumeChangeFade) * 90, 0, 0);
        g->translate3DScene(0, (this->fVolumeChangeFade * 60 - 60) * sizeMultiplier / 1.5f,
                            ((this->fVolumeChangeFade) * 500 - 500) * sizeMultiplier / 1.5f);
    }

    this->volumeMaster->setPos(osu->getScreenSize() - this->volumeMaster->getSize() -
                           Vector2(this->volumeMaster->getMinimumExtraTextWidth(), this->volumeMaster->getSize().y));
    this->volumeEffects->setPos(this->volumeMaster->getPos() - Vector2(0, this->volumeEffects->getSize().y + 20 * sizeMultiplier));
    this->volumeMusic->setPos(this->volumeEffects->getPos() - Vector2(0, this->volumeMusic->getSize().y + 20 * sizeMultiplier));

    this->volumeSliderOverlayContainer->draw();

    if(this->fVolumeChangeFade != 1.0f) g->pop3DScene();
}

void VolumeOverlay::mouse_update(bool *propagate_clicks) {
    this->volumeMaster->setEnabled(this->fVolumeChangeTime > engine->getTime());
    this->volumeEffects->setEnabled(this->volumeMaster->isEnabled());
    this->volumeMusic->setEnabled(this->volumeMaster->isEnabled());
    this->volumeSliderOverlayContainer->setSize(osu->getScreenSize());
    this->volumeSliderOverlayContainer->mouse_update(propagate_clicks);

    if(!this->volumeMaster->isBusy())
        this->volumeMaster->setValue(cv::volume_master.getFloat(), false);
    else {
        cv::volume_master.setValue(this->volumeMaster->getFloat());
        this->fLastVolume = this->volumeMaster->getFloat();
    }

    if(!this->volumeMusic->isBusy())
        this->volumeMusic->setValue(cv::volume_music.getFloat(), false);
    else
        cv::volume_music.setValue(this->volumeMusic->getFloat());

    if(!this->volumeEffects->isBusy())
        this->volumeEffects->setValue(cv::volume_effects.getFloat(), false);
    else
        cv::volume_effects.setValue(this->volumeEffects->getFloat());

    // force focus back to master if no longer active
    if(engine->getTime() > this->fVolumeChangeTime && !this->volumeMaster->isSelected()) {
        this->volumeMusic->setSelected(false);
        this->volumeEffects->setSelected(false);
        this->volumeMaster->setSelected(true);
    }

    // keep overlay alive as long as the user is doing something
    // switch selection if cursor moves inside one of the sliders
    if(this->volumeSliderOverlayContainer->isBusy() ||
       (this->volumeMaster->isEnabled() &&
        (this->volumeMaster->isMouseInside() || this->volumeEffects->isMouseInside() || this->volumeMusic->isMouseInside()))) {
        this->animate();

        const std::vector<CBaseUIElement *> &elements = this->volumeSliderOverlayContainer->getElements();
        for(int i = 0; i < elements.size(); i++) {
            if(((UIVolumeSlider *)elements[i])->checkWentMouseInside()) {
                for(int c = 0; c < elements.size(); c++) {
                    if(c != i) ((UIVolumeSlider *)elements[c])->setSelected(false);
                }
                ((UIVolumeSlider *)elements[i])->setSelected(true);
            }
        }
    }

    // volume inactive to active animation
    if(this->bVolumeInactiveToActiveScheduled && this->fVolumeInactiveToActiveAnim > 0.0f) {
        soundEngine->setVolume(std::lerp(cv::volume_master_inactive.getFloat() * cv::volume_master.getFloat(),
                                                  cv::volume_master.getFloat(), this->fVolumeInactiveToActiveAnim));

        // check if we're done
        if(this->fVolumeInactiveToActiveAnim == 1.0f) this->bVolumeInactiveToActiveScheduled = false;
    }

    // scroll wheel events (should be separate from mouse_update events, but... oh well...)
    const int wheelDelta = mouse->getWheelDeltaVertical() / 120;
    if(this->canChangeVolume() && wheelDelta != 0) {
        if(wheelDelta > 0) {
            this->volumeUp(wheelDelta);
        } else {
            this->volumeDown(-wheelDelta);
        }
    }
}

void VolumeOverlay::updateLayout() {
    const float dpiScale = Osu::getUIScale();
    const float sizeMultiplier = cv::hud_volume_size_multiplier.getFloat() * dpiScale;

    this->volumeMaster->setSize(300 * sizeMultiplier, 50 * sizeMultiplier);
    this->volumeMaster->setBlockSize(this->volumeMaster->getSize().y + 7 * dpiScale, this->volumeMaster->getSize().y);

    this->volumeEffects->setSize(this->volumeMaster->getSize().x, this->volumeMaster->getSize().y / 1.5f);
    this->volumeEffects->setBlockSize((this->volumeMaster->getSize().y + 7 * dpiScale) / 1.5f,
                                  this->volumeMaster->getSize().y / 1.5f);

    this->volumeMusic->setSize(this->volumeMaster->getSize().x, this->volumeMaster->getSize().y / 1.5f);
    this->volumeMusic->setBlockSize((this->volumeMaster->getSize().y + 7 * dpiScale) / 1.5f,
                                this->volumeMaster->getSize().y / 1.5f);
}

void VolumeOverlay::onResolutionChange(Vector2  /*newResolution*/) { this->updateLayout(); }

void VolumeOverlay::onKeyDown(KeyboardEvent &key) {
    if(!this->canChangeVolume()) return;

    if(key == (KEYCODE)cv::INCREASE_VOLUME.getInt()) {
        this->volumeUp();
        key.consume();
    } else if(key == (KEYCODE)cv::DECREASE_VOLUME.getInt()) {
        this->volumeDown();
        key.consume();
    } else if(this->isVisible()) {
        if(key == KEY_LEFT) {
            const std::vector<CBaseUIElement *> &elements = this->volumeSliderOverlayContainer->getElements();
            for(int i = 0; i < elements.size(); i++) {
                if(((UIVolumeSlider *)elements[i])->isSelected()) {
                    const int nextIndex = (i == elements.size() - 1 ? 0 : i + 1);
                    ((UIVolumeSlider *)elements[i])->setSelected(false);
                    ((UIVolumeSlider *)elements[nextIndex])->setSelected(true);
                    break;
                }
            }
            this->animate();
            key.consume();
        } else if(key == KEY_RIGHT) {
            const std::vector<CBaseUIElement *> &elements = this->volumeSliderOverlayContainer->getElements();
            for(int i = 0; i < elements.size(); i++) {
                if(((UIVolumeSlider *)elements[i])->isSelected()) {
                    const int prevIndex = (i == 0 ? elements.size() - 1 : i - 1);
                    ((UIVolumeSlider *)elements[i])->setSelected(false);
                    ((UIVolumeSlider *)elements[prevIndex])->setSelected(true);
                    break;
                }
            }
            this->animate();
            key.consume();
        }
    }
}

bool VolumeOverlay::isBusy() {
    return (this->volumeMaster->isEnabled() && (this->volumeMaster->isBusy() || this->volumeMaster->isMouseInside())) ||
           (this->volumeEffects->isEnabled() && (this->volumeEffects->isBusy() || this->volumeEffects->isMouseInside())) ||
           (this->volumeMusic->isEnabled() && (this->volumeMusic->isBusy() || this->volumeMusic->isMouseInside()));
}

bool VolumeOverlay::isVisible() { return engine->getTime() < this->fVolumeChangeTime; }

bool VolumeOverlay::canChangeVolume() {
    bool can_scroll = true;
    if(osu->songBrowser2->isVisible() && db->isFinished() == 1.f) {
        can_scroll = false;
    }
    if(osu->optionsMenu->isVisible()) can_scroll = false;
    if(osu->optionsMenu->contextMenu->isVisible()) can_scroll = false;
    if(osu->changelog->isVisible()) can_scroll = false;
    if(osu->rankingScreen->isVisible()) can_scroll = false;
    if(osu->modSelector->isMouseInScrollView()) can_scroll = false;
    if(osu->chat->isMouseInChat()) can_scroll = false;
    if(osu->isInPlayMode() && cv::disable_mousewheel.getBool() && !osu->pauseMenu->isVisible()) can_scroll = false;

    if(this->isBusy()) can_scroll = true;
    if(keyboard->isAltDown()) can_scroll = true;

    return can_scroll;
}

void VolumeOverlay::gainFocus() {
    if(soundEngine->hasExclusiveOutput()) return;

    this->fVolumeInactiveToActiveAnim = 0.0f;
    anim->moveLinear(&this->fVolumeInactiveToActiveAnim, 1.0f, 0.3f, 0.1f, true);
}

void VolumeOverlay::loseFocus() {
    if(soundEngine->hasExclusiveOutput()) return;

    this->bVolumeInactiveToActiveScheduled = true;
    anim->deleteExistingAnimation(&this->fVolumeInactiveToActiveAnim);
    this->fVolumeInactiveToActiveAnim = 0.0f;
    soundEngine->setVolume(cv::volume_master_inactive.getFloat() * cv::volume_master.getFloat());
}

void VolumeOverlay::onVolumeChange(int multiplier) {
    // sanity reset
    this->bVolumeInactiveToActiveScheduled = false;
    anim->deleteExistingAnimation(&this->fVolumeInactiveToActiveAnim);
    this->fVolumeInactiveToActiveAnim = 0.0f;

    // chose which volume to change, depending on the volume overlay, default is master
    ConVar *volumeConVar = &cv::volume_master;
    if(this->volumeMusic->isSelected())
        volumeConVar = &cv::volume_music;
    else if(this->volumeEffects->isSelected())
        volumeConVar = &cv::volume_effects;

    // change the volume
    float newVolume =
        std::clamp<float>(volumeConVar->getFloat() + cv::volume_change_interval.getFloat() * multiplier, 0.0f, 1.0f);
    volumeConVar->setValue(newVolume);
    this->animate();
}

void VolumeOverlay::onMasterVolumeChange(const UString & /*oldValue*/, const UString &newValue) {
    if(this->bVolumeInactiveToActiveScheduled) return;  // not very clean, but w/e

    float newVolume = newValue.toFloat();
    soundEngine->setVolume(newVolume);
}

void VolumeOverlay::onEffectVolumeChange() {
    float volume = cv::volume_effects.getFloat();

    auto skin = osu->getSkin();
    for(int i = 0; i < skin->sounds.size(); i++) {
        skin->sounds[i]->setVolume(volume);
    }

    skin->resetSampleVolume();
}

void VolumeOverlay::onMusicVolumeChange(const UString & /*oldValue*/, const UString & /*newValue*/) {
    auto music = osu->getSelectedBeatmap()->getMusic();
    if(music != NULL) {
        music->setVolume(osu->getSelectedBeatmap()->getIdealVolume());
    }
}
