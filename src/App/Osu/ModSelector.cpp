// Copyright (c) 2015, PG, All rights reserved.
#include "ModSelector.h"

#include <utility>

#include "AnimationHandler.h"
#include "Bancho.h"
#include "BanchoNetworking.h"
#include "Beatmap.h"
#include "CBaseUICheckbox.h"
#include "CBaseUIContainer.h"
#include "CBaseUIImageButton.h"
#include "CBaseUILabel.h"
#include "CBaseUIScrollView.h"
#include "CBaseUISlider.h"
#include "ConVar.h"
#include "DatabaseBeatmap.h"
#include "Engine.h"
#include "Environment.h"
#include "GameRules.h"
#include "HUD.h"
#include "Icons.h"
#include "KeyBindings.h"
#include "Keyboard.h"
#include "LegacyReplay.h"
#include "Mouse.h"
#include "OptionsMenu.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "RichPresence.h"
#include "RoomScreen.h"
#include "Skin.h"
#include "SkinImage.h"
#include "SongBrowser/SongBrowser.h"
#include "SoundEngine.h"
#include "TooltipOverlay.h"
#include "UIButton.h"
#include "UICheckbox.h"
#include "UIModSelectorModButton.h"
#include "UISlider.h"
#include "VertexArrayObject.h"

class ModSelectorOverrideSliderDescButton : public CBaseUIButton {
   public:
    ModSelectorOverrideSliderDescButton(float xPos, float yPos, float xSize, float ySize, UString name, UString text)
        : CBaseUIButton(xPos, yPos, xSize, ySize, std::move(name), std::move(text)) {}

    void mouse_update(bool *propagate_clicks) override {
        if(!this->bVisible) return;
        CBaseUIButton::mouse_update(propagate_clicks);

        if(this->isMouseInside() && this->sTooltipText.length() > 0) {
            osu->getTooltipOverlay()->begin();
            {
                osu->getTooltipOverlay()->addLine(this->sTooltipText);
            }
            osu->getTooltipOverlay()->end();
        }
    }

    void setTooltipText(UString tooltipText) { this->sTooltipText = std::move(tooltipText); }

   private:
    void drawText() override {
        if(this->font != nullptr && this->sText.length() > 0) {
            float xPosAdd = this->vSize.x / 2.0f - this->fStringWidth / 2.0f;

            // g->pushClipRect(McRect(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y));
            {
                g->setColor(this->textColor);
                g->pushTransform();
                {
                    g->translate((int)(this->vPos.x + (xPosAdd)),
                                 (int)(this->vPos.y + this->vSize.y / 2.0f + this->fStringHeight / 2.0f));
                    g->drawString(this->font, this->sText);
                }
                g->popTransform();
            }
            // g->popClipRect();
        }
    }

    UString sTooltipText;
};

class ModSelectorOverrideSliderLockButton : public CBaseUICheckbox {
   public:
    ModSelectorOverrideSliderLockButton(float xPos, float yPos, float xSize, float ySize, UString name, UString text)
        : CBaseUICheckbox(xPos, yPos, xSize, ySize, std::move(name), std::move(text)) {
        this->fAnim = 1.0f;
    }

    void draw() override {
        if(!this->bVisible) return;

        const wchar_t icon = (this->bChecked ? Icons::LOCK : Icons::UNLOCK);
        UString iconString;
        iconString.insert(0, icon);

        McFont *iconFont = osu->getFontIcons();
        const float scale = (this->vSize.y / iconFont->getHeight()) * this->fAnim;
        g->setColor(this->bChecked ? 0xffffffff : 0xff1166ff);

        g->pushTransform();
        {
            g->scale(scale, scale);
            g->translate(this->vPos.x + this->vSize.x / 2.0f - iconFont->getStringWidth(iconString) * scale / 2.0f,
                         this->vPos.y + this->vSize.y / 2.0f + (iconFont->getHeight() * scale / 2.0f) * 0.8f);
            g->drawString(iconFont, iconString);
        }
        g->popTransform();
    }

   private:
    void onPressed() override {
        CBaseUICheckbox::onPressed();
        soundEngine->play(this->isChecked() ? osu->getSkin()->getCheckOn() : osu->getSkin()->getCheckOff());

        if(this->isChecked()) {
            // anim->moveQuadOut(&m_fAnim, 1.5f, 0.060f, true);
            // anim->moveQuadIn(&m_fAnim, 1.0f, 0.250f, 0.150f, false);
        } else {
            anim->deleteExistingAnimation(&this->fAnim);
            this->fAnim = 1.0f;
        }
    }

    float fAnim;
};

