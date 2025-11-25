// Copyright (c) 2016, PG, All rights reserved.
#include "OptionsMenu.h"

#include "AsyncIOHandler.h"
#include "ByteBufferedFile.h"
#include "DiscordInterface.h"
#include "Parsing.h"
#include "SString.h"
#include "crypto.h"
#include "AnimationHandler.h"
#include "Bancho.h"
#include "ConVar.h"
#include "ConVarHandler.h"
#include "CBaseUICheckbox.h"
#include "CBaseUIContainer.h"
#include "CBaseUILabel.h"
#include "CBaseUIScrollView.h"
#include "CBaseUITextbox.h"
#include "Chat.h"
#include "ConVar.h"
#include "Database.h"
#include "Engine.h"
#include "Environment.h"
#include "File.h"
#include "GameRules.h"
#include "HitObjects.h"
#include "HUD.h"
#include "Icons.h"
#include "KeyBindings.h"
#include "Keyboard.h"
#include "MainMenu.h"
#include "ModSelector.h"
#include "Mouse.h"
#include "NotificationOverlay.h"
#include "Osu.h"
#include "PeppyImporter.h"
#include "ResourceManager.h"
#include "Skin.h"
#include "SkinImage.h"
#include "SliderRenderer.h"
#include "SongBrowser/LoudnessCalcThread.h"
#include "SongBrowser/SongBrowser.h"
#include "TooltipOverlay.h"
#include "UIBackButton.h"
#include "UIButton.h"
#include "UICheckbox.h"
#include "UIContextMenu.h"
#include "UISearchOverlay.h"
#include "UISlider.h"
#include "Logging.h"

#include "SoundEngine.h"
#if defined(MCENGINE_PLATFORM_WINDOWS) && defined(MCENGINE_FEATURE_BASS)
#include "BassManager.h"
#include "BassSoundEngine.h"  // for ASIO-specific stuff
#endif

#include "fmt/chrono.h"

#include <cwctype>
#include <algorithm>
#include <fstream>
#include <utility>

class OptionsMenuSkinPreviewElement final : public CBaseUIElement {
   public:
    OptionsMenuSkinPreviewElement(float xPos, float yPos, float xSize, float ySize, UString name)
        : CBaseUIElement(xPos, yPos, xSize, ySize, std::move(name)) {
        this->iMode = 0;
    }

    void draw() override {
        if(!this->bVisible) return;

        const auto &skin = osu->getSkin();

        float hitcircleDiameter = this->vSize.y * 0.5f;
        float numberScale =
            (hitcircleDiameter / (160.0f * (skin->i_defaults[1].scale()))) * 1 * cv::number_scale_multiplier.getFloat();
        float overlapScale = (hitcircleDiameter / (160.0f)) * 1 * cv::number_scale_multiplier.getFloat();
        float scoreScale = 0.5f;

        if(this->iMode == 0) {
            float approachScale = std::clamp<float>(1.0f + 1.5f - fmod(engine->getTime() * 3, 3.0f), 0.0f, 2.5f);
            float approachAlpha = std::clamp<float>(fmod(engine->getTime() * 3, 3.0f) / 1.5f, 0.0f, 1.0f);
            approachAlpha = -approachAlpha * (approachAlpha - 2.0f);
            approachAlpha = -approachAlpha * (approachAlpha - 2.0f);
            float approachCircleAlpha = approachAlpha;
            approachAlpha = 1.0f;

            const int number = 1;
            const int colorCounter = 42;
            const int colorOffset = 0;
            const float colorRGBMultiplier = 1.0f;

            Circle::drawCircle(osu->getSkin(),
                               this->vPos + vec2(0, this->vSize.y / 2) + vec2(this->vSize.x * (1.0f / 5.0f), 0.0f),
                               hitcircleDiameter, numberScale, overlapScale, number, colorCounter, colorOffset,
                               colorRGBMultiplier, approachScale, approachAlpha, approachAlpha, true, false);
            Circle::drawHitResult(osu->getSkin(), hitcircleDiameter, hitcircleDiameter,
                                  this->vPos + vec2(0, this->vSize.y / 2) + vec2(this->vSize.x * (2.0f / 5.0f), 0.0f),
                                  LiveScore::HIT::HIT_100, 0.45f, 0.33f);
            Circle::drawHitResult(osu->getSkin(), hitcircleDiameter, hitcircleDiameter,
                                  this->vPos + vec2(0, this->vSize.y / 2) + vec2(this->vSize.x * (3.0f / 5.0f), 0.0f),
                                  LiveScore::HIT::HIT_50, 0.45f, 0.66f);
            Circle::drawHitResult(osu->getSkin(), hitcircleDiameter, hitcircleDiameter,
                                  this->vPos + vec2(0, this->vSize.y / 2) + vec2(this->vSize.x * (4.0f / 5.0f), 0.0f),
                                  LiveScore::HIT::HIT_MISS, 0.45f, 1.0f);
            Circle::drawApproachCircle(
                osu->getSkin(), this->vPos + vec2(0, this->vSize.y / 2) + vec2(this->vSize.x * (1.0f / 5.0f), 0.0f),
                osu->getSkin()->getComboColorForCounter(colorCounter, colorOffset), hitcircleDiameter, approachScale,
                approachCircleAlpha, false, false);
        } else if(this->iMode == 1) {
            const int numNumbers = 6;
            for(int i = 1; i < numNumbers + 1; i++) {
                Circle::drawHitCircleNumber(skin, numberScale, overlapScale,
                                            this->vPos + vec2(0, this->vSize.y / 2) +
                                                vec2(this->vSize.x * ((float)i / (numNumbers + 1.0f)), 0.0f),
                                            i - 1, 1.0f, 1.0f);
            }
        } else if(this->iMode == 2) {
            const int numNumbers = 6;
            for(int i = 1; i < numNumbers + 1; i++) {
                vec2 pos = this->vPos + vec2(0, this->vSize.y / 2) +
                           vec2(this->vSize.x * ((float)i / (numNumbers + 1.0f)), 0.0f);

                g->pushTransform();
                g->scale(scoreScale, scoreScale);
                g->translate(pos.x - skin->i_scores[0]->getWidth() * scoreScale, pos.y);
                osu->getHUD()->drawScoreNumber(i - 1, 1.0f);
                g->popTransform();
            }
        }
    }

    void onMouseUpInside(bool /*left*/, bool /*right*/) override {
        this->iMode++;
        this->iMode = this->iMode % 3;
    }

   private:
    int iMode;
};

class OptionsMenuSliderPreviewElement final : public CBaseUIElement {
   public:
    OptionsMenuSliderPreviewElement(float xPos, float yPos, float xSize, float ySize, UString name)
        : CBaseUIElement(xPos, yPos, xSize, ySize, std::move(name)) {}

    void draw() override {
        if(!this->bVisible) return;

        const float hitcircleDiameter = this->vSize.y * 0.5f;
        const float numberScale = (hitcircleDiameter / (160.0f * (osu->getSkin()->i_defaults[1].scale()))) * 1 *
                                  cv::number_scale_multiplier.getFloat();
        const float overlapScale = (hitcircleDiameter / (160.0f)) * 1 * cv::number_scale_multiplier.getFloat();

        const float approachScale = std::clamp<float>(1.0f + 1.5f - fmod(engine->getTime() * 3, 3.0f), 0.0f, 2.5f);
        float approachAlpha = std::clamp<float>(fmod(engine->getTime() * 3, 3.0f) / 1.5f, 0.0f, 1.0f);

        approachAlpha = -approachAlpha * (approachAlpha - 2.0f);
        approachAlpha = -approachAlpha * (approachAlpha - 2.0f);

        const float approachCircleAlpha = approachAlpha;
        approachAlpha = 1.0f;

        const float length = (this->vSize.x - hitcircleDiameter);
        const int numPoints = length;
        const float pointDist = length / numPoints;

        static std::vector<vec2> emptyVector;
        std::vector<vec2> points;

        const bool useLegacyRenderer =
            (cv::options_slider_preview_use_legacy_renderer.getBool() || cv::force_legacy_slider_renderer.getBool());

        for(int i = 0; i < numPoints; i++) {
            int heightAdd = i;
            if(i > numPoints / 2) heightAdd = numPoints - i;

            float heightAddPercent = (float)heightAdd / (float)(numPoints / 2.0f);
            float temp = 1.0f - heightAddPercent;
            temp *= temp;
            heightAddPercent = 1.0f - temp;

            points.emplace_back((useLegacyRenderer ? this->vPos.x : 0) + hitcircleDiameter / 2 + i * pointDist,
                                (useLegacyRenderer ? this->vPos.y : 0) + this->vSize.y / 2 - hitcircleDiameter / 3 +
                                    heightAddPercent * (this->vSize.y / 2 - hitcircleDiameter / 2));
        }

        if(points.size() > 0) {
            // draw regular circle with animated approach circle beneath slider
            {
                const int number = 2;
                const int colorCounter = 420;
                const int colorOffset = 0;
                const float colorRGBMultiplier = 1.0f;

                Circle::drawCircle(osu->getSkin(),
                                   points[numPoints / 2] + (!useLegacyRenderer ? this->vPos : vec2(0, 0)),
                                   hitcircleDiameter, numberScale, overlapScale, number, colorCounter, colorOffset,
                                   colorRGBMultiplier, approachScale, approachAlpha, approachAlpha, true, false);
                Circle::drawApproachCircle(osu->getSkin(),
                                           points[numPoints / 2] + (!useLegacyRenderer ? this->vPos : vec2(0, 0)),
                                           osu->getSkin()->getComboColorForCounter(420, 0), hitcircleDiameter,
                                           approachScale, approachCircleAlpha, false, false);
            }

            // draw slider body
            {
                // recursive shared usage of the same RenderTarget is invalid, therefore we block slider rendering while
                // the options menu is animating
                if(this->bDrawSliderHack) {
                    if(useLegacyRenderer)
                        SliderRenderer::draw(points, emptyVector, hitcircleDiameter, 0, 1,
                                             osu->getSkin()->getComboColorForCounter(420, 0));
                    else {
                        // (lazy generate vao)
                        if(!this->vao || length != this->fPrevLength) {
                            this->fPrevLength = length;

                            debugLog("Regenerating options menu slider preview vao ...");

                            this->vao = SliderRenderer::generateVAO(points, hitcircleDiameter, vec3(0, 0, 0), false);
                        }
                        SliderRenderer::draw(this->vao.get(), emptyVector, this->vPos, 1, hitcircleDiameter, 0, 1,
                                             osu->getSkin()->getComboColorForCounter(420, 0));
                    }
                }
            }

            // and slider head/tail circles
            {
                const int number = 1;
                const int colorCounter = 420;
                const int colorOffset = 0;
                const float colorRGBMultiplier = 1.0f;

                Circle::drawSliderStartCircle(
                    osu->getSkin(), points[0] + (!useLegacyRenderer ? this->vPos : vec2(0, 0)), hitcircleDiameter,
                    numberScale, overlapScale, number, colorCounter, colorOffset, colorRGBMultiplier);
                Circle::drawSliderEndCircle(osu->getSkin(),
                                            points.back() + (!useLegacyRenderer ? this->vPos : vec2(0, 0)),
                                            hitcircleDiameter, numberScale, overlapScale, number, colorCounter,
                                            colorOffset, colorRGBMultiplier, 1.0f, 1.0f, 0.0f, false, false);
            }
        }
    }

    void setDrawSliderHack(bool drawSliderHack) { this->bDrawSliderHack = drawSliderHack; }

   private:
    std::unique_ptr<VertexArrayObject> vao{nullptr};
    float fPrevLength{0.f};
    bool bDrawSliderHack{true};
};

class OptionsMenuKeyBindLabel final : public CBaseUILabel {
   public:
    OptionsMenuKeyBindLabel(float xPos, float yPos, float xSize, float ySize, UString name, const UString &text,
                            ConVar *cvar, CBaseUIButton *bindButton)
        : CBaseUILabel(xPos, yPos, xSize, ySize, std::move(name), text) {
        this->key = cvar;
        this->keyCode = -1;
        this->bindButton = bindButton;

        this->textColorBound = 0xffffd700;
        this->textColorUnbound = 0xffbb0000;
    }

    void mouse_update(bool *propagate_clicks) override {
        if(!this->bVisible) return;
        CBaseUILabel::mouse_update(propagate_clicks);

        const auto newKeyCode = (SCANCODE)this->key->getInt();
        if(this->keyCode == newKeyCode) return;

        this->keyCode = newKeyCode;

        // succ
        UString labelText = env->keyCodeToString(this->keyCode);
        if(labelText.find("?") != -1) labelText.append(UString::format("  (%i)", this->key->getInt()));

        // handle bound/unbound
        if(this->keyCode == 0) {
            labelText = "<UNBOUND>";
            this->setTextColor(this->textColorUnbound);
        } else
            this->setTextColor(this->textColorBound);

        // update text
        this->setText(labelText);
    }

    void setTextColorBound(Color textColorBound) { this->textColorBound = textColorBound; }
    void setTextColorUnbound(Color textColorUnbound) { this->textColorUnbound = textColorUnbound; }

   private:
    void onMouseUpInside(bool left, bool right) override {
        CBaseUILabel::onMouseUpInside(left, right);
        this->bindButton->click(left, right);
    }

    ConVar *key;
    CBaseUIButton *bindButton;

    Color textColorBound;
    Color textColorUnbound;
    SCANCODE keyCode;
};

class OptionsMenuKeyBindButton final : public UIButton {
   public:
    OptionsMenuKeyBindButton(float xPos, float yPos, float xSize, float ySize, UString name, UString text)
        : UIButton(xPos, yPos, xSize, ySize, std::move(name), std::move(text)) {
        this->bDisallowLeftMouseClickBinding = false;
    }

    void setDisallowLeftMouseClickBinding(bool disallowLeftMouseClickBinding) {
        this->bDisallowLeftMouseClickBinding = disallowLeftMouseClickBinding;
    }

    [[nodiscard]] inline bool isLeftMouseClickBindingAllowed() const { return !this->bDisallowLeftMouseClickBinding; }

   private:
    bool bDisallowLeftMouseClickBinding;
};

class OptionsMenuCategoryButton final : public CBaseUIButton {
   public:
    OptionsMenuCategoryButton(CBaseUIElement *section, float xPos, float yPos, float xSize, float ySize, UString name,
                              UString text)
        : CBaseUIButton(xPos, yPos, xSize, ySize, std::move(name), std::move(text)) {
        this->section = section;
        this->bActiveCategory = false;
        this->setTextJustification(TEXT_JUSTIFICATION::CENTERED);
        this->setDrawShadow(false);
    }

    void setActiveCategory(bool activeCategory) { this->bActiveCategory = activeCategory; }

    [[nodiscard]] inline CBaseUIElement *getSection() const { return this->section; }
    [[nodiscard]] inline bool isActiveCategory() const { return this->bActiveCategory; }

   private:
    CBaseUIElement *section;
    bool bActiveCategory;
};

class OptionsMenuResetButton final : public CBaseUIButton {
    NOCOPY_NOMOVE(OptionsMenuResetButton)
   public:
    OptionsMenuResetButton(float xPos, float yPos, float xSize, float ySize, UString name, UString text)
        : CBaseUIButton(xPos, yPos, xSize, ySize, std::move(name), std::move(text)) {
        this->fAnim = 1.0f;
    }

    ~OptionsMenuResetButton() override { anim->deleteExistingAnimation(&this->fAnim); }

    void draw() override {
        if(!this->bVisible || this->fAnim <= 0.0f) return;

        const int fullColorBlockSize = 4 * Osu::getUIScale();

        Color left = argb((int)(255 * this->fAnim), 255, 233, 50);
        Color middle = argb((int)(255 * this->fAnim), 255, 211, 50);
        Color right = 0x00000000;

        g->fillGradient(this->vPos.x, this->vPos.y, this->vSize.x * 1.25f, this->vSize.y, middle, right, middle, right);
        g->fillGradient(this->vPos.x, this->vPos.y, fullColorBlockSize, this->vSize.y, left, middle, left, middle);
    }

    void mouse_update(bool *propagate_clicks) override {
        if(!this->bVisible || !this->bEnabled) return;
        CBaseUIButton::mouse_update(propagate_clicks);

        if(this->isMouseInside()) {
            osu->getTooltipOverlay()->begin();
            {
                osu->getTooltipOverlay()->addLine("Reset");
            }
            osu->getTooltipOverlay()->end();
        }
    }

   private:
    void onEnabled() override {
        CBaseUIButton::onEnabled();
        anim->moveQuadOut(&this->fAnim, 1.0f, (1.0f - this->fAnim) * 0.15f, true);
    }

    void onDisabled() override {
        CBaseUIButton::onDisabled();
        anim->moveQuadOut(&this->fAnim, 0.0f, this->fAnim * 0.15f, true);
    }

    float fAnim;
};

