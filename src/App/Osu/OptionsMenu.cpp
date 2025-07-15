#include "OptionsMenu.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <utility>

#include "AnimationHandler.h"
#include "Bancho.h"
#include "BanchoNetworking.h"
#include "CBaseUICheckbox.h"
#include "CBaseUIContainer.h"
#include "CBaseUILabel.h"
#include "CBaseUIScrollView.h"
#include "CBaseUITextbox.h"
#include "Chat.h"
#include "Circle.h"
#include "ConVar.h"
#include "Database.h"
#include "Engine.h"
#include "Environment.h"
#include "File.h"
#include "GameRules.h"
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
#include "SliderRenderer.h"
#include "SongBrowser/LoudnessCalcThread.h"
#include "SongBrowser/SongBrowser.h"
#include "SoundEngine.h"
#include "TooltipOverlay.h"
#include "UIBackButton.h"
#include "UIButton.h"
#include "UICheckbox.h"
#include "UIContextMenu.h"
#include "UIModSelectorModButton.h"
#include "UISearchOverlay.h"
#include "UISlider.h"

class OptionsMenuSkinPreviewElement : public CBaseUIElement {
   public:
    OptionsMenuSkinPreviewElement(float xPos, float yPos, float xSize, float ySize, UString name)
        : CBaseUIElement(xPos, yPos, xSize, ySize, std::move(name)) {
        this->iMode = 0;
    }

    void draw() override {
        if(!this->bVisible) return;

        Skin *skin = osu->getSkin();

        float hitcircleDiameter = this->vSize.y * 0.5f;
        float numberScale = (hitcircleDiameter / (160.0f * (skin->isDefault12x() ? 2.0f : 1.0f))) * 1 *
                            cv::number_scale_multiplier.getFloat();
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

            Circle::drawCircle(
                osu->getSkin(),
                this->vPos + Vector2(0, this->vSize.y / 2) + Vector2(this->vSize.x * (1.0f / 5.0f), 0.0f),
                hitcircleDiameter, numberScale, overlapScale, number, colorCounter, colorOffset, colorRGBMultiplier,
                approachScale, approachAlpha, approachAlpha, true, false);
            Circle::drawHitResult(
                osu->getSkin(), hitcircleDiameter, hitcircleDiameter,
                this->vPos + Vector2(0, this->vSize.y / 2) + Vector2(this->vSize.x * (2.0f / 5.0f), 0.0f),
                LiveScore::HIT::HIT_100, 0.45f, 0.33f);
            Circle::drawHitResult(
                osu->getSkin(), hitcircleDiameter, hitcircleDiameter,
                this->vPos + Vector2(0, this->vSize.y / 2) + Vector2(this->vSize.x * (3.0f / 5.0f), 0.0f),
                LiveScore::HIT::HIT_50, 0.45f, 0.66f);
            Circle::drawHitResult(
                osu->getSkin(), hitcircleDiameter, hitcircleDiameter,
                this->vPos + Vector2(0, this->vSize.y / 2) + Vector2(this->vSize.x * (4.0f / 5.0f), 0.0f),
                LiveScore::HIT::HIT_MISS, 0.45f, 1.0f);
            Circle::drawApproachCircle(
                osu->getSkin(),
                this->vPos + Vector2(0, this->vSize.y / 2) + Vector2(this->vSize.x * (1.0f / 5.0f), 0.0f),
                osu->getSkin()->getComboColorForCounter(colorCounter, colorOffset), hitcircleDiameter, approachScale,
                approachCircleAlpha, false, false);
        } else if(this->iMode == 1) {
            const int numNumbers = 6;
            for(int i = 1; i < numNumbers + 1; i++) {
                Circle::drawHitCircleNumber(skin, numberScale, overlapScale,
                                            this->vPos + Vector2(0, this->vSize.y / 2) +
                                                Vector2(this->vSize.x * ((float)i / (numNumbers + 1.0f)), 0.0f),
                                            i - 1, 1.0f, 1.0f);
            }
        } else if(this->iMode == 2) {
            const int numNumbers = 6;
            for(int i = 1; i < numNumbers + 1; i++) {
                Vector2 pos = this->vPos + Vector2(0, this->vSize.y / 2) +
                              Vector2(this->vSize.x * ((float)i / (numNumbers + 1.0f)), 0.0f);

                g->pushTransform();
                g->scale(scoreScale, scoreScale);
                g->translate(pos.x - skin->getScore0()->getWidth() * scoreScale, pos.y);
                osu->getHUD()->drawScoreNumber(i - 1, 1.0f);
                g->popTransform();
            }
        }
    }

    void onMouseUpInside() override {
        this->iMode++;
        this->iMode = this->iMode % 3;
    }

   private:
    int iMode;
};

class OptionsMenuSliderPreviewElement : public CBaseUIElement {
   public:
    OptionsMenuSliderPreviewElement(float xPos, float yPos, float xSize, float ySize, UString name)
        : CBaseUIElement(xPos, yPos, xSize, ySize, std::move(name)) {
        this->bDrawSliderHack = true;
        this->fPrevLength = 0.0f;
        this->vao = NULL;
    }

    void draw() override {
        if(!this->bVisible) return;

        const float hitcircleDiameter = this->vSize.y * 0.5f;
        const float numberScale = (hitcircleDiameter / (160.0f * (osu->getSkin()->isDefault12x() ? 2.0f : 1.0f))) * 1 *
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

        static std::vector<Vector2> emptyVector;
        std::vector<Vector2> points;

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
                                   points[numPoints / 2] + (!useLegacyRenderer ? this->vPos : Vector2(0, 0)),
                                   hitcircleDiameter, numberScale, overlapScale, number, colorCounter, colorOffset,
                                   colorRGBMultiplier, approachScale, approachAlpha, approachAlpha, true, false);
                Circle::drawApproachCircle(osu->getSkin(),
                                           points[numPoints / 2] + (!useLegacyRenderer ? this->vPos : Vector2(0, 0)),
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
                        if(this->vao == NULL || length != this->fPrevLength) {
                            this->fPrevLength = length;

                            debugLog("Regenerating options menu slider preview vao ...\n");

                            if(this->vao != NULL) {
                                resourceManager->destroyResource(this->vao);
                                this->vao = NULL;
                            }

                            if(this->vao == NULL)
                                this->vao =
                                    SliderRenderer::generateVAO(points, hitcircleDiameter, Vector3(0, 0, 0), false);
                        }
                        SliderRenderer::draw(this->vao, emptyVector, this->vPos, 1, hitcircleDiameter, 0, 1,
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
                    osu->getSkin(), points[0] + (!useLegacyRenderer ? this->vPos : Vector2(0, 0)), hitcircleDiameter,
                    numberScale, overlapScale, number, colorCounter, colorOffset, colorRGBMultiplier);
                Circle::drawSliderEndCircle(
                    osu->getSkin(), points[points.size() - 1] + (!useLegacyRenderer ? this->vPos : Vector2(0, 0)),
                    hitcircleDiameter, numberScale, overlapScale, number, colorCounter, colorOffset, colorRGBMultiplier,
                    1.0f, 1.0f, 0.0f, false, false);
            }
        }
    }

    void setDrawSliderHack(bool drawSliderHack) { this->bDrawSliderHack = drawSliderHack; }

   private:
    bool bDrawSliderHack;
    VertexArrayObject *vao;
    float fPrevLength;
};

class OptionsMenuKeyBindLabel : public CBaseUILabel {
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

        const KEYCODE newKeyCode = (KEYCODE)this->key->getInt();
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
    void onMouseUpInside() override {
        CBaseUILabel::onMouseUpInside();
        this->bindButton->click();
    }

    ConVar *key;
    CBaseUIButton *bindButton;

    Color textColorBound;
    Color textColorUnbound;
    KEYCODE keyCode;
};

class OptionsMenuKeyBindButton : public UIButton {
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

class OptionsMenuCategoryButton : public CBaseUIButton {
   public:
    OptionsMenuCategoryButton(CBaseUIElement *section, float xPos, float yPos, float xSize, float ySize, UString name,
                              UString text)
        : CBaseUIButton(xPos, yPos, xSize, ySize, std::move(name), std::move(text)) {
        this->section = section;
        this->bActiveCategory = false;
    }

    void drawText() override {
        if(this->font != NULL && this->sText.length() > 0) {
            g->pushClipRect(McRect(this->vPos.x + 1, this->vPos.y + 1, this->vSize.x - 1, this->vSize.y - 1));
            {
                g->setColor(this->textColor);
                g->pushTransform();
                {
                    g->translate((int)(this->vPos.x + this->vSize.x / 2.0f - (this->fStringWidth / 2.0f)),
                                 (int)(this->vPos.y + this->vSize.y / 2.0f + (this->fStringHeight / 2.0f)));
                    g->drawString(this->font, this->sText);
                }
                g->popTransform();
            }
            g->popClipRect();
        }
    }

    void setActiveCategory(bool activeCategory) { this->bActiveCategory = activeCategory; }

    [[nodiscard]] inline CBaseUIElement *getSection() const { return this->section; }
    [[nodiscard]] inline bool isActiveCategory() const { return this->bActiveCategory; }

   private:
    CBaseUIElement *section;
    bool bActiveCategory;
};

class OptionsMenuResetButton : public CBaseUIButton {
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
            { osu->getTooltipOverlay()->addLine("Reset"); }
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
    cv::skin_use_skin_hitsounds.setCallback(
        fastdelegate::MakeDelegate(this, &OptionsMenu::onUseSkinsSoundSamplesChange));
    cv::options_high_quality_sliders.setCallback(
        fastdelegate::MakeDelegate(this, &OptionsMenu::onHighQualitySlidersConVarChange));

    osu->getNotificationOverlay()->addKeyListener(this);

    this->waitingKey = NULL;
    this->resolutionLabel = NULL;
    this->fullscreenCheckbox = NULL;
    this->sliderQualitySlider = NULL;
    this->outputDeviceLabel = NULL;
    this->outputDeviceResetButton = NULL;
    this->wasapiBufferSizeSlider = NULL;
    this->wasapiPeriodSizeSlider = NULL;
    this->asioBufferSizeResetButton = NULL;
    this->wasapiBufferSizeResetButton = NULL;
    this->wasapiPeriodSizeResetButton = NULL;
    this->dpiTextbox = NULL;
    this->cm360Textbox = NULL;
    this->letterboxingOffsetResetButton = NULL;
    this->uiScaleSlider = NULL;
    this->uiScaleResetButton = NULL;
    this->notelockSelectButton = NULL;
    this->notelockSelectLabel = NULL;
    this->notelockSelectResetButton = NULL;
    this->hpDrainSelectButton = NULL;
    this->hpDrainSelectLabel = NULL;
    this->hpDrainSelectResetButton = NULL;

    this->fOsuFolderTextboxInvalidAnim = 0.0f;
    this->fVibrationStrengthExampleTimer = 0.0f;
    this->bLetterboxingOffsetUpdateScheduled = false;
    this->bUIScaleChangeScheduled = false;
    this->bUIScaleScrollToSliderScheduled = false;
    this->bDPIScalingScrollToSliderScheduled = false;
    this->bASIOBufferChangeScheduled = false;
    this->bWASAPIBufferChangeScheduled = false;
    this->bWASAPIPeriodChangeScheduled = false;
    this->bSearchLayoutUpdateScheduled = false;

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

    this->search = new UISearchOverlay(0, 0, 0, 0, "");
    this->search->setOffsetRight(20);
    this->addBaseUIElement(this->search);

    this->spacer = new CBaseUILabel(0, 0, 1, 40, "", "");
    this->spacer->setDrawBackground(false);
    this->spacer->setDrawFrame(false);

    //**************************************************************************************************************************//

    CBaseUIElement *sectionGeneral = this->addSection("General");

    this->addSubSection("osu!folder");
    this->addLabel("1) If you have an existing osu! installation:")->setTextColor(0xff666666);
    this->addLabel("2) osu! > Options > \"Open osu! folder\"")->setTextColor(0xff666666);
    this->addLabel("3) Copy paste the full path into the textbox:")->setTextColor(0xff666666);
    this->addLabel("");
    this->osuFolderTextbox = this->addTextbox(cv::osu_folder.getString().c_str(), &cv::osu_folder);
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
                      "Keep touch device pp nerf mod active even when resetting all mods.", &cv::mod_touchdevice_always);

    this->addSubSection("Songbrowser");
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

    //**************************************************************************************************************************//

    CBaseUIElement *sectionGraphics = this->addSection("Graphics");

    this->addSubSection("Renderer");
    this->addCheckbox("VSync", "If enabled: plz enjoy input lag.", &cv::vsync);

    if constexpr(Env::cfg(OS::WINDOWS))
        this->addCheckbox("High Priority", "Sets the game process priority to high", &cv::win_processpriority);

    this->addCheckbox("Show FPS Counter", &cv::draw_fps);
    this->addSpacer();

    this->addCheckbox("Unlimited FPS", &cv::fps_unlimited);