ModSelector::ModSelector() : OsuScreen() {
    this->fAnimation = 0.0f;
    this->fExperimentalAnimation = 0.0f;
    this->bScheduledHide = false;
    this->bExperimentalVisible = false;
    this->setSize(osu->getScreenWidth(), osu->getScreenHeight());
    this->overrideSliderContainer = new CBaseUIContainer(0, 0, osu->getScreenWidth(), osu->getScreenHeight(), "");
    this->experimentalContainer = new CBaseUIScrollView(-1, 0, osu->getScreenWidth(), osu->getScreenHeight(), "");
    this->experimentalContainer->setHorizontalScrolling(false);
    this->experimentalContainer->setVerticalScrolling(true);
    this->experimentalContainer->setDrawFrame(false);
    this->experimentalContainer->setDrawBackground(false);

    this->bWaitForF1KeyUp = false;

    this->bWaitForCSChangeFinished = false;
    this->bWaitForSpeedChangeFinished = false;
    this->bWaitForHPChangeFinished = false;

    this->speedSlider = nullptr;
    this->bShowOverrideSliderALTHint = true;

    // build mod grid buttons
    this->iGridWidth = 6;
    this->iGridHeight = 3;

    for(int x = 0; x < this->iGridWidth; x++) {
        for(int y = 0; y < this->iGridHeight; y++) {
            auto *imageButton = new UIModSelectorModButton(this, 50, 50, 100, 100, "");
            imageButton->setDrawBackground(false);
            imageButton->setVisible(false);

            this->addBaseUIElement(imageButton);
            this->modButtons.push_back(imageButton);
        }
    }

    // build override sliders
    OVERRIDE_SLIDER overrideCS = this->addOverrideSlider("CS Override", "CS:", &cv::cs_override, 0.0f, 12.5f,
                                                         "Circle Size (higher number = smaller circles).");
    OVERRIDE_SLIDER overrideAR =
        this->addOverrideSlider("AR Override", "AR:", &cv::ar_override, 0.0f, 12.5f,
                                "Approach Rate (higher number = faster circles).", &cv::ar_override_lock);
    OVERRIDE_SLIDER overrideOD =
        this->addOverrideSlider("OD Override", "OD:", &cv::od_override, 0.0f, 12.5f,
                                "Overall Difficulty (higher number = harder accuracy).", &cv::od_override_lock);
    OVERRIDE_SLIDER overrideHP = this->addOverrideSlider("HP Override", "HP:", &cv::hp_override, 0.0f, 12.5f,
                                                         "Hit/Health Points (higher number = harder survival).");

    overrideCS.slider->setAnimated(false);  // quick fix for otherwise possible inconsistencies due to slider vertex
                                            // buffers and animated CS changes
    overrideCS.slider->setChangeCallback(SA::MakeDelegate<&ModSelector::onOverrideSliderChange>(this));
    overrideAR.slider->setChangeCallback(SA::MakeDelegate<&ModSelector::onOverrideSliderChange>(this));
    overrideOD.slider->setChangeCallback(SA::MakeDelegate<&ModSelector::onOverrideSliderChange>(this));
    overrideHP.slider->setChangeCallback(SA::MakeDelegate<&ModSelector::onOverrideSliderChange>(this));

    overrideAR.desc->setClickCallback(SA::MakeDelegate<&ModSelector::onOverrideARSliderDescClicked>(this));
    overrideOD.desc->setClickCallback(SA::MakeDelegate<&ModSelector::onOverrideODSliderDescClicked>(this));

    this->CSSlider = overrideCS.slider;
    this->ARSlider = overrideAR.slider;
    this->ODSlider = overrideOD.slider;
    this->HPSlider = overrideHP.slider;
    this->ARLock = overrideAR.lock;
    this->ODLock = overrideOD.lock;

    OVERRIDE_SLIDER overrideSpeed =
        this->addOverrideSlider("Speed/BPM Multiplier", "x", &cv::speed_override, 0.9f, 2.5f);

    overrideSpeed.slider->setChangeCallback(SA::MakeDelegate<&ModSelector::onOverrideSliderChange>(this));
    // overrideSpeed.slider->setValue(-1.0f, false);
    overrideSpeed.slider->setAnimated(false);  // same quick fix as above
    overrideSpeed.slider->setLiveUpdate(false);

    this->speedSlider = overrideSpeed.slider;

    // build experimental buttons
    this->addExperimentalLabel(" Experimental Mods (!)");
    this->addExperimentalCheckbox(
        "FPoSu: Strafing", "Playfield moves in 3D space (see fposu_mod_strafing_...).\nOnly works in FPoSu mode!",
        &cv::fposu_mod_strafing);
    this->addExperimentalCheckbox("Wobble", "Playfield rotates and moves.", &cv::mod_wobble);
    this->addExperimentalCheckbox("AR Wobble", "Approach rate oscillates between -1 and +1.", &cv::mod_arwobble);
    this->addExperimentalCheckbox(
        "Approach Different",
        "Customize the approach circle animation.\nSee osu_mod_approach_different_style.\nSee "
        "osu_mod_approach_different_initial_size.",
        &cv::mod_approach_different);
    this->addExperimentalCheckbox("Timewarp", "Speed increases from 100% to 150% over the course of the beatmap.",
                                  &cv::mod_timewarp);
    this->addExperimentalCheckbox(
        "AR Timewarp", "Approach rate decreases from 100% to 50% over the course of the beatmap.", &cv::mod_artimewarp);
    this->addExperimentalCheckbox("Minimize", "Circle size decreases from 100% to 50% over the course of the beatmap.",
                                  &cv::mod_minimize);
    this->addExperimentalCheckbox("Fading Cursor", "The cursor fades the higher the combo, becoming invisible at 50.",
                                  &cv::mod_fadingcursor);
    this->addExperimentalCheckbox("First Person", "Centered cursor.", &cv::mod_fps);
    this->addExperimentalCheckbox("Full Alternate", "You can never use the same key twice in a row.",
                                  &cv::mod_fullalternate);
    this->addExperimentalCheckbox("Jigsaw 1", "Unnecessary clicks count as misses.", &cv::mod_jigsaw1);
    this->addExperimentalCheckbox("Jigsaw 2", "Massively reduced slider follow circle radius.", &cv::mod_jigsaw2);
    this->addExperimentalCheckbox("Reverse Sliders",
                                  "Reverses the direction of all sliders. (Reload beatmap to apply!)",
                                  &cv::mod_reverse_sliders);
    this->addExperimentalCheckbox("No 50s", "Only 300s or 100s. Try harder.", &cv::mod_no50s);
    this->addExperimentalCheckbox("No 100s no 50s", "300 or miss. PF \"lite\"", &cv::mod_no100s);
    this->addExperimentalCheckbox("MinG3012", "No 100s. Only 300s or 50s. Git gud.", &cv::mod_ming3012);
    this->addExperimentalCheckbox(
        "Half Timing Window", "The hit timing window is cut in half. Hit early or perfect (300).", &cv::mod_halfwindow);
    this->addExperimentalCheckbox("MillhioreF", "Go below AR 0. Doubled approach time.", &cv::mod_millhioref);
    this->addExperimentalCheckbox(
        "Mafham",
        "Approach rate is set to negative infinity. See the entire beatmap at once.\nUses very "
        "aggressive optimizations to keep the framerate high, you have been warned!",
        &cv::mod_mafham);
    this->addExperimentalCheckbox(
        "Strict Tracking", "Leaving sliders in any way counts as a miss and combo break. (Reload beatmap to apply!)",
        &cv::mod_strict_tracking);
    this->addExperimentalCheckbox("Flip Up/Down", "Playfield is flipped upside down (mirrored at horizontal axis).",
                                  &cv::playfield_mirror_horizontal);
    this->addExperimentalCheckbox("Flip Left/Right", "Playfield is flipped left/right (mirrored at vertical axis).",
                                  &cv::playfield_mirror_vertical);

    this->nonSubmittableWarning = new CBaseUILabel();
    this->nonSubmittableWarning->setDrawFrame(false);
    this->nonSubmittableWarning->setDrawBackground(false);
    this->nonSubmittableWarning->setText("WARNING: Score submission will be disabled due to non-vanilla mod selection.");
    this->nonSubmittableWarning->setTextColor(0xffff0000);
    this->nonSubmittableWarning->setCenterText(true);
    this->nonSubmittableWarning->setVisible(false);
    this->addBaseUIElement(this->nonSubmittableWarning);

    // build score multiplier label
    this->scoreMultiplierLabel = new CBaseUILabel();
    this->scoreMultiplierLabel->setDrawFrame(false);
    this->scoreMultiplierLabel->setDrawBackground(false);
    this->scoreMultiplierLabel->setCenterText(true);
    this->addBaseUIElement(this->scoreMultiplierLabel);

    // build action buttons
    this->resetModsButton = this->addActionButton("1. Reset All Mods");
    this->resetModsButton->setClickCallback(SA::MakeDelegate<&ModSelector::resetModsUserInitiated>(this));
    this->resetModsButton->setColor(0xffc62b00);
    this->closeButton = this->addActionButton("2. Close");
    this->closeButton->setClickCallback(SA::MakeDelegate<&ModSelector::close>(this));
    this->closeButton->setColor(0xff636363);

    this->updateButtons(true);
    this->updateLayout();
}

void ModSelector::updateButtons(bool initial) {
    this->modButtonEasy = this->setModButtonOnGrid(
        0, 0, 0, initial && osu->getModEZ(), &cv::mod_easy, "ez",
        "Reduces overall difficulty - larger circles, more forgiving HP drain, less accuracy required.",
        []() -> SkinImage * { return osu->getSkin()->getSelectionModEasy(); });
    this->modButtonNofail = this->setModButtonOnGrid(
        1, 0, 0, initial && osu->getModNF(), &cv::mod_nofail, "nf",
        "You can't fail. No matter what.\nNOTE: To disable drain completely:\nOptions > Gameplay > "
        "Mechanics > \"Select HP Drain\" > \"None\".",
        []() -> SkinImage * { return osu->getSkin()->getSelectionModNoFail(); });
    this->setModButtonOnGrid(4, 0, 0, initial && osu->getModNightmare(), &cv::mod_nightmare, "nightmare",
                             "Unnecessary clicks count as misses.\nMassively reduced slider follow circle radius.",
                             []() -> SkinImage * { return osu->getSkin()->getSelectionModNightmare(); });

    this->modButtonHardrock = this->setModButtonOnGrid(
        0, 1, 0, initial && osu->getModHR(), &cv::mod_hardrock, "hr", "Everything just got a bit harder...",
        []() -> SkinImage * { return osu->getSkin()->getSelectionModHardRock(); });
    this->modButtonSuddendeath = this->setModButtonOnGrid(
        1, 1, 0, initial && osu->getModSD(), &cv::mod_suddendeath, "sd", "Miss a note and fail.",
        []() -> SkinImage * { return osu->getSkin()->getSelectionModSuddenDeath(); });
    this->setModButtonOnGrid(1, 1, 1, initial && osu->getModSS(), &cv::mod_perfect, "ss", "SS or quit.",
                             []() -> SkinImage * { return osu->getSkin()->getSelectionModPerfect(); });

    if(cv::nightcore_enjoyer.getBool()) {
        this->modButtonHalftime = this->setModButtonOnGrid(
            2, 0, 0, initial && cv::mod_halftime_dummy.getBool(), &cv::mod_halftime_dummy, "dc", "A E S T H E T I C",
            []() -> SkinImage * { return osu->getSkin()->getSelectionModDayCore(); });
        this->modButtonDoubletime = this->setModButtonOnGrid(
            2, 1, 0, initial && cv::mod_doubletime_dummy.getBool(), &cv::mod_doubletime_dummy, "nc", "uguuuuuuuu",
            []() -> SkinImage * { return osu->getSkin()->getSelectionModNightCore(); });
    } else {
        this->modButtonHalftime = this->setModButtonOnGrid(
            2, 0, 0, initial && cv::mod_halftime_dummy.getBool(), &cv::mod_halftime_dummy, "ht", "Less zoom.",
            []() -> SkinImage * { return osu->getSkin()->getSelectionModHalfTime(); });
        this->modButtonDoubletime = this->setModButtonOnGrid(
            2, 1, 0, initial && cv::mod_doubletime_dummy.getBool(), &cv::mod_doubletime_dummy, "dt", "Zoooooooooom.",
            []() -> SkinImage * { return osu->getSkin()->getSelectionModDoubleTime(); });
    }

    this->modButtonHidden =
        this->setModButtonOnGrid(3, 1, 0, initial && osu->getModHD(), &cv::mod_hidden, "hd",
                                 "Play with no approach circles and fading notes for a slight score advantage.",
                                 []() -> SkinImage * { return osu->getSkin()->getSelectionModHidden(); });

    this->modButtonFlashlight = this->setModButtonOnGrid(
        4, 1, 0, initial && osu->getModFlashlight(), &cv::mod_flashlight, "fl", "Restricted view area.",
        []() -> SkinImage * { return osu->getSkin()->getSelectionModFlashlight(); });
    this->setModButtonOnGrid(4, 1, 1, initial && cv::mod_actual_flashlight.getBool(), &cv::mod_actual_flashlight, "afl",
                             "Actual flashlight.",
                             []() -> SkinImage * { return osu->getSkin()->getSelectionModFlashlight(); });

    this->modButtonTD = this->setModButtonOnGrid(5, 1, 0, initial && osu->getModTD(), &cv::mod_touchdevice, "nerftd",
                                                 "Simulate pp nerf for touch devices.\nOnly affects pp calculation.",
                                                 []() -> SkinImage * { return osu->getSkin()->getSelectionModTD(); });
    this->getModButtonOnGrid(5, 1)->setAvailable(!cv::mod_touchdevice_always.getBool());

    this->modButtonRelax = this->setModButtonOnGrid(
        0, 2, 0, initial && osu->getModRelax(), &cv::mod_relax, "relax",
        "You don't need to click.\nGive your clicking/tapping fingers a break from the heat of things.\n** UNRANKED **",
        []() -> SkinImage * { return osu->getSkin()->getSelectionModRelax(); });
    this->modButtonAutopilot =
        this->setModButtonOnGrid(1, 2, 0, initial && osu->getModAutopilot(), &cv::mod_autopilot, "autopilot",
                                 "Automatic cursor movement - just follow the rhythm.\n** UNRANKED **",
                                 []() -> SkinImage * { return osu->getSkin()->getSelectionModAutopilot(); });
    this->modButtonSpunout =
        this->setModButtonOnGrid(2, 2, 0, initial && osu->getModSpunout(), &cv::mod_spunout, "spunout",
                                 "Spinners will be automatically completed.",
                                 []() -> SkinImage * { return osu->getSkin()->getSelectionModSpunOut(); });
    this->modButtonAuto =
        this->setModButtonOnGrid(3, 2, 0, initial && osu->getModAuto(), &cv::mod_autoplay, "auto",
                                 "Watch a perfect automated play through the song.",
                                 []() -> SkinImage * { return osu->getSkin()->getSelectionModAutoplay(); });
    this->setModButtonOnGrid(
        4, 2, 0, initial && osu->getModTarget(), &cv::mod_target, "practicetarget",
        "Accuracy is based on the distance to the center of all hitobjects.\n300s still require at "
        "least being in the hit window of a 100 in addition to the rule above.",
        []() -> SkinImage * { return osu->getSkin()->getSelectionModTarget(); });
    this->modButtonScoreV2 =
        this->setModButtonOnGrid(5, 2, 0, initial && osu->getModScorev2(), &cv::mod_scorev2, "v2",
                                 "Try the future scoring system.\n** UNRANKED **",
                                 []() -> SkinImage * { return osu->getSkin()->getSelectionModScorev2(); });

    // Enable all mods that we disable conditionally below
    this->getModButtonOnGrid(2, 0)->setAvailable(true);
    this->getModButtonOnGrid(2, 1)->setAvailable(true);
    this->getModButtonOnGrid(3, 2)->setAvailable(true);
    this->getModButtonOnGrid(4, 2)->setAvailable(true);
    this->getModButtonOnGrid(4, 0)->setAvailable(true);
    this->getModButtonOnGrid(5, 2)->setAvailable(true);

    if(BanchoState::is_in_a_multi_room()) {
        if(BanchoState::room.freemods && !BanchoState::room.is_host()) {
            this->getModButtonOnGrid(2, 0)->setAvailable(false);  // Disable DC/HT
            this->getModButtonOnGrid(2, 1)->setAvailable(false);  // Disable DT/NC
            this->getModButtonOnGrid(4, 2)->setAvailable(false);  // Disable Target
        }

        this->getModButtonOnGrid(5, 2)->setAvailable(false);  // Disable ScoreV2 (we use win condition instead)
        this->getModButtonOnGrid(4, 0)->setAvailable(false);  // Disable nightmare mod
        this->getModButtonOnGrid(3, 2)->setAvailable(false);  // Disable auto mod
    }
}