OptionsMenu::OptionsMenu() : ScreenBackable() {
    this->bFullscreen = false;
    this->fAnimation = 0.0f;

    // convar callbacks
    cv::skin_use_skin_hitsounds.setCallback(SA::MakeDelegate<&OptionsMenu::onUseSkinsSoundSamplesChange>(this));

    cv::options_slider_quality.setCallback([](float newValue) -> void {
        // wrapper callback
        float value = std::lerp(1.0f, 2.5f, 1.0f - newValue);
        cv::slider_curve_points_separation.setValue(value);
    });

    cv::options_high_quality_sliders.setCallback(
        SA::MakeDelegate<&OptionsMenu::onHighQualitySlidersConVarChange>(this));

    osu->getNotificationOverlay()->addKeyListener(this);

    this->waitingKey = nullptr;
    this->resolutionLabel = nullptr;
    this->fullscreenCheckbox = nullptr;
    this->sliderQualitySlider = nullptr;
    this->outputDeviceLabel = nullptr;
    this->outputDeviceResetButton = nullptr;
    this->wasapiBufferSizeSlider = nullptr;
    this->wasapiPeriodSizeSlider = nullptr;
    this->asioBufferSizeResetButton = nullptr;
    this->wasapiBufferSizeResetButton = nullptr;
    this->wasapiPeriodSizeResetButton = nullptr;
    this->dpiTextbox = nullptr;
    this->cm360Textbox = nullptr;
    this->letterboxingOffsetResetButton = nullptr;
    this->uiScaleSlider = nullptr;
    this->uiScaleResetButton = nullptr;
    this->notelockSelectButton = nullptr;
    this->notelockSelectLabel = nullptr;
    this->notelockSelectResetButton = nullptr;
    this->hpDrainSelectButton = nullptr;
    this->hpDrainSelectLabel = nullptr;
    this->hpDrainSelectResetButton = nullptr;

    this->fOsuFolderTextboxInvalidAnim = 0.0f;
    this->fVibrationStrengthExampleTimer = 0.0f;
    this->bLetterboxingOffsetUpdateScheduled = false;
    this->bUIScaleChangeScheduled = false;
    this->bUIScaleScrollToSliderScheduled = false;
    this->bDPIScalingScrollToSliderScheduled = false;
    this->bASIOBufferChangeScheduled = false;
    this->bWASAPIBufferChangeScheduled = false;
    this->bWASAPIPeriodChangeScheduled = false;
    this->bLayoutUpdateScheduled.store(false, std::memory_order_release);

    this->iNumResetAllKeyBindingsPressed = 0;
    this->iNumResetEverythingPressed = 0;

    this->fSearchOnCharKeybindHackTime = 0.0f;

    this->notelockTypes.emplace_back("None");
    this->notelockTypes.emplace_back("McOsu");
    this->notelockTypes.emplace_back("osu!stable (default)");
    this->notelockTypes.emplace_back("osu!lazer 2020");

    this->setPos(-1, 0);

    this->options = new CBaseUIScrollView(0, -1, 0, 0, "");
    this->options->setDrawFrame(true);
    this->options->setDrawBackground(true);
    this->options->setBackgroundColor(0xdd000000);
    this->options->setHorizontalScrolling(false);
    this->addBaseUIElement(this->options);

    this->categories = new CBaseUIScrollView(0, -1, 0, 0, "");
    this->categories->setDrawFrame(true);
    this->categories->setDrawBackground(true);
    this->categories->setBackgroundColor(0xff000000);
    this->categories->setHorizontalScrolling(false);
    this->categories->setVerticalScrolling(true);
    this->categories->setScrollResistance(30);  // since all categories are always visible anyway
    this->addBaseUIElement(this->categories);

    this->contextMenu = new UIContextMenu(50, 50, 150, 0, "", this->options);
    this->contextMenu->setBackgroundColor(argb(200, 36, 36, 48));
    this->contextMenu->setFrameColor(argb(240, 240, 240, 255));

    this->search = new UISearchOverlay(0, 0, 0, 0, "");
    this->search->setOffsetRight(20);
    this->addBaseUIElement(this->search);

    this->spacer = new CBaseUILabel(0, 0, 1, 40, "", "");
    this->spacer->setDrawBackground(false);
    this->spacer->setDrawFrame(false);

    //**************************************************************************************************************************//

    this->sectionGeneral = this->addSection("General");

    this->addSubSection("Online server");

    // Only renders if server submission policy is unknown
    {
        this->addLabel("If the server admins don't explicitly allow neosu,")->setTextColor(0xff666666);
        this->elemContainers.back()->render_condition = RenderCondition::SCORE_SUBMISSION_POLICY;
        this->addLabel("you might get banned!")->setTextColor(0xff666666);
        this->elemContainers.back()->render_condition = RenderCondition::SCORE_SUBMISSION_POLICY;
        this->addLabel("");
        this->elemContainers.back()->render_condition = RenderCondition::SCORE_SUBMISSION_POLICY;
    }

    this->serverTextbox = this->addTextbox(cv::mp_server.getString().c_str(), "Server address:", &cv::mp_server);

    // Only renders if server submission policy is unknown
    {
        this->submitScoresCheckbox = this->addCheckbox("Submit scores", &cv::submit_scores);
        this->elemContainers.back()->render_condition = RenderCondition::SCORE_SUBMISSION_POLICY;
    }

    // Only renders if server isn't OAuth
    {
        this->addSubSection("Login details (username/password)");
        this->elemContainers.back()->render_condition = RenderCondition::PASSWORD_AUTH;
        this->nameTextbox = this->addTextbox(cv::name.getString().c_str(), &cv::name);
        this->elemContainers.back()->render_condition = RenderCondition::PASSWORD_AUTH;
        const auto &md5pass = cv::mp_password_md5.getString();
        this->passwordTextbox = this->addTextbox(md5pass.empty() ? "" : md5pass.c_str(), &cv::mp_password);
        this->passwordTextbox->is_password = true;
        this->elemContainers.back()->render_condition = RenderCondition::PASSWORD_AUTH;
    }

    {
        enum ELEMS : uint8_t { LOGINBTN = 0, KEEPSIGNEDCBX = 1 };
        OPTIONS_ELEMENT *loginElement = this->addButtonCheckbox("Log in", "Keep me logged in");

        this->logInButton = static_cast<UIButton *>(loginElement->baseElems[LOGINBTN]);

        this->logInButton->setHandleRightMouse(true);  // for canceling logins
        this->logInButton->setClickCallback(SA::MakeDelegate<&OptionsMenu::onLogInClicked>(this));
        this->logInButton->setColor(0xff00d900);
        this->logInButton->setTextColor(0xffffffff);

        auto *keepCbx = static_cast<UICheckbox *>(loginElement->baseElems[KEEPSIGNEDCBX]);

        keepCbx->setChecked(cv::mp_autologin.getBool());
        keepCbx->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onCheckboxChange>(this));
        loginElement->cvars[keepCbx] = &cv::mp_autologin;
    }

    this->addSubSection("osu!folder");
    this->addLabel("1) If you have an existing osu!stable installation:")->setTextColor(0xff666666);
    this->addLabel("2) osu! > Options > \"Open osu! folder\"")->setTextColor(0xff666666);
    this->addLabel("3) Copy paste the full path into the textbox:")->setTextColor(0xff666666);
    this->addLabel("");
    this->osuFolderTextbox = this->addTextbox(cv::osu_folder.getString().c_str(), &cv::osu_folder);
    UIButton *importSettingsButton = this->addButton("Import settings from osu!stable");
    importSettingsButton->setClickCallback(SA::MakeDelegate<&OptionsMenu::onImportSettingsFromStable>(this));
    this->addSpacer();
    this->addCheckbox(
        "Use osu!.db database (read-only)",
        "If you have an existing osu! installation,\nthen this will speed up the initial loading process.",
        &cv::database_enabled);
    this->addCheckbox(
        "Load osu! collection.db (read-only)",
        "If you have an existing osu! installation,\nalso load and display your created collections from there.",
        &cv::collections_legacy_enabled);

    this->addSpacer();
    this->addCheckbox(
        "Include Relax/Autopilot for total weighted pp/acc",
        "NOTE: osu! does not allow this (since these mods are unranked).\nShould relax/autopilot scores be "
        "included in the weighted pp/acc calculation?",
        &cv::user_include_relax_and_autopilot_for_stats);
    this->addCheckbox("Show pp instead of score in scorebrowser", "Only neosu scores will show pp.",
                      &cv::scores_sort_by_pp);
    this->addCheckbox("Always enable touch device pp nerf mod",
                      "Keep touch device pp nerf mod active even when resetting all mods.",
                      &cv::mod_touchdevice_always);

    this->addSubSection("Songbrowser");
    this->addCheckbox("Prefer metadata in original language", &cv::prefer_cjk);
    this->addCheckbox("Draw Strain Graph in Songbrowser",
                      "Hold either SHIFT/CTRL to show only speed/aim strains.\nSpeed strain is red, aim strain is "
                      "green.\n(See osu_hud_scrubbing_timeline_strains_*)",
                      &cv::draw_songbrowser_strain_graph);
    this->addCheckbox("Draw Strain Graph in Scrubbing Timeline",
                      "Speed strain is red, aim strain is green.\n(See osu_hud_scrubbing_timeline_strains_*)",
                      &cv::draw_scrubbing_timeline_strain_graph);

    this->addSubSection("Window");
    this->addCheckbox("Pause on Focus Loss", "Should the game pause when you switch to another application?",
                      &cv::pause_on_focus_loss);

    this->addSubSection("Alerts");
    this->addCheckbox("Notify when friends change status", &cv::notify_friend_status_change);
    this->addCheckbox("Notify when receiving a direct message", &cv::chat_notify_on_dm);
    this->addCheckbox("Notify when mentioned", &cv::chat_notify_on_mention);
    this->addCheckbox("Ping when mentioned", &cv::chat_ping_on_mention);
    this->addCheckbox("Show notifications during gameplay", &cv::notify_during_gameplay);

    this->addSubSection("In-game chat");
    this->addCheckbox("Chat ticker", &cv::chat_ticker);
    this->addCheckbox("Automatically hide chat during gameplay", &cv::chat_auto_hide);
    this->addTextbox(cv::chat_ignore_list.getString().c_str(),
                     "Chat word ignore list (space-separated):", &cv::chat_ignore_list);
    // this->addTextbox(cv::chat_highlight_words.getString().c_str(), "Chat word highlight list (space-separated):", &cv::chat_highlight_words);

    this->addSubSection("Privacy");
    this->addCheckbox("Automatically update neosu to the latest version", &cv::auto_update);
    // this->addCheckbox("Allow private messages from strangers", &cv::allow_stranger_dms);
    // this->addCheckbox("Allow game invites from strangers", &cv::allow_mp_invites);
    this->addCheckbox("Replace main menu logo with server logo", &cv::main_menu_use_server_logo);
    this->addCheckbox("Show spectator list", &cv::draw_spectator_list);
    this->addCheckbox("Share currently played map with spectators", &cv::spec_share_map);
    if constexpr(Env::cfg(FEAT::DISCORD)) {
        this->addCheckbox(
            "Enable Discord Rich Presence",
            "Shows your current game state in your friends' friendslists.\ne.g.: Playing Gavin G - Reach Out "
            "[Cherry Blossom's Insane]",
            &cv::rich_presence);
        this->addCheckbox("Draw map backgrounds in Discord Rich Presence", &cv::rich_presence_map_backgrounds);

        // there's an issue where if the game starts with discord closed, then the SDK fails to initialize, and there's
        // no way to try reinitializing it (without restarting the game)
        // so allow "turning it off and on again" to try reinitializing
        // (DiscRPC::init() does nothing if already initialized)
        cv::rich_presence.setCallback([](float newValue) -> void { return newValue > 0 ? DiscRPC::init() : (void)0; });

        // XXX: have a generic "update_activity"
        cv::rich_presence_map_backgrounds.setCallback([]() { DiscRPC::clear_activity(); });
    }

    //**************************************************************************************************************************//

    CBaseUIElement *sectionGraphics = this->addSection("Graphics");

    this->addSubSection("Renderer");
    this->addCheckbox("VSync", "If enabled: plz enjoy input lag.", &cv::vsync);

    this->addCheckbox("High Priority", "Sets the game process priority to high", &cv::win_processpriority);

    this->addCheckbox("Show FPS Counter", &cv::draw_fps);

    CBaseUISlider *prerenderedFramesSlider =
        addSlider("Max Queued Frames", 1.0f, 3.0f, &cv::r_sync_max_frames, -1.0f, true);
    prerenderedFramesSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangeInt>(this));
    prerenderedFramesSlider->setKeyDelta(1);
    addLabel("Raise for higher fps, decrease for lower latency")->setTextColor(0xff666666);

    this->addSpacer();

    CBaseUISlider *fpsSlider = this->addSlider("FPS Limiter:", 0.0f, 1000.0f, &cv::fps_max, -1.0f, true);
    fpsSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onFPSSliderChange>(this));
    fpsSlider->setKeyDelta(1);

    CBaseUISlider *fpsSlider2 = this->addSlider("FPS Limiter (menus):", 0.0f, 1000.0f, &cv::fps_max_menu, -1.0f, true);
    fpsSlider2->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onFPSSliderChange>(this));
    fpsSlider2->setKeyDelta(1);

    this->addSubSection("Layout");
    OPTIONS_ELEMENT *resolutionSelect = this->addButton(
        "Select Resolution", UString::format("%ix%i", osu->getVirtScreenWidth(), osu->getVirtScreenHeight()));
    this->resolutionSelectButton = (CBaseUIButton *)resolutionSelect->baseElems[0];
    this->resolutionSelectButton->setClickCallback(SA::MakeDelegate<&OptionsMenu::onResolutionSelect>(this));
    this->resolutionLabel = (CBaseUILabel *)resolutionSelect->baseElems[1];
    this->fullscreenCheckbox = this->addCheckbox("Fullscreen");
    this->fullscreenCheckbox->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onFullscreenChange>(this));
    this->addCheckbox("Keep Aspect Ratio",
                      "Black borders instead of a stretched image.\nOnly relevant if fullscreen is enabled, and "
                      "letterboxing is disabled.\nUse the two position sliders below to move the viewport around.",
                      &cv::resolution_keep_aspect_ratio);
    this->addCheckbox(
        "Letterboxing",
        "Useful to get the low latency of fullscreen with a smaller game resolution.\nUse the two position "
        "sliders below to move the viewport around.",
        &cv::letterboxing);
    this->letterboxingOffsetXSlider =
        this->addSlider("Horizontal position", -1.0f, 1.0f, &cv::letterboxing_offset_x, 170)
            ->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangeLetterboxingOffset>(this))
            ->setKeyDelta(0.01f)
            ->setAnimated(false);
    this->letterboxingOffsetYSlider =
        this->addSlider("Vertical position", -1.0f, 1.0f, &cv::letterboxing_offset_y, 170)
            ->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangeLetterboxingOffset>(this))
            ->setKeyDelta(0.01f)
            ->setAnimated(false);

    this->addSubSection("UI Scaling");
    this->addCheckbox(
            "DPI Scaling",
            UString::format("Automatically scale to the DPI of your display: %i DPI.\nScale factor = %i / 96 = %.2gx",
                            env->getDPI(), env->getDPI(), env->getDPIScale()),
            &cv::ui_scale_to_dpi)
        ->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onDPIScalingChange>(this));
    this->uiScaleSlider = this->addSlider("UI Scale:", 1.0f, 1.5f, &cv::ui_scale, 0.f, false, true);
    this->uiScaleSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangeUIScale>(this));
    this->uiScaleSlider->setKeyDelta(0.01f);
    this->uiScaleSlider->setAnimated(false);

    this->addSubSection("Detail Settings");
    this->addCheckbox("Animate scoreboard", "Use fancy animations for the in-game scoreboard",
                      &cv::scoreboard_animations);
    this->addCheckbox(
        "Avoid flashing elements",
        "Disables cosmetic flash effects\nDisables dimming when holding silders with Flashlight mod enabled",
        &cv::avoid_flashes);
    this->addCheckbox(
        "Mipmaps",
        "Reload your skin to apply! (CTRL + ALT + S)\nGenerate mipmaps for each skin element, at the cost of "
        "VRAM.\nProvides smoother visuals on lower resolutions for @2x-only skins.",
        &cv::skin_mipmaps);
    this->addSpacer();
    this->addCheckbox(
        "Snaking in sliders",
        "\"Growing\" sliders.\nSliders gradually snake out from their starting point while fading in.\nHas no "
        "impact on performance whatsoever.",
        &cv::snaking_sliders);
    this->addCheckbox("Snaking out sliders",
                      "\"Shrinking\" sliders.\nSliders will shrink with the sliderball while sliding.\nCan improve "
                      "performance a tiny bit, since there will be less to draw overall.",
                      &cv::slider_shrink);
    this->addSpacer();
    this->addCheckbox("Legacy Slider Renderer (!)",
                      "WARNING: Only try enabling this on shitty old computers!\nMay or may not improve fps while few "
                      "sliders are visible.\nGuaranteed lower fps while many sliders are visible!",
                      &cv::force_legacy_slider_renderer);
    this->addCheckbox("Higher Quality Sliders (!)", "Disable this if your fps drop too low while sliders are visible.",
                      &cv::options_high_quality_sliders)
        ->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onHighQualitySlidersCheckboxChange>(this));

    this->sliderQualitySlider = this->addSlider("Slider Quality", 0.0f, 1.0f, &cv::options_slider_quality);
    this->sliderQualitySlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangeSliderQuality>(this));

    //**************************************************************************************************************************//

    CBaseUIElement *sectionAudio = this->addSection("Audio");

    this->addSubSection("Devices");
    {
        OPTIONS_ELEMENT *outputDeviceSelect = this->addButton("Select Output Device", "Default", true);
        this->outputDeviceResetButton = outputDeviceSelect->resetButton;
        this->outputDeviceResetButton->setClickCallback(
            SA::MakeDelegate<&OptionsMenu::onOutputDeviceResetClicked>(this));
        this->outputDeviceSelectButton = (CBaseUIButton *)outputDeviceSelect->baseElems[0];
        this->outputDeviceSelectButton->setClickCallback(SA::MakeDelegate<&OptionsMenu::onOutputDeviceSelect>(this));

        this->outputDeviceLabel = (CBaseUILabel *)outputDeviceSelect->baseElems[1];

        {
            OPTIONS_ELEMENT *soloudBackendSelect = this->addButtonButtonLabel("MiniAudio", "SDL", "");
            const auto &MAButton = static_cast<UIButton *>(soloudBackendSelect->baseElems[0]);
            const auto &SDLButton = static_cast<UIButton *>(soloudBackendSelect->baseElems[1]);
            static auto MAButton_static = MAButton;
            static auto SDLButton_static = SDLButton;
            MAButton_static = MAButton;  // always update
            SDLButton_static = SDLButton;

            const auto &driverLabel = static_cast<CBaseUILabel *>(soloudBackendSelect->baseElems[2]);
            driverLabel->setVisible(false);

            // i would put the tooltip on the label, but CBaseUILabel doesn't support that
            const UString outputTooltip = "Pick the one you feel works best,\nthere should be very little difference.";
            MAButton->setTooltipText(outputTooltip);
            SDLButton->setTooltipText(outputTooltip);

            soloudBackendSelect->cvars[MAButton] = &cv::snd_soloud_backend;
            soloudBackendSelect->cvars[SDLButton] = &cv::snd_soloud_backend;
            soloudBackendSelect->render_condition = {[]() -> bool {
                bool ret =
                    soundEngine ? soundEngine->getOutputDriverType() >= SoundEngine::OutputDriver::SOLOUD_MA : false;
                return ret;
            }};

            static auto setActiveColors = []() -> void {
                const std::string &current = cv::snd_soloud_backend.getString();
                const bool MAactive = !SString::contains_ncase(current, "sdl");

                constexpr const auto inactiveGrey = rgba(150, 150, 150, 255);
                constexpr const auto activeGreen = rgba(50, 155, 50, 255);

                MAButton_static->setColor(MAactive ? activeGreen : inactiveGrey);
                SDLButton_static->setColor(MAactive ? inactiveGrey : activeGreen);
            };

            // set default active colors
            setActiveColors();

            MAButton->setClickCallback(
                [](CBaseUIButton *btn) -> void { cv::snd_soloud_backend.setValue(btn->getName()); });
            SDLButton->setClickCallback(
                [](CBaseUIButton *btn) -> void { cv::snd_soloud_backend.setValue(btn->getName()); });

            // need to use a change callback here because we already have a single-arg callback for the convar...
            cv::snd_soloud_backend.setCallback([](float /**/, float /**/) -> void { setActiveColors(); });

            this->addCheckbox("Auto-disable exclusive mode",
                              "Toggle exclusive mode off/on\nwhen losing/gaining focus, if already\nin exclusive mode.",
                              &cv::snd_disable_exclusive_unfocused);

            this->elemContainers.back()->render_condition = {[]() -> bool {
                if constexpr(!Env::cfg(OS::WINDOWS)) return false;
                bool ret =
                    soundEngine ? soundEngine->getOutputDriverType() == SoundEngine::OutputDriver::SOLOUD_MA : false;
                return ret;
            }};
        }

        // Dirty...
        auto wasapi_idx = this->elemContainers.size();
        {
            this->addSubSection("WASAPI");

            this->addCheckbox("Low-latency Callbacks",
                              "Run BASSWASAPI in callback mode to potentially further decrease "
                              "latency.\n(period/buffer size are ignored)",
                              &cv::win_snd_wasapi_event_callbacks);

            this->wasapiBufferSizeSlider =
                this->addSlider("Buffer Size:", 0.000f, 0.050f, &cv::win_snd_wasapi_buffer_size);
            this->wasapiBufferSizeSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onWASAPIBufferChange>(this));
            this->wasapiBufferSizeSlider->setKeyDelta(0.001f);
            this->wasapiBufferSizeSlider->setAnimated(false);
            this->addLabel("Windows 7: Start at 11 ms,")->setTextColor(0xff666666);
            this->addLabel("Windows 10: Start at 1 ms,")->setTextColor(0xff666666);
            this->addLabel("and if crackling: increment until fixed.")->setTextColor(0xff666666);
            this->addLabel("(lower is better, non-wasapi has ~40 ms minimum)")->setTextColor(0xff666666);
            this->addCheckbox(
                "Exclusive Mode",
                "Dramatically reduces latency, but prevents other applications from capturing/playing audio.",
                &cv::win_snd_wasapi_exclusive);
            this->addLabel("");
            this->addLabel("");
            this->addLabel("WARNING: Only if you know what you are doing")->setTextColor(0xffff0000);
            this->wasapiPeriodSizeSlider =
                this->addSlider("Period Size:", 0.0f, 0.050f, &cv::win_snd_wasapi_period_size);
            this->wasapiPeriodSizeSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onWASAPIPeriodChange>(this));
            this->wasapiPeriodSizeSlider->setKeyDelta(0.001f);
            this->wasapiPeriodSizeSlider->setAnimated(false);
            UIButton *restartSoundEngine = this->addButton("Restart SoundEngine");
            restartSoundEngine->setClickCallback(SA::MakeDelegate<&OptionsMenu::onOutputDeviceRestart>(this));
            restartSoundEngine->setColor(0xff003947);
            this->addLabel("");
        }
        auto wasapi_end_idx = this->elemContainers.size();
        for(auto i = wasapi_idx; i < wasapi_end_idx; i++) {
            this->elemContainers[i]->render_condition = RenderCondition::WASAPI_ENABLED;
        }

        // Dirty...
        auto asio_idx = this->elemContainers.size();
        {
            this->addSubSection("ASIO");
            this->asioBufferSizeSlider = this->addSlider("Buffer Size:", 0, 44100, &cv::asio_buffer_size);
            this->asioBufferSizeSlider->setKeyDelta(512);
            this->asioBufferSizeSlider->setAnimated(false);
            this->asioBufferSizeSlider->setLiveUpdate(false);
            this->asioBufferSizeSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onASIOBufferChange>(this));
            this->addLabel("");
            UIButton *asio_settings_btn = this->addButton("Open ASIO settings");
            asio_settings_btn->setClickCallback(SA::MakeDelegate<&OptionsMenu::OpenASIOSettings>(this));
            asio_settings_btn->setColor(0xff003947);
            UIButton *restartSoundEngine = this->addButton("Restart SoundEngine");
            restartSoundEngine->setClickCallback(SA::MakeDelegate<&OptionsMenu::onOutputDeviceRestart>(this));
            restartSoundEngine->setColor(0xff003947);
        }
        auto asio_end_idx = this->elemContainers.size();
        for(auto i = asio_idx; i < asio_end_idx; i++) {
            this->elemContainers[i]->render_condition = RenderCondition::ASIO_ENABLED;
        }
    }

    this->addSubSection("Volume");

    if(Env::cfg(AUD::BASS) && soundEngine->getTypeId() == SoundEngine::BASS) {
        this->addCheckbox("Normalize loudness across songs", &cv::normalize_loudness)
            ->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onLoudnessNormalizationToggle>(this));
    }

    CBaseUISlider *masterVolumeSlider = this->addSlider("Master:", 0.0f, 1.0f, &cv::volume_master, 70.0f);
    masterVolumeSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangePercent>(this));
    masterVolumeSlider->setKeyDelta(0.01f);
    CBaseUISlider *inactiveVolumeSlider = this->addSlider("Inactive:", 0.0f, 1.0f, &cv::volume_master_inactive, 70.0f);
    inactiveVolumeSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangePercent>(this));
    inactiveVolumeSlider->setKeyDelta(0.01f);
    CBaseUISlider *musicVolumeSlider = this->addSlider("Music:", 0.0f, 1.0f, &cv::volume_music, 70.0f);
    musicVolumeSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangePercent>(this));
    musicVolumeSlider->setKeyDelta(0.01f);
    CBaseUISlider *effectsVolumeSlider = this->addSlider("Effects:", 0.0f, 1.0f, &cv::volume_effects, 70.0f);
    effectsVolumeSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangePercent>(this));
    effectsVolumeSlider->setKeyDelta(0.01f);

    this->addSubSection("Offset Adjustment");
    CBaseUISlider *offsetSlider = this->addSlider("Universal Offset:", -300.0f, 300.0f, &cv::universal_offset);
    offsetSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangeIntMS>(this));
    offsetSlider->setKeyDelta(1);

    this->addSubSection("Songbrowser");
    this->addCheckbox("Apply speed/pitch mods while browsing",
                      "Whether to always apply all mods, or keep the preview music normal.",
                      &cv::beatmap_preview_mods_live);

    this->addSubSection("Gameplay");
    this->addCheckbox("Change hitsound pitch based on accuracy", &cv::snd_pitch_hitsounds);
    this->addCheckbox("Prefer Nightcore over Double Time", &cv::nightcore_enjoyer)
        ->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onModChangingToggle>(this));

    //**************************************************************************************************************************//

    this->skinSection = this->addSection("Skin");

    this->addSubSection("Skin");
    this->addSkinPreview();
    {
        OPTIONS_ELEMENT skinSelect;
        {
            skinSelect = *this->addButton("Select Skin", "default");
            this->skinSelectLocalButton = skinSelect.baseElems[0];
            this->skinLabel = (CBaseUILabel *)skinSelect.baseElems[1];
        }
        ((CBaseUIButton *)this->skinSelectLocalButton)
            ->setClickCallback(SA::MakeDelegate<&OptionsMenu::onSkinSelect>(this));

        this->addButton("Open current Skin folder")
            ->setClickCallback(SA::MakeDelegate<&OptionsMenu::openCurrentSkinFolder>(this));

        OPTIONS_ELEMENT skinReload = *this->addButtonButton("Reload Skin", "Random Skin");
        ((UIButton *)skinReload.baseElems[0])->setClickCallback(SA::MakeDelegate<&OptionsMenu::onSkinReload>(this));
        ((UIButton *)skinReload.baseElems[0])->setTooltipText("(CTRL + ALT + S)");
        ((UIButton *)skinReload.baseElems[1])->setClickCallback(SA::MakeDelegate<&OptionsMenu::onSkinRandom>(this));
        ((UIButton *)skinReload.baseElems[1])
            ->setTooltipText(
                "Temporary, does not change your configured skin (reload to reset).\nUse \"osu_skin_random 1\" to "
                "randomize on every skin reload.\nUse \"osu_skin_random_elements 1\" to mix multiple skins.\nUse "
                "\"osu_skin_export\" to export the currently active skin.");
        ((UIButton *)skinReload.baseElems[1])->setColor(0xff003947);
    }
    this->addSpacer();
    this->addCheckbox("Sort Skins Alphabetically",
                      "Less like stable, but useful if you don't\nlike obnoxious skin names floating to the top.",
                      &cv::sort_skins_cleaned);
    CBaseUISlider *numberScaleSlider =
        this->addSlider("Number Scale:", 0.01f, 3.0f, &cv::number_scale_multiplier, 135.0f);
    numberScaleSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangePercent>(this));
    numberScaleSlider->setKeyDelta(0.01f);
    CBaseUISlider *hitResultScaleSlider =
        this->addSlider("HitResult Scale:", 0.01f, 3.0f, &cv::hitresult_scale, 135.0f);
    hitResultScaleSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangePercent>(this));
    hitResultScaleSlider->setKeyDelta(0.01f);
    this->addCheckbox("Draw Numbers", &cv::draw_numbers);
    this->addCheckbox("Draw Approach Circles", &cv::draw_approach_circles);
    this->addCheckbox("Instafade Circles", &cv::instafade);
    this->addCheckbox("Instafade Sliders", &cv::instafade_sliders);
    this->addSpacer();
    this->addCheckbox(
        "Ignore Beatmap Sample Volume",
        "Ignore beatmap timingpoint effect volumes.\nQuiet hitsounds can destroy accuracy and concentration, "
        "enabling this will fix that.",
        &cv::ignore_beatmap_sample_volume);
    this->addCheckbox("Ignore Beatmap Combo Colors", &cv::ignore_beatmap_combo_colors);
    this->addCheckbox("Use skin's sound samples",
                      "If this is not selected, then the default skin hitsounds will be used.",
                      &cv::skin_use_skin_hitsounds);
    this->addCheckbox("Load HD @2x",
                      "On very low resolutions (below 1600x900) you can disable this to get smoother visuals.",
                      &cv::skin_hd);
    this->addSpacer();
    this->addCheckbox("Draw Cursor Trail", &cv::draw_cursor_trail);
    this->addCheckbox(
        "Force Smooth Cursor Trail",
        "Usually, the presence of the cursormiddle.png skin image enables smooth cursortrails.\nThis option "
        "allows you to force enable smooth cursortrails for all skins.",
        &cv::cursor_trail_smooth_force);
    this->addCheckbox("Always draw Cursor Trail", "Draw the cursor trail even when the cursor isn't moving",
                      &cv::always_render_cursor_trail);
    this->addSlider("Cursor trail spacing:", 0.f, 30.f, &cv::cursor_trail_spacing, -1.f, true)
        ->setAnimated(false)
        ->setKeyDelta(0.01f);
    this->cursorSizeSlider = this->addSlider("Cursor Size:", 0.01f, 5.0f, &cv::cursor_scale, -1.0f, true);
    this->cursorSizeSlider->setAnimated(false);
    this->cursorSizeSlider->setKeyDelta(0.01f);
    this->addCheckbox("Automatic Cursor Size", "Cursor size will adjust based on the CS of the current beatmap.",
                      &cv::automatic_cursor_size);
    this->addSpacer();
    this->sliderPreviewElement = (OptionsMenuSliderPreviewElement *)this->addSliderPreview();
    this->addSlider("Slider Border Size", 0.0f, 9.0f, &cv::slider_border_size_multiplier)->setKeyDelta(0.01f);
    this->addSlider("Slider Opacity", 0.0f, 1.0f, &cv::slider_alpha_multiplier, 200.0f);
    this->addSlider("Slider Body Opacity", 0.0f, 1.0f, &cv::slider_body_alpha_multiplier, 200.0f, true);
    this->addSlider("Slider Body Saturation", 0.0f, 1.0f, &cv::slider_body_color_saturation, 200.0f, true);
    this->addCheckbox(
        "Use slidergradient.png",
        "Enabling this will improve performance,\nbut also block all dynamic slider (color/border) features.",
        &cv::slider_use_gradient_image);
    this->addCheckbox("Use osu!lazer Slider Style",
                      "Only really looks good if your skin doesn't \"SliderTrackOverride\" too dark.",
                      &cv::slider_osu_next_style);
    this->addCheckbox("Use combo color as tint for slider ball", &cv::slider_ball_tint_combo_color);
    this->addCheckbox("Use combo color as tint for slider border", &cv::slider_border_tint_combo_color);
    this->addCheckbox("Draw Slider End Circle", &cv::slider_draw_endcircle);

    //**************************************************************************************************************************//

    CBaseUIElement *sectionInput = this->addSection("Input");

    this->addSubSection("Mouse", "scroll sensitivity");

    {
        UISlider *sensSlider = this->addSlider("Sensitivity:", 0.1f, 6.0f, &cv::mouse_sensitivity);
        sensSlider->setKeyDelta(0.01f);
        sensSlider->setUpdateRelPosOnChange(true);

        static const RenderCondition sensWarningCondition{
            []() -> bool { return cv::mouse_sensitivity.getFloat() != 1.f; }};
        this->addLabel("");
        this->elemContainers.back()->render_condition = sensWarningCondition;
        this->addLabel("WARNING: Set Sensitivity to 1 for tablets!")->setTextColor(0xffff0000);
        this->elemContainers.back()->render_condition = sensWarningCondition;
        this->addLabel("");
        this->elemContainers.back()->render_condition = sensWarningCondition;
    }

    this->addCheckbox("Raw Mouse Input", &cv::mouse_raw_input);
    this->addCheckbox("Confine Cursor (Windowed)", &cv::confine_cursor_windowed);
    this->addCheckbox("Confine Cursor (Fullscreen)", &cv::confine_cursor_fullscreen);
    this->addCheckbox("Confine Cursor (NEVER)", "Overrides automatic cursor clipping during gameplay.",
                      &cv::confine_cursor_never);
    this->addCheckbox("Disable Mouse Wheel in Play Mode", &cv::disable_mousewheel);
    this->addCheckbox("Disable Mouse Buttons in Play Mode", &cv::disable_mousebuttons);
    this->addCheckbox("Cursor ripples", "The cursor will ripple outwards on clicking.", &cv::draw_cursor_ripples);

    this->addSpacer();
    const UString keyboardSectionTags = "keyboard keys key bindings binds keybinds keybindings";
    CBaseUIElement *subSectionKeyboard = this->addSubSection("Keyboard", keyboardSectionTags);
    if constexpr(Env::cfg(OS::WINDOWS)) {
        this->addCheckbox("Raw Keyboard Input", &cv::keyboard_raw_input);
    }
    UIButton *resetAllKeyBindingsButton = this->addButton("Reset all key bindings");
    resetAllKeyBindingsButton->setColor(0xffd90000);
    resetAllKeyBindingsButton->setClickCallback(SA::MakeDelegate<&OptionsMenu::onKeyBindingsResetAllPressed>(this));
    this->addSubSection("Keys - In-Game", keyboardSectionTags);
    this->addKeyBindButton("Left Click", &cv::LEFT_CLICK);
    this->addKeyBindButton("Right Click", &cv::RIGHT_CLICK);
    this->addKeyBindButton("Left Click (2)", &cv::LEFT_CLICK_2);
    this->addKeyBindButton("Right Click (2)", &cv::RIGHT_CLICK_2);
    this->addKeyBindButton("Smoke", &cv::SMOKE);
    this->addKeyBindButton("Game Pause", &cv::GAME_PAUSE)->setDisallowLeftMouseClickBinding(true);
    this->addKeyBindButton("Skip Cutscene", &cv::SKIP_CUTSCENE);
    this->addKeyBindButton("Toggle Scoreboard", &cv::TOGGLE_SCOREBOARD);
    this->addKeyBindButton("Scrubbing (+ Click Drag!)", &cv::SEEK_TIME);
    this->addKeyBindButton("Quick Seek -5sec <<<", &cv::SEEK_TIME_BACKWARD);
    this->addKeyBindButton("Quick Seek +5sec >>>", &cv::SEEK_TIME_FORWARD);
    this->addKeyBindButton("Increase Local Song Offset", &cv::INCREASE_LOCAL_OFFSET);
    this->addKeyBindButton("Decrease Local Song Offset", &cv::DECREASE_LOCAL_OFFSET);
    this->addKeyBindButton("Quick Retry (hold briefly)", &cv::QUICK_RETRY);
    this->addKeyBindButton("Quick Save", &cv::QUICK_SAVE);
    this->addKeyBindButton("Quick Load", &cv::QUICK_LOAD);
    this->addKeyBindButton("Instant Replay", &cv::INSTANT_REPLAY);
    this->addSubSection("Keys - FPoSu", keyboardSectionTags);
    this->addKeyBindButton("Zoom", &cv::FPOSU_ZOOM);
    this->addSubSection("Keys - Universal", keyboardSectionTags);
    this->addKeyBindButton("Toggle chat", &cv::TOGGLE_CHAT);
    this->addKeyBindButton("Toggle user list", &cv::TOGGLE_EXTENDED_CHAT);
    this->addKeyBindButton("Save Screenshot", &cv::SAVE_SCREENSHOT);
    this->addKeyBindButton("Increase Volume", &cv::INCREASE_VOLUME);
    this->addKeyBindButton("Decrease Volume", &cv::DECREASE_VOLUME);
    this->addKeyBindButton("Disable Mouse Buttons", &cv::DISABLE_MOUSE_BUTTONS);
    this->addKeyBindButton("Toggle Map Background", &cv::TOGGLE_MAP_BACKGROUND);
    this->addKeyBindButton("Boss Key (Minimize)", &cv::BOSS_KEY);
    this->addKeyBindButton("Open Skin Selection Menu", &cv::OPEN_SKIN_SELECT_MENU);
    this->addSubSection("Keys - Song Select", keyboardSectionTags);
    this->addKeyBindButton("Toggle Mod Selection Screen", &cv::TOGGLE_MODSELECT)
        ->setTooltipText("(F1 can not be unbound. This is just an additional key.)");
    this->addKeyBindButton("Random Beatmap", &cv::RANDOM_BEATMAP)
        ->setTooltipText("(F2 can not be unbound. This is just an additional key.)");
    this->addSubSection("Keys - Mod Select", keyboardSectionTags);
    this->addKeyBindButton("Easy", &cv::MOD_EASY);
    this->addKeyBindButton("No Fail", &cv::MOD_NOFAIL);
    this->addKeyBindButton("Half Time", &cv::MOD_HALFTIME);
    this->addKeyBindButton("Hard Rock", &cv::MOD_HARDROCK);
    this->addKeyBindButton("Sudden Death", &cv::MOD_SUDDENDEATH);
    this->addKeyBindButton("Double Time", &cv::MOD_DOUBLETIME);
    this->addKeyBindButton("Hidden", &cv::MOD_HIDDEN);
    this->addKeyBindButton("Flashlight", &cv::MOD_FLASHLIGHT);
    this->addKeyBindButton("Relax", &cv::MOD_RELAX);
    this->addKeyBindButton("Autopilot", &cv::MOD_AUTOPILOT);
    this->addKeyBindButton("Spunout", &cv::MOD_SPUNOUT);
    this->addKeyBindButton("Auto", &cv::MOD_AUTO);
    this->addKeyBindButton("Score V2", &cv::MOD_SCOREV2);

    //**************************************************************************************************************************//

    CBaseUIElement *sectionGameplay = this->addSection("Gameplay");

    this->addSubSection("General");
    this->backgroundDimSlider = this->addSlider("Background Dim:", 0.0f, 1.0f, &cv::background_dim, 220.0f);
    this->backgroundDimSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangePercent>(this));
    this->backgroundBrightnessSlider =
        this->addSlider("Background Brightness:", 0.0f, 1.0f, &cv::background_brightness, 220.0f);
    this->backgroundBrightnessSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangePercent>(this));
    this->addSpacer();
    this->addCheckbox("Don't change dim level during breaks",
                      "Makes the background basically impossible to see during breaks.\nNot recommended.",
                      &cv::background_dont_fade_during_breaks);
    this->addCheckbox("Show approach circle on first \"Hidden\" object",
                      &cv::show_approach_circle_on_first_hidden_object);
    this->addCheckbox("SuddenDeath restart on miss", "Skips the failing animation, and instantly restarts like SS/PF.",
                      &cv::mod_suddendeath_restart);
    this->addCheckbox("Show Skip Button during Intro", "Skip intro to first hitobject.", &cv::skip_intro_enabled);
    this->addCheckbox("Show Skip Button during Breaks", "Skip breaks in the middle of beatmaps.",
                      &cv::skip_breaks_enabled);
    // FIXME: broken
    // this->addCheckbox("Save Failed Scores", "Allow failed scores to be saved as F ranks.",
    //                   &cv::save_failed_scores);
    this->addSpacer();
    this->addSubSection("Mechanics", "health drain notelock lock block blocking noteblock");
    this->addCheckbox(
        "Kill Player upon Failing",
        "Enabled: Singleplayer default. You die upon failing and the beatmap stops.\nDisabled: Multiplayer "
        "default. Allows you to keep playing even after failing.",
        &cv::drain_kill);
    this->addCheckbox("Disable HP drain",
                      "Like NF, but entirely disables HP mechanics. Will block online score submission.",
                      &cv::drain_disabled)
        ->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onModChangingToggle>(this));

    this->addSpacer();
    this->addLabel("");

    OPTIONS_ELEMENT *notelockSelect = this->addButton("Select [Notelock]", "None", true);
    ((CBaseUIButton *)notelockSelect->baseElems[0])
        ->setClickCallback(SA::MakeDelegate<&OptionsMenu::onNotelockSelect>(this));
    this->notelockSelectButton = notelockSelect->baseElems[0];
    this->notelockSelectLabel = (CBaseUILabel *)notelockSelect->baseElems[1];
    this->notelockSelectResetButton = notelockSelect->resetButton;
    this->notelockSelectResetButton->setClickCallback(
        SA::MakeDelegate<&OptionsMenu::onNotelockSelectResetClicked>(this));
    this->addLabel("");
    this->addLabel("Info about different notelock algorithms:")->setTextColor(0xff666666);
    this->addLabel("");
    this->addLabel("- McOsu: Auto miss previous circle, always.")->setTextColor(0xff666666);
    this->addLabel("- osu!stable: Locked until previous circle is miss.")->setTextColor(0xff666666);
    this->addLabel("- osu!lazer 2020: Auto miss previous circle if > time.")->setTextColor(0xff666666);
    this->addLabel("");
    this->addSpacer();
    this->addSubSection("Backgrounds", "image thumbnail");
    this->addCheckbox("Load Background Images (!)", "NOTE: Disabling this will disable ALL beatmap images everywhere!",
                      &cv::load_beatmap_background_images);
    this->addCheckbox("Draw Background in Beatmap", &cv::draw_beatmap_background_image);
    this->addCheckbox("Draw Background in SongBrowser",
                      "NOTE: You can disable this if you always want menu-background.",
                      &cv::draw_songbrowser_background_image);
    this->addCheckbox("Draw Background Thumbnails in SongBrowser", &cv::draw_songbrowser_thumbnails);
    this->addCheckbox("Draw Background in Ranking/Results Screen", &cv::draw_rankingscreen_background_image);
    this->addCheckbox("Draw menu-background in Menu", &cv::draw_menu_background);
    this->addCheckbox("Draw menu-background in SongBrowser",
                      "NOTE: Only applies if \"Draw Background in SongBrowser\" is disabled.",
                      &cv::draw_songbrowser_menu_background_image);
    this->addSpacer();
    // addCheckbox("Show pp on ranking screen", &cv::rankingscreen_pp);

    this->addSubSection("HUD");
    this->addCheckbox("Draw HUD", "NOTE: You can also press SHIFT + TAB while playing to toggle this.", &cv::draw_hud);
    this->addCheckbox(
        "SHIFT + TAB toggles everything",
        "Enabled: neosu default (toggle \"Draw HUD\")\nDisabled: osu! default (always show hiterrorbar + key overlay)",
        &cv::hud_shift_tab_toggles_everything);
    this->addSpacer();
    this->addCheckbox("Draw Score", &cv::draw_score);
    this->addCheckbox("Draw Combo", &cv::draw_combo);
    this->addCheckbox("Draw Accuracy", &cv::draw_accuracy);
    this->addCheckbox("Draw ProgressBar", &cv::draw_progressbar);
    this->addCheckbox("Draw HitErrorBar", &cv::draw_hiterrorbar);
    this->addCheckbox("Draw HitErrorBar UR", "Unstable Rate", &cv::draw_hiterrorbar_ur);
    this->addCheckbox("Draw ScoreBar", "Health/HP Bar.", &cv::draw_scorebar);
    this->addCheckbox(
        "Draw ScoreBar-bg",
        "Some skins abuse this as the playfield background image.\nIt is actually just the background image "
        "for the Health/HP Bar.",
        &cv::draw_scorebarbg);
    this->addCheckbox("Draw ScoreBoard in singleplayer", &cv::draw_scoreboard);
    this->addCheckbox("Draw ScoreBoard in multiplayer", &cv::draw_scoreboard_mp);
    this->addCheckbox("Draw Key Overlay", &cv::draw_inputoverlay);
    this->addCheckbox("Draw Scrubbing Timeline", &cv::draw_scrubbing_timeline);
    this->addCheckbox("Draw Miss Window on HitErrorBar", &cv::hud_hiterrorbar_showmisswindow);
    this->addSpacer();
    this->addCheckbox(
        "Draw Stats: pp",
        "Realtime pp counter.\nDynamically calculates earned pp by incrementally updating the star rating.",
        &cv::draw_statistics_pp);
    this->addCheckbox("Draw Stats: pp (SS)", "Max possible total pp for active mods (full combo + perfect acc).",
                      &cv::draw_statistics_perfectpp);
    this->addCheckbox("Draw Stats: Misses", "Number of misses.", &cv::draw_statistics_misses);
    this->addCheckbox("Draw Stats: SliderBreaks", "Number of slider breaks.", &cv::draw_statistics_sliderbreaks);
    this->addCheckbox("Draw Stats: Max Possible Combo", &cv::draw_statistics_maxpossiblecombo);
    this->addCheckbox("Draw Stats: Stars*** (Until Now)",
                      "Incrementally updates the star rating (aka \"realtime stars\").",
                      &cv::draw_statistics_livestars);
    this->addCheckbox("Draw Stats: Stars* (Total)", "Total stars for active mods.", &cv::draw_statistics_totalstars);
    this->addCheckbox("Draw Stats: BPM", &cv::draw_statistics_bpm);
    this->addCheckbox("Draw Stats: AR", &cv::draw_statistics_ar);
    this->addCheckbox("Draw Stats: CS", &cv::draw_statistics_cs);
    this->addCheckbox("Draw Stats: OD", &cv::draw_statistics_od);
    this->addCheckbox("Draw Stats: HP", &cv::draw_statistics_hp);
    this->addCheckbox("Draw Stats: 300 hitwindow", "Timing window for hitting a 300 (e.g. +-25ms).",
                      &cv::draw_statistics_hitwindow300);
    this->addCheckbox("Draw Stats: Notes Per Second", "How many clicks per second are currently required.",
                      &cv::draw_statistics_nps);
    this->addCheckbox("Draw Stats: Note Density", "How many objects are visible at the same time.",
                      &cv::draw_statistics_nd);
    this->addCheckbox("Draw Stats: Unstable Rate", &cv::draw_statistics_ur);
    this->addCheckbox(
        "Draw Stats: Accuracy Error",
        "Average hit error delta (e.g. -5ms +15ms).\nSee \"osu_hud_statistics_hitdelta_chunksize 30\",\nit "
        "defines how many recent hit deltas are averaged.",
        &cv::draw_statistics_hitdelta);
    this->addSpacer();
    this->hudSizeSlider = this->addSlider("HUD Scale:", 0.01f, 3.0f, &cv::hud_scale, 165.0f);
    this->hudSizeSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangePercent>(this));
    this->hudSizeSlider->setKeyDelta(0.01f);
    this->addSpacer();
    this->hudScoreScaleSlider = this->addSlider("Score Scale:", 0.01f, 3.0f, &cv::hud_score_scale, 165.0f);
    this->hudScoreScaleSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangePercent>(this));
    this->hudScoreScaleSlider->setKeyDelta(0.01f);
    this->hudComboScaleSlider = this->addSlider("Combo Scale:", 0.01f, 3.0f, &cv::hud_combo_scale, 165.0f);
    this->hudComboScaleSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangePercent>(this));
    this->hudComboScaleSlider->setKeyDelta(0.01f);
    this->hudAccuracyScaleSlider = this->addSlider("Accuracy Scale:", 0.01f, 3.0f, &cv::hud_accuracy_scale, 165.0f);
    this->hudAccuracyScaleSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangePercent>(this));
    this->hudAccuracyScaleSlider->setKeyDelta(0.01f);
    this->hudHiterrorbarScaleSlider =
        this->addSlider("HitErrorBar Scale:", 0.01f, 3.0f, &cv::hud_hiterrorbar_scale, 165.0f);
    this->hudHiterrorbarScaleSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangePercent>(this));
    this->hudHiterrorbarScaleSlider->setKeyDelta(0.01f);
    this->hudHiterrorbarURScaleSlider =
        this->addSlider("HitErrorBar UR Scale:", 0.01f, 3.0f, &cv::hud_hiterrorbar_ur_scale, 165.0f);
    this->hudHiterrorbarURScaleSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangePercent>(this));
    this->hudHiterrorbarURScaleSlider->setKeyDelta(0.01f);
    this->hudProgressbarScaleSlider =
        this->addSlider("ProgressBar Scale:", 0.01f, 3.0f, &cv::hud_progressbar_scale, 165.0f);
    this->hudProgressbarScaleSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangePercent>(this));
    this->hudProgressbarScaleSlider->setKeyDelta(0.01f);
    this->hudScoreBarScaleSlider = this->addSlider("ScoreBar Scale:", 0.01f, 3.0f, &cv::hud_scorebar_scale, 165.0f);
    this->hudScoreBarScaleSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangePercent>(this));
    this->hudScoreBarScaleSlider->setKeyDelta(0.01f);
    this->hudScoreBoardScaleSlider =
        this->addSlider("ScoreBoard Scale:", 0.01f, 3.0f, &cv::hud_scoreboard_scale, 165.0f);
    this->hudScoreBoardScaleSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangePercent>(this));
    this->hudScoreBoardScaleSlider->setKeyDelta(0.01f);
    this->hudInputoverlayScaleSlider =
        this->addSlider("Key Overlay Scale:", 0.01f, 3.0f, &cv::hud_inputoverlay_scale, 165.0f);
    this->hudInputoverlayScaleSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangePercent>(this));
    this->hudInputoverlayScaleSlider->setKeyDelta(0.01f);
    this->statisticsOverlayScaleSlider =
        this->addSlider("Statistics Scale:", 0.01f, 3.0f, &cv::hud_statistics_scale, 165.0f);
    this->statisticsOverlayScaleSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangePercent>(this));
    this->statisticsOverlayScaleSlider->setKeyDelta(0.01f);
    this->addSpacer();
    this->statisticsOverlayXOffsetSlider =
        this->addSlider("Statistics X Offset:", 0.0f, 2000.0f, &cv::hud_statistics_offset_x, 165.0f, true);
    this->statisticsOverlayXOffsetSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangeInt>(this));
    this->statisticsOverlayXOffsetSlider->setKeyDelta(1.0f);
    this->statisticsOverlayYOffsetSlider =
        this->addSlider("Statistics Y Offset:", 0.0f, 1000.0f, &cv::hud_statistics_offset_y, 165.0f, true);
    this->statisticsOverlayYOffsetSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangeInt>(this));
    this->statisticsOverlayYOffsetSlider->setKeyDelta(1.0f);

    this->addSubSection("Playfield");
    this->addCheckbox("Draw FollowPoints", &cv::draw_followpoints);
    this->addCheckbox("Draw Playfield Border", "Correct border relative to the current Circle Size.",
                      &cv::draw_playfield_border);
    this->addSpacer();
    this->playfieldBorderSizeSlider =
        this->addSlider("Playfield Border Size:", 0.0f, 500.0f, &cv::hud_playfield_border_size);
    this->playfieldBorderSizeSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangeInt>(this));
    this->playfieldBorderSizeSlider->setKeyDelta(1.0f);

    this->addSubSection("Hitobjects");
    this->addCheckbox(
        "Use Fast Hidden Fading Sliders (!)",
        "NOTE: osu! doesn't do this, so don't enable it for serious practicing.\nIf enabled: Fade out sliders "
        "with the same speed as circles.",
        &cv::mod_hd_slider_fast_fade);

    //**************************************************************************************************************************//

    CBaseUIElement *sectionFposu = this->addSection("FPoSu (3D)");

    this->addSubSection("FPoSu - General");
    this->addCheckbox("FPoSu",
                      "The real 3D FPS mod.\nPlay from a first person shooter perspective in a 3D environment.\nThis "
                      "is only intended for mouse! (Enable \"Tablet/Absolute Mode\" for tablets.)",
                      &cv::mod_fposu);
    this->addLabel("");
    this->addLabel("NOTE: Use CTRL + O during gameplay to get here!")->setTextColor(0xff555555);
    this->addLabel("");
    this->addLabel("LEFT/RIGHT arrow keys to precisely adjust sliders.")->setTextColor(0xff555555);
    this->addLabel("");
    CBaseUISlider *fposuDistanceSlider = this->addSlider("Distance:", 0.01f, 5.0f, &cv::fposu_distance, -1.0f, true);
    fposuDistanceSlider->setKeyDelta(0.01f);
    this->addCheckbox("Vertical FOV", "If enabled: Vertical FOV.\nIf disabled: Horizontal FOV (default).",
                      &cv::fposu_vertical_fov);
    CBaseUISlider *fovSlider = this->addSlider("FOV:", 10.0f, 160.0f, &cv::fposu_fov, -1.0f, true, true);
    fovSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangeTwoDecimalPlaces>(this));
    fovSlider->setKeyDelta(0.01f);
    CBaseUISlider *zoomedFovSlider =
        this->addSlider("FOV (Zoom):", 10.0f, 160.0f, &cv::fposu_zoom_fov, -1.0f, true, true);
    zoomedFovSlider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangeTwoDecimalPlaces>(this));
    zoomedFovSlider->setKeyDelta(0.01f);
    this->addCheckbox("Zoom Key Toggle", "Enabled: Zoom key toggles zoom.\nDisabled: Zoom while zoom key is held.",
                      &cv::fposu_zoom_toggle);
    this->addSubSection("FPoSu - Playfield");
    this->addCheckbox("Curved play area", &cv::fposu_curved);
    this->addCheckbox("Background cube", &cv::fposu_cube);
    this->addCheckbox("Skybox", "NOTE: Overrides \"Background cube\".\nSee skybox_example.png for cubemap layout.",
                      &cv::fposu_skybox);
    this->addSlider("Background Opacity", 0.0f, 1.0f, &cv::background_alpha)
        ->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChangePercent>(this));

    this->addSubSection("FPoSu - Mouse");
    UIButton *cm360CalculatorLinkButton = this->addButton("https://www.mouse-sensitivity.com/");
    cm360CalculatorLinkButton->setClickCallback(SA::MakeDelegate<&OptionsMenu::onCM360CalculatorLinkClicked>(this));
    cm360CalculatorLinkButton->setColor(0xff0e4a59);
    this->addLabel("");
    this->dpiTextbox = this->addTextbox(cv::fposu_mouse_dpi.getString().c_str(), "DPI:", &cv::fposu_mouse_dpi);
    this->cm360Textbox =
        this->addTextbox(cv::fposu_mouse_cm_360.getString().c_str(), "cm per 360:", &cv::fposu_mouse_cm_360);
    this->addLabel("");
    this->addCheckbox("Invert Vertical", &cv::fposu_invert_vertical);
    this->addCheckbox("Invert Horizontal", &cv::fposu_invert_horizontal);
    this->addCheckbox("Tablet/Absolute Mode (!)",
                      "Don't enable this if you are using a mouse.\nIf this is enabled, then DPI and cm per "
                      "360 will be ignored!",
                      &cv::fposu_absolute_mode);

    //**************************************************************************************************************************//

    this->addSection("Bullshit");

    this->addSubSection("Why");
    this->addCheckbox("Rainbow Circles", &cv::circle_rainbow);
    this->addCheckbox("Rainbow Sliders", &cv::slider_rainbow);
    this->addCheckbox("Rainbow Numbers", &cv::circle_number_rainbow);
    this->addCheckbox("Draw 300s", &cv::hitresult_draw_300s);

    this->addSection("Maintenance");
    this->addSubSection("Restore");
    UIButton *resetAllSettingsButton = this->addButton("Reset all settings");
    resetAllSettingsButton->setClickCallback(SA::MakeDelegate<&OptionsMenu::onResetEverythingClicked>(this));
    resetAllSettingsButton->setColor(0xffd90000);
    this->addSubSection("Testing");
    this->addCheckbox("Use bleeding edge release stream", &cv::bleedingedge);
    this->addSpacer();
    this->addSpacer();
    this->addSpacer();
    this->addSpacer();
    this->addSpacer();
    this->addSpacer();
    this->addSpacer();
    this->addSpacer();

    //**************************************************************************************************************************//

    // build categories
    this->addCategory(this->sectionGeneral, Icons::GEAR);
    this->addCategory(sectionGraphics, Icons::DESKTOP);
    this->addCategory(sectionAudio, Icons::VOLUME_UP);
    this->addCategory(this->skinSection, Icons::PAINTBRUSH);
    this->addCategory(sectionInput, Icons::GAMEPAD);
    this->addCategory(subSectionKeyboard, Icons::KEYBOARD);
    this->addCategory(sectionGameplay, Icons::CIRCLE);
    this->fposuCategoryButton = this->addCategory(sectionFposu, Icons::CUBE);

    //**************************************************************************************************************************//

    // the context menu gets added last (drawn on top of everything)
    this->options->container->addBaseUIElement(this->contextMenu);

    // HACKHACK: force current value update
    if(this->sliderQualitySlider != nullptr)
        this->onHighQualitySlidersConVarChange(cv::options_high_quality_sliders.getFloat());
}