    CBaseUISlider *fpsSlider = this->addSlider("FPS Limiter:", 60.0f, 1000.0f, &cv::fps_max, -1.0f, true);
    fpsSlider->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangeInt));
    fpsSlider->setKeyDelta(1);

    this->addSubSection("Layout");
    OPTIONS_ELEMENT resolutionSelect =
        this->addButton("Select Resolution", UString::format("%ix%i", osu->getScreenWidth(), osu->getScreenHeight()));
    this->resolutionSelectButton = (CBaseUIButton *)resolutionSelect.elements[0];
    this->resolutionSelectButton->setClickCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onResolutionSelect));
    this->resolutionLabel = (CBaseUILabel *)resolutionSelect.elements[1];
    this->fullscreenCheckbox = this->addCheckbox("Fullscreen");
    this->fullscreenCheckbox->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onFullscreenChange));
    this->addCheckbox("Borderless",
                      "May cause extra input lag if enabled.\nDepends on your operating system version/updates.",
                      &cv::fullscreen_windowed_borderless)
        ->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onBorderlessWindowedChange));
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
            ->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangeLetterboxingOffset))
            ->setKeyDelta(0.01f)
            ->setAnimated(false);
    this->letterboxingOffsetYSlider =
        this->addSlider("Vertical position", -1.0f, 1.0f, &cv::letterboxing_offset_y, 170)
            ->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangeLetterboxingOffset))
            ->setKeyDelta(0.01f)
            ->setAnimated(false);

    this->addSubSection("UI Scaling");
    this->addCheckbox(
            "DPI Scaling",
            UString::format("Automatically scale to the DPI of your display: %i DPI.\nScale factor = %i / 96 = %.2gx",
                            env->getDPI(), env->getDPI(), env->getDPIScale()),
            &cv::ui_scale_to_dpi)
        ->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onDPIScalingChange));
    this->uiScaleSlider = this->addSlider("UI Scale:", 1.0f, 1.5f, &cv::ui_scale);
    this->uiScaleSlider->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangeUIScale));
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
        ->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onHighQualitySlidersCheckboxChange));
    this->sliderQualitySlider = this->addSlider("Slider Quality", 0.0f, 1.0f, &cv::options_slider_quality);
    this->sliderQualitySlider->setChangeCallback(
        fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangeSliderQuality));

    //**************************************************************************************************************************//

    CBaseUIElement *sectionAudio = this->addSection("Audio");

    this->addSubSection("Devices");
    {
        OPTIONS_ELEMENT outputDeviceSelect = this->addButton("Select Output Device", "Default", true);
        this->outputDeviceResetButton = outputDeviceSelect.resetButton;
        this->outputDeviceResetButton->setClickCallback(
            fastdelegate::MakeDelegate(this, &OptionsMenu::onOutputDeviceResetClicked));
        this->outputDeviceSelectButton = (CBaseUIButton *)outputDeviceSelect.elements[0];
        this->outputDeviceSelectButton->setClickCallback(
            fastdelegate::MakeDelegate(this, &OptionsMenu::onOutputDeviceSelect));

        this->outputDeviceLabel = (CBaseUILabel *)outputDeviceSelect.elements[1];

        // Dirty...
        auto wasapi_idx = this->elements.size();
        {
            this->addSubSection("WASAPI");
            this->wasapiBufferSizeSlider =
                this->addSlider("Buffer Size:", 0.000f, 0.050f, &cv::win_snd_wasapi_buffer_size);
            this->wasapiBufferSizeSlider->setChangeCallback(
                fastdelegate::MakeDelegate(this, &OptionsMenu::onWASAPIBufferChange));
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
            this->wasapiPeriodSizeSlider->setChangeCallback(
                fastdelegate::MakeDelegate(this, &OptionsMenu::onWASAPIPeriodChange));
            this->wasapiPeriodSizeSlider->setKeyDelta(0.001f);
            this->wasapiPeriodSizeSlider->setAnimated(false);
            UIButton *restartSoundEngine = this->addButton("Restart SoundEngine");
            restartSoundEngine->setClickCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onOutputDeviceRestart));
            restartSoundEngine->setColor(0xff00566b);
            this->addLabel("");
        }
        auto wasapi_end_idx = this->elements.size();
        for(auto i = wasapi_idx; i < wasapi_end_idx; i++) {
            this->elements[i].render_condition = RenderCondition::WASAPI_ENABLED;
        }

        // Dirty...
        auto asio_idx = this->elements.size();
        {
            this->addSubSection("ASIO");
            this->asioBufferSizeSlider = this->addSlider("Buffer Size:", 0, 44100, &cv::asio_buffer_size);
            this->asioBufferSizeSlider->setKeyDelta(512);
            this->asioBufferSizeSlider->setAnimated(false);
            this->asioBufferSizeSlider->setLiveUpdate(false);
            this->asioBufferSizeSlider->setChangeCallback(
                fastdelegate::MakeDelegate(this, &OptionsMenu::onASIOBufferChange));
            this->addLabel("");
            UIButton *asio_settings_btn = this->addButton("Open ASIO settings");
            asio_settings_btn->setClickCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::OpenASIOSettings));
            asio_settings_btn->setColor(0xff00566b);
            UIButton *restartSoundEngine = this->addButton("Restart SoundEngine");
            restartSoundEngine->setClickCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onOutputDeviceRestart));
            restartSoundEngine->setColor(0xff00566b);
        }
        auto asio_end_idx = this->elements.size();
        for(auto i = asio_idx; i < asio_end_idx; i++) {
            this->elements[i].render_condition = RenderCondition::ASIO_ENABLED;
        }
    }

    this->addSubSection("Volume");

    if constexpr(Env::cfg(AUD::BASS)) {
        this->addCheckbox("Normalize loudness across songs", &cv::normalize_loudness)
            ->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onLoudnessNormalizationToggle));
    }

    CBaseUISlider *masterVolumeSlider = this->addSlider("Master:", 0.0f, 1.0f, &cv::volume_master, 70.0f);
    masterVolumeSlider->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangePercent));
    masterVolumeSlider->setKeyDelta(0.01f);
    CBaseUISlider *inactiveVolumeSlider = this->addSlider("Inactive:", 0.0f, 1.0f, &cv::volume_master_inactive, 70.0f);
    inactiveVolumeSlider->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangePercent));
    inactiveVolumeSlider->setKeyDelta(0.01f);
    CBaseUISlider *musicVolumeSlider = this->addSlider("Music:", 0.0f, 1.0f, &cv::volume_music, 70.0f);
    musicVolumeSlider->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangePercent));
    musicVolumeSlider->setKeyDelta(0.01f);
    CBaseUISlider *effectsVolumeSlider = this->addSlider("Effects:", 0.0f, 1.0f, &cv::volume_effects, 70.0f);
    effectsVolumeSlider->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangePercent));
    effectsVolumeSlider->setKeyDelta(0.01f);

    this->addSubSection("Offset Adjustment");
    CBaseUISlider *offsetSlider = this->addSlider("Universal Offset:", -300.0f, 300.0f, &cv::universal_offset);
    offsetSlider->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangeIntMS));
    offsetSlider->setKeyDelta(1);

    this->addSubSection("Songbrowser");
    this->addCheckbox("Apply speed/pitch mods while browsing",
                      "Whether to always apply all mods, or keep the preview music normal.",
                      &cv::beatmap_preview_mods_live);

    this->addSubSection("Gameplay");
    this->addCheckbox("Change hitsound pitch based on accuracy", &cv::snd_pitch_hitsounds);
    this->addCheckbox("Prefer Nightcore over Double Time", &cv::nightcore_enjoyer)
        ->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onNightcoreToggle));

    //**************************************************************************************************************************//

    this->skinSection = this->addSection("Skin");

    this->addSubSection("Skin");
    this->addSkinPreview();
    {
        OPTIONS_ELEMENT skinSelect;
        {
            skinSelect = this->addButton("Select Skin", "default");
            this->skinSelectLocalButton = skinSelect.elements[0];
            this->skinLabel = (CBaseUILabel *)skinSelect.elements[1];
        }
        ((CBaseUIButton *)this->skinSelectLocalButton)
            ->setClickCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSkinSelect));

        this->addButton("Open current Skin folder")
            ->setClickCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::openCurrentSkinFolder));

        OPTIONS_ELEMENT skinReload = this->addButtonButton("Reload Skin", "Random Skin");
        ((UIButton *)skinReload.elements[0])
            ->setClickCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSkinReload));
        ((UIButton *)skinReload.elements[0])->setTooltipText("(CTRL + ALT + S)");
        ((UIButton *)skinReload.elements[1])
            ->setClickCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSkinRandom));
        ((UIButton *)skinReload.elements[1])
            ->setTooltipText(
                "Temporary, does not change your configured skin (reload to reset).\nUse \"osu_skin_random 1\" to "
                "randomize on every skin reload.\nUse \"osu_skin_random_elements 1\" to mix multiple skins.\nUse "
                "\"osu_skin_export\" to export the currently active skin.");
        ((UIButton *)skinReload.elements[1])->setColor(0xff00566b);
    }
    this->addSpacer();
    CBaseUISlider *numberScaleSlider =
        this->addSlider("Number Scale:", 0.01f, 3.0f, &cv::number_scale_multiplier, 135.0f);
    numberScaleSlider->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangePercent));
    numberScaleSlider->setKeyDelta(0.01f);
    CBaseUISlider *hitResultScaleSlider = this->addSlider("HitResult Scale:", 0.01f, 3.0f, &cv::hitresult_scale, 135.0f);
    hitResultScaleSlider->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangePercent));
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

    this->addSubSection("Mouse", "scroll");
    if constexpr(Env::cfg(OS::WINDOWS | OS::MAC | OS::LINUX)) {
        this->addSlider("Sensitivity:", 0.1f, 6.0f, &cv::mouse_sensitivity)->setKeyDelta(0.01f);

        if constexpr(Env::cfg(OS::MAC)) {
            this->addLabel("");
            this->addLabel("WARNING: Set Sensitivity to 1 for tablets!")->setTextColor(0xffff0000);
            this->addLabel("");
        }
    }
    if constexpr(Env::cfg(OS::WINDOWS | OS::LINUX)) {
        this->addCheckbox("Raw Input", &cv::mouse_raw_input);
        if constexpr(Env::cfg(OS::WINDOWS)) {
            this->addCheckbox("[Beta] RawInputBuffer",
                              "Improves performance problems caused by insane mouse usb polling rates above 1000 "
                              "Hz.\nOnly relevant if \"Raw Input\" is enabled, or if in FPoSu mode (with disabled "
                              "\"Tablet/Absolute Mode\").",
                              &cv::win_mouse_raw_input_buffer);
            this->addCheckbox("Map Absolute Raw Input to Window", &cv::mouse_raw_input_absolute_to_window)
                ->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onRawInputToAbsoluteWindowChange));
        }
    }
    // if constexpr(Env::cfg(OS::LINUX)) {
    //     this->addLabel("Use system settings to change the mouse sensitivity.")->setTextColor(0xff555555);
    //     this->addLabel("");
    //     this->addLabel("Use xinput or xsetwacom to change the tablet area.")->setTextColor(0xff555555);
    //     this->addLabel("");
    // }
    this->addCheckbox("Confine Cursor (Windowed)", &cv::confine_cursor_windowed);
    this->addCheckbox("Confine Cursor (Fullscreen)", &cv::confine_cursor_fullscreen);
    this->addCheckbox("Confine Cursor (NEVER)", "Overrides automatic cursor clipping during gameplay.",
                      &cv::confine_cursor_never);
    this->addCheckbox("Disable Mouse Wheel in Play Mode", &cv::disable_mousewheel);
    this->addCheckbox("Disable Mouse Buttons in Play Mode", &cv::disable_mousebuttons);
    this->addCheckbox("Cursor ripples", "The cursor will ripple outwards on clicking.", &cv::draw_cursor_ripples);

    if constexpr(Env::cfg(OS::WINDOWS)) {
        this->addSubSection("Tablet");
#ifndef MCENGINE_FEATURE_SDL

        this->addCheckbox(
            "Windows Ink Workaround",
            "Enable this if your tablet cursor is stuck in a tiny area on the top left of the screen.\nIf this "
            "doesn't fix it, use \"Ignore Raw Input/Sensitivity\" below.",
            &cv::win_ink_workaround);
#endif
        this->addCheckbox(
            "Ignore Raw Input/Sensitivity",
            "Only use this if nothing else works.\nIf this is enabled, then the in-game sensitivity slider "
            "will no longer work for tablets!\n(You can then instead use your tablet configuration software to "
            "change the tablet area.)",
            &cv::tablet_sensitivity_ignore);
    }

    this->addSpacer();
    const UString keyboardSectionTags = "keyboard keys key bindings binds keybinds keybindings";
    CBaseUIElement *subSectionKeyboard = this->addSubSection("Keyboard", keyboardSectionTags);
    UIButton *resetAllKeyBindingsButton = this->addButton("Reset all key bindings");
    resetAllKeyBindingsButton->setColor(0xffff0000);
    resetAllKeyBindingsButton->setClickCallback(
        fastdelegate::MakeDelegate(this, &OptionsMenu::onKeyBindingsResetAllPressed));
    this->addSubSection("Keys - osu! Standard Mode", keyboardSectionTags);
    this->addKeyBindButton("Left Click", &cv::LEFT_CLICK);
    this->addKeyBindButton("Right Click", &cv::RIGHT_CLICK);
    this->addSpacer();
    this->addKeyBindButton("Left Click (2)", &cv::LEFT_CLICK_2);
    this->addKeyBindButton("Right Click (2)", &cv::RIGHT_CLICK_2);
    this->addSubSection("Keys - FPoSu", keyboardSectionTags);
    this->addKeyBindButton("Zoom", &cv::FPOSU_ZOOM);
    this->addSubSection("Keys - In-Game", keyboardSectionTags);
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
    this->backgroundDimSlider->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangePercent));
    this->backgroundBrightnessSlider =
        this->addSlider("Background Brightness:", 0.0f, 1.0f, &cv::background_brightness, 220.0f);
    this->backgroundBrightnessSlider->setChangeCallback(
        fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangePercent));
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
    this->addSpacer();
    this->addSubSection("Mechanics", "health drain notelock lock block blocking noteblock");
    this->addCheckbox(
        "Kill Player upon Failing",
        "Enabled: Singleplayer default. You die upon failing and the beatmap stops.\nDisabled: Multiplayer "
        "default. Allows you to keep playing even after failing.",
        &cv::drain_kill);
    this->addSpacer();
    this->addLabel("");

    OPTIONS_ELEMENT notelockSelect = this->addButton("Select [Notelock]", "None", true);
    ((CBaseUIButton *)notelockSelect.elements[0])
        ->setClickCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onNotelockSelect));
    this->notelockSelectButton = notelockSelect.elements[0];
    this->notelockSelectLabel = (CBaseUILabel *)notelockSelect.elements[1];
    this->notelockSelectResetButton = notelockSelect.resetButton;
    this->notelockSelectResetButton->setClickCallback(
        fastdelegate::MakeDelegate(this, &OptionsMenu::onNotelockSelectResetClicked));
    this->addLabel("");
    this->addLabel("Info about different notelock algorithms:")->setTextColor(0xff666666);
    this->addLabel("");
    this->addLabel("- McOsu: Auto miss previous circle, always.")->setTextColor(0xff666666);
    this->addLabel("- osu!stable: Locked until previous circle is miss.")->setTextColor(0xff666666);
    this->addLabel("- osu!lazer 2020: Auto miss previous circle if > time.")->setTextColor(0xff666666);
    this->addLabel("");
    this->addSpacer();
    this->addSubSection("Backgrounds");
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
                      "Incrementally updates the star rating (aka \"realtime stars\").", &cv::draw_statistics_livestars);
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
    this->hudSizeSlider->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangePercent));
    this->hudSizeSlider->setKeyDelta(0.01f);
    this->addSpacer();
    this->hudScoreScaleSlider = this->addSlider("Score Scale:", 0.01f, 3.0f, &cv::hud_score_scale, 165.0f);
    this->hudScoreScaleSlider->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangePercent));
    this->hudScoreScaleSlider->setKeyDelta(0.01f);
    this->hudComboScaleSlider = this->addSlider("Combo Scale:", 0.01f, 3.0f, &cv::hud_combo_scale, 165.0f);
    this->hudComboScaleSlider->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangePercent));
    this->hudComboScaleSlider->setKeyDelta(0.01f);
    this->hudAccuracyScaleSlider = this->addSlider("Accuracy Scale:", 0.01f, 3.0f, &cv::hud_accuracy_scale, 165.0f);
    this->hudAccuracyScaleSlider->setChangeCallback(
        fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangePercent));
    this->hudAccuracyScaleSlider->setKeyDelta(0.01f);
    this->hudHiterrorbarScaleSlider =
        this->addSlider("HitErrorBar Scale:", 0.01f, 3.0f, &cv::hud_hiterrorbar_scale, 165.0f);
    this->hudHiterrorbarScaleSlider->setChangeCallback(
        fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangePercent));
    this->hudHiterrorbarScaleSlider->setKeyDelta(0.01f);
    this->hudHiterrorbarURScaleSlider =
        this->addSlider("HitErrorBar UR Scale:", 0.01f, 3.0f, &cv::hud_hiterrorbar_ur_scale, 165.0f);
    this->hudHiterrorbarURScaleSlider->setChangeCallback(
        fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangePercent));
    this->hudHiterrorbarURScaleSlider->setKeyDelta(0.01f);
    this->hudProgressbarScaleSlider =
        this->addSlider("ProgressBar Scale:", 0.01f, 3.0f, &cv::hud_progressbar_scale, 165.0f);
    this->hudProgressbarScaleSlider->setChangeCallback(
        fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangePercent));
    this->hudProgressbarScaleSlider->setKeyDelta(0.01f);
    this->hudScoreBarScaleSlider = this->addSlider("ScoreBar Scale:", 0.01f, 3.0f, &cv::hud_scorebar_scale, 165.0f);
    this->hudScoreBarScaleSlider->setChangeCallback(
        fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangePercent));
    this->hudScoreBarScaleSlider->setKeyDelta(0.01f);
    this->hudScoreBoardScaleSlider =
        this->addSlider("ScoreBoard Scale:", 0.01f, 3.0f, &cv::hud_scoreboard_scale, 165.0f);
    this->hudScoreBoardScaleSlider->setChangeCallback(
        fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangePercent));
    this->hudScoreBoardScaleSlider->setKeyDelta(0.01f);
    this->hudInputoverlayScaleSlider =
        this->addSlider("Key Overlay Scale:", 0.01f, 3.0f, &cv::hud_inputoverlay_scale, 165.0f);
    this->hudInputoverlayScaleSlider->setChangeCallback(
        fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangePercent));
    this->hudInputoverlayScaleSlider->setKeyDelta(0.01f);
    this->statisticsOverlayScaleSlider =
        this->addSlider("Statistics Scale:", 0.01f, 3.0f, &cv::hud_statistics_scale, 165.0f);
    this->statisticsOverlayScaleSlider->setChangeCallback(
        fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangePercent));
    this->statisticsOverlayScaleSlider->setKeyDelta(0.01f);
    this->addSpacer();
    this->statisticsOverlayXOffsetSlider =
        this->addSlider("Statistics X Offset:", 0.0f, 2000.0f, &cv::hud_statistics_offset_x, 165.0f, true);
    this->statisticsOverlayXOffsetSlider->setChangeCallback(
        fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangeInt));
    this->statisticsOverlayXOffsetSlider->setKeyDelta(1.0f);
    this->statisticsOverlayYOffsetSlider =
        this->addSlider("Statistics Y Offset:", 0.0f, 1000.0f, &cv::hud_statistics_offset_y, 165.0f, true);
    this->statisticsOverlayYOffsetSlider->setChangeCallback(
        fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangeInt));
    this->statisticsOverlayYOffsetSlider->setKeyDelta(1.0f);

    this->addSubSection("Playfield");
    this->addCheckbox("Draw FollowPoints", &cv::draw_followpoints);
    this->addCheckbox("Draw Playfield Border", "Correct border relative to the current Circle Size.",
                      &cv::draw_playfield_border);
    this->addSpacer();
    this->playfieldBorderSizeSlider =
        this->addSlider("Playfield Border Size:", 0.0f, 500.0f, &cv::hud_playfield_border_size);
    this->playfieldBorderSizeSlider->setChangeCallback(
        fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangeInt));
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
    this->addCheckbox(
        "FPoSu",
        (Env::cfg(OS::WINDOWS)
             ? "The real 3D FPS mod.\nPlay from a first person shooter perspective in a 3D environment.\nThis "
               "is only intended for mouse! (Enable \"Tablet/Absolute Mode\" for tablets.)"
             : "The real 3D FPS mod.\nPlay from a first person shooter perspective in a 3D environment.\nThis "
               "is only intended for mouse!"),
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
    fovSlider->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangeTwoDecimalPlaces));
    fovSlider->setKeyDelta(0.01f);
    CBaseUISlider *zoomedFovSlider =
        this->addSlider("FOV (Zoom):", 10.0f, 160.0f, &cv::fposu_zoom_fov, -1.0f, true, true);
    zoomedFovSlider->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChangeTwoDecimalPlaces));
    zoomedFovSlider->setKeyDelta(0.01f);
    this->addCheckbox("Zoom Key Toggle", "Enabled: Zoom key toggles zoom.\nDisabled: Zoom while zoom key is held.",
                      &cv::fposu_zoom_toggle);
    this->addSubSection("FPoSu - Playfield");
    this->addCheckbox("Curved play area", &cv::fposu_curved);
    this->addCheckbox("Background cube", &cv::fposu_cube);
    this->addCheckbox("Skybox", "NOTE: Overrides \"Background cube\".\nSee skybox_example.png for cubemap layout.",
                      &cv::fposu_skybox);
    if constexpr(Env::cfg(OS::WINDOWS)) {
        this->addSubSection("FPoSu - Mouse");
        UIButton *cm360CalculatorLinkButton = this->addButton("https://www.mouse-sensitivity.com/");
        cm360CalculatorLinkButton->setClickCallback(
            fastdelegate::MakeDelegate(this, &OptionsMenu::onCM360CalculatorLinkClicked));
        cm360CalculatorLinkButton->setColor(0xff10667b);
        this->addLabel("");
        this->dpiTextbox = this->addTextbox(cv::fposu_mouse_dpi.getString().c_str(), "DPI:", &cv::fposu_mouse_dpi);
        this->cm360Textbox =
            this->addTextbox(cv::fposu_mouse_cm_360.getString().c_str(), "cm per 360:", &cv::fposu_mouse_cm_360);
        this->addLabel("");
        this->addCheckbox("Invert Vertical", &cv::fposu_invert_vertical);
        this->addCheckbox("Invert Horizontal", &cv::fposu_invert_horizontal);
        this->addCheckbox(
            "Tablet/Absolute Mode (!)",
            "WARNING: Do NOT enable this if you are using a mouse!\nIf this is enabled, then DPI and cm per "
            "360 will be ignored!",
            &cv::fposu_absolute_mode);
    }

    //**************************************************************************************************************************//

    this->sectionOnline = this->addSection("Online");

    this->addSubSection("Online server");
    this->addLabel("If the server admins don't explicitly allow neosu,")->setTextColor(0xff666666);
    this->addLabel("you might get banned!")->setTextColor(0xff666666);
    this->addLabel("");
    this->serverTextbox = this->addTextbox(cv::mp_server.getString().c_str(), &cv::mp_server);
    this->submitScoresCheckbox = this->addCheckbox("Submit scores", &cv::submit_scores);
    this->elements.back().render_condition = RenderCondition::SCORE_SUBMISSION_POLICY;

    this->addSubSection("Login details (username/password)");
    this->nameTextbox = this->addTextbox(cv::name.getString().c_str(), &cv::name);
    this->passwordTextbox = this->addTextbox(cv::mp_password.getString().c_str(), &cv::mp_password);
    this->passwordTextbox->is_password = true;
    this->logInButton = this->addButton("Log in");
    this->logInButton->setClickCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onLogInClicked));
    this->logInButton->setColor(0xff00ff00);
    this->logInButton->setTextColor(0xffffffff);

    this->addSubSection("Online settings");
    this->addCheckbox("Automatically update neosu to the latest version", &cv::auto_update);
    this->addCheckbox("Replace main menu logo with server logo", &cv::main_menu_use_server_logo);
    this->addCheckbox("Enable Discord Rich Presence",
                      "Shows your current game state in your friends' friendslists.\ne.g.: Playing Gavin G - Reach Out "
                      "[Cherry Blossom's Insane]",
                      &cv::rich_presence);

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
    resetAllSettingsButton->setClickCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onResetEverythingClicked));
    resetAllSettingsButton->setColor(0xffff0000);
    UIButton *importSettingsButton = this->addButton("Import settings from osu!stable");
    importSettingsButton->setClickCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onImportSettingsFromStable));
    importSettingsButton->setColor(0xffff0000);
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
    this->addCategory(sectionGeneral, Icons::GEAR);
    this->addCategory(sectionGraphics, Icons::DESKTOP);
    this->addCategory(sectionAudio, Icons::VOLUME_UP);
    this->addCategory(this->skinSection, Icons::PAINTBRUSH);
    this->addCategory(sectionInput, Icons::GAMEPAD);
    this->addCategory(subSectionKeyboard, Icons::KEYBOARD);
    this->addCategory(sectionGameplay, Icons::CIRCLE);
    this->fposuCategoryButton = this->addCategory(sectionFposu, Icons::CUBE);

    if(this->sectionOnline != NULL) this->addCategory(this->sectionOnline, Icons::GLOBE);

    //**************************************************************************************************************************//

    // the context menu gets added last (drawn on top of everything)
    this->options->getContainer()->addBaseUIElement(this->contextMenu);

    // HACKHACK: force current value update
    if(this->sliderQualitySlider != NULL)
        this->onHighQualitySlidersConVarChange(cv::options_high_quality_sliders.getString().c_str());
}