void ModSelector::updateScoreMultiplierLabelText() {
    const float scoreMultiplier = osu->getScoreMultiplier();

    const int alpha = 200;
    if(scoreMultiplier > 1.0f)
        this->scoreMultiplierLabel->setTextColor(argb(alpha, 173, 255, 47));
    else if(scoreMultiplier == 1.0f)
        this->scoreMultiplierLabel->setTextColor(argb(alpha, 255, 255, 255));
    else
        this->scoreMultiplierLabel->setTextColor(argb(alpha, 255, 69, 00));

    this->scoreMultiplierLabel->setText(UString::format("Score Multiplier: %.2fX", scoreMultiplier));
}

void ModSelector::updateExperimentalButtons() {
    for(auto &experimentalMod : this->experimentalMods) {
        ConVar *cvar = experimentalMod.cvar;
        if(cvar != nullptr) {
            auto *checkboxPointer = dynamic_cast<CBaseUICheckbox *>(experimentalMod.element);
            if(checkboxPointer != nullptr) {
                if(cvar->getBool() != checkboxPointer->isChecked()) checkboxPointer->setChecked(cvar->getBool(), false);
            }
        }
    }
}

ModSelector::~ModSelector() {
    SAFE_DELETE(this->overrideSliderContainer);
    SAFE_DELETE(this->experimentalContainer);
}

void ModSelector::draw() {
    if(!this->bVisible && !this->bScheduledHide) return;

    // for compact mode (and experimental mods)
    const int margin = 10;
    const Color backgroundColor = 0x88000000;

    const float experimentalModsAnimationTranslation =
        -(this->experimentalContainer->getSize().x + 2.0f) * (1.0f - this->fExperimentalAnimation);

    if(BanchoState::is_in_a_multi_room()) {
        // get mod button element bounds
        vec2 modGridButtonsStart = vec2(osu->getScreenWidth(), osu->getScreenHeight());
        vec2 modGridButtonsSize = vec2(0, osu->getScreenHeight());
        for(auto button : this->modButtons) {
            if(button->getPos().x < modGridButtonsStart.x) modGridButtonsStart.x = button->getPos().x;
            if(button->getPos().y < modGridButtonsStart.y) modGridButtonsStart.y = button->getPos().y;

            if(button->getPos().x + button->getSize().x > modGridButtonsSize.x)
                modGridButtonsSize.x = button->getPos().x + button->getSize().x;
            if(button->getPos().y < modGridButtonsSize.y) modGridButtonsSize.y = button->getPos().y;
        }
        modGridButtonsSize.x -= modGridButtonsStart.x;

        // draw mod grid buttons
        g->pushTransform();
        {
            g->translate(0, (1.0f - this->fAnimation) * modGridButtonsSize.y);
            g->setColor(backgroundColor);
            g->fillRect(modGridButtonsStart.x - margin, modGridButtonsStart.y - margin,
                        modGridButtonsSize.x + 2 * margin, modGridButtonsSize.y + 2 * margin);
            OsuScreen::draw();
        }
        g->popTransform();
    } else if(this->isInCompactMode()) {
        // if we are in compact mode, draw some backgrounds under the override sliders & mod grid buttons

        // get override slider element bounds
        vec2 overrideSlidersStart = vec2(osu->getScreenWidth(), 0);
        vec2 overrideSlidersSize{0.f};
        for(auto &overrideSlider : this->overrideSliders) {
            CBaseUIButton *desc = overrideSlider.desc;
            CBaseUILabel *label = overrideSlider.label;

            if(desc->getPos().x < overrideSlidersStart.x) overrideSlidersStart.x = desc->getPos().x;

            if((label->getPos().x + label->getSize().x) > overrideSlidersSize.x)
                overrideSlidersSize.x = (label->getPos().x + label->getSize().x);
            if(desc->getPos().y + desc->getSize().y > overrideSlidersSize.y)
                overrideSlidersSize.y = desc->getPos().y + desc->getSize().y;
        }
        overrideSlidersSize.x -= overrideSlidersStart.x;

        // get mod button element bounds
        vec2 modGridButtonsStart = vec2(osu->getScreenWidth(), osu->getScreenHeight());
        vec2 modGridButtonsSize = vec2(0, osu->getScreenHeight());
        for(auto button : this->modButtons) {
            if(button->getPos().x < modGridButtonsStart.x) modGridButtonsStart.x = button->getPos().x;
            if(button->getPos().y < modGridButtonsStart.y) modGridButtonsStart.y = button->getPos().y;

            if(button->getPos().x + button->getSize().x > modGridButtonsSize.x)
                modGridButtonsSize.x = button->getPos().x + button->getSize().x;
            if(button->getPos().y < modGridButtonsSize.y) modGridButtonsSize.y = button->getPos().y;
        }
        modGridButtonsSize.x -= modGridButtonsStart.x;

        // draw mod grid buttons
        g->pushTransform();
        {
            g->translate(0, (1.0f - this->fAnimation) * modGridButtonsSize.y);
            g->setColor(backgroundColor);
            g->fillRect(modGridButtonsStart.x - margin, modGridButtonsStart.y - margin,
                        modGridButtonsSize.x + 2 * margin, modGridButtonsSize.y + 2 * margin);
            OsuScreen::draw();
        }
        g->popTransform();

        // draw override sliders
        g->pushTransform();
        {
            g->translate(0, -(1.0f - this->fAnimation) * overrideSlidersSize.y);
            g->setColor(backgroundColor);
            g->fillRect(overrideSlidersStart.x - margin, 0, overrideSlidersSize.x + 2 * margin,
                        overrideSlidersSize.y + margin);
            this->overrideSliderContainer->draw();
        }
        g->popTransform();
    } else  // normal mode, just draw everything
    {
        // draw hint text on left edge of screen
        {
            const float dpiScale = Osu::getUIScale();

            UString experimentalText = "Experimental Mods";
            McFont *experimentalFont = osu->getSubTitleFont();

            const float experimentalTextWidth = experimentalFont->getStringWidth(experimentalText);
            const float experimentalTextHeight = experimentalFont->getHeight();

            const float rectMargin = 5 * dpiScale;
            const float rectWidth = experimentalTextWidth + 2 * rectMargin;
            const float rectHeight = experimentalTextHeight + 2 * rectMargin;

            g->pushTransform();
            {
                g->rotate(90);
                g->translate(
                    (int)(experimentalTextHeight / 3.0f + std::max(0.0f, experimentalModsAnimationTranslation +
                                                                             this->experimentalContainer->getSize().x)),
                    (int)(osu->getScreenHeight() / 2 - experimentalTextWidth / 2));
                g->setColor(Color(0xff777777).setA(1.0f - this->fExperimentalAnimation * this->fExperimentalAnimation));

                g->drawString(experimentalFont, experimentalText);
            }
            g->popTransform();

            g->pushTransform();
            {
                g->rotate(90);
                g->translate((int)(rectHeight + std::max(0.0f, experimentalModsAnimationTranslation +
                                                                   this->experimentalContainer->getSize().x)),
                             (int)(osu->getScreenHeight() / 2 - rectWidth / 2));
                g->drawRect(0, 0, rectWidth, rectHeight);
            }
            g->popTransform();
        }

        OsuScreen::draw();
        this->overrideSliderContainer->draw();
    }

    // draw experimental mods
    if(!BanchoState::is_in_a_multi_room()) {
        g->pushTransform();
        {
            g->translate(experimentalModsAnimationTranslation, 0);
            g->setColor(backgroundColor);
            g->fillRect(this->experimentalContainer->getPos().x - margin,
                        this->experimentalContainer->getPos().y - margin,
                        this->experimentalContainer->getSize().x + 2 * margin * this->fExperimentalAnimation,
                        this->experimentalContainer->getSize().y + 2 * margin);
            this->experimentalContainer->draw();
        }
        g->popTransform();
    }
}