OptionsMenu::~OptionsMenu() {
    this->options->invalidate();

    SAFE_DELETE(this->spacer);
    SAFE_DELETE(this->contextMenu);

    for(auto &element : this->elemContainers) {
        if(!element) continue;
        SAFE_DELETE(element->resetButton);
        for(auto &sub : element->baseElems) {
            SAFE_DELETE(sub);
        }
        SAFE_DELETE(element);
    }
}

void OptionsMenu::draw() {
    const bool isAnimating = anim->isAnimating(&this->fAnimation);
    if(!this->bVisible && !isAnimating) {
        this->contextMenu->draw();
        return;
    }

    this->sliderPreviewElement->setDrawSliderHack(!isAnimating);

    if(isAnimating) {
        osu->getSliderFrameBuffer()->enable();

        g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_PREMUL_ALPHA);
    }

    const bool isPlayingBeatmap = osu->isInPlayMode();

    // interactive sliders

    if(this->backgroundBrightnessSlider->isActive()) {
        if(!isPlayingBeatmap) {
            const float brightness = std::clamp<float>(this->backgroundBrightnessSlider->getFloat(), 0.0f, 1.0f);
            const short red = std::clamp<float>(brightness * cv::background_color_r.getFloat(), 0.0f, 255.0f);
            const short green = std::clamp<float>(brightness * cv::background_color_g.getFloat(), 0.0f, 255.0f);
            const short blue = std::clamp<float>(brightness * cv::background_color_b.getFloat(), 0.0f, 255.0f);
            if(brightness > 0.0f) {
                g->setColor(argb(255, red, green, blue));
                g->fillRect(0, 0, osu->getVirtScreenWidth(), osu->getVirtScreenHeight());
            }
        }
    }

    if(this->backgroundDimSlider->isActive()) {
        if(!isPlayingBeatmap) {
            const short dim = std::clamp<float>(this->backgroundDimSlider->getFloat(), 0.0f, 1.0f) * 255.0f;
            g->setColor(argb(dim, 0, 0, 0));
            g->fillRect(0, 0, osu->getVirtScreenWidth(), osu->getVirtScreenHeight());
        }
    }

    if(this->hudSizeSlider->isActive() || this->hudComboScaleSlider->isActive() ||
       this->hudScoreScaleSlider->isActive() || this->hudAccuracyScaleSlider->isActive() ||
       this->hudHiterrorbarScaleSlider->isActive() || this->hudHiterrorbarURScaleSlider->isActive() ||
       this->hudProgressbarScaleSlider->isActive() || this->hudScoreBarScaleSlider->isActive() ||
       this->hudScoreBoardScaleSlider->isActive() || this->hudInputoverlayScaleSlider->isActive() ||
       this->statisticsOverlayScaleSlider->isActive() || this->statisticsOverlayXOffsetSlider->isActive() ||
       this->statisticsOverlayYOffsetSlider->isActive()) {
        if(!isPlayingBeatmap) osu->getHUD()->drawDummy();
    } else if(this->playfieldBorderSizeSlider->isActive()) {
        osu->getHUD()->drawPlayfieldBorder(GameRules::getPlayfieldCenter(), GameRules::getPlayfieldSize(), 100);
    }

    ScreenBackable::draw();

    // Re-drawing context menu to make sure it's drawn on top of the back button
    // Context menu input still gets processed first, so this is fine
    this->contextMenu->draw();

    if(this->cursorSizeSlider->getFloat() < 0.15f) mouse->drawDebug();

    if(isAnimating) {
        // HACKHACK:
        if(!this->bVisible) this->backButton->draw();

        g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_ALPHA);

        osu->getSliderFrameBuffer()->disable();

        g->push3DScene(McRect(0, 0, this->options->getSize().x, this->options->getSize().y));
        {
            g->rotate3DScene(0, -(1.0f - this->fAnimation) * 90, 0);
            g->translate3DScene(-(1.0f - this->fAnimation) * this->options->getSize().x * 1.25f, 0,
                                -(1.0f - this->fAnimation) * 700);

            osu->getSliderFrameBuffer()->setColor(argb(this->fAnimation, 1.0f, 1.0f, 1.0f));
            osu->getSliderFrameBuffer()->draw(0, 0);
        }
        g->pop3DScene();
    }
}