OptionsMenu::~OptionsMenu() {
    this->options->getContainer()->empty();

    SAFE_DELETE(this->spacer);
    SAFE_DELETE(this->contextMenu);

    for(auto element : this->elements) {
        SAFE_DELETE(element.resetButton);
        for(auto sub : element.elements) {
            SAFE_DELETE(sub);
        }
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
                g->fillRect(0, 0, osu->getScreenWidth(), osu->getScreenHeight());
            }
        }
    }

    if(this->backgroundDimSlider->isActive()) {
        if(!isPlayingBeatmap) {
            const short dim = std::clamp<float>(this->backgroundDimSlider->getFloat(), 0.0f, 1.0f) * 255.0f;
            g->setColor(argb(dim, 0, 0, 0));
            g->fillRect(0, 0, osu->getScreenWidth(), osu->getScreenHeight());
        }
    }

    ScreenBackable::draw();

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
    } else {
        ScreenBackable::draw();
    }

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

void OptionsMenu::mouse_update(bool *propagate_clicks) {
    if(this->bSearchLayoutUpdateScheduled) this->updateLayout();

    // force context menu focus
    this->contextMenu->mouse_update(propagate_clicks);
    if(!*propagate_clicks) return;

    if(!this->bVisible) return;

    ScreenBackable::mouse_update(propagate_clicks);
    if(!*propagate_clicks) return;

    if(this->bDPIScalingScrollToSliderScheduled) {
        this->bDPIScalingScrollToSliderScheduled = false;
        this->options->scrollToElement(this->uiScaleSlider, 0, 200 * Osu::getUIScale());
    }

    // flash osu!folder textbox red if incorrect
    if(this->fOsuFolderTextboxInvalidAnim > engine->getTime()) {
        char redness = std::abs(std::sin((this->fOsuFolderTextboxInvalidAnim - engine->getTime()) * 3)) * 128;
        this->osuFolderTextbox->setBackgroundColor(argb(255, redness, 0, 0));
    } else
        this->osuFolderTextbox->setBackgroundColor(0xff000000);

    // hack to avoid entering search text while binding keys
    if(osu->getNotificationOverlay()->isVisible() && osu->getNotificationOverlay()->isWaitingForKey())
        this->fSearchOnCharKeybindHackTime = engine->getTime() + 0.1f;

    // highlight active category depending on scroll position
    if(this->categoryButtons.size() > 0) {
        OptionsMenuCategoryButton *activeCategoryButton = this->categoryButtons[0];
        for(int i = 0; i < this->categoryButtons.size(); i++) {
            OptionsMenuCategoryButton *categoryButton = this->categoryButtons[i];
            if(categoryButton != NULL && categoryButton->getSection() != NULL) {
                categoryButton->setActiveCategory(false);
                categoryButton->setTextColor(0xff737373);

                if(categoryButton->getSection()->getPos().y < this->options->getSize().y * 0.4)
                    activeCategoryButton = categoryButton;
            }
        }
        if(activeCategoryButton != NULL) {
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

            cv::win_snd_wasapi_period_size.setValue(this->wasapiPeriodSizeSlider->getFloat());

            // and update reset buttons as usual
            this->onResetUpdate(this->wasapiPeriodSizeResetButton);
        }
    }

    // apply textbox changes on enter key
    if(this->osuFolderTextbox->hitEnter()) this->updateOsuFolder();

    cv::name.setValue(this->nameTextbox->getText());
    cv::mp_password.setValue(this->passwordTextbox->getText());
    cv::mp_server.setValue(this->serverTextbox->getText());
    if(this->nameTextbox->hitEnter()) {
        cv::name.setValue(this->nameTextbox->getText());
        this->nameTextbox->stealFocus();
        this->passwordTextbox->focus();
    }
    if(this->passwordTextbox->hitEnter()) {
        this->passwordTextbox->stealFocus();
        reconnect();
    }
    if(this->serverTextbox->hitEnter()) {
        this->serverTextbox->stealFocus();
        reconnect();
    }

    if(this->dpiTextbox != NULL && this->dpiTextbox->hitEnter()) this->updateFposuDPI();
    if(this->cm360Textbox != NULL && this->cm360Textbox->hitEnter()) this->updateFposuCMper360();
}