void ModSelector::mouse_update(bool *propagate_clicks) {
    // HACKHACK: updating while invisible is stupid, but the only quick solution for still animating otherwise stuck
    // sliders while closed
    if(!this->bVisible) {
        for(auto &overrideSlider : this->overrideSliders) {
            if(overrideSlider.slider->hasChanged()) overrideSlider.slider->mouse_update(propagate_clicks);
        }
        if(this->bScheduledHide) {
            if(this->fAnimation == 0.0f) {
                this->bScheduledHide = false;
            }
        }
        return;
    }

    // update experimental mods, they take focus precedence over everything else
    if(this->bExperimentalVisible) {
        this->experimentalContainer->mouse_update(propagate_clicks);
    }

    // update
    OsuScreen::mouse_update(propagate_clicks);

    this->nonSubmittableWarning->setVisible(BanchoState::can_submit_scores() && !cvars->areAllCvarsSubmittable());
    if(this->nonSubmittableWarning->isVisible() && this->nonSubmittableWarning->isMouseInside()) {
        osu->getTooltipOverlay()->begin();
        for(const auto& cvar : cvars->getNonSubmittableCvars()) {
            osu->getTooltipOverlay()->addLine(cvar->getName().c_str());
        }
        osu->getTooltipOverlay()->end();
    }

    if(!BanchoState::is_in_a_multi_room()) {
        this->overrideSliderContainer->mouse_update(propagate_clicks);

        // override slider tooltips (ALT)
        if(this->bShowOverrideSliderALTHint) {
            for(auto &overrideSlider : this->overrideSliders) {
                if(overrideSlider.slider->isBusy()) {
                    osu->getTooltipOverlay()->begin();
                    {
                        osu->getTooltipOverlay()->addLine("Hold [ALT] to slide in 0.01 increments.");
                    }
                    osu->getTooltipOverlay()->end();

                    if(keyboard->isAltDown()) this->bShowOverrideSliderALTHint = false;
                }
            }
        }

        // handle experimental mods visibility
        bool experimentalModEnabled = false;
        for(auto &experimentalMod : this->experimentalMods) {
            auto *checkboxPointer = dynamic_cast<CBaseUICheckbox *>(experimentalMod.element);
            if(checkboxPointer != nullptr && checkboxPointer->isChecked()) {
                experimentalModEnabled = true;
                break;
            }
        }

        McRect experimentalTrigger = McRect(
            0, 0, this->bExperimentalVisible ? this->experimentalContainer->getSize().x : osu->getScreenWidth() * 0.05f,
            osu->getScreenHeight());
        if(experimentalTrigger.contains(mouse->getPos())) {
            if(!this->bExperimentalVisible) {
                this->bExperimentalVisible = true;
                anim->moveQuadOut(&this->fExperimentalAnimation, 1.0f, (1.0f - this->fExperimentalAnimation) * 0.11f,
                                  0.0f, true);
            }
        } else if(this->bExperimentalVisible && !this->experimentalContainer->isMouseInside() &&
                  !this->experimentalContainer->isActive() && !experimentalModEnabled) {
            this->bExperimentalVisible = false;
            anim->moveQuadIn(&this->fExperimentalAnimation, 0.0f, this->fExperimentalAnimation * 0.11f, 0.0f, true);
        }
    }

    // delayed onModUpdate() triggers when changing some values
    {
        // handle dynamic CS and slider vertex buffer updates
        if(this->CSSlider != nullptr && (this->CSSlider->isActive() || this->CSSlider->hasChanged())) {
            this->bWaitForCSChangeFinished = true;
        } else if(this->bWaitForCSChangeFinished) {
            this->bWaitForCSChangeFinished = false;
            this->bWaitForSpeedChangeFinished = false;
            this->bWaitForHPChangeFinished = false;

            if(osu->isInPlayMode()) osu->getSelectedBeatmap()->onModUpdate();
        }

        // handle dynamic live pp calculation updates (when CS or Speed/BPM changes)
        if(this->speedSlider != nullptr && (this->speedSlider->isActive() || this->speedSlider->hasChanged())) {
            this->bWaitForSpeedChangeFinished = true;
        } else if(this->bWaitForSpeedChangeFinished) {
            this->bWaitForCSChangeFinished = false;
            this->bWaitForSpeedChangeFinished = false;
            this->bWaitForHPChangeFinished = false;

            if(osu->isInPlayMode()) osu->getSelectedBeatmap()->onModUpdate();
        }

        // handle dynamic HP drain updates
        if(this->HPSlider != nullptr && (this->HPSlider->isActive() || this->HPSlider->hasChanged())) {
            this->bWaitForHPChangeFinished = true;
        } else if(this->bWaitForHPChangeFinished) {
            this->bWaitForCSChangeFinished = false;
            this->bWaitForSpeedChangeFinished = false;
            this->bWaitForHPChangeFinished = false;
            if(osu->isInPlayMode()) osu->getSelectedBeatmap()->onModUpdate();
        }
    }
}

void ModSelector::onKeyDown(KeyboardEvent &key) {
    OsuScreen::onKeyDown(key);  // only used for options menu
    if(!this->bVisible || key.isConsumed()) return;

    this->overrideSliderContainer->onKeyDown(key);

    if(key == KEY_1) this->resetModsUserInitiated();

    if(((key == KEY_F1 || key == (KEYCODE)cv::TOGGLE_MODSELECT.getInt()) && !this->bWaitForF1KeyUp) || key == KEY_2 ||
       key == (KEYCODE)cv::GAME_PAUSE.getInt() || key == KEY_ESCAPE || key == KEY_ENTER || key == KEY_NUMPAD_ENTER)
        this->close();

    // mod hotkeys
    if(key == (KEYCODE)cv::MOD_EASY.getInt()) this->modButtonEasy->click();
    if(key == (KEYCODE)cv::MOD_NOFAIL.getInt()) this->modButtonNofail->click();
    if(key == (KEYCODE)cv::MOD_HARDROCK.getInt()) this->modButtonHardrock->click();
    if(key == (KEYCODE)cv::MOD_SUDDENDEATH.getInt()) this->modButtonSuddendeath->click();
    if(key == (KEYCODE)cv::MOD_HIDDEN.getInt()) this->modButtonHidden->click();
    if(key == (KEYCODE)cv::MOD_FLASHLIGHT.getInt()) this->modButtonFlashlight->click();
    if(key == (KEYCODE)cv::MOD_RELAX.getInt()) this->modButtonRelax->click();
    if(key == (KEYCODE)cv::MOD_AUTOPILOT.getInt()) this->modButtonAutopilot->click();
    if(key == (KEYCODE)cv::MOD_SPUNOUT.getInt()) this->modButtonSpunout->click();
    if(key == (KEYCODE)cv::MOD_AUTO.getInt()) this->modButtonAuto->click();
    if(key == (KEYCODE)cv::MOD_SCOREV2.getInt()) this->modButtonScoreV2->click();
    if(key == (KEYCODE)cv::MOD_HALFTIME.getInt()) this->modButtonHalftime->click();
    if(key == (KEYCODE)cv::MOD_DOUBLETIME.getInt()) this->modButtonDoubletime->click();

    key.consume();
}