void OptionsMenu::update_login_button(bool loggedIn) {
    if(loggedIn || BanchoState::is_online()) {
        this->logInButton->setText("Disconnect");
        this->logInButton->setColor(0xffd90000);
    } else {
        bool oauth = this->should_use_oauth_login();
        this->logInButton->setText(oauth ? "Log in with osu!" : "Log in");
        this->logInButton->setColor(oauth ? 0xffdd30c1 : 0xff00d900);
    }
    this->logInButton->is_loading = BanchoState::is_logging_in();
}

void OptionsMenu::mouse_update(bool *propagate_clicks) {
    if(this->bLayoutUpdateScheduled.load(std::memory_order_relaxed)) {
        this->updateLayout();
    }

    if(!this->bVisible) return;

    // force context menu click handling focus
    this->contextMenu->mouse_update(propagate_clicks);
    if(!*propagate_clicks) return;

    // force context menu mouse-inside focus
    // NOTE: "propagate_clicks" does not solve the issue of mouse hover focus
    // this hack is still required, otherwise hovering underneath e.g. skin dropdown will play duplicate sounds for
    // each element selected (at all z-orders!)
    if(this->contextMenu->isVisible()) {
        this->backButton->stealFocus();
    }

    ScreenBackable::mouse_update(propagate_clicks);
    if(!*propagate_clicks) return;

    if(this->contextMenu->isVisible()) {
        // eyes are bleeding...
        for(auto e : this->options->container->vElements) {
            if(e == this->contextMenu) continue;
            e->stealFocus();
        }
    }

    if(this->bDPIScalingScrollToSliderScheduled) {
        this->bDPIScalingScrollToSliderScheduled = false;
        this->options->scrollToElement(this->uiScaleSlider, 0, 200 * Osu::getUIScale());
    }

    // flash osu!folder textbox red if incorrect
    if(this->fOsuFolderTextboxInvalidAnim > engine->getTime()) {
        float redness = std::fabs(std::sin((this->fOsuFolderTextboxInvalidAnim - engine->getTime()) * 3.)) * 0.5f;
        this->osuFolderTextbox->setBackgroundColor(argb(1.f, redness, 0.f, 0.f));
    } else
        this->osuFolderTextbox->setBackgroundColor(0xff000000);

    // hack to avoid entering search text while binding keys
    if(osu->getNotificationOverlay()->isVisible() && osu->getNotificationOverlay()->isWaitingForKey())
        this->fSearchOnCharKeybindHackTime = engine->getTime() + 0.1f;

    // highlight active category depending on scroll position
    if(this->categoryButtons.size() > 0) {
        OptionsMenuCategoryButton *activeCategoryButton = this->categoryButtons[0];
        for(auto categoryButton : this->categoryButtons) {
            if(categoryButton != nullptr && categoryButton->getSection() != nullptr) {
                categoryButton->setActiveCategory(false);
                categoryButton->setTextColor(0xff737373);

                if(categoryButton->getSection()->getPos().y < this->options->getSize().y * 0.4)
                    activeCategoryButton = categoryButton;
            }
        }
        if(activeCategoryButton != nullptr) {
            activeCategoryButton->setActiveCategory(true);
            activeCategoryButton->setTextColor(0xffffffff);
        }
    }

    // delayed update letterboxing mouse scale/offset settings
    if(this->bLetterboxingOffsetUpdateScheduled) {
        if(!this->letterboxingOffsetXSlider->isActive() && !this->letterboxingOffsetYSlider->isActive()) {
            this->bLetterboxingOffsetUpdateScheduled = false;

            cv::letterboxing_offset_x.setValue(this->letterboxingOffsetXSlider->getFloat());
            cv::letterboxing_offset_y.setValue(this->letterboxingOffsetYSlider->getFloat());

            // and update reset buttons as usual
            this->onResetUpdate(this->letterboxingOffsetResetButton);
        }
    }

    if(this->bUIScaleScrollToSliderScheduled) {
        this->bUIScaleScrollToSliderScheduled = false;
        this->options->scrollToElement(this->uiScaleSlider, 0, 200 * Osu::getUIScale());
    }

    // delayed UI scale change
    if(this->bUIScaleChangeScheduled) {
        if(!this->uiScaleSlider->isActive()) {
            this->bUIScaleChangeScheduled = false;

            const float oldUIScale = Osu::getUIScale();

            cv::ui_scale.setValue(this->uiScaleSlider->getFloat());

            const float newUIScale = Osu::getUIScale();

            // and update reset buttons as usual
            this->onResetUpdate(this->uiScaleResetButton);

            // additionally compensate scroll pos (but delay 1 frame)
            if(oldUIScale != newUIScale) this->bUIScaleScrollToSliderScheduled = true;
        }
    }

    // delayed WASAPI buffer/period change
    if(this->bASIOBufferChangeScheduled) {
        if(!this->asioBufferSizeSlider->isActive()) {
            this->bASIOBufferChangeScheduled = false;

            cv::asio_buffer_size.setValue(this->asioBufferSizeSlider->getFloat());

            // and update reset buttons as usual
            this->onResetUpdate(this->asioBufferSizeResetButton);
        }
    }

    if(!cv::win_snd_wasapi_event_callbacks.getBool()) {
        if(this->bWASAPIBufferChangeScheduled) {
            if(!this->wasapiBufferSizeSlider->isActive()) {
                this->bWASAPIBufferChangeScheduled = false;

                cv::win_snd_wasapi_buffer_size.setValue(this->wasapiBufferSizeSlider->getFloat());

                // and update reset buttons as usual
                this->onResetUpdate(this->wasapiBufferSizeResetButton);
            }
        }

        if(this->bWASAPIPeriodChangeScheduled) {
            if(!this->wasapiPeriodSizeSlider->isActive()) {
                this->bWASAPIPeriodChangeScheduled = false;

                // ignore if event callback mode is enabled
                cv::win_snd_wasapi_period_size.setValue(this->wasapiPeriodSizeSlider->getFloat());

                // and update reset buttons as usual
                this->onResetUpdate(this->wasapiPeriodSizeResetButton);
            }
        }
    }

    // apply textbox changes on enter key
    if(this->osuFolderTextbox->hitEnter()) cv::osu_folder.setValue(this->osuFolderTextbox->getText());

    // HACKHACK (should just use callback)
    // XXX: disable serverTextbox while logging in
    // XXX: jank logic split between update_login_button/setLogingLoadingState
    const auto server_text{this->serverTextbox->getText().utf8View()};
    if(cv::mp_server.getString() != server_text) {
        cv::mp_server.setValue(server_text);
        this->update_login_button();
        this->scheduleLayoutUpdate();  // toggle user/pass fields based on oauthness
    }

    // why does all of this need to run in update()...
    const auto &name_text{this->nameTextbox->getText().utf8View()};
    if(!name_text.empty() && cv::name.getString() != name_text) {
        cv::name.setValue(name_text);
    }
    const auto &password_text{this->passwordTextbox->getText().utf8View()};
    if(!password_text.empty() && password_text != cv::mp_password_md5.getString()) {
        // passwordTextbox gets overwritten with mp_password_md5 on login
        // mp_password should remain empty after that
        cv::mp_password.setValue(password_text);
    }
    if(this->nameTextbox->hitEnter()) {
        this->nameTextbox->stealFocus();
        this->passwordTextbox->focus();
    }
    if(this->passwordTextbox->hitEnter()) {
        this->passwordTextbox->stealFocus();
        this->logInButton->click();
    }
    if(this->serverTextbox->hitEnter()) {
        this->serverTextbox->stealFocus();
        this->logInButton->click();
    }

    if(this->dpiTextbox != nullptr && this->dpiTextbox->hitEnter()) this->updateFposuDPI();
    if(this->cm360Textbox != nullptr && this->cm360Textbox->hitEnter()) this->updateFposuCMper360();
}