void OptionsMenu::onKeyDown(KeyboardEvent &e) {
    if(!this->bVisible) return;

    this->contextMenu->onKeyDown(e);
    if(e.isConsumed()) return;

    // KEY_TAB doesn't work on Linux
    if(e.getKeyCode() == 65056 || e.getKeyCode() == KEY_TAB) {
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
    if(this->sSearchString.length() > 0) {
        switch(e.getKeyCode()) {
            case KEY_DELETE:
            case KEY_BACKSPACE:
                if(this->sSearchString.length() > 0) {
                    if(keyboard->isControlDown()) {
                        // delete everything from the current caret position to the left, until after the first
                        // non-space character (but including it)
                        bool foundNonSpaceChar = false;
                        while(this->sSearchString.length() > 0) {
                            UString curChar = this->sSearchString.substr(this->sSearchString.length() - 1, 1);

                            if(foundNonSpaceChar && curChar.isWhitespaceOnly()) break;

                            if(!curChar.isWhitespaceOnly()) foundNonSpaceChar = true;

                            this->sSearchString.erase(this->sSearchString.length() - 1, 1);
                            e.consume();
                            this->scheduleSearchUpdate();
                            return;
                        }
                    } else {
                        this->sSearchString = this->sSearchString.substr(0, this->sSearchString.length() - 1);
                        e.consume();
                        this->scheduleSearchUpdate();
                        return;
                    }
                }
                break;

            case KEY_ESCAPE:
                this->sSearchString = "";
                this->scheduleSearchUpdate();
                e.consume();
                return;
        }
    }

    ScreenBackable::onKeyDown(e);

    // paste clipboard support
    if(!e.isConsumed() && keyboard->isControlDown() && e == KEY_V) {
        const UString clipstring = env->getClipBoardText();
        if(clipstring.length() > 0) {
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

    KEYCODE charCode = e.getCharCode();
    UString stringChar = "";
    stringChar.insert(0, charCode);
    this->sSearchString.append(stringChar);

    this->scheduleSearchUpdate();

    e.consume();
}

void OptionsMenu::onResolutionChange(Vector2 newResolution) {
    ScreenBackable::onResolutionChange(newResolution);

    // HACKHACK: magic
    if constexpr(Env::cfg(OS::WINDOWS)) {
        if((env->isFullscreen() && env->isFullscreenWindowedBorderless() &&
            (int)newResolution.y == (int)env->getNativeScreenSize().y + 1))
            newResolution.y--;
    }

    if(this->resolutionLabel != NULL)
        this->resolutionLabel->setText(UString::format("%ix%i", (int)newResolution.x, (int)newResolution.y));
}

void OptionsMenu::onKey(KeyboardEvent &e) {
    // from NotificationOverlay
    if(this->waitingKey != NULL) {
        this->waitingKey->setValue((float)e.getKeyCode());
        this->waitingKey = NULL;
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
    osu->chat->updateVisibility();

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
    this->bSearchLayoutUpdateScheduled = false;
    this->updating_layout = true;

    // set all elements to the current convar values, and update the reset button states
    for(int i = 0; i < this->elements.size(); i++) {
        switch(this->elements[i].type) {
            case 6:  // checkbox
                if(this->elements[i].cvar != NULL) {
                    for(int e = 0; e < this->elements[i].elements.size(); e++) {
                        CBaseUICheckbox *checkboxPointer =
                            dynamic_cast<CBaseUICheckbox *>(this->elements[i].elements[e]);
                        if(checkboxPointer != NULL) checkboxPointer->setChecked(this->elements[i].cvar->getBool());
                    }
                }
                break;
            case 7:  // slider
                if(this->elements[i].cvar != NULL) {
                    if(this->elements[i].elements.size() == 3) {
                        auto *sliderPointer = dynamic_cast<CBaseUISlider *>(this->elements[i].elements[1]);
                        if(sliderPointer != NULL) {
                            // allow users to overscale certain values via the console
                            if(this->elements[i].allowOverscale &&
                               this->elements[i].cvar->getFloat() > sliderPointer->getMax())
                                sliderPointer->setBounds(sliderPointer->getMin(), this->elements[i].cvar->getFloat());

                            // allow users to underscale certain values via the console
                            if(this->elements[i].allowUnderscale &&
                               this->elements[i].cvar->getFloat() < sliderPointer->getMin())
                                sliderPointer->setBounds(this->elements[i].cvar->getFloat(), sliderPointer->getMax());

                            sliderPointer->setValue(this->elements[i].cvar->getFloat(), false);
                            sliderPointer->fireChangeCallback();
                        }
                    }
                }
                break;
            case 8:  // textbox
                if(this->elements[i].cvar != NULL) {
                    if(this->elements[i].elements.size() == 1) {
                        auto *textboxPointer = dynamic_cast<CBaseUITextbox *>(this->elements[i].elements[0]);
                        if(textboxPointer != NULL) textboxPointer->setText(this->elements[i].cvar->getString().c_str());
                    } else if(this->elements[i].elements.size() == 2) {
                        auto *textboxPointer = dynamic_cast<CBaseUITextbox *>(this->elements[i].elements[1]);
                        if(textboxPointer != NULL) textboxPointer->setText(this->elements[i].cvar->getString().c_str());
                    }
                }
                break;
        }

        this->onResetUpdate(this->elements[i].resetButton);
    }

    if(this->fullscreenCheckbox != NULL) this->fullscreenCheckbox->setChecked(env->isFullscreen(), false);

    this->updateSkinNameLabel();
    this->updateNotelockSelectLabel();

    if(this->outputDeviceLabel != NULL) this->outputDeviceLabel->setText(soundEngine->getOutputDeviceName());

    this->onOutputDeviceResetUpdate();
    this->onNotelockSelectResetUpdate();

    //************************************************************************************************************************************//

    // TODO: correctly scale individual UI elements to dpiScale (depend on initial value in e.g. addCheckbox())

    ScreenBackable::updateLayout();

    const float dpiScale = Osu::getUIScale();

    this->setSize(osu->getScreenSize());

    // options panel
    const float optionsScreenWidthPercent = 0.5f;
    const float categoriesOptionsPercent = 0.135f;

    int optionsWidth = (int)(osu->getScreenWidth() * optionsScreenWidthPercent);
    if(!this->bFullscreen)
        optionsWidth = std::min((int)(725.0f * (1.0f - categoriesOptionsPercent)), optionsWidth) * dpiScale;

    const int categoriesWidth = optionsWidth * categoriesOptionsPercent;

    this->options->setRelPosX(
        (!this->bFullscreen ? -1 : osu->getScreenWidth() / 2 - (optionsWidth + categoriesWidth) / 2) + categoriesWidth);
    this->options->setSize(optionsWidth, osu->getScreenHeight() + 1);

    this->search->setRelPos(this->options->getRelPos());
    this->search->setSize(this->options->getSize());

    this->categories->setRelPosX(this->options->getRelPos().x - categoriesWidth);
    this->categories->setSize(categoriesWidth, osu->getScreenHeight() + 1);

    // reset
    this->options->getContainer()->empty();

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
    const std::string search = this->sSearchString.length() > 0 ? this->sSearchString.toUtf8() : "";
    for(int i = 0; i < this->elements.size(); i++) {
        if(this->elements[i].render_condition == RenderCondition::ASIO_ENABLED && !soundEngine->isASIO()) continue;
        if(this->elements[i].render_condition == RenderCondition::WASAPI_ENABLED && !soundEngine->isWASAPI()) continue;
        if(this->elements[i].render_condition == RenderCondition::SCORE_SUBMISSION_POLICY &&
           bancho->score_submission_policy != ServerPolicy::NO_PREFERENCE)
            continue;

        // searching logic happens here:
        // section
        //     content
        //     subsection
        //           content

        // case 1: match in content -> section + subsection + content     -> section + subsection matcher
        // case 2: match in content -> section + content                  -> section matcher
        // if match in section or subsection -> display entire section (disregard content match)
        // matcher is run through all remaining elements at every section + subsection

        if(this->sSearchString.length() > 0) {
            const std::string searchTags = this->elements[i].searchTags.toUtf8();

            // if this is a section
            if(this->elements[i].type == 1) {
                bool sectionMatch = false;

                const std::string sectionTitle = this->elements[i].elements[0]->getName().toUtf8();
                sectionTitleMatch = Osu::findIgnoreCase(sectionTitle, search);

                subSectionTitleMatch = false;
                if(inSkipSection) inSkipSection = false;

                for(int s = i + 1; s < this->elements.size(); s++) {
                    if(this->elements[s].type == 1)  // stop at next section
                        break;

                    for(int e = 0; e < this->elements[s].elements.size(); e++) {
                        if(this->elements[s].elements[e]->getName().length() > 0) {
                            std::string tags = this->elements[s].elements[e]->getName().toUtf8();

                            if(Osu::findIgnoreCase(tags, search)) {
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
            if(this->elements[i].type == 2) {
                bool subSectionMatch = false;

                const std::string subSectionTitle = this->elements[i].elements[0]->getName().toUtf8();
                subSectionTitleMatch =
                    Osu::findIgnoreCase(subSectionTitle, search) || Osu::findIgnoreCase(searchTags, search);

                if(inSkipSubSection) inSkipSubSection = false;

                for(int s = i + 1; s < this->elements.size(); s++) {
                    if(this->elements[s].type == 2)  // stop at next subsection
                        break;

                    for(int e = 0; e < this->elements[s].elements.size(); e++) {
                        if(this->elements[s].elements[e]->getName().length() > 0) {
                            std::string tags = this->elements[s].elements[e]->getName().toUtf8();

                            if(Osu::findIgnoreCase(tags, search)) {
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

                if(this->elements[i].type > 2) {
                    for(int e = 0; e < this->elements[i].elements.size(); e++) {
                        if(this->elements[i].elements[e]->getName().length() > 0) {
                            std::string tags = this->elements[i].elements[e]->getName().toUtf8();

                            if(Osu::findIgnoreCase(tags, search)) {
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
            if(this->elements[i].resetButton != NULL)
                this->options->getContainer()->addBaseUIElement(this->elements[i].resetButton);

            // (sub-)elements
            for(int e = 0; e < this->elements[i].elements.size(); e++) {
                this->options->getContainer()->addBaseUIElement(this->elements[i].elements[e]);
            }
        }

        // and build the layout

        // if this element is a new section, add even more spacing
        if(i > 0 && this->elements[i].type == 1) yCounter += sectionEndSpacing;

        // if this element is a new subsection, add even more spacing
        if(i > 0 && this->elements[i].type == 2) yCounter += subsectionEndSpacing;

        const int elementWidth = optionsWidth - 2 * sideMargin - 2 * dpiScale;
        const bool isKeyBindButton = (this->elements[i].type == 5);

        if(this->elements[i].resetButton != NULL) {
            CBaseUIButton *resetButton = this->elements[i].resetButton;
            resetButton->setSize(Vector2(35, 50) * dpiScale);
            resetButton->setRelPosY(yCounter);
            resetButton->setRelPosX(0);
        }

        for(int j = 0; j < this->elements[i].elements.size(); j++) {
            CBaseUIElement *e = this->elements[i].elements[j];
            e->setSizeY(e->getRelSize().y * dpiScale);
        }

        if(this->elements[i].elements.size() == 1) {
            CBaseUIElement *e = this->elements[i].elements[0];

            int sideMarginAdd = 0;
            auto *labelPointer = dynamic_cast<CBaseUILabel *>(e);
            if(labelPointer != NULL) {
                labelPointer->onResized();  // HACKHACK: framework, setSize*() does not update string metrics
                labelPointer->setSizeToContent(0, 0);
                sideMarginAdd += elementTextStartOffset;
            }

            auto *buttonPointer = dynamic_cast<CBaseUIButton *>(e);
            if(buttonPointer != NULL)
                buttonPointer->onResized();  // HACKHACK: framework, setSize*() does not update string metrics

            auto *checkboxPointer = dynamic_cast<CBaseUICheckbox *>(e);
            if(checkboxPointer != NULL) {
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
        } else if(this->elements[i].elements.size() == 2 || isKeyBindButton) {
            CBaseUIElement *e1 = this->elements[i].elements[0];
            CBaseUIElement *e2 = this->elements[i].elements[1];

            int spacing = 15 * dpiScale;

            int sideMarginAdd = 0;
            auto *labelPointer = dynamic_cast<CBaseUILabel *>(e1);
            if(labelPointer != NULL) sideMarginAdd += elementTextStartOffset;

            auto *buttonPointer = dynamic_cast<CBaseUIButton *>(e1);
            if(buttonPointer != NULL)
                buttonPointer->onResized();  // HACKHACK: framework, setSize*() does not update string metrics

            // button-button spacing
            auto *buttonPointer2 = dynamic_cast<CBaseUIButton *>(e2);
            if(buttonPointer != NULL && buttonPointer2 != NULL) spacing *= 0.35f;

            if(isKeyBindButton) {
                CBaseUIElement *e3 = this->elements[i].elements[2];

                const float dividerMiddle = 5.0f / 8.0f;
                const float dividerEnd = 2.0f / 8.0f;

                e1->setRelPos(sideMargin, yCounter);
                e1->setSizeX(e1->getSize().y);

                e2->setRelPos(sideMargin + e1->getSize().x + 0.5f * spacing, yCounter);
                e2->setSizeX(elementWidth * dividerMiddle - spacing);

                e3->setRelPos(sideMargin + e1->getSize().x + e2->getSize().x + 1.5f * spacing, yCounter);
                e3->setSizeX(elementWidth * dividerEnd - spacing);
            } else {
                float dividerEnd = 1.0f / 2.0f;
                float dividerBegin = 1.0f - dividerEnd;

                e1->setRelPos(sideMargin + sideMarginAdd, yCounter);
                e1->setSizeX(elementWidth * dividerBegin - spacing);

                e2->setRelPos(sideMargin + e1->getSize().x + 2 * spacing, yCounter);
                e2->setSizeX(elementWidth * dividerEnd - spacing);
            }

            yCounter += e1->getSize().y;
        } else if(this->elements[i].elements.size() == 3) {
            CBaseUIElement *e1 = this->elements[i].elements[0];
            CBaseUIElement *e2 = this->elements[i].elements[1];
            CBaseUIElement *e3 = this->elements[i].elements[2];

            if(this->elements[i].type == 4) {
                const int buttonButtonLabelOffset = 10 * dpiScale;

                const int buttonSize = elementWidth / 3 - 2 * buttonButtonLabelOffset;

                auto *button1Pointer = dynamic_cast<CBaseUIButton *>(e1);
                if(button1Pointer != NULL)
                    button1Pointer->onResized();  // HACKHACK: framework, setSize*() does not update string metrics

                auto *button2Pointer = dynamic_cast<CBaseUIButton *>(e2);
                if(button2Pointer != NULL)
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
                if(label1Pointer != NULL) {
                    label1Pointer->onResized();  // HACKHACK: framework, setSize*() does not update string metrics
                    if(this->elements[i].label1Width > 0.0f)
                        label1Pointer->setSizeX(this->elements[i].label1Width * dpiScale);
                    else
                        label1Pointer->setSizeX(label1Pointer->getRelSize().x * (96.0f / this->elements[i].relSizeDPI) *
                                                dpiScale);
                }

                auto *sliderPointer = dynamic_cast<CBaseUISlider *>(e2);
                if(sliderPointer != NULL) sliderPointer->setBlockSize(20 * dpiScale, 20 * dpiScale);

                auto *label2Pointer = dynamic_cast<CBaseUILabel *>(e3);
                if(label2Pointer != NULL) {
                    label2Pointer->onResized();  // HACKHACK: framework, setSize*() does not update string metrics
                    label2Pointer->setSizeX(label2Pointer->getRelSize().x * (96.0f / this->elements[i].relSizeDPI) *
                                            dpiScale);
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

        switch(this->elements[i].type) {
            case 0:
                yCounter += spaceSpacing;
                break;
            case 1:
                yCounter += sectionSpacing;
                break;
            case 2:
                yCounter += subsectionSpacing;
                break;
            default:
                break;
        }
    }
    this->spacer->setPosY(yCounter);
    this->options->getContainer()->addBaseUIElement(this->spacer);

    this->options->setScrollSizeToContent();
    if(!enableHorizontalScrolling) this->options->scrollToLeft();
    this->options->setHorizontalScrolling(enableHorizontalScrolling);

    this->options->getContainer()->addBaseUIElement(this->contextMenu);

    this->options->getContainer()->update_pos();

    const int categoryPaddingTopBottom = this->categories->getSize().y * 0.15f;
    const int categoryHeight =
        (this->categories->getSize().y - categoryPaddingTopBottom * 2) / this->categoryButtons.size();
    for(int i = 0; i < this->categoryButtons.size(); i++) {
        OptionsMenuCategoryButton *category = this->categoryButtons[i];
        category->onResized();  // HACKHACK: framework, setSize*() does not update string metrics
        category->setRelPosY(categoryPaddingTopBottom + categoryHeight * i);
        category->setSize(this->categories->getSize().x - 1, categoryHeight);
    }
    this->categories->getContainer()->update_pos();
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
    this->options->scrollToElement(this->sectionOnline, 0, 100 * osu->getUIScale());
    this->nameTextbox->focus();
}

void OptionsMenu::updateOsuFolder() {
    this->osuFolderTextbox->stealFocus();

    // automatically insert a slash at the end if the user forgets
    UString newOsuFolder = this->osuFolderTextbox->getText();
    newOsuFolder = newOsuFolder.trim();
    if(newOsuFolder.length() > 0) {
        if(newOsuFolder[newOsuFolder.length() - 1] != L'/' && newOsuFolder[newOsuFolder.length() - 1] != L'\\') {
            newOsuFolder.append(PREF_PATHSEP);
            this->osuFolderTextbox->setText(newOsuFolder);
        }
    }

    // set convar to new folder
    cv::osu_folder.setValue(newOsuFolder);
}

void OptionsMenu::updateFposuDPI() {
    if(this->dpiTextbox == NULL) return;

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
    if(this->cm360Textbox == NULL) return;

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
    if(this->skinLabel == NULL) return;

    this->skinLabel->setText(cv::skin.getString().c_str());
    this->skinLabel->setTextColor(0xffffffff);
}

void OptionsMenu::updateNotelockSelectLabel() {
    if(this->notelockSelectLabel == NULL) return;

    this->notelockSelectLabel->setText(
        this->notelockTypes[std::clamp<int>(cv::notelock_type.getInt(), 0, this->notelockTypes.size() - 1)]);
}

void OptionsMenu::onFullscreenChange(CBaseUICheckbox *checkbox) {
    if(checkbox->isChecked())
        env->enableFullscreen();
    else
        env->disableFullscreen();

    // and update reset button as usual
    for(int i = 0; i < this->elements.size(); i++) {
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == checkbox) {
                this->onResetUpdate(this->elements[i].resetButton);

                break;
            }
        }
    }
}

void OptionsMenu::onBorderlessWindowedChange(CBaseUICheckbox *checkbox) {
    this->onCheckboxChange(checkbox);

    if(checkbox->isChecked() && !env->isFullscreen()) env->enableFullscreen();

    // and update reset button as usual
    for(int i = 0; i < this->elements.size(); i++) {
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == checkbox) {
                this->onResetUpdate(this->elements[i].resetButton);

                break;
            }
        }
    }
}

void OptionsMenu::onDPIScalingChange(CBaseUICheckbox *checkbox) {
    for(int i = 0; i < this->elements.size(); i++) {
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == checkbox) {
                const float prevUIScale = Osu::getUIScale();

                if(this->elements[i].cvar != NULL) this->elements[i].cvar->setValue(checkbox->isChecked());

                this->onResetUpdate(this->elements[i].resetButton);

                if(Osu::getUIScale() != prevUIScale) this->bDPIScalingScrollToSliderScheduled = true;

                break;
            }
        }
    }
}

void OptionsMenu::onRawInputToAbsoluteWindowChange(CBaseUICheckbox *checkbox) {
    for(int i = 0; i < this->elements.size(); i++) {
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == checkbox) {
                if(this->elements[i].cvar != NULL) this->elements[i].cvar->setValue(checkbox->isChecked());

                this->onResetUpdate(this->elements[i].resetButton);

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
        env->openDirectory(MCENGINE_DATA_DIR "materials" PREF_PATHSEP "default");
    } else {
        std::string neosuSkinFolder = MCENGINE_DATA_DIR "skins/";
        neosuSkinFolder.append(current_skin);
        if(env->directoryExists(neosuSkinFolder)) {
            env->openDirectory(neosuSkinFolder);
        } else {
            std::string currentSkinFolder =
                fmt::format("{}/{}{}", cv::osu_folder.getString(), cv::osu_folder_sub_skins.getString(), current_skin);
            env->openDirectory(currentSkinFolder);
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

    this->updateOsuFolder();

    if(osu->isSkinLoading()) return;

    std::string skinFolder{cv::osu_folder.getString()};
    skinFolder.append("/");
    skinFolder.append(cv::osu_folder_sub_skins.getString());

    std::vector<std::string> skinFolders;
    for(const auto &dir : {env->getFoldersInFolder(MCENGINE_DATA_DIR "skins/"), env->getFoldersInFolder(skinFolder)}) {
        for(const auto &skin : dir) {
            skinFolders.push_back(skin);
        }
    }

    if(cv::sort_skins_cleaned.getBool()) {
        // Sort skins only by alphanum characters, ignore the others
        std::ranges::sort(skinFolders, [](const std::string &a, const std::string &b) {
            int i = 0;
            int j = 0;
            while(i < a.length() && j < b.length()) {
                if(!isalnum((u8)a[i])) {
                    i++;
                    continue;
                }
                if(!isalnum((u8)b[j])) {
                    j++;
                    continue;
                }
                char la = tolower(a[i]);
                char lb = tolower(b[j]);
                if(la != lb) return la < lb;

                i++;
                j++;
            }

            return false;
        });
    } else {
        // more stable-like sorting (i.e. "-     Cookiezi" comes before "Cookiezi")
        std::ranges::stable_sort(skinFolders, [](const std::string &a, const std::string &b) {
            return strcasecmp(a.c_str(), b.c_str()) < 0;
        });
    }

    if(skinFolders.size() > 0) {
        if(this->bVisible) {
            this->contextMenu->setPos(this->skinSelectLocalButton->getPos());
            this->contextMenu->setRelPos(this->skinSelectLocalButton->getRelPos());
            this->options->setScrollSizeToContent();
        } else {
            // Put it 50px from top, we'll move it later
            this->contextMenu->setPos(Vector2{0, 100});
        }

        this->contextMenu->begin();

        CBaseUIButton *buttonDefault = this->contextMenu->addButton("default");
        if(cv::skin.getString() == "default") buttonDefault->setTextBrightColor(0xff00ff00);

        for(int i = 0; i < skinFolders.size(); i++) {
            if(skinFolders[i].compare(".") == 0 || skinFolders[i].compare("..") == 0) continue;

            CBaseUIButton *button = this->contextMenu->addButton(skinFolders[i].c_str());
            if(skinFolders[i].compare(cv::skin.getString()) == 0) button->setTextBrightColor(0xff00ff00);
        }
        this->contextMenu->end(false, !this->bVisible);
        this->contextMenu->setClickCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSkinSelect2));

        if(!this->bVisible) {
            // Center context menu
            this->contextMenu->setPos(Vector2{
                osu->getScreenWidth() / 2.f - this->contextMenu->getSize().x / 2.f,
                osu->getScreenHeight() / 2.f - this->contextMenu->getSize().y / 2.f,
            });
        }
    } else {
        osu->getNotificationOverlay()->addToast("Error: Couldn't find any skins", 0xffff0000);
        this->options->scrollToTop();
        this->fOsuFolderTextboxInvalidAnim = engine->getTime() + 3.0f;
    }
}

void OptionsMenu::onSkinSelect2(const UString &skinName, int  /*id*/) {
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
    std::vector<Vector2> resolutions;

    // 4:3
    resolutions.emplace_back(800, 600);
    resolutions.emplace_back(1024, 768);
    resolutions.emplace_back(1152, 864);
    resolutions.emplace_back(1280, 960);
    resolutions.emplace_back(1280, 1024);
    resolutions.emplace_back(1600, 1200);
    resolutions.emplace_back(1920, 1440);
    resolutions.emplace_back(2560, 1920);

    // 16:9 and 16:10
    resolutions.emplace_back(1024, 600);
    resolutions.emplace_back(1280, 720);
    resolutions.emplace_back(1280, 768);
    resolutions.emplace_back(1280, 800);
    resolutions.emplace_back(1360, 768);
    resolutions.emplace_back(1366, 768);
    resolutions.emplace_back(1440, 900);
    resolutions.emplace_back(1600, 900);
    resolutions.emplace_back(1600, 1024);
    resolutions.emplace_back(1680, 1050);
    resolutions.emplace_back(1920, 1080);
    resolutions.emplace_back(1920, 1200);
    resolutions.emplace_back(2560, 1440);
    resolutions.emplace_back(2560, 1600);
    resolutions.emplace_back(3840, 2160);
    resolutions.emplace_back(5120, 2880);
    resolutions.emplace_back(7680, 4320);

    // wtf
    resolutions.emplace_back(4096, 2160);

    // get custom resolutions
    std::vector<Vector2> customResolutions;
    std::ifstream customres(MCENGINE_DATA_DIR "cfg/customres.cfg");
    std::string curLine;
    while(std::getline(customres, curLine)) {
        const char *curLineChar = curLine.c_str();
        if(curLine.find("//") == std::string::npos)  // ignore comments
        {
            int width = 0;
            int height = 0;
            if(sscanf(curLineChar, "%ix%i\n", &width, &height) == 2) {
                if(width > 319 && height > 239)  // 320x240 sanity check
                    customResolutions.emplace_back(width, height);
            }
        }
    }

    // native resolution at the end
    Vector2 nativeResolution = env->getNativeScreenSize();
    bool containsNativeResolution = false;
    for(int i = 0; i < resolutions.size(); i++) {
        if(resolutions[i] == nativeResolution) {
            containsNativeResolution = true;
            break;
        }
    }
    if(!containsNativeResolution) resolutions.push_back(nativeResolution);

    // build context menu
    this->contextMenu->begin();
    for(int i = 0; i < resolutions.size(); i++) {
        if(resolutions[i].x > nativeResolution.x || resolutions[i].y > nativeResolution.y) continue;

        const UString resolution =
            UString::format("%ix%i", (int)std::round(resolutions[i].x), (int)std::round(resolutions[i].y));
        CBaseUIButton *button = this->contextMenu->addButton(resolution);
        if(this->resolutionLabel != NULL && resolution == this->resolutionLabel->getText())
            button->setTextBrightColor(0xff00ff00);
    }
    for(int i = 0; i < customResolutions.size(); i++) {
        this->contextMenu->addButton(
            UString::format("%ix%i", (int)std::round(customResolutions[i].x), (int)std::round(customResolutions[i].y)));
    }
    this->contextMenu->end(false, false);
    this->contextMenu->setClickCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onResolutionSelect2));

    // reposition context monu
    f32 menu_width = this->contextMenu->getSize().x;
    f32 btn_width = this->resolutionSelectButton->getSize().x;
    f32 menu_offset = btn_width / 2.f - menu_width / 2.f;
    this->contextMenu->setPos(this->resolutionSelectButton->getPos().x + menu_offset,
                              this->resolutionSelectButton->getPos().y);
    this->contextMenu->setRelPos(this->resolutionSelectButton->getRelPos().x + menu_offset,
                                 this->resolutionSelectButton->getRelPos().y);
    this->options->setScrollSizeToContent();
}

void OptionsMenu::onResolutionSelect2(const UString &resolution, int  /*id*/) {
    if(env->isFullscreen()) {
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

    std::vector<OUTPUT_DEVICE> outputDevices = soundEngine->getOutputDevices();

    // build context menu
    this->contextMenu->setPos(this->outputDeviceSelectButton->getPos());
    this->contextMenu->setRelPos(this->outputDeviceSelectButton->getRelPos());
    this->contextMenu->begin();
    for(auto device : outputDevices) {
        CBaseUIButton *button = this->contextMenu->addButton(device.name);
        if(device.name == soundEngine->getOutputDeviceName()) button->setTextBrightColor(0xff00ff00);
    }
    this->contextMenu->end(false, true);
    this->contextMenu->setClickCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onOutputDeviceSelect2));
    this->options->setScrollSizeToContent();
}

void OptionsMenu::onOutputDeviceSelect2(const UString &outputDeviceName, int  /*id*/) {
    if(outputDeviceName == soundEngine->getOutputDeviceName()) {
        debugLog("SoundEngine::setOutputDevice() \"%s\" already is the current device.\n", outputDeviceName.toUtf8());
        return;
    }

    for(auto device : soundEngine->getOutputDevices()) {
        if(device.name != outputDeviceName) continue;

        soundEngine->setOutputDevice(device);
        return;
    }

    debugLog("SoundEngine::setOutputDevice() couldn't find output device \"%s\"!\n", outputDeviceName.toUtf8());
}

void OptionsMenu::onOutputDeviceResetClicked() { soundEngine->setOutputDevice(soundEngine->getDefaultDevice()); }

void OptionsMenu::onOutputDeviceResetUpdate() {
    if(this->outputDeviceResetButton != NULL) {
        this->outputDeviceResetButton->setEnabled(soundEngine->getOutputDeviceName() !=
                                                  soundEngine->getDefaultDevice().name);
    }
}

void OptionsMenu::onOutputDeviceRestart() { soundEngine->restart(); }

void OptionsMenu::onLogInClicked() {
    if(this->logInButton->is_loading) return;
    soundEngine->play(osu->getSkin()->getMenuHit());

    if(bancho->user_id > 0) {
        disconnect();
    } else {
        reconnect();
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
    this->contextMenu->setClickCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onNotelockSelect2));
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
    if(this->notelockSelectResetButton != NULL)
        this->notelockSelectResetButton->setEnabled(cv::notelock_type.getInt() !=
                                                    (int)cv::notelock_type.getDefaultFloat());
}

void OptionsMenu::onCheckboxChange(CBaseUICheckbox *checkbox) {
    for(int i = 0; i < this->elements.size(); i++) {
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == checkbox) {
                if(this->elements[i].cvar != NULL) this->elements[i].cvar->setValue(checkbox->isChecked());

                this->onResetUpdate(this->elements[i].resetButton);

                break;
            }
        }
    }
}

void OptionsMenu::onSliderChange(CBaseUISlider *slider) {
    for(int i = 0; i < this->elements.size(); i++) {
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == slider) {
                if(this->elements[i].cvar != NULL)
                    this->elements[i].cvar->setValue(std::round(slider->getFloat() * 100.0f) /
                                                     100.0f);  // round to 2 decimal places

                if(this->elements[i].elements.size() == 3) {
                    auto *labelPointer = dynamic_cast<CBaseUILabel *>(this->elements[i].elements[2]);
                    labelPointer->setText(this->elements[i].cvar->getString().c_str());
                }

                this->onResetUpdate(this->elements[i].resetButton);

                break;
            }
        }
    }
}

void OptionsMenu::onSliderChangeOneDecimalPlace(CBaseUISlider *slider) {
    for(int i = 0; i < this->elements.size(); i++) {
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == slider) {
                if(this->elements[i].cvar != NULL)
                    this->elements[i].cvar->setValue(std::round(slider->getFloat() * 10.0f) /
                                                     10.0f);  // round to 1 decimal place

                if(this->elements[i].elements.size() == 3) {
                    auto *labelPointer = dynamic_cast<CBaseUILabel *>(this->elements[i].elements[2]);
                    labelPointer->setText(this->elements[i].cvar->getString().c_str());
                }

                this->onResetUpdate(this->elements[i].resetButton);

                break;
            }
        }
    }
}

void OptionsMenu::onSliderChangeTwoDecimalPlaces(CBaseUISlider *slider) {
    for(int i = 0; i < this->elements.size(); i++) {
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == slider) {
                if(this->elements[i].cvar != NULL)
                    this->elements[i].cvar->setValue(std::round(slider->getFloat() * 100.0f) /
                                                     100.0f);  // round to 2 decimal places

                if(this->elements[i].elements.size() == 3) {
                    auto *labelPointer = dynamic_cast<CBaseUILabel *>(this->elements[i].elements[2]);
                    labelPointer->setText(this->elements[i].cvar->getString().c_str());
                }

                this->onResetUpdate(this->elements[i].resetButton);

                break;
            }
        }
    }
}

void OptionsMenu::onSliderChangeOneDecimalPlaceMeters(CBaseUISlider *slider) {
    for(int i = 0; i < this->elements.size(); i++) {
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == slider) {
                if(this->elements[i].cvar != NULL)
                    this->elements[i].cvar->setValue(std::round(slider->getFloat() * 10.0f) /
                                                     10.0f);  // round to 1 decimal place

                if(this->elements[i].elements.size() == 3) {
                    auto *labelPointer = dynamic_cast<CBaseUILabel *>(this->elements[i].elements[2]);
                    labelPointer->setText(UString::format("%.1f m", this->elements[i].cvar->getFloat()));
                }

                this->onResetUpdate(this->elements[i].resetButton);

                break;
            }
        }
    }
}

void OptionsMenu::onSliderChangeInt(CBaseUISlider *slider) {
    for(int i = 0; i < this->elements.size(); i++) {
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == slider) {
                if(this->elements[i].cvar != NULL)
                    this->elements[i].cvar->setValue(std::round(slider->getFloat()));  // round to int

                if(this->elements[i].elements.size() == 3) {
                    auto *labelPointer = dynamic_cast<CBaseUILabel *>(this->elements[i].elements[2]);
                    labelPointer->setText(this->elements[i].cvar->getString().c_str());
                }

                this->onResetUpdate(this->elements[i].resetButton);

                break;
            }
        }
    }
}