void ModSelector::onKeyUp(KeyboardEvent &key) {
    if(!this->bVisible) return;

    if(key == KEY_F1 || key == (KEYCODE)cv::TOGGLE_MODSELECT.getInt()) this->bWaitForF1KeyUp = false;
}

CBaseUIContainer *ModSelector::setVisible(bool visible) {
    if(visible && !this->bVisible) {
        this->bScheduledHide = false;

        this->updateButtons(true);          // force state update without firing callbacks
        this->updateExperimentalButtons();  // force state update without firing callbacks
        this->updateLayout();
        this->updateScoreMultiplierLabelText();
        this->updateOverrideSliderLabels();

        this->fAnimation = 0.0f;
        anim->moveQuadOut(&this->fAnimation, 1.0f, 0.1f, 0.0f, true);

        bool experimentalModEnabled = false;
        for(auto &experimentalMod : this->experimentalMods) {
            auto *checkboxPointer = dynamic_cast<CBaseUICheckbox *>(experimentalMod.element);
            if(checkboxPointer != nullptr && checkboxPointer->isChecked()) {
                experimentalModEnabled = true;
                break;
            }
        }
        if(experimentalModEnabled) {
            this->bExperimentalVisible = true;
            if(this->isInCompactMode())
                anim->moveQuadOut(&this->fExperimentalAnimation, 1.0f, (1.0f - this->fExperimentalAnimation) * 0.06f,
                                  0.0f, true);
            else
                this->fExperimentalAnimation = 1.0f;
        } else {
            this->bExperimentalVisible = false;
            this->fExperimentalAnimation = 0.0f;
            anim->deleteExistingAnimation(&this->fExperimentalAnimation);
        }
    } else if(!visible && this->bVisible) {
        this->bScheduledHide = this->isInCompactMode();

        this->fAnimation = 1.0f;
        anim->moveQuadIn(&this->fAnimation, 0.0f, 0.06f, 0.0f, true);
        this->updateScoreMultiplierLabelText();
        this->updateOverrideSliderLabels();

        this->bExperimentalVisible = false;
        anim->moveQuadIn(&this->fExperimentalAnimation, 0.0f, 0.06f, 0.0f, true);
    }

    this->bVisible = visible;
    return this;
}

bool ModSelector::isInCompactMode() { return osu->isInPlayMode(); }

bool ModSelector::isCSOverrideSliderActive() { return this->CSSlider->isActive(); }

bool ModSelector::isMouseInScrollView() { return this->experimentalContainer->isMouseInside() && this->isVisible(); }

bool ModSelector::isMouseInside() {
    bool isMouseInsideAnyModSelectorModButton = false;
    for(auto &modButton : this->modButtons) {
        if(modButton->isMouseInside()) {
            isMouseInsideAnyModSelectorModButton = true;
            break;
        }
    }

    bool isMouseInsideAnyOverrideSliders = false;
    for(auto &overrideSlider : this->overrideSliders) {
        if((overrideSlider.lock != nullptr && overrideSlider.lock->isMouseInside()) ||
           overrideSlider.desc->isMouseInside() || overrideSlider.slider->isMouseInside() ||
           overrideSlider.label->isMouseInside()) {
            isMouseInsideAnyOverrideSliders = true;
            break;
        }
    }

    return this->isVisible() && (this->experimentalContainer->isMouseInside() || isMouseInsideAnyModSelectorModButton ||
                                 isMouseInsideAnyOverrideSliders);
}

void ModSelector::updateLayout() {
    if(this->modButtons.size() < 1 || this->overrideSliders.size() < 1) return;

    const float dpiScale = Osu::getUIScale();
    const float uiScale = cv::ui_scale.getFloat();

    if(!this->isInCompactMode())  // normal layout
    {
        // mod grid buttons
        vec2 center = osu->getScreenSize() / 2.0f;
        vec2 size = osu->getSkin()->getSelectionModEasy()->getSizeBase() * uiScale;
        vec2 offset = vec2(size.x * 1.0f, size.y * 0.25f);
        vec2 start = vec2(center.x - (size.x * this->iGridWidth) / 2.0f - (offset.x * (this->iGridWidth - 1)) / 2.0f,
                          center.y - (size.y * this->iGridHeight) / 2.0f - (offset.y * (this->iGridHeight - 1)) / 2.0f);

        for(int x = 0; x < this->iGridWidth; x++) {
            for(int y = 0; y < this->iGridHeight; y++) {
                UIModSelectorModButton *button = this->getModButtonOnGrid(x, y);

                if(button != nullptr) {
                    button->setPos(start + vec2(size.x * x + offset.x * x, size.y * y + offset.y * y));
                    button->setBaseScale(1.0f * uiScale, 1.0f * uiScale);
                    button->setSize(size);
                }
            }
        }

        // override sliders (down here because they depend on the mod grid button alignment)
        const int margin = 10 * dpiScale;
        const int overrideSliderWidth = osu->getUIScale(250.0f);
        const int overrideSliderHeight = 25 * dpiScale;
        const int overrideSliderOffsetY =
            ((start.y - this->overrideSliders.size() * overrideSliderHeight) / (this->overrideSliders.size() - 1)) *
            0.35f;
        const vec2 overrideSliderStart =
            vec2(osu->getScreenWidth() / 2 - overrideSliderWidth / 2,
                 start.y / 2 - (this->overrideSliders.size() * overrideSliderHeight +
                                (this->overrideSliders.size() - 1) * overrideSliderOffsetY) /
                                   1.75f);
        for(int i = 0; i < this->overrideSliders.size(); i++) {
            this->overrideSliders[i].desc->setSizeToContent(5 * dpiScale, 0);
            this->overrideSliders[i].desc->setSizeY(overrideSliderHeight);

            this->overrideSliders[i].slider->setBlockSize(20 * dpiScale, 20 * dpiScale);
            this->overrideSliders[i].slider->setPos(
                overrideSliderStart.x, overrideSliderStart.y + i * overrideSliderHeight + i * overrideSliderOffsetY);
            this->overrideSliders[i].slider->setSize(overrideSliderWidth, overrideSliderHeight);

            this->overrideSliders[i].desc->setPos(
                this->overrideSliders[i].slider->getPos().x - this->overrideSliders[i].desc->getSize().x - margin,
                this->overrideSliders[i].slider->getPos().y);

            if(this->overrideSliders[i].lock != nullptr && this->overrideSliders.size() > 1) {
                this->overrideSliders[i].lock->setPos(this->overrideSliders[1].desc->getPos().x -
                                                          this->overrideSliders[i].lock->getSize().x - margin -
                                                          3 * dpiScale,
                                                      this->overrideSliders[i].desc->getPos().y);
                this->overrideSliders[i].lock->setSizeY(overrideSliderHeight);
            }

            this->overrideSliders[i].label->setPos(
                this->overrideSliders[i].slider->getPos().x + this->overrideSliders[i].slider->getSize().x + margin,
                this->overrideSliders[i].slider->getPos().y);
            this->overrideSliders[i].label->setSizeToContent(0, 0);
            this->overrideSliders[i].label->setSizeY(overrideSliderHeight);
        }

        // action buttons
        float actionMinY = start.y + size.y * this->iGridHeight +
                           offset.y * (this->iGridHeight - 1);  // exact bottom of the mod buttons
        vec2 actionSize = vec2(osu->getUIScale(448.0f) * uiScale, size.y * 0.75f);
        float actionOffsetY = actionSize.y * 0.5f;
        vec2 actionStart = vec2(
            osu->getScreenWidth() / 2.0f - actionSize.x / 2.0f,
            actionMinY + (osu->getScreenHeight() - actionMinY) / 2.0f -
                (actionSize.y * this->actionButtons.size() + actionOffsetY * (this->actionButtons.size() - 1)) / 2.0f);
        for(int i = 0; i < this->actionButtons.size(); i++) {
            this->actionButtons[i]->setVisible(true);
            this->actionButtons[i]->setPos(actionStart.x, actionStart.y + actionSize.y * i + actionOffsetY * i);
            this->actionButtons[i]->onResized();  // HACKHACK: framework, setSize*() does not update string metrics
            this->actionButtons[i]->setSize(actionSize);
        }

        // score multiplier info label
        const float modGridMaxY = start.y + size.y * this->iGridHeight +
                                  offset.y * (this->iGridHeight - 1);  // exact bottom of the mod buttons

        this->nonSubmittableWarning->setSizeToContent();
        this->nonSubmittableWarning->setSize(vec2(osu->getScreenWidth(), 20 * uiScale));
        this->nonSubmittableWarning->setPos(
            0, modGridMaxY + std::abs(actionStart.y - modGridMaxY) / 2 - this->nonSubmittableWarning->getSize().y);

        this->scoreMultiplierLabel->setVisible(true);
        this->scoreMultiplierLabel->setSizeToContent();
        this->scoreMultiplierLabel->setSize(vec2(osu->getScreenWidth(), 30 * uiScale));
        this->scoreMultiplierLabel->setPos(0, this->nonSubmittableWarning->getPos().y + 20 * uiScale);
    } else  // compact in-beatmap mode
    {
        // mod grid buttons
        vec2 center = osu->getScreenSize() / 2.0f;
        vec2 blockSize = osu->getSkin()->getSelectionModEasy()->getSizeBase() * uiScale;
        vec2 offset = vec2(blockSize.x * 0.15f, blockSize.y * 0.05f);
        vec2 size = vec2((blockSize.x * this->iGridWidth) + (offset.x * (this->iGridWidth - 1)),
                         (blockSize.y * this->iGridHeight) + (offset.y * (this->iGridHeight - 1)));
        center.y = osu->getScreenHeight() - size.y / 2 - offset.y * 3.0f;
        vec2 start = vec2(center.x - size.x / 2.0f, center.y - size.y / 2.0f);

        for(int x = 0; x < this->iGridWidth; x++) {
            for(int y = 0; y < this->iGridHeight; y++) {
                UIModSelectorModButton *button = this->getModButtonOnGrid(x, y);

                if(button != nullptr) {
                    button->setPos(start + vec2(blockSize.x * x + offset.x * x, blockSize.y * y + offset.y * y));
                    button->setBaseScale(1 * uiScale, 1 * uiScale);
                    button->setSize(blockSize);
                }
            }
        }

        // override sliders (down here because they depend on the mod grid button alignment)
        const int margin = 10 * dpiScale;
        int overrideSliderWidth = osu->getUIScale(250.0f);
        int overrideSliderHeight = 25 * dpiScale;
        int overrideSliderOffsetY = 5 * dpiScale;
        vec2 overrideSliderStart = vec2(osu->getScreenWidth() / 2 - overrideSliderWidth / 2, 5 * dpiScale);
        for(int i = 0; i < this->overrideSliders.size(); i++) {
            this->overrideSliders[i].desc->setSizeToContent(5 * dpiScale, 0);
            this->overrideSliders[i].desc->setSizeY(overrideSliderHeight);

            this->overrideSliders[i].slider->setPos(
                overrideSliderStart.x, overrideSliderStart.y + i * overrideSliderHeight + i * overrideSliderOffsetY);
            this->overrideSliders[i].slider->setSizeX(overrideSliderWidth);

            this->overrideSliders[i].desc->setPos(
                this->overrideSliders[i].slider->getPos().x - this->overrideSliders[i].desc->getSize().x - margin,
                this->overrideSliders[i].slider->getPos().y);

            if(this->overrideSliders[i].lock != nullptr && this->overrideSliders.size() > 1) {
                this->overrideSliders[i].lock->setPos(this->overrideSliders[1].desc->getPos().x -
                                                          this->overrideSliders[i].lock->getSize().x - margin -
                                                          3 * dpiScale,
                                                      this->overrideSliders[i].desc->getPos().y);
                this->overrideSliders[i].lock->setSizeY(overrideSliderHeight);
            }

            this->overrideSliders[i].label->setPos(
                this->overrideSliders[i].slider->getPos().x + this->overrideSliders[i].slider->getSize().x + margin,
                this->overrideSliders[i].slider->getPos().y);
            this->overrideSliders[i].label->setSizeToContent(0, 0);
            this->overrideSliders[i].label->setSizeY(overrideSliderHeight);
        }

        // action buttons
        for(auto &actionButton : this->actionButtons) {
            actionButton->setVisible(false);
        }

        // score multiplier info label
        this->scoreMultiplierLabel->setVisible(false);
    }

    this->updateExperimentalLayout();
}