void OptionsMenu::onKeyDown(KeyboardEvent &e) {
    if(!this->bVisible) return;

    this->contextMenu->onKeyDown(e);
    if(e.isConsumed()) return;

    // KEY_TAB doesn't work on Linux
    if(e.getScanCode() == 65056 || e.getScanCode() == KEY_TAB) {
        if(this->serverTextbox->isActive()) {
            this->serverTextbox->stealFocus();
            this->nameTextbox->focus();
            e.consume();
            return;
        } else if(this->nameTextbox->isActive()) {
            this->nameTextbox->stealFocus();
            this->passwordTextbox->focus();
            e.consume();
            return;
        }
    }

    // searching text delete
    if(!this->sSearchString.isEmpty()) {
        switch(e.getScanCode()) {
            case KEY_DELETE:
            case KEY_BACKSPACE:
                if(keyboard->isControlDown()) {
                    // delete everything from the current caret position to the left, until after the first
                    // non-space character (but including it)
                    bool foundNonSpaceChar = false;
                    while(!this->sSearchString.isEmpty()) {
                        const auto &curChar = this->sSearchString.back();

                        const bool whitespace = std::iswspace(static_cast<wint_t>(curChar)) != 0;
                        if(foundNonSpaceChar && whitespace) break;

                        if(!whitespace) foundNonSpaceChar = true;

                        this->sSearchString.pop_back();
                    }
                } else {
                    this->sSearchString.pop_back();
                }

                e.consume();
                this->scheduleSearchUpdate();
                return;

            case KEY_ESCAPE:
                this->sSearchString.clear();
                this->scheduleSearchUpdate();
                e.consume();
                return;
            default:
                break;
        }
    }

    ScreenBackable::onKeyDown(e);

    // paste clipboard support
    if(!e.isConsumed() && keyboard->isControlDown() && e == KEY_V) {
        const auto &clipstring = env->getClipBoardText();
        if(!clipstring.isEmpty()) {
            this->sSearchString.append(clipstring);
            this->scheduleSearchUpdate();
            e.consume();
            return;
        }
    }

    // Consuming all keys so they're not forwarded to main menu or chat when searching for a setting
    e.consume();
}

void OptionsMenu::onChar(KeyboardEvent &e) {
    if(!this->bVisible) return;

    ScreenBackable::onChar(e);
    if(e.isConsumed()) return;

    // handle searching
    if(e.getCharCode() < 32 || !this->bVisible || (keyboard->isControlDown() && !keyboard->isAltDown()) ||
       this->fSearchOnCharKeybindHackTime > engine->getTime())
        return;

    this->sSearchString.append(e.getCharCode());

    this->scheduleSearchUpdate();

    e.consume();
}

void OptionsMenu::onResolutionChange(vec2 newResolution) {
    ScreenBackable::onResolutionChange(newResolution);

    // HACKHACK: magic
    if constexpr(Env::cfg(OS::WINDOWS)) {
        if((env->winFullscreened() && (int)newResolution.y == (int)env->getNativeScreenSize().y + 1)) newResolution.y--;
    }

    if(this->resolutionLabel != nullptr)
        this->resolutionLabel->setText(UString::format("%ix%i", (int)newResolution.x, (int)newResolution.y));
}

void OptionsMenu::onKey(KeyboardEvent &e) {
    // from NotificationOverlay
    if(this->waitingKey != nullptr) {
        this->waitingKey->setValue((float)e.getScanCode());
        this->waitingKey = nullptr;
    }
}

CBaseUIContainer *OptionsMenu::setVisible(bool visible) {
    this->setVisibleInt(visible, false);
    return this;
}

void OptionsMenu::setVisibleInt(bool visible, bool fromOnBack) {
    if(visible != this->bVisible) {
        // open/close animation
        if(!this->bFullscreen) {
            if(!this->bVisible)
                anim->moveQuartOut(&this->fAnimation, 1.0f, 0.25f * (1.0f - this->fAnimation), true);
            else
                anim->moveQuadOut(&this->fAnimation, 0.0f, 0.25f * this->fAnimation, true);
        }

        // save even if not closed via onBack(), e.g. if closed via setVisible(false) from outside
        if(!visible && !fromOnBack) {
            osu->getNotificationOverlay()->stopWaitingForKey();
            this->save();
        }
    }

    this->bVisible = visible;
    osu->getChat()->updateVisibility();

    if(visible) {
        this->updateLayout();
    } else {
        this->contextMenu->setVisible2(false);
    }

    // usability: auto scroll to fposu settings if opening options while in fposu gamemode
    if(visible && osu->isInPlayMode() && cv::mod_fposu.getBool() && !this->fposuCategoryButton->isActiveCategory())
        this->onCategoryClicked(this->fposuCategoryButton);

    if(visible) {
        // reset reset counters
        this->iNumResetAllKeyBindingsPressed = 0;
        this->iNumResetEverythingPressed = 0;
    }
}

void OptionsMenu::setUsername(UString username) { this->nameTextbox->setText(std::move(username)); }

bool OptionsMenu::isMouseInside() {
    return (this->backButton->isMouseInside() || this->options->isMouseInside() || this->categories->isMouseInside()) &&
           this->isVisible();
}

bool OptionsMenu::isBusy() {
    return (this->backButton->isActive() || this->options->isBusy() || this->categories->isBusy()) && this->isVisible();
}

void OptionsMenu::updateLayout() {
    this->bLayoutUpdateScheduled.store(false, std::memory_order_release);
    this->updating_layout = true;

    bool oauth = this->should_use_oauth_login();
    this->update_login_button();

    // set all elements to the current convar values, and update the reset button states
    for(auto &element : this->elemContainers) {
        for(auto &[baseElem, cv] : element->cvars) {
            if(!baseElem || !cv) continue;
            switch(element->type) {
                case CBX:
                case CBX_BTN: {
                    auto *checkboxPointer = dynamic_cast<CBaseUICheckbox *>(baseElem);
                    if(checkboxPointer != nullptr) checkboxPointer->setChecked(cv->getBool());
                } break;
                case SLDR:
                    if(element->baseElems.size() == 3) {
                        auto *sliderPointer = dynamic_cast<CBaseUISlider *>(element->baseElems[1]);
                        if(sliderPointer != nullptr) {
                            // allow users to overscale certain values via the console
                            if(element->allowOverscale && cv->getFloat() > sliderPointer->getMax())
                                sliderPointer->setBounds(sliderPointer->getMin(), cv->getFloat());

                            // allow users to underscale certain values via the console
                            if(element->allowUnderscale && cv->getFloat() < sliderPointer->getMin())
                                sliderPointer->setBounds(cv->getFloat(), sliderPointer->getMax());

                            sliderPointer->setValue(cv->getFloat(), false);
                            sliderPointer->fireChangeCallback();
                        }
                    }
                    break;
                case TBX:
                    if(element->baseElems.size() == 1) {
                        auto *textboxPointer = dynamic_cast<CBaseUITextbox *>(element->baseElems[0]);
                        if(textboxPointer != nullptr) {
                            // HACKHACK: don't override textbox with mp_password (which gets deleted on login)
                            UString textToSet{cv->getString().c_str()};
                            if(cv == &cv::mp_password && cv::mp_password.getString().empty()) {
                                textToSet = cv::mp_password_md5.getString().c_str();
                            }
                            textboxPointer->setText(textToSet);
                        }
                    } else if(element->baseElems.size() == 2) {
                        auto *textboxPointer = dynamic_cast<CBaseUITextbox *>(element->baseElems[1]);
                        if(textboxPointer != nullptr) textboxPointer->setText(cv->getString().c_str());
                    }
                    break;
                default:
                    break;
            }
        }

        this->onResetUpdate(element->resetButton);
    }

    if(this->fullscreenCheckbox != nullptr) this->fullscreenCheckbox->setChecked(env->winFullscreened(), false);

    this->updateSkinNameLabel();
    this->updateNotelockSelectLabel();

    if(this->outputDeviceLabel != nullptr) this->outputDeviceLabel->setText(soundEngine->getOutputDeviceName());

    this->onOutputDeviceResetUpdate();
    this->onNotelockSelectResetUpdate();

    //************************************************************************************************************************************//

    // TODO: correctly scale individual UI elements to dpiScale (depend on initial value in e.g. addCheckbox())

    ScreenBackable::updateLayout();

    const float dpiScale = Osu::getUIScale();

    this->setSize(osu->getVirtScreenSize());

    // options panel
    const float optionsScreenWidthPercent = 0.5f;
    const float categoriesOptionsPercent = 0.135f;

    int optionsWidth = (int)(osu->getVirtScreenWidth() * optionsScreenWidthPercent);
    if(!this->bFullscreen)
        optionsWidth = std::min((int)(725.0f * (1.0f - categoriesOptionsPercent)), optionsWidth) * dpiScale;

    const int categoriesWidth = optionsWidth * categoriesOptionsPercent;

    this->options->setRelPosX(
        (!this->bFullscreen ? -1 : osu->getVirtScreenWidth() / 2 - (optionsWidth + categoriesWidth) / 2) +
        categoriesWidth);
    this->options->setSize(optionsWidth, osu->getVirtScreenHeight() + 1);

    this->search->setRelPos(this->options->getRelPos());
    this->search->setSize(this->options->getSize());

    this->categories->setRelPosX(this->options->getRelPos().x - categoriesWidth);
    this->categories->setSize(categoriesWidth, osu->getVirtScreenHeight() + 1);

    // reset
    this->options->invalidate();

    // build layout
    bool enableHorizontalScrolling = false;
    int sideMargin = 25 * 2 * dpiScale;
    int spaceSpacing = 25 * dpiScale;
    int sectionSpacing = -15 * dpiScale;            // section title to first element
    int subsectionSpacing = 15 * dpiScale;          // subsection title to first element
    int sectionEndSpacing = /*70*/ 120 * dpiScale;  // last section element to next section title
    int subsectionEndSpacing = 65 * dpiScale;       // last subsection element to next subsection title
    int elementSpacing = 5 * dpiScale;
    int elementTextStartOffset = 11 * dpiScale;  // e.g. labels in front of sliders
    int yCounter = sideMargin + 20 * dpiScale;
    bool inSkipSection = false;
    bool inSkipSubSection = false;
    bool sectionTitleMatch = false;
    bool subSectionTitleMatch = false;
    const auto &search = this->sSearchString;
    for(int i = 0; i < this->elemContainers.size(); i++) {
        if(!this->elemContainers[i] || !this->elemContainers[i]->render_condition()) continue;
        if(this->elemContainers[i]->render_condition == RenderCondition::ASIO_ENABLED &&
           !(soundEngine->getOutputDriverType() == SoundEngine::OutputDriver::BASS_ASIO))
            continue;
        if(this->elemContainers[i]->render_condition == RenderCondition::WASAPI_ENABLED &&
           !(soundEngine->getOutputDriverType() == SoundEngine::OutputDriver::BASS_WASAPI))
            continue;

        // XXX: we should hide the checkbox for hardcoded servers that ban it (before connecting)
        //      ...and disable serverTextbox editing while connected
        if(this->elemContainers[i]->render_condition == RenderCondition::SCORE_SUBMISSION_POLICY &&
           (BanchoState::score_submission_policy != ServerPolicy::NO_PREFERENCE || oauth))
            continue;

        if(this->elemContainers[i]->render_condition == RenderCondition::PASSWORD_AUTH && oauth) continue;

        // searching logic happens here:
        // section
        //     content
        //     subsection
        //           content

        // case 1: match in content -> section + subsection + content     -> section + subsection matcher
        // case 2: match in content -> section + content                  -> section matcher
        // if match in section or subsection -> display entire section (disregard content match)
        // matcher is run through all remaining elements at every section + subsection

        if(!search.isEmpty()) {
            const auto &searchTags = this->elemContainers[i]->searchTags;

            // if this is a section
            if(this->elemContainers[i]->type == SECT) {
                bool sectionMatch = false;

                const auto &sectionTitle = this->elemContainers[i]->baseElems[0]->getName();
                sectionTitleMatch = sectionTitle.findIgnoreCase(search) != -1;

                subSectionTitleMatch = false;
                if(inSkipSection) inSkipSection = false;

                for(int s = i + 1; s < this->elemContainers.size(); s++) {
                    if(!this->elemContainers[s]) continue;
                    if(this->elemContainers[s]->type == SECT)  // stop at next section
                        break;

                    for(auto &element : this->elemContainers[s]->baseElems) {
                        if(!element->getName().isEmpty()) {
                            const auto &tags = element->getName();

                            if(tags.findIgnoreCase(search) != -1) {
                                sectionMatch = true;
                                break;
                            }
                        }
                    }
                }

                inSkipSection = !sectionMatch;
                if(!inSkipSection) inSkipSubSection = false;
            }

            // if this is a subsection
            if(this->elemContainers[i]->type == SUBSECT) {
                bool subSectionMatch = false;

                const auto &subSectionTitle = this->elemContainers[i]->baseElems[0]->getName();
                subSectionTitleMatch =
                    subSectionTitle.findIgnoreCase(search) != -1 || searchTags.findIgnoreCase(search) != -1;

                if(inSkipSubSection) inSkipSubSection = false;

                for(int s = i + 1; s < this->elemContainers.size(); s++) {
                    if(!this->elemContainers[s]) continue;
                    if(this->elemContainers[s]->type == SUBSECT)  // stop at next subsection
                        break;

                    for(auto &element : this->elemContainers[s]->baseElems) {
                        if(!element->getName().isEmpty()) {
                            const auto &tags = element->getName();

                            if(tags.findIgnoreCase(search) != -1) {
                                subSectionMatch = true;
                                break;
                            }
                        }
                    }
                }

                inSkipSubSection = !subSectionMatch;
            }

            bool inSkipContent = false;
            if(!inSkipSection && !inSkipSubSection) {
                bool contentMatch = false;

                if(this->elemContainers[i]->type > SUBSECT) {
                    for(auto &element : this->elemContainers[i]->baseElems) {
                        if(!element) continue;
                        if(!element->getName().isEmpty()) {
                            const auto &tags = element->getName();

                            if(tags.findIgnoreCase(search) != -1) {
                                contentMatch = true;
                                break;
                            }
                        }
                    }

                    // if section or subsection titles match, then include all content of that (sub)section (even if
                    // content doesn't match)
                    inSkipContent = !contentMatch;
                }
            }

            if((inSkipSection || inSkipSubSection || inSkipContent) && !sectionTitleMatch && !subSectionTitleMatch)
                continue;
        }

        // add all elements of the current entry
        {
            // reset button
            if(this->elemContainers[i]->resetButton != nullptr)
                this->options->container->addBaseUIElement(this->elemContainers[i]->resetButton);

            // (sub-)elements
            for(auto &element : this->elemContainers[i]->baseElems) {
                this->options->container->addBaseUIElement(element);
            }
        }

        // and build the layout

        // if this element is a new section, add even more spacing
        if(i > 0 && this->elemContainers[i]->type == SECT) yCounter += sectionEndSpacing;

        // if this element is a new subsection, add even more spacing
        if(i > 0 && this->elemContainers[i]->type == SUBSECT) yCounter += subsectionEndSpacing;

        const int elementWidth = optionsWidth - 2 * sideMargin - 2 * dpiScale;
        const bool isKeyBindButton = (this->elemContainers[i]->type == BINDBTN);
        const bool isButtonCheckbox = (this->elemContainers[i]->type == CBX_BTN);

        if(this->elemContainers[i]->resetButton != nullptr) {
            CBaseUIButton *resetButton = this->elemContainers[i]->resetButton;
            resetButton->setSize(vec2(35, 50) * dpiScale);
            resetButton->setRelPosY(yCounter);
            resetButton->setRelPosX(0);
        }

        for(auto e : this->elemContainers[i]->baseElems) {
            e->setSizeY(e->getRelSize().y * dpiScale);
        }

        if(this->elemContainers[i]->baseElems.size() == 1) {
            CBaseUIElement *e = this->elemContainers[i]->baseElems[0];

            int sideMarginAdd = 0;
            auto *labelPointer = dynamic_cast<CBaseUILabel *>(e);
            if(labelPointer != nullptr) {
                labelPointer->onResized();  // HACKHACK: framework, setSize*() does not update string metrics
                labelPointer->setSizeToContent(0, 0);
                sideMarginAdd += elementTextStartOffset;
            }

            auto *buttonPointer = dynamic_cast<CBaseUIButton *>(e);
            if(buttonPointer != nullptr)
                buttonPointer->onResized();  // HACKHACK: framework, setSize*() does not update string metrics

            auto *checkboxPointer = dynamic_cast<CBaseUICheckbox *>(e);
            if(checkboxPointer != nullptr) {
                checkboxPointer->onResized();  // HACKHACK: framework, setWidth*() does not update string metrics
                checkboxPointer->setWidthToContent(0);
                if(checkboxPointer->getSize().x > elementWidth)
                    enableHorizontalScrolling = true;
                else
                    e->setSizeX(elementWidth);
            } else
                e->setSizeX(elementWidth);

            e->setRelPosX(sideMargin + sideMarginAdd);
            e->setRelPosY(yCounter);

            yCounter += e->getSize().y;
        } else if(this->elemContainers[i]->baseElems.size() == 2 || isKeyBindButton) {
            CBaseUIElement *e1 = this->elemContainers[i]->baseElems[0];
            CBaseUIElement *e2 = this->elemContainers[i]->baseElems[1];

            int spacing = 15 * dpiScale;

            int sideMarginAdd = 0;
            auto *labelPointer = dynamic_cast<CBaseUILabel *>(e1);
            if(labelPointer != nullptr) sideMarginAdd += elementTextStartOffset;

            auto *buttonPointer = dynamic_cast<CBaseUIButton *>(e1);
            if(buttonPointer != nullptr)
                buttonPointer->onResized();  // HACKHACK: framework, setSize*() does not update string metrics

            // button-button spacing
            auto *buttonPointer2 = dynamic_cast<CBaseUIButton *>(e2);
            if(buttonPointer != nullptr && buttonPointer2 != nullptr) spacing *= 0.35f;

            if(isKeyBindButton) {
                CBaseUIElement *e3 = this->elemContainers[i]->baseElems[2];

                const float dividerMiddle = 5.0f / 8.0f;
                const float dividerEnd = 2.0f / 8.0f;

                e1->setRelPos(sideMargin, yCounter);
                e1->setSizeX(e1->getSize().y);

                e2->setRelPos(sideMargin + e1->getSize().x + 0.5f * spacing, yCounter);
                e2->setSizeX(elementWidth * dividerMiddle - spacing);

                e3->setRelPos(sideMargin + e1->getSize().x + e2->getSize().x + 1.5f * spacing, yCounter);
                e3->setSizeX(elementWidth * dividerEnd - spacing);

                yCounter += e1->getSize().y;
            } else if(isButtonCheckbox) {
                // make checkbox square (why the hell does this always need to be done here?)
                e2->setSizeX(e2->getSize().y);

                // button
                e1->setRelPos(sideMargin, yCounter);
                e1->setSizeX(elementWidth - e2->getSize().x - 2 * spacing);

                //  checkbox
                e2->setRelPos(sideMargin + e1->getSize().x + spacing, yCounter);

                yCounter += e1->getSize().y;
            } else if(labelPointer != nullptr) {
                // Labeled textbox
                e1->setRelPos(sideMargin, yCounter);
                e1->setSizeX(elementWidth);
                yCounter += e1->getSize().y;
                e2->setRelPos(sideMargin, yCounter);
                e2->setSizeX(elementWidth);
                yCounter += e2->getSize().y;
            } else {
                float dividerEnd = 1.0f / 2.0f;
                float dividerBegin = 1.0f - dividerEnd;

                e1->setRelPos(sideMargin + sideMarginAdd, yCounter);
                e1->setSizeX(elementWidth * dividerBegin - spacing);

                e2->setRelPos(sideMargin + e1->getSize().x + 2 * spacing, yCounter);
                e2->setSizeX(elementWidth * dividerEnd - spacing);

                yCounter += e1->getSize().y;
            }
        } else if(this->elemContainers[i]->baseElems.size() == 3) {
            CBaseUIElement *e1 = this->elemContainers[i]->baseElems[0];
            CBaseUIElement *e2 = this->elemContainers[i]->baseElems[1];
            CBaseUIElement *e3 = this->elemContainers[i]->baseElems[2];

            if(this->elemContainers[i]->type == BTN) {
                const int buttonButtonLabelOffset = 10 * dpiScale;

                const int buttonSize = elementWidth / 3 - 2 * buttonButtonLabelOffset;

                auto *button1Pointer = dynamic_cast<CBaseUIButton *>(e1);
                if(button1Pointer != nullptr)
                    button1Pointer->onResized();  // HACKHACK: framework, setSize*() does not update string metrics

                auto *button2Pointer = dynamic_cast<CBaseUIButton *>(e2);
                if(button2Pointer != nullptr)
                    button2Pointer->onResized();  // HACKHACK: framework, setSize*() does not update string metrics

                e1->setSizeX(buttonSize);
                e2->setSizeX(buttonSize);
                e3->setSizeX(buttonSize);

                e1->setRelPos(sideMargin, yCounter);
                e2->setRelPos(e1->getRelPos().x + e1->getSize().x + buttonButtonLabelOffset, yCounter);
                e3->setRelPos(e2->getRelPos().x + e2->getSize().x + buttonButtonLabelOffset, yCounter);
            } else {
                const int labelSliderLabelOffset = 15 * dpiScale;

                // this is a big mess, because some elements rely on fixed initial widths from default strings, combined
                // with variable font dpi on startup, will clean up whenever
                auto *label1Pointer = dynamic_cast<CBaseUILabel *>(e1);
                if(label1Pointer != nullptr) {
                    label1Pointer->onResized();  // HACKHACK: framework, setSize*() does not update string metrics
                    if(this->elemContainers[i]->label1Width > 0.0f)
                        label1Pointer->setSizeX(this->elemContainers[i]->label1Width * dpiScale);
                    else
                        label1Pointer->setSizeX(label1Pointer->getRelSize().x *
                                                (96.0f / this->elemContainers[i]->relSizeDPI) * dpiScale);
                }

                auto *sliderPointer = dynamic_cast<CBaseUISlider *>(e2);
                if(sliderPointer != nullptr) sliderPointer->setBlockSize(20 * dpiScale, 20 * dpiScale);

                auto *label2Pointer = dynamic_cast<CBaseUILabel *>(e3);
                if(label2Pointer != nullptr) {
                    label2Pointer->onResized();  // HACKHACK: framework, setSize*() does not update string metrics
                    label2Pointer->setSizeX(label2Pointer->getRelSize().x *
                                            (96.0f / this->elemContainers[i]->relSizeDPI) * dpiScale);
                }

                int sliderSize = elementWidth - e1->getSize().x - e3->getSize().x;
                if(sliderSize < 100) {
                    enableHorizontalScrolling = true;
                    sliderSize = 100;
                }

                e1->setRelPos(sideMargin + elementTextStartOffset, yCounter);

                e2->setRelPos(e1->getRelPos().x + e1->getSize().x + labelSliderLabelOffset, yCounter);
                e2->setSizeX(sliderSize - 2 * elementTextStartOffset - labelSliderLabelOffset * 2);

                e3->setRelPos(e2->getRelPos().x + e2->getSize().x + labelSliderLabelOffset, yCounter);
            }

            yCounter += e2->getSize().y;
        }

        yCounter += elementSpacing;

        switch(this->elemContainers[i]->type) {
            case SPCR:
                yCounter += spaceSpacing;
                break;
            case SECT:
                yCounter += sectionSpacing;
                break;
            case SUBSECT:
                yCounter += subsectionSpacing;
                break;
            default:
                break;
        }
    }
    this->spacer->setPosY(yCounter);
    this->options->container->addBaseUIElement(this->spacer);

    this->options->setScrollSizeToContent();
    if(!enableHorizontalScrolling) this->options->scrollToLeft();
    this->options->setHorizontalScrolling(enableHorizontalScrolling);

    this->options->container->addBaseUIElement(this->contextMenu);

    this->options->container->update_pos();

    // TODO: wrong? look at button borders when hovering... ew...
    f32 sidebarHeight = this->categories->getSize().y - this->backButton->getSize().y;
    i32 categoryPaddingTopBottom = sidebarHeight * 0.15f;
    i32 categoryHeight = (sidebarHeight - categoryPaddingTopBottom * 2) / this->categoryButtons.size();
    for(int i = 0; i < this->categoryButtons.size(); i++) {
        OptionsMenuCategoryButton *category = this->categoryButtons[i];
        category->onResized();  // HACKHACK: framework, setSize*() does not update string metrics
        category->setRelPosY(categoryPaddingTopBottom + categoryHeight * i);
        category->setSize(this->categories->getSize().x - 1, categoryHeight);
    }
    this->categories->container->update_pos();
    this->categories->setScrollSizeToContent();

    this->update_pos();

    this->updating_layout = false;
}