void OptionsMenu::onSliderChangeIntMS(CBaseUISlider *slider) {
    for(int i = 0; i < this->elements.size(); i++) {
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == slider) {
                if(this->elements[i].cvar != NULL)
                    this->elements[i].cvar->setValue(std::round(slider->getFloat()));  // round to int

                if(this->elements[i].elements.size() == 3) {
                    auto *labelPointer = dynamic_cast<CBaseUILabel *>(this->elements[i].elements[2]);
                    std::string text = this->elements[i].cvar->getString();
                    text.append(" ms");
                    labelPointer->setText(text.c_str());
                }

                this->onResetUpdate(this->elements[i].resetButton);

                break;
            }
        }
    }
}

void OptionsMenu::onSliderChangeFloatMS(CBaseUISlider *slider) {
    for(int i = 0; i < this->elements.size(); i++) {
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == slider) {
                if(this->elements[i].cvar != NULL) this->elements[i].cvar->setValue(slider->getFloat());

                if(this->elements[i].elements.size() == 3) {
                    auto *labelPointer = dynamic_cast<CBaseUILabel *>(this->elements[i].elements[2]);
                    UString text = UString::format("%i", (int)std::round(this->elements[i].cvar->getFloat() * 1000.0f));
                    text.append(" ms");
                    labelPointer->setText(text);
                }

                this->onResetUpdate(this->elements[i].resetButton);

                break;
            }
        }
    }
}