void ModSelector::updateExperimentalLayout() {
    const float dpiScale = Osu::getUIScale();

    // experimental mods
    int yCounter = 5 * dpiScale;
    int experimentalMaxWidth = 0;
    int experimentalOffsetY = 6 * dpiScale;
    for(int i = 0; i < this->experimentalMods.size(); i++) {
        CBaseUIElement *e = this->experimentalMods[i].element;
        e->setRelPosY(yCounter);
        e->setSizeY(e->getRelSize().y * dpiScale);

        // custom
        {
            auto *checkboxPointer = dynamic_cast<CBaseUICheckbox *>(e);
            if(checkboxPointer != nullptr) {
                checkboxPointer->onResized();
                checkboxPointer->setWidthToContent(0);
            }

            auto *labelPointer = dynamic_cast<CBaseUILabel *>(e);
            if(labelPointer != nullptr) {
                labelPointer->onResized();
                labelPointer->setWidthToContent(0);
            }
        }

        if(e->getSize().x > experimentalMaxWidth) experimentalMaxWidth = e->getSize().x;

        yCounter += e->getSize().y + experimentalOffsetY;

        if(i == 0) yCounter += 8 * dpiScale;
    }

    // laziness
    if(osu->getScreenHeight() > yCounter)
        yCounter = 5 * dpiScale + osu->getScreenHeight() / 2.0f - yCounter / 2.0f;
    else
        yCounter = 5 * dpiScale;

    for(int i = 0; i < this->experimentalMods.size(); i++) {
        CBaseUIElement *e = this->experimentalMods[i].element;
        e->setRelPosY(yCounter);

        if(e->getSize().x > experimentalMaxWidth) experimentalMaxWidth = e->getSize().x;

        yCounter += e->getSize().y + experimentalOffsetY;

        if(i == 0) yCounter += 8 * dpiScale;
    }

    this->experimentalContainer->setSizeX(experimentalMaxWidth + 25 * dpiScale /*, yCounter*/);
    this->experimentalContainer->setPosY(-1);
    this->experimentalContainer->setScrollSizeToContent(1 * dpiScale);
    this->experimentalContainer->getContainer()->update_pos();
    this->experimentalContainer->setVisible(!BanchoState::is_in_a_multi_room());
}

UIModSelectorModButton *ModSelector::setModButtonOnGrid(int x, int y, int state, bool initialState, ConVar *modCvar,
                                                        UString modName, const UString &tooltipText,
                                                        std::function<SkinImage *()> getImageFunc) {
    UIModSelectorModButton *modButton = this->getModButtonOnGrid(x, y);

    if(modButton != nullptr) {
        modButton->setState(state, initialState, modCvar, std::move(modName), tooltipText, std::move(getImageFunc));
        modButton->setVisible(true);
    }

    return modButton;
}

UIModSelectorModButton *ModSelector::getModButtonOnGrid(int x, int y) {
    const int index = x * this->iGridHeight + y;

    if(index < this->modButtons.size())
        return this->modButtons[index];
    else
        return nullptr;
}

ModSelector::OVERRIDE_SLIDER ModSelector::addOverrideSlider(UString text, const UString &labelText, ConVar *cvar,
                                                            float min, float max, UString tooltipText,
                                                            ConVar *lockCvar) {
    int height = 25;

    OVERRIDE_SLIDER os;
    if(lockCvar != nullptr) {
        os.lock = new ModSelectorOverrideSliderLockButton(0, 0, height, height, "", "");
        os.lock->setChangeCallback(SA::MakeDelegate<&ModSelector::onOverrideSliderLockChange>(this));
    }
    os.desc = new ModSelectorOverrideSliderDescButton(0, 0, 100, height, "", std::move(text));
    os.desc->setTooltipText(std::move(tooltipText));
    os.slider = new UISlider(0, 0, 100, height, "");
    os.label = new CBaseUILabel(0, 0, 100, height, labelText, labelText);
    os.cvar = cvar;
    os.lockCvar = lockCvar;

    bool debugDrawFrame = false;

    os.slider->setDrawFrame(debugDrawFrame);
    os.slider->setDrawBackground(false);
    os.slider->setFrameColor(0xff777777);
    // os.desc->setTextJustification(CBaseUILabel::TEXT_JUSTIFICATION::TEXT_JUSTIFICATION_CENTERED);
    os.desc->setEnabled(lockCvar != nullptr);
    os.desc->setDrawFrame(debugDrawFrame);
    os.desc->setDrawBackground(false);
    os.desc->setTextColor(0xff777777);
    os.label->setDrawFrame(debugDrawFrame);
    os.label->setDrawBackground(false);
    os.label->setTextColor(0xff777777);

    os.slider->setBounds(min, max + 1.0f);
    os.slider->setKeyDelta(0.1f);
    os.slider->setLiveUpdate(true);
    os.slider->setAllowMouseWheel(false);

    if(os.cvar != nullptr) os.slider->setValue(os.cvar->getFloat() + 1.0f, false);

    if(os.lock != nullptr) this->overrideSliderContainer->addBaseUIElement(os.lock);

    this->overrideSliderContainer->addBaseUIElement(os.desc);
    this->overrideSliderContainer->addBaseUIElement(os.slider);
    this->overrideSliderContainer->addBaseUIElement(os.label);
    this->overrideSliders.push_back(os);

    return os;
}

UIButton *ModSelector::addActionButton(const UString &text) {
    auto *actionButton = new UIButton(50, 50, 100, 100, text, text);
    this->actionButtons.push_back(actionButton);
    this->addBaseUIElement(actionButton);

    return actionButton;
}