void OptionsMenu::onBack() {
    osu->getNotificationOverlay()->stopWaitingForKey();

    this->save();

    if(this->bFullscreen)
        osu->toggleOptionsMenu();
    else
        this->setVisibleInt(false, true);
}

void OptionsMenu::scheduleSearchUpdate() {
    this->updateLayout();
    this->options->scrollToTop();
    this->search->setSearchString(this->sSearchString);

    if(this->contextMenu->isVisible()) this->contextMenu->setVisible2(false);
}

void OptionsMenu::askForLoginDetails() {
    this->setVisible(true);
    this->options->scrollToElement(this->sectionGeneral, 0, 100 * osu->getUIScale());
    this->nameTextbox->focus();
}

void OptionsMenu::updateFposuDPI() {
    if(this->dpiTextbox == nullptr) return;

    this->dpiTextbox->stealFocus();

    const UString &text = this->dpiTextbox->getText();
    UString value;
    for(int i = 0; i < text.length(); i++) {
        if(text[i] == L',')
            value.append(L'.');
        else
            value.append(text[i]);
    }
    cv::fposu_mouse_dpi.setValue(value);
}

void OptionsMenu::updateFposuCMper360() {
    if(this->cm360Textbox == nullptr) return;

    this->cm360Textbox->stealFocus();

    const UString &text = this->cm360Textbox->getText();
    UString value;
    for(int i = 0; i < text.length(); i++) {
        if(text[i] == L',')
            value.append(L'.');
        else
            value.append(text[i]);
    }
    cv::fposu_mouse_cm_360.setValue(value);
}

void OptionsMenu::updateSkinNameLabel() {
    if(this->skinLabel == nullptr) return;

    this->skinLabel->setText(cv::skin.getString().c_str());
    this->skinLabel->setTextColor(0xffffffff);
}

void OptionsMenu::updateNotelockSelectLabel() {
    if(this->notelockSelectLabel == nullptr) return;

    this->notelockSelectLabel->setText(
        this->notelockTypes[std::clamp<int>(cv::notelock_type.getInt(), 0, this->notelockTypes.size() - 1)]);
}

void OptionsMenu::onFullscreenChange(CBaseUICheckbox *checkbox) {
    if(checkbox->isChecked())
        env->enableFullscreen();
    else
        env->disableFullscreen();

    // and update reset button as usual
    for(auto &element : this->elemContainers) {
        for(int e = 0; e < element->baseElems.size(); e++) {
            if(element->baseElems[e] == checkbox) {
                this->onResetUpdate(element->resetButton);

                break;
            }
        }
    }
}

void OptionsMenu::onDPIScalingChange(CBaseUICheckbox *checkbox) {
    for(auto &element : this->elemContainers) {
        for(int e = 0; e < element->baseElems.size(); e++) {
            if(element->baseElems[e] == checkbox) {
                const float prevUIScale = Osu::getUIScale();

                auto *cv = element->cvars[element->baseElems[e]];
                if(cv != nullptr) {
                    cv->setValue(checkbox->isChecked());
                }

                this->onResetUpdate(element->resetButton);

                if(Osu::getUIScale() != prevUIScale) this->bDPIScalingScrollToSliderScheduled = true;

                break;
            }
        }
    }
}

void OptionsMenu::onRawInputToAbsoluteWindowChange(CBaseUICheckbox *checkbox) {
    for(auto &element : this->elemContainers) {
        for(int e = 0; e < element->baseElems.size(); e++) {
            if(element->baseElems[e] == checkbox) {
                auto *cv = element->cvars[element->baseElems[e]];
                if(cv != nullptr) {
                    cv->setValue(checkbox->isChecked());
                }

                this->onResetUpdate(element->resetButton);

                // special case: this requires a virtual mouse offset update, but since it is an engine convar we can't
                // use callbacks
                osu->updateMouseSettings();

                break;
            }
        }
    }
}

void OptionsMenu::openCurrentSkinFolder() {
    auto current_skin = cv::skin.getString();
    if(strcasecmp(current_skin.c_str(), "default") == 0) {
        env->openFileBrowser(MCENGINE_IMAGES_PATH "/default");
    } else {
        std::string neosuSkinFolder = fmt::format(NEOSU_SKINS_PATH "/{}", current_skin);
        if(env->directoryExists(neosuSkinFolder)) {
            env->openFileBrowser(neosuSkinFolder);
        } else {
            std::string currentSkinFolder =
                fmt::format("{}{}{}", cv::osu_folder.getString(), cv::osu_folder_sub_skins.getString(), current_skin);
            env->openFileBrowser(currentSkinFolder);
        }
    }
}

void OptionsMenu::onSkinSelect() {
    // XXX: Instead of a dropdown, we should make a dedicated skin select screen with search bar

    // Just close the skin selection menu if it's already open
    if(this->contextMenu->isVisible()) {
        this->contextMenu->setVisible2(false);
        return;
    }

    if(osu->isSkinLoading()) return;

    cv::osu_folder.setValue(this->osuFolderTextbox->getText());
    std::string skinFolder{cv::osu_folder.getString()};
    skinFolder.append("/");
    skinFolder.append(cv::osu_folder_sub_skins.getString());

    std::vector<std::string> skinFolders;
    for(const auto &dir : {env->getFoldersInFolder(NEOSU_SKINS_PATH "/"), env->getFoldersInFolder(skinFolder)}) {
        for(const auto &skin : dir) {
            skinFolders.push_back(skin);
        }
    }

    if(cv::sort_skins_cleaned.getBool()) {
        // Sort skins only by alphanum characters, ignore the others
        std::ranges::sort(skinFolders, SString::alnum_comp);
    } else {
        // more stable-like sorting (i.e. "-     Cookiezi" comes before "Cookiezi")
        std::ranges::sort(
            skinFolders, [](const char *s1, const char *s2) { return strcasecmp(s1, s2) < 0; },
            [](const std::string &str) -> const char * { return str.c_str(); });
    }

    if(skinFolders.size() > 0) {
        if(this->bVisible) {
            this->contextMenu->setPos(this->skinSelectLocalButton->getPos());
            this->contextMenu->setRelPos(this->skinSelectLocalButton->getRelPos());
            this->options->setScrollSizeToContent();
        } else {
            // Put it 50px from top, we'll move it later
            this->contextMenu->setPos(vec2{0, 100});
        }

        this->contextMenu->begin();

        CBaseUIButton *buttonDefault = this->contextMenu->addButton("default");
        if(cv::skin.getString() == "default") buttonDefault->setTextBrightColor(0xff00ff00);

        for(const auto &skinFolder : skinFolders) {
            if(skinFolder.compare(".") == 0 || skinFolder.compare("..") == 0) continue;

            CBaseUIButton *button = this->contextMenu->addButton(skinFolder.c_str());
            if(skinFolder.compare(cv::skin.getString()) == 0) button->setTextBrightColor(0xff00ff00);
        }
        this->contextMenu->end(false, !this->bVisible);
        this->contextMenu->setClickCallback(SA::MakeDelegate<&OptionsMenu::onSkinSelect2>(this));

        if(!this->bVisible) {
            // Center context menu
            this->contextMenu->setPos(vec2{
                osu->getVirtScreenWidth() / 2.f - this->contextMenu->getSize().x / 2.f,
                osu->getVirtScreenHeight() / 2.f - this->contextMenu->getSize().y / 2.f,
            });
        }
    } else {
        osu->getNotificationOverlay()->addToast(ULITERAL("Error: Couldn't find any skins"), ERROR_TOAST);
        this->options->scrollToTop();
        this->fOsuFolderTextboxInvalidAnim = engine->getTime() + 3.0f;
    }
}

void OptionsMenu::onSkinSelect2(const UString &skinName, int /*id*/) {
    cv::skin.setValue(skinName);
    this->updateSkinNameLabel();
}

void OptionsMenu::onSkinReload() { osu->reloadSkin(); }

void OptionsMenu::onSkinRandom() {
    const bool isRandomSkinEnabled = cv::skin_random.getBool();

    if(!isRandomSkinEnabled) cv::skin_random.setValue(1.0f);

    osu->reloadSkin();

    if(!isRandomSkinEnabled) cv::skin_random.setValue(0.0f);
}

void OptionsMenu::onResolutionSelect() {
    std::vector<ivec2> resolutions{{800, 600},  // 4:3
                                   {1024, 768},  {1152, 864},  {1280, 960},  {1280, 1024}, {1600, 1200}, {1920, 1440},
                                   {2560, 1920}, {1024, 600},  // 16:9 and 16:10
                                   {1280, 720},  {1280, 768},  {1280, 800},  {1360, 768},  {1366, 768},  {1440, 900},
                                   {1600, 900},  {1600, 1024}, {1680, 1050}, {1920, 1080}, {1920, 1200}, {2560, 1440},
                                   {2560, 1600}, {3840, 2160}, {5120, 2880}, {7680, 4320}, {4096, 2160}};  // wtf

    // get custom resolutions
    std::vector<ivec2> customResolutions;
    {
        File customres(MCENGINE_CFG_PATH "/customres.cfg");
        for(auto line = customres.readLine(); customres.canRead(); line = customres.readLine()) {
            if(line.starts_with("//") || line.starts_with('#')) continue;  // ignore comments
            auto parsed = Parsing::parse_resolution(line);
            if(parsed.has_value()) {
                customResolutions.emplace_back(parsed->x, parsed->y);
            }
        }
    }

    // native resolution at the end
    ivec2 nativeResolution = env->getNativeScreenSize();
    bool containsNativeResolution = false;
    for(auto resolution : resolutions) {
        if(resolution == nativeResolution) {
            containsNativeResolution = true;
            break;
        }
    }
    if(!containsNativeResolution) resolutions.push_back(nativeResolution);

    // build context menu
    this->contextMenu->begin();
    for(const auto &i : resolutions) {
        if(i.x > nativeResolution.x || i.y > nativeResolution.y) continue;

        const auto resolution = fmt::format("{}x{}", i.x, i.y);
        CBaseUIButton *button = this->contextMenu->addButton(resolution);
        if(this->resolutionLabel != nullptr && resolution == this->resolutionLabel->getText())
            button->setTextBrightColor(0xff00ff00);
    }
    for(auto &customResolution : customResolutions) {
        this->contextMenu->addButton(fmt::format("{}x{}", customResolution.x, customResolution.y));
    }
    this->contextMenu->end(false, false);
    this->contextMenu->setClickCallback(SA::MakeDelegate<&OptionsMenu::onResolutionSelect2>(this));

    // reposition context menu
    f32 menu_width = this->contextMenu->getSize().x;
    f32 btn_width = this->resolutionSelectButton->getSize().x;
    f32 menu_offset = btn_width / 2.f - menu_width / 2.f;
    this->contextMenu->setPos(this->resolutionSelectButton->getPos().x + menu_offset,
                              this->resolutionSelectButton->getPos().y);
    this->contextMenu->setRelPos(this->resolutionSelectButton->getRelPos().x + menu_offset,
                                 this->resolutionSelectButton->getRelPos().y);
    this->options->setScrollSizeToContent();
}

void OptionsMenu::onResolutionSelect2(const UString &resolution, int /*id*/) {
    const bool win_fs = env->winFullscreened();
    if(win_fs && cv::letterboxing.getBool()) {
        cv::letterboxed_resolution.setValue(resolution);
    } else if(win_fs) {
        cv::resolution.setValue(resolution);
    } else {
        cv::windowed_resolution.setValue(resolution);
    }
}

void OptionsMenu::onOutputDeviceSelect() {
    // Just close the device selection menu if it's already open
    if(this->contextMenu->isVisible()) {
        this->contextMenu->setVisible2(false);
        return;
    }

    // build context menu
    this->contextMenu->setPos(this->outputDeviceSelectButton->getPos());
    this->contextMenu->setRelPos(this->outputDeviceSelectButton->getRelPos());
    this->contextMenu->begin();
    for(const auto &device : soundEngine->getOutputDevices()) {
        CBaseUIButton *button = this->contextMenu->addButton(device.name);
        if(device.name == soundEngine->getOutputDeviceName()) button->setTextBrightColor(0xff00ff00);
    }
    this->contextMenu->end(false, true);
    this->contextMenu->setClickCallback(SA::MakeDelegate<&OptionsMenu::onOutputDeviceSelect2>(this));
    this->options->setScrollSizeToContent();
}

void OptionsMenu::onOutputDeviceSelect2(const UString &outputDeviceName, int /*id*/) {
    if(outputDeviceName == soundEngine->getOutputDeviceName()) {
        if(this->outputDeviceLabel != nullptr) this->outputDeviceLabel->setText(soundEngine->getOutputDeviceName());
        debugLog("SoundEngine::setOutputDevice() \"{:s}\" already is the current device.", outputDeviceName.toUtf8());
        return;
    }

    for(const auto &device : soundEngine->getOutputDevices()) {
        if(device.name != outputDeviceName) continue;

        soundEngine->setOutputDevice(device);
        if((soundEngine->getOutputDriverType() == SoundEngine::OutputDriver::SOLOUD_MA)) {
            if(device.name.find("(Exclusive)") != -1 && soundEngine->getOutputDeviceName().find("(Exclusive)") == -1) {
                osu->getNotificationOverlay()->addToast("Tried to enable exclusive mode, but couldn't :(", ERROR_TOAST);
            }
        }
        if(this->outputDeviceLabel != nullptr) this->outputDeviceLabel->setText(soundEngine->getOutputDeviceName());
        return;
    }

    debugLog("SoundEngine::setOutputDevice() couldn't find output device \"{:s}\"!", outputDeviceName.toUtf8());
}

void OptionsMenu::onOutputDeviceResetClicked() { soundEngine->setOutputDevice(soundEngine->getDefaultDevice()); }

void OptionsMenu::onOutputDeviceResetUpdate() {
    if(this->outputDeviceResetButton != nullptr) {
        this->outputDeviceResetButton->setEnabled(soundEngine->getOutputDeviceName() !=
                                                  soundEngine->getDefaultDevice().name);
    }
}

void OptionsMenu::onOutputDeviceRestart() { soundEngine->restart(); }

void OptionsMenu::onLogInClicked(bool left, bool right) {
    if(left && BanchoState::is_logging_in()) {
        return;
    }
    soundEngine->play(osu->getSkin()->s_menu_hit);

    // Clear mp_oauth_token if the user is connecting to a non-oauth server
    // This makes it easy to check what we need to do when building the login packet
    if(!this->should_use_oauth_login()) {
        cv::mp_oauth_token.setValue("");
    }

    if((right && BanchoState::is_logging_in()) || BanchoState::is_online()) {
        BanchoState::disconnect();

        // Manually clicked disconnect button: clear oauth token
        cv::mp_oauth_token.setValue("");
    } else {
        if(this->should_use_oauth_login() && cv::mp_oauth_token.getString().empty()) {
            BanchoState::endpoint = cv::mp_server.getString();

            crypto::rng::get_rand(BanchoState::oauth_verifier);
            crypto::hash::sha256(&BanchoState::oauth_verifier[0], 32, &BanchoState::oauth_challenge[0]);

            auto challenge_b64 = env->urlEncode(crypto::conv::encode64(BanchoState::oauth_challenge));
            auto url = fmt::format("https://{}/connect/start?challenge={}", BanchoState::endpoint, challenge_b64);

            env->openURLInDefaultBrowser(url);
        } else {
            BanchoState::reconnect();
        }
    }
}