void OptionsMenu::onSliderChangePercent(CBaseUISlider *slider) {
    for(int i = 0; i < this->elements.size(); i++) {
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == slider) {
                if(this->elements[i].cvar != NULL)
                    this->elements[i].cvar->setValue(std::round(slider->getFloat() * 100.0f) / 100.0f);

                if(this->elements[i].elements.size() == 3) {
                    int percent = std::round(this->elements[i].cvar->getFloat() * 100.0f);

                    auto *labelPointer = dynamic_cast<CBaseUILabel *>(this->elements[i].elements[2]);
                    labelPointer->setText(UString::format("%i%%", percent));
                }

                this->onResetUpdate(this->elements[i].resetButton);

                break;
            }
        }
    }
}

void OptionsMenu::onKeyBindingButtonPressed(CBaseUIButton *button) {
    for(int i = 0; i < this->elements.size(); i++) {
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == button) {
                if(this->elements[i].cvar != NULL) {
                    this->waitingKey = this->elements[i].cvar;

                    UString notificationText = "Press new key for ";
                    notificationText.append(button->getText());
                    notificationText.append(":");

                    const bool waitForKey = true;
                    osu->getNotificationOverlay()->addNotification(notificationText, 0xffffffff, waitForKey);
                    osu->getNotificationOverlay()->setDisallowWaitForKeyLeftClick(
                        !(dynamic_cast<OptionsMenuKeyBindButton *>(button)->isLeftMouseClickBindingAllowed()));
                }
                break;
            }
        }
    }
}