CBaseUILabel *ModSelector::addExperimentalLabel(const UString &text) {
    auto *label = new CBaseUILabel(0, 0, 0, 25, text, text);
    label->setFont(osu->getSubTitleFont());
    label->setWidthToContent(0);
    label->setDrawBackground(false);
    label->setDrawFrame(false);
    this->experimentalContainer->getContainer()->addBaseUIElement(label);

    EXPERIMENTAL_MOD em;
    em.element = label;
    em.cvar = nullptr;
    this->experimentalMods.push_back(em);

    return label;
}

UICheckbox *ModSelector::addExperimentalCheckbox(const UString &text, const UString &tooltipText, ConVar *cvar) {
    auto *checkbox = new UICheckbox(0, 0, 0, 35, text, text);
    checkbox->setTooltipText(tooltipText);
    checkbox->setWidthToContent(0);
    if(cvar != nullptr) {
        checkbox->setChecked(cvar->getBool());
        checkbox->setChangeCallback(SA::MakeDelegate<&ModSelector::onCheckboxChange>(this));
    }
    this->experimentalContainer->getContainer()->addBaseUIElement(checkbox);

    EXPERIMENTAL_MOD em;
    em.element = checkbox;
    em.cvar = cvar;
    this->experimentalMods.push_back(em);

    return checkbox;
}

void ModSelector::resetModsUserInitiated() {
    this->resetMods();

    soundEngine->play(osu->getSkin()->getCheckOff());
    this->resetModsButton->animateClickColor();

    if(BanchoState::is_online()) {
        RichPresence::updateBanchoMods();
    }

    if(BanchoState::is_in_a_multi_room()) {
        for(auto &slot : BanchoState::room.slots) {
            if(slot.player_id != BanchoState::get_uid()) continue;

            if(BanchoState::room.is_host()) {
                BanchoState::room.mods = this->getModFlags();
            } else {
                this->enableModsFromFlags(BanchoState::room.mods);
            }

            slot.mods = BanchoState::room.mods;

            Packet packet;
            packet.id = MATCH_CHANGE_MODS;
            BANCHO::Proto::write<u32>(&packet, slot.mods);
            BANCHO::Net::send_packet(packet);

            osu->room->updateLayout(osu->getScreenSize());
            break;
        }
    }
}

void ModSelector::resetMods() {
    // cv::mod_fposu.setValue(false);  // intentionally commented out so people can play it in multi

    for(auto &overrideSlider : this->overrideSliders) {
        if(overrideSlider.lock != nullptr) overrideSlider.lock->setChecked(false);
    }

    for(auto &overrideSlider : this->overrideSliders) {
        // HACKHACK: force small delta to force an update (otherwise values could get stuck, e.g. for "Use Mods" context
        // menu) HACKHACK: only animate while visible to workaround "Use mods" bug (if custom speed multiplier already
        // set and then "Use mods" with different custom speed multiplier would reset to 1.0x because of anim)
        overrideSlider.slider->setValue(overrideSlider.slider->getMin() + 0.0001f, this->bVisible);
        overrideSlider.slider->setValue(overrideSlider.slider->getMin(), this->bVisible);
    }

    for(auto &modButton : this->modButtons) {
        modButton->resetState();
    }

    for(auto &experimentalMod : this->experimentalMods) {
        ConVar *cvar = experimentalMod.cvar;
        auto *checkboxPointer = dynamic_cast<CBaseUICheckbox *>(experimentalMod.element);
        if(checkboxPointer != nullptr) {
            // HACKHACK: we update both just in case because if the mod selector was not yet visible after a convar
            // change (e.g. because of "Use mods") then the checkbox has not yet updated its internal state
            checkboxPointer->setChecked(false);
            if(cvar != nullptr) cvar->setValue(0.0f);
        }
    }
}

u32 ModSelector::getModFlags() { return osu->getScore()->getModsLegacy(); }

void ModSelector::enableModsFromFlags(u32 flags) {
    using namespace ModMasks;
    if(legacy_eq(flags, LegacyFlags::DoubleTime) || legacy_eq(flags, LegacyFlags::Nightcore)) {
        cv::speed_override.setValue(1.5f);
    } else if(legacy_eq(flags, LegacyFlags::HalfTime)) {
        cv::speed_override.setValue(0.75f);
    } else {
        cv::speed_override.setValue(-1.f);
    }

    cv::mod_suddendeath.setValue(false);
    cv::mod_perfect.setValue(false);
    if(legacy_eq(flags, LegacyFlags::Perfect)) {
        this->modButtonSuddendeath->setState(1);
        this->modButtonSuddendeath->setOn(true, true);
    } else if(legacy_eq(flags, LegacyFlags::SuddenDeath)) {
        this->modButtonSuddendeath->setState(0);
        this->modButtonSuddendeath->setOn(true, true);
    }

    this->modButtonNofail->setOn(legacy_eq(flags, LegacyFlags::NoFail), true);
    this->modButtonEasy->setOn(legacy_eq(flags, LegacyFlags::Easy), true);
    this->modButtonTD->setOn(legacy_eq(flags, LegacyFlags::TouchDevice), true);
    this->modButtonHidden->setOn(legacy_eq(flags, LegacyFlags::Hidden), true);
    this->modButtonHardrock->setOn(legacy_eq(flags, LegacyFlags::HardRock), true);
    this->modButtonRelax->setOn(legacy_eq(flags, LegacyFlags::Relax), true);
    this->modButtonSpunout->setOn(legacy_eq(flags, LegacyFlags::SpunOut), true);
    this->modButtonAutopilot->setOn(legacy_eq(flags, LegacyFlags::Autopilot), true);
    this->getModButtonOnGrid(4, 2)->setOn(legacy_eq(flags, LegacyFlags::Target), true);
    this->modButtonFlashlight->setOn(legacy_eq(flags, LegacyFlags::Flashlight), true);
    this->modButtonScoreV2->setOn(legacy_eq(flags, LegacyFlags::ScoreV2), true);

    osu->updateMods();
}

void ModSelector::close() {
    this->closeButton->animateClickColor();
    osu->toggleModSelection();
}

void ModSelector::onOverrideSliderChange(CBaseUISlider *slider) {
    for(auto &overrideSlider : this->overrideSliders) {
        if(overrideSlider.slider != slider) continue;

        float sliderValue = slider->getFloat() - 1.0f;
        const float rawSliderValue = slider->getFloat();

        // alt key allows rounding to only 1 decimal digit
        if(!keyboard->isAltDown())
            sliderValue = std::round(sliderValue * 10.0f) / 10.0f;
        else
            sliderValue = std::round(sliderValue * 100.0f) / 100.0f;

        if(sliderValue < 0.0f) {
            sliderValue = -1.0f;
            overrideSlider.label->setWidthToContent(0);

            // HACKHACK: dirty
            if(osu->getSelectedBeatmap()->getSelectedDifficulty2() != nullptr) {
                if(overrideSlider.label->getName().find("BPM") != -1) {
                    // reset AR and OD override sliders if the bpm slider was reset
                    if(!this->ARLock->isChecked()) this->ARSlider->setValue(0.0f, false);
                    if(!this->ODLock->isChecked()) this->ODSlider->setValue(0.0f, false);
                }
            }

            // usability: auto disable lock if override slider is fully set to -1.0f (disabled)
            if(rawSliderValue == 0.0f) {
                if(overrideSlider.lock != nullptr && overrideSlider.lock->isChecked())
                    overrideSlider.lock->setChecked(false);
            }
        } else {
            // HACKHACK: dirty
            if(overrideSlider.label->getName().find("BPM") != -1) {
                // HACKHACK: force Speed/BPM slider to have a min value of 0.05 instead of 0 (because that's the
                // minimum for BASS) note that the BPM slider is just a 'fake' slider, it directly controls the
                // speed slider to do its thing (thus it needs the same limits)
                sliderValue = std::max(sliderValue, 0.05f);

                // speed slider may not be used in conjunction
                this->speedSlider->setValue(0.0f, false);

                // force early update
                overrideSlider.cvar->setValue(sliderValue);

                // AR/OD lock may not be used in conjunction with BPM
                this->ARLock->setChecked(false);
                this->ODLock->setChecked(false);

                // force change all other depending sliders
                if(osu->getSelectedBeatmap()->getSelectedDifficulty2() != nullptr) {
                    const float newAR = osu->getSelectedBeatmap()->getConstantApproachRateForSpeedMultiplier();
                    const float newOD = osu->getSelectedBeatmap()->getConstantOverallDifficultyForSpeedMultiplier();

                    // '+1' to compensate for turn-off area of the override sliders
                    this->ARSlider->setValue(newAR + 1.0f, false);
                    this->ODSlider->setValue(newOD + 1.0f, false);
                }
            }
        }

        // update convar with final value (e.g. cv::ar_override, etc.)
        overrideSlider.cvar->setValue(sliderValue);

        this->updateOverrideSliderLabels();

        break;
    }

    osu->updateMods();
}