void OptionsMenu::onCM360CalculatorLinkClicked() {
    osu->getNotificationOverlay()->addNotification("Opening browser, please wait ...", 0xffffffff, false, 0.75f);
    env->openURLInDefaultBrowser("https://www.mouse-sensitivity.com/");
}

void OptionsMenu::onNotelockSelect() {
    // build context menu
    this->contextMenu->setPos(this->notelockSelectButton->getPos());
    this->contextMenu->setRelPos(this->notelockSelectButton->getRelPos());
    this->contextMenu->begin(this->notelockSelectButton->getSize().x);
    {
        for(int i = 0; i < this->notelockTypes.size(); i++) {
            CBaseUIButton *button = this->contextMenu->addButton(this->notelockTypes[i], i);
            if(i == cv::notelock_type.getInt()) button->setTextBrightColor(0xff00ff00);
        }
    }
    this->contextMenu->end(false, false);
    this->contextMenu->setClickCallback(SA::MakeDelegate<&OptionsMenu::onNotelockSelect2>(this));
    this->options->setScrollSizeToContent();
}

void OptionsMenu::onNotelockSelect2(const UString & /*notelockType*/, int id) {
    cv::notelock_type.setValue(id);
    this->updateNotelockSelectLabel();

    // and update the reset button as usual
    this->onNotelockSelectResetUpdate();
}

void OptionsMenu::onNotelockSelectResetClicked() {
    if(this->notelockTypes.size() > 1 && (size_t)cv::notelock_type.getDefaultFloat() < this->notelockTypes.size())
        this->onNotelockSelect2(this->notelockTypes[(size_t)cv::notelock_type.getDefaultFloat()],
                                (int)cv::notelock_type.getDefaultFloat());
}

void OptionsMenu::onNotelockSelectResetUpdate() {
    if(this->notelockSelectResetButton != nullptr)
        this->notelockSelectResetButton->setEnabled(cv::notelock_type.getInt() !=
                                                    (int)cv::notelock_type.getDefaultFloat());
}

// insane macros, don't think about it too much
#define FOR_EACH_ELEMENT_MATCHING(param__)                                                     \
    OPTIONS_ELEMENT *element = nullptr;                                                        \
    auto findfunc = []<typename T>(T &&param, const auto &begin, const auto &end) {            \
        return std::find_if(begin, end, [&](const auto &optElem) {                             \
            return std::ranges::contains(optElem->baseElems, std::forward<T>(param));          \
        });                                                                                    \
    };                                                                                         \
    for(auto it = findfunc(param__, this->elemContainers.begin(), this->elemContainers.end()); \
        it != this->elemContainers.end() && (element = *it) != nullptr;                        \
        it = findfunc(param__, std::next(it), this->elemContainers.end()))

#define DO_UPDATE_LAYOUT_CHECK(slider__)                               \
    if(!this->updating_layout && !(slider__)->isBusy() &&              \
       static_cast<UISlider *>(slider__)->getUpdateRelPosOnChange()) { \
        this->scheduleLayoutUpdate();                                  \
    }

void OptionsMenu::onCheckboxChange(CBaseUICheckbox *checkbox) {
    FOR_EACH_ELEMENT_MATCHING(checkbox) {
        auto *cv = element->cvars[checkbox];
        if(cv != nullptr) {
            cv->setValue(checkbox->isChecked());
        }

        this->onResetUpdate(element->resetButton);
    }
}

void OptionsMenu::onSliderChange(CBaseUISlider *slider) {
    FOR_EACH_ELEMENT_MATCHING(slider) {
        auto *cv = element->cvars[slider];
        if(cv != nullptr) {
            cv->setValue(std::round(slider->getFloat() * 100.0f) / 100.0f);  // round to 2 decimal places

            if(element->baseElems.size() == 3) {
                auto *labelPointer = dynamic_cast<CBaseUILabel *>(element->baseElems[2]);
                labelPointer->setText(cv->getString().c_str());
            }
        }

        this->onResetUpdate(element->resetButton);
        DO_UPDATE_LAYOUT_CHECK(slider);
    }
}

void OptionsMenu::onFPSSliderChange(CBaseUISlider *slider) {
    FOR_EACH_ELEMENT_MATCHING(slider) {
        auto *cv = element->cvars[slider];
        if(cv != nullptr) {
            if(slider->getFloat() < 60.f) {
                cv->setValue(0.f);
                if(element->baseElems.size() == 3) {
                    auto *labelPointer = dynamic_cast<CBaseUILabel *>(element->baseElems[2]);
                    labelPointer->setText("∞");
                }
            } else {
                cv->setValue(std::round(slider->getFloat()));  // round to int
                if(element->baseElems.size() == 3) {
                    auto *labelPointer = dynamic_cast<CBaseUILabel *>(element->baseElems[2]);
                    labelPointer->setText(cv->getString().c_str());
                }
            }
        }
        this->onResetUpdate(element->resetButton);
        DO_UPDATE_LAYOUT_CHECK(slider);
    }
}

void OptionsMenu::onSliderChangeOneDecimalPlace(CBaseUISlider *slider) {
    FOR_EACH_ELEMENT_MATCHING(slider) {
        auto *cv = element->cvars[slider];
        if(cv != nullptr) {
            cv->setValue(std::round(slider->getFloat() * 10.0f) / 10.0f);  // round to 1 decimal place

            if(element->baseElems.size() == 3) {
                auto *labelPointer = dynamic_cast<CBaseUILabel *>(element->baseElems[2]);
                labelPointer->setText(cv->getString().c_str());
            }
        }
        this->onResetUpdate(element->resetButton);
        DO_UPDATE_LAYOUT_CHECK(slider);
    }
}

void OptionsMenu::onSliderChangeTwoDecimalPlaces(CBaseUISlider *slider) {
    FOR_EACH_ELEMENT_MATCHING(slider) {
        auto *cv = element->cvars[slider];
        if(cv != nullptr) {
            cv->setValue(std::round(slider->getFloat() * 100.0f) / 100.0f);  // round to 2 decimal places

            if(element->baseElems.size() == 3) {
                auto *labelPointer = dynamic_cast<CBaseUILabel *>(element->baseElems[2]);
                labelPointer->setText(cv->getString().c_str());
            }
        }
        this->onResetUpdate(element->resetButton);
        DO_UPDATE_LAYOUT_CHECK(slider);
    }
}

void OptionsMenu::onSliderChangeOneDecimalPlaceMeters(CBaseUISlider *slider) {
    FOR_EACH_ELEMENT_MATCHING(slider) {
        auto *cv = element->cvars[slider];
        if(cv != nullptr) {
            cv->setValue(std::round(slider->getFloat() * 10.0f) / 10.0f);  // round to 1 decimal place

            if(element->baseElems.size() == 3) {
                auto *labelPointer = dynamic_cast<CBaseUILabel *>(element->baseElems[2]);
                labelPointer->setText(UString::format("%.1f m", cv->getFloat()));
            }
        }
        this->onResetUpdate(element->resetButton);
        DO_UPDATE_LAYOUT_CHECK(slider);
    }
}

void OptionsMenu::onSliderChangeInt(CBaseUISlider *slider) {
    FOR_EACH_ELEMENT_MATCHING(slider) {
        auto *cv = element->cvars[slider];
        if(cv != nullptr) {
            if(cv != nullptr) cv->setValue(std::round(slider->getFloat()));  // round to int

            if(element->baseElems.size() == 3) {
                auto *labelPointer = dynamic_cast<CBaseUILabel *>(element->baseElems[2]);
                labelPointer->setText(cv->getString().c_str());
            }
        }
        this->onResetUpdate(element->resetButton);
        DO_UPDATE_LAYOUT_CHECK(slider);
    }
}

void OptionsMenu::onSliderChangeIntMS(CBaseUISlider *slider) {
    FOR_EACH_ELEMENT_MATCHING(slider) {
        auto *cv = element->cvars[slider];
        if(cv != nullptr) {
            cv->setValue(std::round(slider->getFloat()));  // round to int

            if(element->baseElems.size() == 3) {
                auto *labelPointer = dynamic_cast<CBaseUILabel *>(element->baseElems[2]);
                std::string text = cv->getString();
                text.append(" ms");
                labelPointer->setText(text.c_str());
            }
        }
        this->onResetUpdate(element->resetButton);
        DO_UPDATE_LAYOUT_CHECK(slider);
    }
}

void OptionsMenu::onSliderChangeFloatMS(CBaseUISlider *slider) {
    FOR_EACH_ELEMENT_MATCHING(slider) {
        auto *cv = element->cvars[slider];
        if(cv != nullptr) {
            cv->setValue(slider->getFloat());

            if(element->baseElems.size() == 3) {
                auto *labelPointer = dynamic_cast<CBaseUILabel *>(element->baseElems[2]);
                UString text = UString::format("%i", (int)std::round(cv->getFloat() * 1000.0f));
                text.append(" ms");
                labelPointer->setText(text);
            }
        }
        this->onResetUpdate(element->resetButton);
        DO_UPDATE_LAYOUT_CHECK(slider);
    }
}

void OptionsMenu::onSliderChangePercent(CBaseUISlider *slider) {
    FOR_EACH_ELEMENT_MATCHING(slider) {
        auto *cv = element->cvars[slider];
        if(cv != nullptr) {
            cv->setValue(std::round(slider->getFloat() * 100.0f) / 100.0f);

            if(element->baseElems.size() == 3) {
                int percent = std::round(cv->getFloat() * 100.0f);

                auto *labelPointer = dynamic_cast<CBaseUILabel *>(element->baseElems[2]);
                labelPointer->setText(UString::format("%i%%", percent));
            }
        }
        this->onResetUpdate(element->resetButton);
        DO_UPDATE_LAYOUT_CHECK(slider);
    }
}

void OptionsMenu::onKeyBindingButtonPressed(CBaseUIButton *button) {
    FOR_EACH_ELEMENT_MATCHING(button) {
        auto *cv = element->cvars[button];
        if(cv != nullptr) {
            this->waitingKey = cv;

            UString notificationText = "Press new key for ";
            notificationText.append(button->getText());
            notificationText.append(":");

            const bool waitForKey = true;
            osu->getNotificationOverlay()->addNotification(notificationText, 0xffffffff, waitForKey);
            osu->getNotificationOverlay()->setDisallowWaitForKeyLeftClick(
                !(dynamic_cast<OptionsMenuKeyBindButton *>(button)->isLeftMouseClickBindingAllowed()));
        }
    }
}

void OptionsMenu::onKeyUnbindButtonPressed(CBaseUIButton *button) {
    soundEngine->play(osu->getSkin()->s_check_off);

    FOR_EACH_ELEMENT_MATCHING(button) {
        auto *cv = element->cvars[button];
        if(cv != nullptr) {
            cv->setValue(0.0f);
        }
    }
}

void OptionsMenu::onKeyBindingsResetAllPressed(CBaseUIButton * /*button*/) {
    this->iNumResetAllKeyBindingsPressed++;

    const int numRequiredPressesUntilReset = 4;
    const int remainingUntilReset = numRequiredPressesUntilReset - this->iNumResetAllKeyBindingsPressed;

    if(this->iNumResetAllKeyBindingsPressed > (numRequiredPressesUntilReset - 1)) {
        this->iNumResetAllKeyBindingsPressed = 0;

        for(ConVar *bind : KeyBindings::ALL) {
            bind->setValue(bind->getDefaultFloat());
        }

        osu->getNotificationOverlay()->addNotification("All key bindings have been reset.", 0xff00ff00);
    } else {
        if(remainingUntilReset > 1)
            osu->getNotificationOverlay()->addNotification(
                UString::format("Press %i more times to confirm.", remainingUntilReset));
        else
            osu->getNotificationOverlay()->addNotification(
                UString::format("Press %i more time to confirm!", remainingUntilReset), 0xffffff00);
    }
}

void OptionsMenu::onSliderChangeSliderQuality(CBaseUISlider *slider) {
    FOR_EACH_ELEMENT_MATCHING(slider) {
        auto *cv = element->cvars[slider];
        if(cv != nullptr) {
            cv->setValue(std::round(slider->getFloat() * 100.0f) / 100.0f);  // round to 2 decimal places
        }

        if(element->baseElems.size() == 3) {
            auto *labelPointer = dynamic_cast<CBaseUILabel *>(element->baseElems[2]);

            int percent = std::round((slider->getPercent()) * 100.0f);
            UString text = UString::format(percent > 49 ? "%i !" : "%i", percent);
            labelPointer->setText(text);
        }

        this->onResetUpdate(element->resetButton);
        DO_UPDATE_LAYOUT_CHECK(slider);
    }
}

void OptionsMenu::onSliderChangeLetterboxingOffset(CBaseUISlider *slider) {
    this->bLetterboxingOffsetUpdateScheduled = true;

    FOR_EACH_ELEMENT_MATCHING(slider) {
        const float newValue = std::round(slider->getFloat() * 100.0f) / 100.0f;

        if(element->baseElems.size() == 3) {
            const int percent = std::round(newValue * 100.0f);

            auto *labelPointer = dynamic_cast<CBaseUILabel *>(element->baseElems[2]);
            labelPointer->setText(UString::format("%i%%", percent));
        }

        this->letterboxingOffsetResetButton = element->resetButton;  // HACKHACK: disgusting

        this->onResetUpdate(element->resetButton);
        DO_UPDATE_LAYOUT_CHECK(slider);
    }
}

void OptionsMenu::onSliderChangeUIScale(CBaseUISlider *slider) {
    this->bUIScaleChangeScheduled = true;

    FOR_EACH_ELEMENT_MATCHING(slider) {
        const float newValue = std::round(slider->getFloat() * 100.0f) / 100.0f;

        if(element->baseElems.size() == 3) {
            const int percent = std::round(newValue * 100.0f);

            auto *labelPointer = dynamic_cast<CBaseUILabel *>(element->baseElems[2]);
            labelPointer->setText(UString::format("%i%%", percent));
        }

        this->uiScaleResetButton = element->resetButton;  // HACKHACK: disgusting

        this->onResetUpdate(element->resetButton);
        DO_UPDATE_LAYOUT_CHECK(slider);
    }
}

void OptionsMenu::OpenASIOSettings() {
#if defined(MCENGINE_PLATFORM_WINDOWS) && defined(MCENGINE_FEATURE_BASS)
    if(soundEngine->getTypeId() != SoundEngine::SndEngineType::BASS) return;
    BASS_ASIO_ControlPanel();
#endif
}

void OptionsMenu::onASIOBufferChange([[maybe_unused]] CBaseUISlider *slider) {
#if defined(MCENGINE_PLATFORM_WINDOWS) && defined(MCENGINE_FEATURE_BASS)
    if(soundEngine->getTypeId() != SoundEngine::SndEngineType::BASS) return;
    if(!this->updating_layout) this->bASIOBufferChangeScheduled = true;

    BASS_ASIO_INFO info{};
    BASS_ASIO_GetInfo(&info);
    cv::asio_buffer_size.setDefaultDouble(info.bufpref);
    slider->setBounds(info.bufmin, info.bufmax);
    slider->setKeyDelta(info.bufgran == -1 ? info.bufmin : info.bufgran);

    u32 bufsize = slider->getInt();
    bufsize = BassSoundEngine::ASIO_clamp(info, bufsize);
    double latency = 1000.0 * (double)bufsize / std::max(BASS_ASIO_GetRate(), 44100.0);

    FOR_EACH_ELEMENT_MATCHING(slider) {
        if(element->baseElems.size() == 3) {
            auto *labelPointer = dynamic_cast<CBaseUILabel *>(element->baseElems[2]);
            if(labelPointer) {
                UString text = UString::format("%.1f ms", latency);
                labelPointer->setText(text);
            }
        }

        this->asioBufferSizeResetButton = element->resetButton;  // HACKHACK: disgusting
        DO_UPDATE_LAYOUT_CHECK(slider);
    }
#endif
}

void OptionsMenu::onWASAPIBufferChange(CBaseUISlider *slider) {
    if(!this->updating_layout) this->bWASAPIBufferChangeScheduled = true;

    FOR_EACH_ELEMENT_MATCHING(slider) {
        if(element->baseElems.size() == 3) {
            auto *labelPointer = dynamic_cast<CBaseUILabel *>(element->baseElems[2]);
            if(labelPointer) {
                UString text = UString::format("%i", (int)std::round(slider->getFloat() * 1000.0f));
                text.append(" ms");
                labelPointer->setText(text);
            }
        }

        this->wasapiBufferSizeResetButton = element->resetButton;  // HACKHACK: disgusting
        DO_UPDATE_LAYOUT_CHECK(slider);
    }
}

void OptionsMenu::onWASAPIPeriodChange(CBaseUISlider *slider) {
    if(!this->updating_layout) this->bWASAPIPeriodChangeScheduled = true;

    FOR_EACH_ELEMENT_MATCHING(slider) {
        if(element->baseElems.size() == 3) {
            auto *labelPointer = dynamic_cast<CBaseUILabel *>(element->baseElems[2]);
            if(labelPointer) {
                UString text = UString::format("%i", (int)std::round(slider->getFloat() * 1000.0f));
                text.append(" ms");
                labelPointer->setText(text);
            }
        }

        this->wasapiPeriodSizeResetButton = element->resetButton;  // HACKHACK: disgusting
        DO_UPDATE_LAYOUT_CHECK(slider);
    }
}

void OptionsMenu::onLoudnessNormalizationToggle(CBaseUICheckbox *checkbox) {
    this->onCheckboxChange(checkbox);

    auto music = osu->getMapInterface()->getMusic();
    if(music != nullptr) {
        music->setBaseVolume(osu->getMapInterface()->getIdealVolume());
    }

    if(cv::normalize_loudness.getBool()) {
        VolNormalization::start_calc(db->loudness_to_calc);
    } else {
        VolNormalization::abort();
    }
}

void OptionsMenu::onModChangingToggle(CBaseUICheckbox *checkbox) {
    this->onCheckboxChange(checkbox);
    osu->getModSelector()->updateButtons();
    osu->updateMods();
}

void OptionsMenu::onUseSkinsSoundSamplesChange() { osu->reloadSkin(); }

void OptionsMenu::onHighQualitySlidersCheckboxChange(CBaseUICheckbox *checkbox) {
    this->onCheckboxChange(checkbox);

    // special case: if the checkbox is clicked and enabled via the UI, force set the quality to 100
    if(checkbox->isChecked()) this->sliderQualitySlider->setValue(1.0f, false);
}

void OptionsMenu::onHighQualitySlidersConVarChange(float newValue) {
    const bool enabled = newValue > 0;
    for(auto &element : this->elemContainers) {
        bool contains = false;
        for(auto &e : element->baseElems) {
            if(e == this->sliderQualitySlider) {
                contains = true;
                break;
            }
        }

        if(contains) {
            // HACKHACK: show/hide quality slider, this is ugly as fuck
            // TODO: containers use setVisible() on all elements. there needs to be a separate API for hiding elements
            // while inside any kind of container
            for(auto &e : element->baseElems) {
                e->setEnabled(enabled);

                auto *sliderPointer = dynamic_cast<UISlider *>(e);
                auto *labelPointer = dynamic_cast<CBaseUILabel *>(e);

                if(sliderPointer != nullptr) sliderPointer->setFrameColor(enabled ? 0xffffffff : 0xff000000);
                if(labelPointer != nullptr) labelPointer->setTextColor(enabled ? 0xffffffff : 0xff000000);
            }

            // reset value if disabled
            if(!enabled) {
                for(auto &cvm : element->cvars) {
                    auto *cv = cvm.second;
                    if(cv != nullptr) {
                        this->sliderQualitySlider->setValue(cv->getDefaultFloat(), false);
                        cv->setValue(cv->getDefaultFloat());
                    }
                }
            }

            this->onResetUpdate(element->resetButton);

            break;
        }
    }
}

void OptionsMenu::onCategoryClicked(CBaseUIButton *button) {
    // reset search
    this->sSearchString.clear();
    this->scheduleSearchUpdate();

    // scroll to category
    auto *categoryButton = dynamic_cast<OptionsMenuCategoryButton *>(button);
    if(categoryButton != nullptr)
        this->options->scrollToElement(categoryButton->getSection(), 0, 100 * Osu::getUIScale());
}

void OptionsMenu::onResetUpdate(CBaseUIButton *button) {
    if(button == nullptr) return;

    for(auto &element : this->elemContainers) {
        if(element->resetButton == button && element->cvars[element->resetButton] != nullptr) {
            auto *cv = element->cvars[element->resetButton];
            switch(element->type) {
                case CBX:
                    element->resetButton->setEnabled(cv->getBool() != (bool)cv->getDefaultFloat());
                    break;
                case SLDR:
                    element->resetButton->setEnabled(cv->getFloat() != cv->getDefaultFloat());
                    break;
                default:
                    break;
            }

            break;
        }
    }
}