void OptionsMenu::onKeyUnbindButtonPressed(CBaseUIButton *button) {
    soundEngine->play(osu->getSkin()->getCheckOff());

    for(int i = 0; i < this->elements.size(); i++) {
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == button) {
                if(this->elements[i].cvar != NULL) {
                    this->elements[i].cvar->setValue(0.0f);
                }
                break;
            }
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
    for(int i = 0; i < this->elements.size(); i++) {
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == slider) {
                if(this->elements[i].cvar != NULL) {
                    this->elements[i].cvar->setValue(std::round(slider->getFloat() * 100.0f) /
                                                     100.0f);  // round to 2 decimal places
                }

                if(this->elements[i].elements.size() == 3) {
                    auto *labelPointer = dynamic_cast<CBaseUILabel *>(this->elements[i].elements[2]);

                    int percent = std::round((slider->getPercent()) * 100.0f);
                    UString text = UString::format(percent > 49 ? "%i !" : "%i", percent);
                    labelPointer->setText(text);
                }

                this->onResetUpdate(this->elements[i].resetButton);

                break;
            }
        }
    }
}

void OptionsMenu::onSliderChangeLetterboxingOffset(CBaseUISlider *slider) {
    this->bLetterboxingOffsetUpdateScheduled = true;

    for(int i = 0; i < this->elements.size(); i++) {
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == slider) {
                const float newValue = std::round(slider->getFloat() * 100.0f) / 100.0f;

                if(this->elements[i].elements.size() == 3) {
                    const int percent = std::round(newValue * 100.0f);

                    auto *labelPointer = dynamic_cast<CBaseUILabel *>(this->elements[i].elements[2]);
                    labelPointer->setText(UString::format("%i%%", percent));
                }

                this->letterboxingOffsetResetButton = this->elements[i].resetButton;  // HACKHACK: disgusting

                this->onResetUpdate(this->elements[i].resetButton);

                break;
            }
        }
    }
}

void OptionsMenu::onSliderChangeUIScale(CBaseUISlider *slider) {
    this->bUIScaleChangeScheduled = true;

    for(int i = 0; i < this->elements.size(); i++) {
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == slider) {
                const float newValue = std::round(slider->getFloat() * 100.0f) / 100.0f;

                if(this->elements[i].elements.size() == 3) {
                    const int percent = std::round(newValue * 100.0f);

                    auto *labelPointer = dynamic_cast<CBaseUILabel *>(this->elements[i].elements[2]);
                    labelPointer->setText(UString::format("%i%%", percent));
                }

                this->uiScaleResetButton = this->elements[i].resetButton;  // HACKHACK: disgusting

                this->onResetUpdate(this->elements[i].resetButton);

                break;
            }
        }
    }
}

void OptionsMenu::OpenASIOSettings() {
#ifdef _WIN32
    BASS_ASIO_ControlPanel();
#endif
}

void OptionsMenu::onASIOBufferChange([[maybe_unused]] CBaseUISlider *slider) {
#ifdef _WIN32
    if(!this->updating_layout) this->bASIOBufferChangeScheduled = true;

    BASS_ASIO_INFO info{};
    BASS_ASIO_GetInfo(&info);
    cv::asio_buffer_size.setDefaultFloat(info.bufpref);
    slider->setBounds(info.bufmin, info.bufmax);
    slider->setKeyDelta(info.bufgran == -1 ? info.bufmin : info.bufgran);

    auto bufsize = slider->getInt();
    bufsize = ASIO_clamp(info, bufsize);
    double latency = 1000.0 * (double)bufsize / std::max(BASS_ASIO_GetRate(), 44100.0);

    for(int i = 0; i < this->elements.size(); i++) {
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == slider) {
                if(this->elements[i].elements.size() == 3) {
                    auto *labelPointer = dynamic_cast<CBaseUILabel *>(this->elements[i].elements[2]);
                    if (labelPointer) {
                        UString text = UString::format("%.1f ms", latency);
                        labelPointer->setText(text);
                    }
                }

                this->asioBufferSizeResetButton = this->elements[i].resetButton;  // HACKHACK: disgusting

                break;
            }
        }
    }
#endif
}

void OptionsMenu::onWASAPIBufferChange(CBaseUISlider *slider) {
    if(!this->updating_layout) this->bWASAPIBufferChangeScheduled = true;

    for(int i = 0; i < this->elements.size(); i++) {
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == slider) {
                if(this->elements[i].elements.size() == 3) {
                    auto *labelPointer = dynamic_cast<CBaseUILabel *>(this->elements[i].elements[2]);
                    if (labelPointer) {
                        UString text = UString::format("%i", (int)std::round(slider->getFloat() * 1000.0f));
                        text.append(" ms");
                        labelPointer->setText(text);
                    }
                }

                this->wasapiBufferSizeResetButton = this->elements[i].resetButton;  // HACKHACK: disgusting

                break;
            }
        }
    }
}

void OptionsMenu::onWASAPIPeriodChange(CBaseUISlider *slider) {
    if(!this->updating_layout) this->bWASAPIPeriodChangeScheduled = true;

    for(int i = 0; i < this->elements.size(); i++) {
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == slider) {
                if(this->elements[i].elements.size() == 3) {
                    auto *labelPointer = dynamic_cast<CBaseUILabel *>(this->elements[i].elements[2]);
                    if (labelPointer)
                    {
                        UString text = UString::format("%i", (int)std::round(slider->getFloat() * 1000.0f));
                        text.append(" ms");
                        labelPointer->setText(text);
                    }
                }

                this->wasapiPeriodSizeResetButton = this->elements[i].resetButton;  // HACKHACK: disgusting

                break;
            }
        }
    }
}

void OptionsMenu::onLoudnessNormalizationToggle(CBaseUICheckbox *checkbox) {
    this->onCheckboxChange(checkbox);

    auto music = osu->getSelectedBeatmap()->getMusic();
    if(music != NULL) {
        music->setVolume(osu->getSelectedBeatmap()->getIdealVolume());
    }

    if(cv::normalize_loudness.getBool()) {
        VolNormalization::start_calc(db->loudness_to_calc);
    } else {
        VolNormalization::abort();
    }
}