void ModSelector::onOverrideSliderLockChange(CBaseUICheckbox *checkbox) {
    for(auto &overrideSlider : this->overrideSliders) {
        if(overrideSlider.lock == checkbox) {
            const bool locked = overrideSlider.lock->isChecked();
            const bool wasLocked = overrideSlider.lockCvar->getBool();

            // update convar with final value (e.g. osu_ar_override_lock, cv::od_override_lock)
            overrideSlider.lockCvar->setValue(locked ? 1.0f : 0.0f);

            // usability: if we just got locked, and the override slider value is < 0.0f (disabled), then set override
            // to current value
            if(locked && !wasLocked) {
                if(checkbox == this->ARLock) {
                    if(this->ARSlider->getFloat() < 1.0f)
                        this->ARSlider->setValue(
                            osu->getSelectedBeatmap()->getRawAR() + 1.0f,
                            false);  // '+1' to compensate for turn-off area of the override sliders
                } else if(checkbox == this->ODLock) {
                    if(this->ODSlider->getFloat() < 1.0f)
                        this->ODSlider->setValue(
                            osu->getSelectedBeatmap()->getRawOD() + 1.0f,
                            false);  // '+1' to compensate for turn-off area of the override sliders
                }
            }

            this->updateOverrideSliderLabels();

            break;
        }
    }

    osu->updateMods();
}

void ModSelector::onOverrideARSliderDescClicked(CBaseUIButton * /*button*/) { this->ARLock->click(); }

void ModSelector::onOverrideODSliderDescClicked(CBaseUIButton * /*button*/) { this->ODLock->click(); }

void ModSelector::updateOverrideSliderLabels() {
    const Color inactiveColor = 0xff777777;
    const Color activeColor = 0xffffffff;
    const Color inactiveLabelColor = 0xff1166ff;

    for(auto &overrideSlider : this->overrideSliders) {
        const float convarValue = overrideSlider.cvar->getFloat();
        const bool isLocked = (overrideSlider.lock != nullptr && overrideSlider.lock->isChecked());

        // update colors
        if(convarValue < 0.0f && !isLocked) {
            overrideSlider.label->setTextColor(inactiveLabelColor);
            overrideSlider.desc->setTextColor(inactiveColor);
            overrideSlider.slider->setFrameColor(inactiveColor);
        } else {
            overrideSlider.label->setTextColor(activeColor);
            overrideSlider.desc->setTextColor(activeColor);
            overrideSlider.slider->setFrameColor(activeColor);
        }

        overrideSlider.desc->setDrawFrame(isLocked);

        // update label text
        overrideSlider.label->setText(this->getOverrideSliderLabelText(overrideSlider, convarValue >= 0.0f));
        overrideSlider.label->setWidthToContent(0);

        // update lock checkbox
        if(overrideSlider.lock != nullptr && overrideSlider.lockCvar != nullptr &&
           overrideSlider.lock->isChecked() != overrideSlider.lockCvar->getBool())
            overrideSlider.lock->setChecked(overrideSlider.lockCvar->getBool());
    }
}

UString ModSelector::getOverrideSliderLabelText(ModSelector::OVERRIDE_SLIDER s, bool active) {
    float convarValue = s.cvar->getFloat();

    UString newLabelText = s.label->getName();
    if(osu->getSelectedBeatmap()->getSelectedDifficulty2() != nullptr) {
        // for relevant values (AR/OD), any non-1.0x speed multiplier should show the fractional parts caused by such a
        // speed multiplier (same for non-1.0x difficulty multiplier)
        const bool forceDisplayTwoDecimalDigits =
            (osu->getSelectedBeatmap()->getSpeedMultiplier() != 1.0f || osu->getDifficultyMultiplier() != 1.0f ||
             osu->getCSDifficultyMultiplier() != 1.0f);

        // HACKHACK: dirty
        bool wasSpeedSlider = false;
        float beatmapValue = 1.0f;
        if(s.label->getName().find("CS") != -1) {
            beatmapValue = std::clamp<float>(
                osu->getSelectedBeatmap()->getSelectedDifficulty2()->getCS() * osu->getCSDifficultyMultiplier(), 0.0f,
                10.0f);
            convarValue = osu->getSelectedBeatmap()->getCS();
        } else if(s.label->getName().find("AR") != -1) {
            beatmapValue = active ? osu->getSelectedBeatmap()->getRawApproachRateForSpeedMultiplier()
                                  : osu->getSelectedBeatmap()->getApproachRateForSpeedMultiplier();

            // compensate and round
            convarValue = osu->getSelectedBeatmap()->getApproachRateForSpeedMultiplier();
            if(!keyboard->isAltDown() && !forceDisplayTwoDecimalDigits)
                convarValue = std::round(convarValue * 10.0f) / 10.0f;
            else
                convarValue = std::round(convarValue * 100.0f) / 100.0f;
        } else if(s.label->getName().find("OD") != -1) {
            beatmapValue = active ? osu->getSelectedBeatmap()->getRawOverallDifficultyForSpeedMultiplier()
                                  : osu->getSelectedBeatmap()->getOverallDifficultyForSpeedMultiplier();

            // compensate and round
            convarValue = osu->getSelectedBeatmap()->getOverallDifficultyForSpeedMultiplier();
            if(!keyboard->isAltDown() && !forceDisplayTwoDecimalDigits)
                convarValue = std::round(convarValue * 10.0f) / 10.0f;
            else
                convarValue = std::round(convarValue * 100.0f) / 100.0f;
        } else if(s.label->getName().find("HP") != -1) {
            beatmapValue = std::clamp<float>(
                osu->getSelectedBeatmap()->getSelectedDifficulty2()->getHP() * osu->getDifficultyMultiplier(), 0.0f,
                10.0f);
            convarValue = osu->getSelectedBeatmap()->getHP();
        } else if(s.desc->getText().find("Speed") != -1) {
            beatmapValue = active ? 1.f : osu->getSelectedBeatmap()->getSpeedMultiplier();

            wasSpeedSlider = true;
            {
                {
                    if(!active)
                        newLabelText.append(UString::format(" %.4g", beatmapValue));
                    else
                        newLabelText.append(UString::format(" %.4g -> %.4g", beatmapValue, convarValue));
                }

                newLabelText.append("  (BPM: ");

                int minBPM = osu->getSelectedBeatmap()->getSelectedDifficulty2()->getMinBPM();
                int maxBPM = osu->getSelectedBeatmap()->getSelectedDifficulty2()->getMaxBPM();
                int mostCommonBPM = osu->getSelectedBeatmap()->getSelectedDifficulty2()->getMostCommonBPM();
                int newMinBPM = minBPM * osu->getSelectedBeatmap()->getSpeedMultiplier();
                int newMaxBPM = maxBPM * osu->getSelectedBeatmap()->getSpeedMultiplier();
                int newMostCommonBPM = mostCommonBPM * osu->getSelectedBeatmap()->getSpeedMultiplier();
                if(!active || osu->getSelectedBeatmap()->getSpeedMultiplier() == 1.0f) {
                    if(minBPM == maxBPM)
                        newLabelText.append(UString::format("%i", newMaxBPM));
                    else
                        newLabelText.append(UString::format("%i-%i (%i)", newMinBPM, newMaxBPM, newMostCommonBPM));
                } else {
                    if(minBPM == maxBPM)
                        newLabelText.append(UString::format("%i -> %i", maxBPM, newMaxBPM));
                    else
                        newLabelText.append(UString::format("%i-%i (%i) -> %i-%i (%i)", minBPM, maxBPM, mostCommonBPM,
                                                            newMinBPM, newMaxBPM, newMostCommonBPM));
                }

                newLabelText.append(")");
            }
        }

        // always round beatmapValue to 1 decimal digit, except for the speed slider, and except for non-1.0x speed
        // multipliers, and except for non-1.0x difficulty multipliers HACKHACK: dirty
        if(s.desc->getText().find("Speed") == -1) {
            if(forceDisplayTwoDecimalDigits)
                beatmapValue = std::round(beatmapValue * 100.0f) / 100.0f;
            else
                beatmapValue = std::round(beatmapValue * 10.0f) / 10.0f;
        }

        // update label
        if(!wasSpeedSlider) {
            if(!active)
                newLabelText.append(UString::format(" %.4g", beatmapValue));
            else
                newLabelText.append(UString::format(" %.4g -> %.4g", beatmapValue, convarValue));
        }
    }

    return newLabelText;
}

void ModSelector::enableAuto() {
    if(!this->modButtonAuto->isOn()) this->modButtonAuto->click();
}

void ModSelector::toggleAuto() { this->modButtonAuto->click(); }

void ModSelector::onCheckboxChange(CBaseUICheckbox *checkbox) {
    for(auto &experimentalMod : this->experimentalMods) {
        if(experimentalMod.element == checkbox) {
            if(experimentalMod.cvar != nullptr) experimentalMod.cvar->setValue(checkbox->isChecked());

            // force mod update
            if(osu->isInPlayMode()) osu->getSelectedBeatmap()->onModUpdate();

            break;
        }
    }

    osu->updateMods();
}

void ModSelector::onResolutionChange(vec2 newResolution) {
    this->setSize(newResolution);
    this->overrideSliderContainer->setSize(newResolution);
    this->experimentalContainer->setSizeY(newResolution.y + 1);

    this->updateLayout();
}