void OptionsMenu::onResetClicked(CBaseUIButton *button) {
    if(button == nullptr) return;

    for(auto &element : this->elemContainers) {
        if(element->resetButton == button && element->cvars[element->resetButton] != nullptr) {
            auto *cv = element->cvars[element->resetButton];
            switch(element->type) {
                case CBX:
                    for(auto &e : element->baseElems) {
                        auto *checkboxPointer = dynamic_cast<CBaseUICheckbox *>(e);
                        if(checkboxPointer != nullptr) checkboxPointer->setChecked((bool)cv->getDefaultFloat());
                    }
                    break;
                case SLDR:
                    if(element->baseElems.size() == 3) {
                        auto *sliderPointer = dynamic_cast<CBaseUISlider *>(element->baseElems[1]);
                        if(sliderPointer != nullptr) {
                            sliderPointer->setValue(cv->getDefaultFloat(), false);
                            sliderPointer->fireChangeCallback();
                        }
                    }
                    break;
                default:
                    break;
            }

            break;
        }
    }

    this->onResetUpdate(button);
}

void OptionsMenu::onResetEverythingClicked(CBaseUIButton * /*button*/) {
    this->iNumResetEverythingPressed++;

    const int numRequiredPressesUntilReset = 4;
    const int remainingUntilReset = numRequiredPressesUntilReset - this->iNumResetEverythingPressed;

    if(this->iNumResetEverythingPressed > (numRequiredPressesUntilReset - 1)) {
        this->iNumResetEverythingPressed = 0;

        // first reset all settings
        for(auto &element : this->elemContainers) {
            OptionsMenuResetButton *resetButton = element->resetButton;
            if(resetButton != nullptr && resetButton->isEnabled()) resetButton->click();
        }

        // and then all key bindings (since these don't use the yellow reset button system)
        for(ConVar *bind : KeyBindings::ALL) {
            bind->setValue(bind->getDefaultFloat());
        }

        osu->getNotificationOverlay()->addNotification("All settings have been reset.", 0xff00ff00);
    } else {
        if(remainingUntilReset > 1)
            osu->getNotificationOverlay()->addNotification(
                UString::format("Press %i more times to confirm.", remainingUntilReset));
        else
            osu->getNotificationOverlay()->addNotification(
                UString::format("Press %i more time to confirm!", remainingUntilReset), 0xffffff00);
    }
}

void OptionsMenu::onImportSettingsFromStable(CBaseUIButton * /*button*/) {
    PeppyImporter::import_settings_from_osu_stable();
}

void OptionsMenu::addSpacer() {
    auto *e = new OPTIONS_ELEMENT;
    e->type = SPCR;
    this->elemContainers.push_back(e);
}

CBaseUILabel *OptionsMenu::addSection(const UString &text) {
    auto *label = new CBaseUILabel(0, 0, this->options->getSize().x, 25, text, text);
    // label->setTextColor(0xff58dafe);
    label->setFont(osu->getTitleFont());
    label->setSizeToContent(0, 0);
    label->setTextJustification(TEXT_JUSTIFICATION::RIGHT);
    label->setDrawFrame(false);
    label->setDrawBackground(false);
    this->options->container->addBaseUIElement(label);

    auto *e = new OPTIONS_ELEMENT;
    e->baseElems.push_back(label);
    e->type = SECT;
    this->elemContainers.push_back(e);

    return label;
}

CBaseUILabel *OptionsMenu::addSubSection(const UString &text, UString searchTags) {
    auto *label = new CBaseUILabel(0, 0, this->options->getSize().x, 25, text, text);
    label->setFont(osu->getSubTitleFont());
    label->setSizeToContent(0, 0);
    label->setDrawFrame(false);
    label->setDrawBackground(false);
    this->options->container->addBaseUIElement(label);

    auto *e = new OPTIONS_ELEMENT;
    e->baseElems.push_back(label);
    e->type = SUBSECT;
    e->searchTags = std::move(searchTags);
    this->elemContainers.push_back(e);

    return label;
}

CBaseUILabel *OptionsMenu::addLabel(const UString &text) {
    auto *label = new CBaseUILabel(0, 0, this->options->getSize().x, 25, text, text);
    label->setSizeToContent(0, 0);
    label->setDrawFrame(false);
    label->setDrawBackground(false);
    this->options->container->addBaseUIElement(label);

    auto *e = new OPTIONS_ELEMENT;
    e->baseElems.push_back(label);
    e->type = LABEL;
    this->elemContainers.push_back(e);

    return label;
}

UIButton *OptionsMenu::addButton(const UString &text) {
    auto *button = new UIButton(0, 0, this->options->getSize().x, 50, text, text);
    button->setColor(0xff0c7c99);
    button->setUseDefaultSkin();
    this->options->container->addBaseUIElement(button);

    auto *e = new OPTIONS_ELEMENT;
    e->baseElems.push_back(button);
    e->type = BTN;
    this->elemContainers.push_back(e);

    return button;
}

OptionsMenu::OPTIONS_ELEMENT *OptionsMenu::addButton(const UString &text, const UString &labelText,
                                                     bool withResetButton) {
    auto *button = new UIButton(0, 0, this->options->getSize().x, 50, text, text);
    button->setColor(0xff0c7c99);
    button->setUseDefaultSkin();
    this->options->container->addBaseUIElement(button);

    auto *label = new CBaseUILabel(0, 0, this->options->getSize().x, 50, labelText, labelText);
    label->setDrawFrame(false);
    label->setDrawBackground(false);
    this->options->container->addBaseUIElement(label);

    auto *e = new OPTIONS_ELEMENT;
    if(withResetButton) {
        e->resetButton = new OptionsMenuResetButton(0, 0, 35, 50, "", "");
    }
    e->baseElems.push_back(button);
    e->baseElems.push_back(label);
    e->type = BTN;
    this->elemContainers.push_back(e);

    return e;
}

OptionsMenu::OPTIONS_ELEMENT *OptionsMenu::addButtonButton(const UString &text1, const UString &text2) {
    auto *button = new UIButton(0, 0, this->options->getSize().x, 50, text1, text1);
    button->setColor(0xff0c7c99);
    button->setUseDefaultSkin();
    this->options->container->addBaseUIElement(button);

    auto *button2 = new UIButton(0, 0, this->options->getSize().x, 50, text2, text2);
    button2->setColor(0xff0c7c99);
    button2->setUseDefaultSkin();
    this->options->container->addBaseUIElement(button2);

    auto *e = new OPTIONS_ELEMENT;
    e->baseElems.push_back(button);
    e->baseElems.push_back(button2);
    e->type = BTN;
    this->elemContainers.push_back(e);

    return e;
}

OptionsMenu::OPTIONS_ELEMENT *OptionsMenu::addButtonButtonLabel(const UString &text1, const UString &text2,
                                                                const UString &labelText, bool withResetButton) {
    auto *button = new UIButton(0, 0, this->options->getSize().x, 50, text1, text1);
    button->setColor(0xff0c7c99);
    button->setUseDefaultSkin();
    this->options->container->addBaseUIElement(button);

    auto *button2 = new UIButton(0, 0, this->options->getSize().x, 50, text2, text2);
    button2->setColor(0xff0c7c99);
    button2->setUseDefaultSkin();
    this->options->container->addBaseUIElement(button2);

    auto *label = new CBaseUILabel(0, 0, this->options->getSize().x, 50, labelText, labelText);
    label->setDrawFrame(false);
    label->setDrawBackground(false);
    this->options->container->addBaseUIElement(label);

    auto *e = new OPTIONS_ELEMENT;
    if(withResetButton) {
        e->resetButton = new OptionsMenuResetButton(0, 0, 35, 50, "", "");
    }
    e->baseElems.push_back(button);
    e->baseElems.push_back(button2);
    e->baseElems.push_back(label);
    e->type = BTN;
    this->elemContainers.push_back(e);

    return e;
}

OptionsMenuKeyBindButton *OptionsMenu::addKeyBindButton(const UString &text, ConVar *cvar) {
    /// UString unbindIconString; unbindIconString.insert(0, Icons::UNDO);
    auto *unbindButton = new UIButton(0, 0, this->options->getSize().x, 50, text, "");
    unbindButton->setTooltipText("Unbind");
    unbindButton->setColor(0x77d90000);
    unbindButton->setUseDefaultSkin();
    unbindButton->setClickCallback(SA::MakeDelegate<&OptionsMenu::onKeyUnbindButtonPressed>(this));
    /// unbindButton->setFont(osu->getFontIcons());
    this->options->container->addBaseUIElement(unbindButton);

    auto *bindButton = new OptionsMenuKeyBindButton(0, 0, this->options->getSize().x, 50, text, text);
    bindButton->setColor(0xff0c7c99);
    bindButton->setUseDefaultSkin();
    bindButton->setClickCallback(SA::MakeDelegate<&OptionsMenu::onKeyBindingButtonPressed>(this));
    this->options->container->addBaseUIElement(bindButton);

    auto *label = new OptionsMenuKeyBindLabel(0, 0, this->options->getSize().x, 50, "", "", cvar, bindButton);
    label->setDrawFrame(false);
    label->setDrawBackground(false);
    this->options->container->addBaseUIElement(label);

    auto *e = new OPTIONS_ELEMENT;
    e->baseElems.push_back(unbindButton);
    e->baseElems.push_back(bindButton);
    e->baseElems.push_back(label);
    e->type = BINDBTN;
    e->cvars[unbindButton] = cvar;
    e->cvars[bindButton] = cvar;
    this->elemContainers.push_back(e);

    return bindButton;
}

CBaseUICheckbox *OptionsMenu::addCheckbox(const UString &text, ConVar *cvar) {
    return this->addCheckbox(text, "", cvar);
}

CBaseUICheckbox *OptionsMenu::addCheckbox(const UString &text, const UString &tooltipText, ConVar *cvar) {
    auto *checkbox = new UICheckbox(0, 0, this->options->getSize().x, 50, text, text);
    checkbox->setDrawFrame(false);
    checkbox->setDrawBackground(false);

    if(tooltipText.length() > 0) checkbox->setTooltipText(tooltipText);

    if(cvar != nullptr) {
        checkbox->setChecked(cvar->getBool());
        checkbox->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onCheckboxChange>(this));
    }

    this->options->container->addBaseUIElement(checkbox);

    auto *e = new OPTIONS_ELEMENT;
    if(cvar != nullptr) {
        e->resetButton = new OptionsMenuResetButton(0, 0, 35, 50, "", "");
        e->resetButton->setClickCallback(SA::MakeDelegate<&OptionsMenu::onResetClicked>(this));
    }
    e->baseElems.push_back(checkbox);
    e->type = CBX;
    e->cvars[e->resetButton] = cvar;
    e->cvars[checkbox] = cvar;
    this->elemContainers.push_back(e);

    return checkbox;
}

OptionsMenu::OPTIONS_ELEMENT *OptionsMenu::addButtonCheckbox(const UString &buttontext, const UString &cbxtooltip) {
    auto *button = new UIButton(0, 0, this->options->getSize().x, 50, buttontext, buttontext);
    button->setColor(0xff0c7c99);
    button->setUseDefaultSkin();
    this->options->container->addBaseUIElement(button);

    auto *checkbox =
        new UICheckbox(button->getSize().x, 0, this->options->getSize().x - button->getSize().x, 50, "", "");
    checkbox->setTooltipText(cbxtooltip);
    checkbox->setWidthToContent(0);
    checkbox->setDrawFrame(false);
    checkbox->setDrawBackground(false);
    this->options->container->addBaseUIElement(checkbox);

    auto *e = new OPTIONS_ELEMENT;
    e->baseElems.push_back(button);
    e->baseElems.push_back(checkbox);
    e->type = CBX_BTN;
    this->elemContainers.push_back(e);

    return e;
}

UISlider *OptionsMenu::addSlider(const UString &text, float min, float max, ConVar *cvar, float label1Width,
                                 bool allowOverscale, bool allowUnderscale) {
    auto *slider = new UISlider(0, 0, 100, 50, text);
    slider->setAllowMouseWheel(false);
    slider->setBounds(min, max);
    slider->setLiveUpdate(true);
    if(cvar != nullptr) {
        slider->setValue(cvar->getFloat(), false);
        slider->setChangeCallback(SA::MakeDelegate<&OptionsMenu::onSliderChange>(this));
    }
    this->options->container->addBaseUIElement(slider);

    auto *label1 = new CBaseUILabel(0, 0, this->options->getSize().x, 50, text, text);
    label1->setDrawFrame(false);
    label1->setDrawBackground(false);
    label1->setWidthToContent();
    if(label1Width > 1) label1->setSizeX(label1Width);
    label1->setRelSizeX(label1->getSize().x);
    this->options->container->addBaseUIElement(label1);

    auto *label2 = new CBaseUILabel(0, 0, this->options->getSize().x, 50, "", "8.81");
    label2->setDrawFrame(false);
    label2->setDrawBackground(false);
    label2->setWidthToContent();
    label2->setRelSizeX(label2->getSize().x);
    this->options->container->addBaseUIElement(label2);

    auto *e = new OPTIONS_ELEMENT;
    if(cvar != nullptr) {
        e->resetButton = new OptionsMenuResetButton(0, 0, 35, 50, "", "");
        e->resetButton->setClickCallback(SA::MakeDelegate<&OptionsMenu::onResetClicked>(this));
    }
    e->baseElems.push_back(label1);
    e->baseElems.push_back(slider);
    e->baseElems.push_back(label2);
    e->type = SLDR;
    e->cvars[e->resetButton] = cvar;
    e->cvars[slider] = cvar;
    e->label1Width = label1Width;
    e->relSizeDPI = label1->getFont()->getDPI();
    e->allowOverscale = allowOverscale;
    e->allowUnderscale = allowUnderscale;
    this->elemContainers.push_back(e);

    return slider;
}

CBaseUITextbox *OptionsMenu::addTextbox(UString text, ConVar *cvar) {
    auto *textbox = new CBaseUITextbox(0, 0, this->options->getSize().x, 40, "");
    textbox->setText(std::move(text));
    this->options->container->addBaseUIElement(textbox);

    auto *e = new OPTIONS_ELEMENT;
    e->baseElems.push_back(textbox);
    e->type = TBX;
    e->cvars[textbox] = cvar;
    this->elemContainers.push_back(e);

    return textbox;
}

CBaseUITextbox *OptionsMenu::addTextbox(UString text, const UString &labelText, ConVar *cvar) {
    auto *textbox = new CBaseUITextbox(0, 0, this->options->getSize().x, 40, "");
    textbox->setText(std::move(text));
    this->options->container->addBaseUIElement(textbox);

    auto *label = new CBaseUILabel(0, 0, this->options->getSize().x, 35, labelText, labelText);
    label->setDrawFrame(false);
    label->setDrawBackground(false);
    label->setTextColor(rgb(200, 200, 200));
    label->setWidthToContent();
    label->setScale(0.9f);
    this->options->container->addBaseUIElement(label);

    auto *e = new OPTIONS_ELEMENT;
    e->baseElems.push_back(label);
    e->baseElems.push_back(textbox);
    e->type = TBX;
    e->cvars[textbox] = cvar;
    this->elemContainers.push_back(e);

    return textbox;
}

CBaseUIElement *OptionsMenu::addSkinPreview() {
    CBaseUIElement *skinPreview = new OptionsMenuSkinPreviewElement(0, 0, 0, 200, "skincirclenumberhitresultpreview");
    this->options->container->addBaseUIElement(skinPreview);

    auto *e = new OPTIONS_ELEMENT;
    e->baseElems.push_back(skinPreview);
    e->type = SKNPRVW;
    this->elemContainers.push_back(e);

    return skinPreview;
}

CBaseUIElement *OptionsMenu::addSliderPreview() {
    CBaseUIElement *sliderPreview = new OptionsMenuSliderPreviewElement(0, 0, 0, 200, "skinsliderpreview");
    this->options->container->addBaseUIElement(sliderPreview);

    auto *e = new OPTIONS_ELEMENT;
    e->baseElems.push_back(sliderPreview);
    e->type = SKNPRVW;
    this->elemContainers.push_back(e);

    return sliderPreview;
}

OptionsMenuCategoryButton *OptionsMenu::addCategory(CBaseUIElement *section, char16_t icon) {
    UString iconString;
    iconString.insert(0, icon);
    auto *button = new OptionsMenuCategoryButton(section, 0, 0, 50, 50, "", iconString);
    button->setFont(osu->getFontIcons());
    button->setDrawBackground(false);
    button->setDrawFrame(false);
    button->setClickCallback(SA::MakeDelegate<&OptionsMenu::onCategoryClicked>(this));
    this->categories->container->addBaseUIElement(button);
    this->categoryButtons.push_back(button);

    return button;
}

void OptionsMenu::save() {
    if(!cv::options_save_on_back.getBool()) {
        debugLog("DEACTIVATED SAVE!!!! @ {:f}", engine->getTime());
        return;
    }

    cv::osu_folder.setValue(this->osuFolderTextbox->getText());
    this->updateFposuDPI();
    this->updateFposuCMper360();

    // Update windowed resolution now, so we're always saving the correct window size
    // (this function is also called on shutdown)
    const bool fs = env->winFullscreened();
    const bool fs_letterboxed = fs && cv::letterboxing.getBool();
    if(!fs && !fs_letterboxed) {
        const auto res_str = fmt::format("{:d}x{:d}", (i32)env->getWindowSize().x, (i32)env->getWindowSize().y);
        cv::windowed_resolution.setValue(res_str, false);
    }

    debugLog("Osu: Saving user config file ...");

    static constexpr const std::string_view cfg_name = NEOSU_CFG_PATH "/osu.cfg"sv;
    static AsyncIOHandler::WriteCallback wr_callback = [func = __FUNCTION__](bool success) -> void {
        if(!success) {
            if(osu) {
                osu->getNotificationOverlay()->addToast(ULITERAL("Failed to save osu.cfg"), ERROR_TOAST);
            } else {
                debugLogLambda("Failed to save osu.cfg");
            }
        } else if(cv::debug_file.getBool()) {
            debugLogLambda("Successfully wrote osu.cfg");
        }
    };

    static AsyncIOHandler::ReadCallback rd_callback = [func = __FUNCTION__](std::vector<u8> read_data) -> void {
        std::vector<std::string> read_lines;
        if(read_data.empty()) {
            if(Environment::fileExists(cfg_name)) {
                debugLogLambda("WARNING: read no data from previous osu.cfg!\n");
                // back it up just in case
                ByteBufferedFile::copy(cfg_name,
                                       fmt::format("{}.{:%F}.bak", cfg_name, fmt::gmtime(std::time(nullptr))));
            }
        } else {
            read_lines = SString::split<std::string>(
                std::string{reinterpret_cast<const char *>(read_data.data()), read_data.size()}, '\n');
        }

        std::string write_lines;
        write_lines.reserve(read_lines.size());

        for(auto &line : read_lines) {
            SString::trim_inplace(line);
            if(line.empty()) continue;
            if(line.starts_with('#') || line.starts_with("//")) {
                if(!line.ends_with('\n')) line.push_back('\n');
                write_lines.append(line);
                continue;
            }

            bool cvar_found = false;
            const auto parts = SString::split(line, ' ');
            for(auto convar : cvars().getConVarArray()) {
                if(convar->isFlagSet(cv::NOSAVE)) continue;
                if(convar->getName() == parts[0]) {
                    cvar_found = true;
                    break;
                }
            }

            if(!cvar_found) {
                if(!line.ends_with('\n')) line.push_back('\n');
                write_lines.append(line);
                continue;
            }
        }

        if(!write_lines.empty()) {
            write_lines.append("\n\n");
        }

        for(auto convar : cvars().getConVarArray()) {
            if(!convar->canHaveValue() || convar->isFlagSet(cv::NOSAVE)) continue;
            if(convar->isDefault()) continue;
            write_lines.append(fmt::format("{} {}\n", convar->getName(), convar->getString()));
        }

        io->write(cfg_name, std::move(write_lines), wr_callback);
    };

    // let the nested write callback handle any error message
    io->read(cfg_name, rd_callback);
}

void OptionsMenu::openAndScrollToSkinSection() {
    const bool wasVisible = this->isVisible();
    if(!wasVisible) osu->toggleOptionsMenu();

    if(!this->skinSelectLocalButton->isVisible() || !wasVisible)
        this->options->scrollToElement(this->skinSection, 0, 100 * Osu::getUIScale());
}

bool OptionsMenu::should_use_oauth_login() {
    if(cv::force_oauth.getBool()) {
        return true;
    }

    // Addresses for which we should use OAuth login instead of username:password login
    static constexpr const auto oauth_servers = std::array{
        "neosu.local"sv,
        "neosu.net"sv,
    };

    const std::string server_endpoint{this->serverTextbox->getText().toUtf8()};
    if(server_endpoint.empty() || !std::ranges::contains(oauth_servers, server_endpoint)) {
        return false;
    }

    // in whitelist
    return true;
}