void OptionsMenu::onNightcoreToggle(CBaseUICheckbox *checkbox) {
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

void OptionsMenu::onHighQualitySlidersConVarChange(const UString &newValue) {
    const bool enabled = newValue.toFloat() > 0;
    for(int i = 0; i < this->elements.size(); i++) {
        bool contains = false;
        for(int e = 0; e < this->elements[i].elements.size(); e++) {
            if(this->elements[i].elements[e] == this->sliderQualitySlider) {
                contains = true;
                break;
            }
        }

        if(contains) {
            // HACKHACK: show/hide quality slider, this is ugly as fuck
            // TODO: containers use setVisible() on all elements. there needs to be a separate API for hiding elements
            // while inside any kind of container
            for(int e = 0; e < this->elements[i].elements.size(); e++) {
                this->elements[i].elements[e]->setEnabled(enabled);

                auto *sliderPointer = dynamic_cast<UISlider *>(this->elements[i].elements[e]);
                auto *labelPointer = dynamic_cast<CBaseUILabel *>(this->elements[i].elements[e]);

                if(sliderPointer != NULL) sliderPointer->setFrameColor(enabled ? 0xffffffff : 0xff000000);
                if(labelPointer != NULL) labelPointer->setTextColor(enabled ? 0xffffffff : 0xff000000);
            }

            // reset value if disabled
            if(!enabled) {
                this->sliderQualitySlider->setValue(this->elements[i].cvar->getDefaultFloat(), false);
                this->elements[i].cvar->setValue(this->elements[i].cvar->getDefaultFloat());
            }

            this->onResetUpdate(this->elements[i].resetButton);

            break;
        }
    }
}

void OptionsMenu::onCategoryClicked(CBaseUIButton *button) {
    // reset search
    this->sSearchString = "";
    this->scheduleSearchUpdate();

    // scroll to category
    auto *categoryButton = dynamic_cast<OptionsMenuCategoryButton *>(button);
    if(categoryButton != NULL) this->options->scrollToElement(categoryButton->getSection(), 0, 100 * Osu::getUIScale());
}

void OptionsMenu::onResetUpdate(CBaseUIButton *button) {
    if(button == NULL) return;

    for(int i = 0; i < this->elements.size(); i++) {
        if(this->elements[i].resetButton == button && this->elements[i].cvar != NULL) {
            switch(this->elements[i].type) {
                case 6:  // checkbox
                    this->elements[i].resetButton->setEnabled(this->elements[i].cvar->getBool() !=
                                                              (bool)this->elements[i].cvar->getDefaultFloat());
                    break;
                case 7:  // slider
                    this->elements[i].resetButton->setEnabled(this->elements[i].cvar->getFloat() !=
                                                              this->elements[i].cvar->getDefaultFloat());
                    break;
            }

            break;
        }
    }
}

void OptionsMenu::onResetClicked(CBaseUIButton *button) {
    if(button == NULL) return;

    for(int i = 0; i < this->elements.size(); i++) {
        if(this->elements[i].resetButton == button && this->elements[i].cvar != NULL) {
            switch(this->elements[i].type) {
                case 6:  // checkbox
                    for(int e = 0; e < this->elements[i].elements.size(); e++) {
                        CBaseUICheckbox *checkboxPointer =
                            dynamic_cast<CBaseUICheckbox *>(this->elements[i].elements[e]);
                        if(checkboxPointer != NULL)
                            checkboxPointer->setChecked((bool)this->elements[i].cvar->getDefaultFloat());
                    }
                    break;
                case 7:  // slider
                    if(this->elements[i].elements.size() == 3) {
                        auto *sliderPointer = dynamic_cast<CBaseUISlider *>(this->elements[i].elements[1]);
                        if(sliderPointer != NULL) {
                            sliderPointer->setValue(this->elements[i].cvar->getDefaultFloat(), false);
                            sliderPointer->fireChangeCallback();
                        }
                    }
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
        for(size_t i = 0; i < this->elements.size(); i++) {
            OptionsMenuResetButton *resetButton = this->elements[i].resetButton;
            if(resetButton != NULL && resetButton->isEnabled()) resetButton->click();
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

void OptionsMenu::onImportSettingsFromStable(CBaseUIButton * /*button*/) { import_settings_from_osu_stable(); }

void OptionsMenu::addSpacer() {
    OPTIONS_ELEMENT e;
    e.type = 0;
    e.cvar = NULL;
    this->elements.push_back(e);
}

CBaseUILabel *OptionsMenu::addSection(const UString &text) {
    CBaseUILabel *label = new CBaseUILabel(0, 0, this->options->getSize().x, 25, text, text);
    // label->setTextColor(0xff58dafe);
    label->setFont(osu->getTitleFont());
    label->setSizeToContent(0, 0);
    label->setTextJustification(CBaseUILabel::TEXT_JUSTIFICATION_RIGHT);
    label->setDrawFrame(false);
    label->setDrawBackground(false);
    this->options->getContainer()->addBaseUIElement(label);

    OPTIONS_ELEMENT e;
    e.elements.push_back(label);
    e.type = 1;
    e.cvar = NULL;
    this->elements.push_back(e);

    return label;
}

CBaseUILabel *OptionsMenu::addSubSection(const UString &text, UString searchTags) {
    CBaseUILabel *label = new CBaseUILabel(0, 0, this->options->getSize().x, 25, text, text);
    label->setFont(osu->getSubTitleFont());
    label->setSizeToContent(0, 0);
    label->setDrawFrame(false);
    label->setDrawBackground(false);
    this->options->getContainer()->addBaseUIElement(label);

    OPTIONS_ELEMENT e;
    e.elements.push_back(label);
    e.type = 2;
    e.cvar = NULL;
    e.searchTags = std::move(searchTags);
    this->elements.push_back(e);

    return label;
}

CBaseUILabel *OptionsMenu::addLabel(const UString &text) {
    CBaseUILabel *label = new CBaseUILabel(0, 0, this->options->getSize().x, 25, text, text);
    label->setSizeToContent(0, 0);
    label->setDrawFrame(false);
    label->setDrawBackground(false);
    this->options->getContainer()->addBaseUIElement(label);

    OPTIONS_ELEMENT e;
    e.elements.push_back(label);
    e.type = 3;
    e.cvar = NULL;
    this->elements.push_back(e);

    return label;
}

UIButton *OptionsMenu::addButton(const UString &text) {
    UIButton *button = new UIButton(0, 0, this->options->getSize().x, 50, text, text);
    button->setColor(0xff0e94b5);
    button->setUseDefaultSkin();
    this->options->getContainer()->addBaseUIElement(button);

    OPTIONS_ELEMENT e;
    e.elements.push_back(button);
    e.type = 4;
    this->elements.push_back(e);

    return button;
}

OptionsMenu::OPTIONS_ELEMENT OptionsMenu::addButton(const UString &text, const UString &labelText,
                                                    bool withResetButton) {
    UIButton *button = new UIButton(0, 0, this->options->getSize().x, 50, text, text);
    button->setColor(0xff0e94b5);
    button->setUseDefaultSkin();
    this->options->getContainer()->addBaseUIElement(button);

    CBaseUILabel *label = new CBaseUILabel(0, 0, this->options->getSize().x, 50, labelText, labelText);
    label->setDrawFrame(false);
    label->setDrawBackground(false);
    this->options->getContainer()->addBaseUIElement(label);

    OPTIONS_ELEMENT e;
    if(withResetButton) {
        e.resetButton = new OptionsMenuResetButton(0, 0, 35, 50, "", "");
    }
    e.elements.push_back(button);
    e.elements.push_back(label);
    e.type = 4;
    this->elements.push_back(e);

    return e;
}

OptionsMenu::OPTIONS_ELEMENT OptionsMenu::addButtonButton(const UString &text1, const UString &text2) {
    UIButton *button = new UIButton(0, 0, this->options->getSize().x, 50, text1, text1);
    button->setColor(0xff0e94b5);
    button->setUseDefaultSkin();
    this->options->getContainer()->addBaseUIElement(button);

    UIButton *button2 = new UIButton(0, 0, this->options->getSize().x, 50, text2, text2);
    button2->setColor(0xff0e94b5);
    button2->setUseDefaultSkin();
    this->options->getContainer()->addBaseUIElement(button2);

    OPTIONS_ELEMENT e;
    e.elements.push_back(button);
    e.elements.push_back(button2);
    e.type = 4;
    this->elements.push_back(e);

    return e;
}

OptionsMenu::OPTIONS_ELEMENT OptionsMenu::addButtonButtonLabel(const UString &text1, const UString &text2,
                                                               const UString &labelText, bool withResetButton) {
    UIButton *button = new UIButton(0, 0, this->options->getSize().x, 50, text1, text1);
    button->setColor(0xff0e94b5);
    button->setUseDefaultSkin();
    this->options->getContainer()->addBaseUIElement(button);

    UIButton *button2 = new UIButton(0, 0, this->options->getSize().x, 50, text2, text2);
    button2->setColor(0xff0e94b5);
    button2->setUseDefaultSkin();
    this->options->getContainer()->addBaseUIElement(button2);

    CBaseUILabel *label = new CBaseUILabel(0, 0, this->options->getSize().x, 50, labelText, labelText);
    label->setDrawFrame(false);
    label->setDrawBackground(false);
    this->options->getContainer()->addBaseUIElement(label);

    OPTIONS_ELEMENT e;
    if(withResetButton) {
        e.resetButton = new OptionsMenuResetButton(0, 0, 35, 50, "", "");
    }
    e.elements.push_back(button);
    e.elements.push_back(button2);
    e.elements.push_back(label);
    e.type = 4;
    this->elements.push_back(e);

    return e;
}

OptionsMenuKeyBindButton *OptionsMenu::addKeyBindButton(const UString &text, ConVar *cvar) {
    /// UString unbindIconString; unbindIconString.insert(0, Icons::UNDO);
    UIButton *unbindButton = new UIButton(0, 0, this->options->getSize().x, 50, text, "");
    unbindButton->setTooltipText("Unbind");
    unbindButton->setColor(0x77ff0000);
    unbindButton->setUseDefaultSkin();
    unbindButton->setClickCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onKeyUnbindButtonPressed));
    /// unbindButton->setFont(osu->getFontIcons());
    this->options->getContainer()->addBaseUIElement(unbindButton);

    OptionsMenuKeyBindButton *bindButton =
        new OptionsMenuKeyBindButton(0, 0, this->options->getSize().x, 50, text, text);
    bindButton->setColor(0xff0e94b5);
    bindButton->setUseDefaultSkin();
    bindButton->setClickCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onKeyBindingButtonPressed));
    this->options->getContainer()->addBaseUIElement(bindButton);

    OptionsMenuKeyBindLabel *label =
        new OptionsMenuKeyBindLabel(0, 0, this->options->getSize().x, 50, "", "", cvar, bindButton);
    label->setDrawFrame(false);
    label->setDrawBackground(false);
    this->options->getContainer()->addBaseUIElement(label);

    OPTIONS_ELEMENT e;
    e.elements.push_back(unbindButton);
    e.elements.push_back(bindButton);
    e.elements.push_back(label);
    e.type = 5;
    e.cvar = cvar;
    this->elements.push_back(e);

    return bindButton;
}

CBaseUICheckbox *OptionsMenu::addCheckbox(const UString &text, ConVar *cvar) {
    return this->addCheckbox(text, "", cvar);
}

CBaseUICheckbox *OptionsMenu::addCheckbox(const UString &text, const UString &tooltipText, ConVar *cvar) {
    UICheckbox *checkbox = new UICheckbox(0, 0, this->options->getSize().x, 50, text, text);
    checkbox->setDrawFrame(false);
    checkbox->setDrawBackground(false);

    if(tooltipText.length() > 0) checkbox->setTooltipText(tooltipText);

    if(cvar != NULL) {
        checkbox->setChecked(cvar->getBool());
        checkbox->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onCheckboxChange));
    }

    this->options->getContainer()->addBaseUIElement(checkbox);

    OPTIONS_ELEMENT e;
    if(cvar != NULL) {
        e.resetButton = new OptionsMenuResetButton(0, 0, 35, 50, "", "");
        e.resetButton->setClickCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onResetClicked));
    }
    e.elements.push_back(checkbox);
    e.type = 6;
    e.cvar = cvar;
    this->elements.push_back(e);

    return checkbox;
}

UISlider *OptionsMenu::addSlider(const UString &text, float min, float max, ConVar *cvar, float label1Width,
                                 bool allowOverscale, bool allowUnderscale) {
    UISlider *slider = new UISlider(0, 0, 100, 50, text);
    slider->setAllowMouseWheel(false);
    slider->setBounds(min, max);
    slider->setLiveUpdate(true);
    if(cvar != NULL) {
        slider->setValue(cvar->getFloat(), false);
        slider->setChangeCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onSliderChange));
    }
    this->options->getContainer()->addBaseUIElement(slider);

    CBaseUILabel *label1 = new CBaseUILabel(0, 0, this->options->getSize().x, 50, text, text);
    label1->setDrawFrame(false);
    label1->setDrawBackground(false);
    label1->setWidthToContent();
    if(label1Width > 1) label1->setSizeX(label1Width);
    label1->setRelSizeX(label1->getSize().x);
    this->options->getContainer()->addBaseUIElement(label1);

    CBaseUILabel *label2 = new CBaseUILabel(0, 0, this->options->getSize().x, 50, "", "8.81");
    label2->setDrawFrame(false);
    label2->setDrawBackground(false);
    label2->setWidthToContent();
    label2->setRelSizeX(label2->getSize().x);
    this->options->getContainer()->addBaseUIElement(label2);

    OPTIONS_ELEMENT e;
    if(cvar != NULL) {
        e.resetButton = new OptionsMenuResetButton(0, 0, 35, 50, "", "");
        e.resetButton->setClickCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onResetClicked));
    }
    e.elements.push_back(label1);
    e.elements.push_back(slider);
    e.elements.push_back(label2);
    e.type = 7;
    e.cvar = cvar;
    e.label1Width = label1Width;
    e.relSizeDPI = label1->getFont()->getDPI();
    e.allowOverscale = allowOverscale;
    e.allowUnderscale = allowUnderscale;
    this->elements.push_back(e);

    return slider;
}

CBaseUITextbox *OptionsMenu::addTextbox(UString text, ConVar *cvar) {
    CBaseUITextbox *textbox = new CBaseUITextbox(0, 0, this->options->getSize().x, 40, "");
    textbox->setText(std::move(text));
    this->options->getContainer()->addBaseUIElement(textbox);

    OPTIONS_ELEMENT e;
    e.elements.push_back(textbox);
    e.type = 8;
    e.cvar = cvar;
    this->elements.push_back(e);

    return textbox;
}

CBaseUITextbox *OptionsMenu::addTextbox(UString text, const UString &labelText, ConVar *cvar) {
    CBaseUITextbox *textbox = new CBaseUITextbox(0, 0, this->options->getSize().x, 40, "");
    textbox->setText(std::move(text));
    this->options->getContainer()->addBaseUIElement(textbox);

    CBaseUILabel *label = new CBaseUILabel(0, 0, this->options->getSize().x, 40, labelText, labelText);
    label->setDrawFrame(false);
    label->setDrawBackground(false);
    label->setWidthToContent();
    this->options->getContainer()->addBaseUIElement(label);

    OPTIONS_ELEMENT e;
    e.elements.push_back(label);
    e.elements.push_back(textbox);
    e.type = 8;
    e.cvar = cvar;
    this->elements.push_back(e);

    return textbox;
}

CBaseUIElement *OptionsMenu::addSkinPreview() {
    CBaseUIElement *skinPreview = new OptionsMenuSkinPreviewElement(0, 0, 0, 200, "skincirclenumberhitresultpreview");
    this->options->getContainer()->addBaseUIElement(skinPreview);

    OPTIONS_ELEMENT e;
    e.elements.push_back(skinPreview);
    e.type = 9;
    e.cvar = NULL;
    this->elements.push_back(e);

    return skinPreview;
}

CBaseUIElement *OptionsMenu::addSliderPreview() {
    CBaseUIElement *sliderPreview = new OptionsMenuSliderPreviewElement(0, 0, 0, 200, "skinsliderpreview");
    this->options->getContainer()->addBaseUIElement(sliderPreview);

    OPTIONS_ELEMENT e;
    e.elements.push_back(sliderPreview);
    e.type = 9;
    e.cvar = NULL;
    this->elements.push_back(e);

    return sliderPreview;
}

OptionsMenuCategoryButton *OptionsMenu::addCategory(CBaseUIElement *section, wchar_t icon) {
    UString iconString;
    iconString.insert(0, icon);
    OptionsMenuCategoryButton *button = new OptionsMenuCategoryButton(section, 0, 0, 50, 50, "", iconString);
    button->setFont(osu->getFontIcons());
    button->setDrawBackground(false);
    button->setDrawFrame(false);
    button->setClickCallback(fastdelegate::MakeDelegate(this, &OptionsMenu::onCategoryClicked));
    this->categories->getContainer()->addBaseUIElement(button);
    this->categoryButtons.push_back(button);

    return button;
}

void OptionsMenu::save() {
    if(!cv::options_save_on_back.getBool()) {
        debugLog("DEACTIVATED SAVE!!!! @ %f\n", engine->getTime());
        return;
    }

    this->updateOsuFolder();
    this->updateFposuDPI();
    this->updateFposuCMper360();

    debugLog("Osu: Saving user config file ...\n");

    UString userConfigFile = MCENGINE_DATA_DIR "cfg/osu.cfg";

    std::vector<UString> user_lines;
    {
        File in(userConfigFile.toUtf8());
        while(in.canRead()) {
            UString line = in.readLine().c_str();
            if(line == UString("")) continue;
            if(line.startsWith("#")) {
                user_lines.push_back(line);
                continue;
            }

            bool cvar_found = false;
            auto parts = line.split(" ");
            for(auto convar : convar->getConVarArray()) {
                if(UString{convar->getName()} == parts[0]) {
                    cvar_found = true;
                    break;
                }
            }

            if(!cvar_found) {
                user_lines.push_back(line);
                continue;
            }
        }
    }

    {
        std::ofstream out(userConfigFile.toUtf8());
        if(!out.good()) {
            engine->showMessageError("Osu Error", "Couldn't write user config file!");
            return;
        }

        for(auto line : user_lines) {
            out << line.toUtf8() << "\n";
        }
        out << "\n";

        if(this->fullscreenCheckbox->isChecked()) {
            out << cv::cmd::fullscreen.getName() << "\n";
        }
        out << "\n";

        for(auto convar : convar->getConVarArray()) {
            if(!convar->hasValue()) continue;
            if(convar->getString() == convar->getDefaultString()) continue;
            out << convar->getName() << " " << convar->getString() << "\n";
        }

        out.close();
    }
}

void OptionsMenu::openAndScrollToSkinSection() {
    const bool wasVisible = this->isVisible();
    if(!wasVisible) osu->toggleOptionsMenu();

    if(!this->skinSelectLocalButton->isVisible() || !wasVisible)
        this->options->scrollToElement(this->skinSection, 0, 100 * Osu::getUIScale());
}
