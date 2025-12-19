// Copyright (c) 2016, PG, All rights reserved.
#include "SongBrowser.h"

#include "AnimationHandler.h"
#include "BackgroundImageHandler.h"
#include "Bancho.h"
#include "BanchoLeaderboard.h"
#include "BanchoNetworking.h"
#include "BeatmapInterface.h"
#include "BeatmapCarousel.h"
#include "BottomBar.h"
#include "CBaseUIContainer.h"
#include "CBaseUILabel.h"
#include "CBaseUIScrollView.h"
#include "Chat.h"
#include "CollectionButton.h"
#include "Collections.h"
#include "OsuConVars.h"
#include "Database.h"
#include "MakeDelegateWrapper.h"
#include "Environment.h"
#include "DatabaseBeatmap.h"
#include "DirectoryWatcher.h"
#include "Downloader.h"
#include "Engine.h"
#include "HUD.h"
#include "Font.h"
#include "Sound.h"
#include "Icons.h"
#include "InfoLabel.h"
#include "KeyBindings.h"
#include "Keyboard.h"
#include "AsyncPPCalculator.h"
#include "LoudnessCalcThread.h"
#include "MainMenu.h"
#include "MapCalcThread.h"
#include "ModSelector.h"
#include "Mouse.h"
#include "NotificationOverlay.h"
#include "OptionsMenu.h"
#include "Osu.h"
#include "RankingScreen.h"
#include "ResourceManager.h"
#include "RichPresence.h"
#include "RoomScreen.h"
#include "ScoreButton.h"
#include "ScoreConverterThread.h"
#include "Skin.h"
#include "SkinImage.h"
#include "SongButton.h"
#include "SongDifficultyButton.h"
#include "SoundEngine.h"
#include "Timing.h"
#include "UIBackButton.h"
#include "UIContextMenu.h"
#include "UISearchOverlay.h"
#include "UserCard.h"
#include "VertexArrayObject.h"
#include "SString.h"
#include "crypto.h"
#include "Logging.h"

#include <algorithm>
#include <memory>
#include <charconv>
#include <cwctype>
#include <utility>

const Color highlightColor = argb(255, 0, 255, 0);
const Color defaultColor = argb(255, 255, 255, 255);

// XXX: remove this
f32 SongBrowser::getUIScale() {
    auto screen = osu->getVirtScreenSize();
    bool is_widescreen = (screen.x / screen.y) > (4.f / 3.f);
    return screen.x / (is_widescreen ? 1366.f : 1024.f);
}

// Because we draw skin elements 'manually' to enforce the correct scaling,
// this helper function automatically adjusts for 2x image resolution.
f32 SongBrowser::getSkinScale(SkinImage *img) { return SongBrowser::getUIScale() * (img->is_2x ? 0.5f : 1.f); }

vec2 SongBrowser::getSkinDimensions(SkinImage *img) {
    return img->getImageSizeForCurrentFrame() * SongBrowser::getSkinScale(img);
}

class SongBrowserBackgroundSearchMatcher final : public Resource {
    NOCOPY_NOMOVE(SongBrowserBackgroundSearchMatcher)
   public:
    SongBrowserBackgroundSearchMatcher() : Resource() {}
    ~SongBrowserBackgroundSearchMatcher() override { this->destroy(); }

    [[nodiscard]] inline bool isDead() const { return this->bDead.load(std::memory_order_acquire); }
    inline void kill() { this->bDead = true; }
    inline void revive() { this->bDead = false; }

    inline void setSongButtonsAndSearchString(const std::vector<SongButton *> &songButtons, const UString &searchString,
                                              const UString &hardcodedSearchString) {
        this->songButtons = songButtons;

        this->sSearchString.clear();
        if(!hardcodedSearchString.isEmpty()) {
            this->sSearchString.append(hardcodedSearchString);
            this->sSearchString.append(u' ');
        }
        this->sSearchString.append(searchString);
        // do case-insensitive searches
        this->sSearchString.lowerCase();
        this->sHardcodedSearchString = hardcodedSearchString;
    }

    [[nodiscard]] inline Type getResType() const override { return APPDEFINED; }  // TODO: handle this better?

   protected:
    inline void init() override { this->setReady(true); }

    inline void initAsync() override {
        if(this->bDead.load(std::memory_order_acquire)) {
            this->setAsyncReady(true);
            return;
        }

        // flag matches across entire database
        const std::vector<std::string> searchStringTokens =
            SString::split<std::string>(this->sSearchString.utf8View(), ' ');  // make a copy
        for(auto &songButton : this->songButtons) {
            // this is unsafe, children could be getting sorted while we do this
            std::vector<SongButton *> children = songButton->getChildren();
            if(children.size() > 0) {
                for(auto c : children) {
                    const bool match = SongBrowser::searchMatcher(c->getDatabaseBeatmap(), searchStringTokens);
                    c->setIsSearchMatch(match);
                }
            } else {
                const bool match = SongBrowser::searchMatcher(songButton->getDatabaseBeatmap(), searchStringTokens);
                songButton->setIsSearchMatch(match);
            }

            // cancellation point
            if(this->bDead.load(std::memory_order_acquire)) break;
        }

        this->setAsyncReady(true);
    }

    inline void destroy() override { ; }

   private:
    std::atomic<bool> bDead{true};  // NOTE: start dead! need to revive() before use

    UString sSearchString{u""};
    UString sHardcodedSearchString{u""};
    std::vector<SongButton *> songButtons;
};

class ScoresStillLoadingElement final : public CBaseUILabel {
   public:
    ScoresStillLoadingElement(const UString &text) : CBaseUILabel(0, 0, 0, 0, "", text) {
        this->sIconString.insert(0, Icons::GLOBE);
    }

    void drawText() override {
        // draw icon
        const float iconScale = 0.6f;
        McFont *iconFont = osu->getFontIcons();
        int iconWidth = 0;
        g->pushTransform();
        {
            const float scale = (this->vSize.y / iconFont->getHeight()) * iconScale;
            const float paddingLeft = scale * 15;

            iconWidth = paddingLeft + iconFont->getStringWidth(this->sIconString) * scale;

            g->scale(scale, scale);
            g->translate((int)(this->vPos.x + paddingLeft),
                         (int)(this->vPos.y + this->vSize.y / 2 + iconFont->getHeight() * scale / 2));
            g->setColor(0xffffffff);
            g->drawString(iconFont, this->sIconString);
        }
        g->popTransform();

        // draw text
        const float textScale = 0.4f;
        McFont *textFont = osu->getSongBrowserFont();
        g->pushTransform();
        {
            const float stringWidth = textFont->getStringWidth(this->sText);

            const float scale = ((this->vSize.x - iconWidth) / stringWidth) * textScale;

            g->scale(scale, scale);
            g->translate((int)(this->vPos.x + iconWidth + (this->vSize.x - iconWidth) / 2 - stringWidth * scale / 2),
                         (int)(this->vPos.y + this->vSize.y / 2 + textFont->getHeight() * scale / 2));
            g->setColor(0xff02c3e5);
            g->drawString(textFont, this->sText);
        }
        g->popTransform();
    }

   private:
    UString sIconString;
};

class NoRecordsSetElement final : public CBaseUILabel {
   public:
    NoRecordsSetElement(const UString &text) : CBaseUILabel(0, 0, 0, 0, "", text) {
        this->sIconString.insert(0, Icons::TROPHY);
    }

    void drawText() override {
        // draw icon
        const float iconScale = 0.6f;
        McFont *iconFont = osu->getFontIcons();
        int iconWidth = 0;
        g->pushTransform();
        {
            const float scale = (this->vSize.y / iconFont->getHeight()) * iconScale;
            const float paddingLeft = scale * 15;

            iconWidth = paddingLeft + iconFont->getStringWidth(this->sIconString) * scale;

            g->scale(scale, scale);
            g->translate((int)(this->vPos.x + paddingLeft),
                         (int)(this->vPos.y + this->vSize.y / 2 + iconFont->getHeight() * scale / 2));
            g->setColor(0xffffffff);
            g->drawString(iconFont, this->sIconString);
        }
        g->popTransform();

        // draw text
        const float textScale = 0.6f;
        McFont *textFont = osu->getSongBrowserFont();
        g->pushTransform();
        {
            const float stringWidth = textFont->getStringWidth(this->sText);

            const float scale = ((this->vSize.x - iconWidth) / stringWidth) * textScale;

            g->scale(scale, scale);
            g->translate((int)(this->vPos.x + iconWidth + (this->vSize.x - iconWidth) / 2 - stringWidth * scale / 2),
                         (int)(this->vPos.y + this->vSize.y / 2 + textFont->getHeight() * scale / 2));
            g->setColor(0xff02c3e5);
            g->drawString(textFont, this->sText);
        }
        g->popTransform();
    }

   private:
    UString sIconString;
};

// used also by SongButton
bool SongBrowser::sort_by_difficulty(SongButton const *a, SongButton const *b) {
    const auto *aPtr = a->getDatabaseBeatmap(), *bPtr = b->getDatabaseBeatmap();
    if((aPtr == nullptr) || (bPtr == nullptr)) return (aPtr == nullptr) < (bPtr == nullptr);

    float stars1 = aPtr->getStarsNomod();
    float stars2 = bPtr->getStarsNomod();
    if(stars1 != stars2) return stars1 < stars2;

    float diff1 = (aPtr->getAR() + 1) * (aPtr->getCS() + 1) * (aPtr->getHP() + 1) * (aPtr->getOD() + 1) *
                  (std::max(aPtr->getMostCommonBPM(), 1));
    float diff2 = (bPtr->getAR() + 1) * (bPtr->getCS() + 1) * (bPtr->getHP() + 1) * (bPtr->getOD() + 1) *
                  (std::max(bPtr->getMostCommonBPM(), 1));

    if(diff1 == diff2) return false;
    return diff1 < diff2;
}

// not used anywhere else
bool SongBrowser::sort_by_artist(SongButton const *a, SongButton const *b) {
    const auto *aPtr = a->getDatabaseBeatmap(), *bPtr = b->getDatabaseBeatmap();
    if((aPtr == nullptr) || (bPtr == nullptr)) return (aPtr == nullptr) < (bPtr == nullptr);

    const auto &artistA{aPtr->getArtistLatin()};
    const auto &artistB{bPtr->getArtistLatin()};

    i32 cmp = strcasecmp(artistA.c_str(), artistB.c_str());
    if(cmp == 0) return sort_by_difficulty(a, b);
    return cmp < 0;
}

bool SongBrowser::sort_by_bpm(SongButton const *a, SongButton const *b) {
    const auto *aPtr = a->getDatabaseBeatmap(), *bPtr = b->getDatabaseBeatmap();
    if((aPtr == nullptr) || (bPtr == nullptr)) return (aPtr == nullptr) < (bPtr == nullptr);

    int bpm1 = aPtr->getMostCommonBPM();
    int bpm2 = bPtr->getMostCommonBPM();
    if(bpm1 == bpm2) return sort_by_difficulty(a, b);
    return bpm1 < bpm2;
}

bool SongBrowser::sort_by_creator(SongButton const *a, SongButton const *b) {
    const auto *aPtr = a->getDatabaseBeatmap(), *bPtr = b->getDatabaseBeatmap();
    if((aPtr == nullptr) || (bPtr == nullptr)) return (aPtr == nullptr) < (bPtr == nullptr);

    const auto &creatorA{aPtr->getCreator()};
    const auto &creatorB{bPtr->getCreator()};

    i32 cmp = strcasecmp(creatorA.c_str(), creatorB.c_str());
    if(cmp == 0) return sort_by_difficulty(a, b);
    return cmp < 0;
}

bool SongBrowser::sort_by_date_added(SongButton const *a, SongButton const *b) {
    const auto *aPtr = a->getDatabaseBeatmap(), *bPtr = b->getDatabaseBeatmap();
    if((aPtr == nullptr) || (bPtr == nullptr)) return (aPtr == nullptr) < (bPtr == nullptr);

    i64 time1 = aPtr->last_modification_time;
    i64 time2 = bPtr->last_modification_time;
    if(time1 == time2) return sort_by_difficulty(a, b);
    return time1 > time2;
}

bool SongBrowser::sort_by_grade(SongButton const *a, SongButton const *b) {
    if(a->grade == b->grade) return sort_by_difficulty(a, b);
    return a->grade < b->grade;
}

bool SongBrowser::sort_by_length(SongButton const *a, SongButton const *b) {
    const auto *aPtr = a->getDatabaseBeatmap(), *bPtr = b->getDatabaseBeatmap();
    if((aPtr == nullptr) || (bPtr == nullptr)) return (aPtr == nullptr) < (bPtr == nullptr);

    u32 length1 = aPtr->getLengthMS();
    u32 length2 = bPtr->getLengthMS();
    if(length1 == length2) return sort_by_difficulty(a, b);
    return length1 < length2;
}

bool SongBrowser::sort_by_title(SongButton const *a, SongButton const *b) {
    const auto *aPtr = a->getDatabaseBeatmap(), *bPtr = b->getDatabaseBeatmap();
    if((aPtr == nullptr) || (bPtr == nullptr)) return (aPtr == nullptr) < (bPtr == nullptr);

    const auto &titleA{aPtr->getTitleLatin()};
    const auto &titleB{bPtr->getTitleLatin()};

    i32 cmp = strcasecmp(titleA.c_str(), titleB.c_str());
    if(cmp == 0) return sort_by_difficulty(a, b);
    return cmp < 0;
}

SongBrowser::SongBrowser() : ScreenBackable(), rngalg(crypto::rng::get_rand<u64>()) {
    // convar callback
    cv::songbrowser_search_hardcoded_filter.setCallback(
        [](std::string_view /* oldValue */, std::string_view newValue) -> void {
            if(newValue.length() == 1 && SString::is_wspace_only(newValue))
                cv::songbrowser_search_hardcoded_filter.setValue("");
        });

    // vars
    this->bSongBrowserRightClickScrollCheck = false;
    this->bSongBrowserRightClickScrolling = false;
    this->bNextScrollToSongButtonJumpFixScheduled = false;
    this->bNextScrollToSongButtonJumpFixUseScrollSizeDelta = false;
    this->fNextScrollToSongButtonJumpFixOldScrollSizeY = 0.0f;
    this->fNextScrollToSongButtonJumpFixOldRelPosY = 0.0f;

    this->selectionPreviousSongButton = nullptr;
    this->selectionPreviousSongDiffButton = nullptr;
    this->selectionPreviousCollectionButton = nullptr;

    this->bF1Pressed = false;
    this->bF2Pressed = false;
    this->bF3Pressed = false;
    this->bShiftPressed = false;
    this->bLeft = false;
    this->bRight = false;

    this->bRandomBeatmapScheduled = false;

    // build topbar left
    this->topbarLeft = new CBaseUIContainer(0, 0, 0, 0, "");
    this->songInfo = new InfoLabel(0, 0, 0, 0, "");
    this->topbarLeft->addBaseUIElement(this->songInfo);

    this->filterScoresDropdown = new CBaseUIButton(0, 0, 0, 0, "", "Local");
    this->filterScoresDropdown->setClickCallback(SA::MakeDelegate<&SongBrowser::onFilterScoresClicked>(this));
    this->topbarLeft->addBaseUIElement(this->filterScoresDropdown);

    this->sortScoresDropdown = new CBaseUIButton(0, 0, 0, 0, "", "By score");
    this->sortScoresDropdown->setClickCallback(SA::MakeDelegate<&SongBrowser::onSortScoresClicked>(this));
    this->topbarLeft->addBaseUIElement(this->sortScoresDropdown);

    this->webButton = new CBaseUIButton(0, 0, 0, 0, "", "Web");
    this->webButton->setClickCallback(SA::MakeDelegate<&SongBrowser::onWebClicked>(this));
    this->topbarLeft->addBaseUIElement(this->webButton);

    // build topbar right
    this->topbarRight = new CBaseUIContainer(0, 0, 0, 0, "");
    {
        this->groupLabel = new CBaseUILabel(0, 0, 0, 0, "", "Group:");
        this->groupLabel->setSizeToContent(3);
        this->groupLabel->setDrawFrame(false);
        this->groupLabel->grabs_clicks = true;
        this->topbarRight->addBaseUIElement(this->groupLabel);

        this->groupButton = new CBaseUIButton(0, 0, 0, 0, "", "No Grouping");
        this->groupButton->setClickCallback(SA::MakeDelegate<&SongBrowser::onGroupClicked>(this));
        this->groupButton->grabs_clicks = true;
        this->topbarRight->addBaseUIElement(this->groupButton);

        this->sortLabel = new CBaseUILabel(0, 0, 0, 0, "", "Sort:");
        this->sortLabel->setSizeToContent(3);
        this->sortLabel->setDrawFrame(false);
        this->sortLabel->grabs_clicks = true;
        this->topbarRight->addBaseUIElement(this->sortLabel);

        this->sortButton = new CBaseUIButton(0, 0, 0, 0, "", "By Date Added");
        this->sortButton->setClickCallback(SA::MakeDelegate<&SongBrowser::onSortClicked>(this));
        this->sortButton->grabs_clicks = true;
        this->topbarRight->addBaseUIElement(this->sortButton);

        // "hardcoded" grouping tabs
        this->groupByCollectionBtn = new CBaseUIButton(0, 0, 0, 0, "", "Collections");
        this->groupByCollectionBtn->setClickCallback(SA::MakeDelegate<&SongBrowser::onQuickGroupClicked>(this));
        this->groupByCollectionBtn->grabs_clicks = true;
        this->topbarRight->addBaseUIElement(this->groupByCollectionBtn);
        this->groupByArtistBtn = new CBaseUIButton(0, 0, 0, 0, "", "By Artist");
        this->groupByArtistBtn->setClickCallback(SA::MakeDelegate<&SongBrowser::onQuickGroupClicked>(this));
        this->groupByArtistBtn->grabs_clicks = true;
        this->topbarRight->addBaseUIElement(this->groupByArtistBtn);
        this->groupByDifficultyBtn = new CBaseUIButton(0, 0, 0, 0, "", "By Difficulty");
        this->groupByDifficultyBtn->setClickCallback(SA::MakeDelegate<&SongBrowser::onQuickGroupClicked>(this));
        this->groupByDifficultyBtn->grabs_clicks = true;
        this->topbarRight->addBaseUIElement(this->groupByDifficultyBtn);
        this->groupByNothingBtn = new CBaseUIButton(0, 0, 0, 0, "", "No Grouping");
        this->groupByNothingBtn->setClickCallback(SA::MakeDelegate<&SongBrowser::onQuickGroupClicked>(this));
        this->groupByNothingBtn->grabs_clicks = true;
        this->topbarRight->addBaseUIElement(this->groupByNothingBtn);
    }

    // context menu
    this->contextMenu = new UIContextMenu(50, 50, 150, 0, "");
    this->contextMenu->setVisible(true);

    // build scorebrowser
    this->scoreBrowser = new CBaseUIScrollView(0, 0, 0, 0, "");
    this->scoreBrowser->bScrollbarOnLeft = true;
    this->scoreBrowser->setDrawBackground(false);
    this->scoreBrowser->setDrawFrame(false);
    this->scoreBrowser->setHorizontalScrolling(false);
    this->scoreBrowser->setScrollbarSizeMultiplier(0.25f);
    this->scoreBrowser->setScrollResistance(15);
    this->scoreBrowser->bHorizontalClipping = false;
    this->scoreBrowserScoresStillLoadingElement = new ScoresStillLoadingElement("Loading...");
    this->scoreBrowserNoRecordsYetElement = new NoRecordsSetElement("No records set!");
    this->scoreBrowser->container->addBaseUIElement(this->scoreBrowserNoRecordsYetElement);

    // NOTE: we don't add localBestContainer to the screen; we draw and update it manually so that
    //       it can be drawn under skins which overlay the scores list.
    this->localBestContainer = std::make_unique<CBaseUIContainer>(0.f, 0.f, 0.f, 0.f, "");
    this->localBestContainer->setVisible(false);
    this->localBestLabel = new CBaseUILabel(0, 0, 0, 0, "", "Personal Best (from local scores)");
    this->localBestLabel->setDrawBackground(false);
    this->localBestLabel->setDrawFrame(false);
    this->localBestLabel->setTextJustification(TEXT_JUSTIFICATION::CENTERED);

    // build carousel
    this->carousel = std::make_unique<BeatmapCarousel>(this, 0.f, 0.f, 0.f, 0.f, "Carousel");
    this->thumbnailYRatio = cv::draw_songbrowser_thumbnails.getBool() ? 1.333333f : 0.f;

    // beatmap database
    this->bBeatmapRefreshScheduled = false;

    // behaviour
    this->fPulseAnimation = 0.0f;
    this->fBackgroundFadeInTime = 0.0f;

    // search
    this->search = new UISearchOverlay(0, 0, 0, 0, "");
    this->search->setOffsetRight(10);
    this->fSearchWaitTime = 0.0f;
    this->bInSearch = (!cv::songbrowser_search_hardcoded_filter.getString().empty());
    this->backgroundSearchMatcher = new SongBrowserBackgroundSearchMatcher();

    this->updateLayout();
}

SongBrowser::~SongBrowser() {
    ScoreConverter::abort_calc();
    AsyncPPC::set_map(nullptr);
    VolNormalization::abort();
    MapCalcThread::abort();
    this->checkHandleKillBackgroundSearchMatcher();

    resourceManager->destroyResource(this->backgroundSearchMatcher);
    this->backgroundSearchMatcher = nullptr;

    for(auto &songButton : this->parentButtons) {
        delete songButton;
    }
    for(auto &collectionButton : this->collectionButtons) {
        delete collectionButton;
    }
    for(auto &artistCollectionButton : this->artistCollectionButtons) {
        delete artistCollectionButton;
    }
    for(auto &bpmCollectionButton : this->bpmCollectionButtons) {
        delete bpmCollectionButton;
    }
    for(auto &difficultyCollectionButton : this->difficultyCollectionButtons) {
        delete difficultyCollectionButton;
    }
    for(auto &creatorCollectionButton : this->creatorCollectionButtons) {
        delete creatorCollectionButton;
    }
    // for(auto &dateaddedCollectionButton : this->dateaddedCollectionButtons) {
    //     delete dateaddedCollectionButton;
    // }
    for(auto &lengthCollectionButton : this->lengthCollectionButtons) {
        delete lengthCollectionButton;
    }
    for(auto &titleCollectionButton : this->titleCollectionButtons) {
        delete titleCollectionButton;
    }

    this->scoreBrowser->invalidate();
    for(ScoreButton *button : this->scoreButtonCache) {
        SAFE_DELETE(button);
    }
    this->scoreButtonCache.clear();

    this->localBestContainer->invalidate();  // contained elements freed manually below
    SAFE_DELETE(this->localBestButton);
    SAFE_DELETE(this->localBestLabel);
    SAFE_DELETE(this->scoreBrowserScoresStillLoadingElement);
    SAFE_DELETE(this->scoreBrowserNoRecordsYetElement);

    SAFE_DELETE(this->contextMenu);
    SAFE_DELETE(this->search);
    SAFE_DELETE(this->topbarLeft);
    SAFE_DELETE(this->topbarRight);
    SAFE_DELETE(this->scoreBrowser);

    // Memory leak on shutdown, maybe
    this->invalidate();
}

void SongBrowser::draw() {
    if(!this->bVisible) return;

    // draw background
    g->setColor(0xff000000);
    g->fillRect(0, 0, osu->getVirtScreenWidth(), osu->getVirtScreenHeight());

    // refreshing (blocks every other call in draw() below it!)
    if(this->bBeatmapRefreshScheduled) {
        UString loadingMessage = UString::format("Loading beatmaps ... (%i %%)", (int)(db->getProgress() * 100.0f));

        g->setColor(0xffffffff);
        g->pushTransform();
        {
            g->translate(
                (int)(osu->getVirtScreenWidth() / 2 - osu->getSubTitleFont()->getStringWidth(loadingMessage) / 2),
                osu->getVirtScreenHeight() - 15);
            g->drawString(osu->getSubTitleFont(), loadingMessage);
        }
        g->popTransform();

        osu->getHUD()->drawBeatmapImportSpinner();
        return;
    }

    // draw background image
    if(cv::draw_songbrowser_background_image.getBool()) {
        float alpha = 1.0f;
        if(cv::songbrowser_background_fade_in_duration.getFloat() > 0.0f) {
            // handle fadein trigger after handler is finished loading
            const Image *loadedImage = nullptr;
            const bool ready = osu->getMapInterface()->getBeatmap() != nullptr &&
                               ((loadedImage = osu->getBackgroundImageHandler()->getLoadBackgroundImage(
                                     osu->getMapInterface()->getBeatmap())) != nullptr) &&
                               loadedImage->isReady();

            if(!ready)
                this->fBackgroundFadeInTime = engine->getTime();
            else if(this->fBackgroundFadeInTime > 0.0f && engine->getTime() > this->fBackgroundFadeInTime) {
                alpha = std::clamp<float>((engine->getTime() - this->fBackgroundFadeInTime) /
                                              cv::songbrowser_background_fade_in_duration.getFloat(),
                                          0.0f, 1.0f);
                alpha = 1.0f - (1.0f - alpha) * (1.0f - alpha);
            }
        }

        osu->getBackgroundImageHandler()->draw(osu->getMapInterface()->getBeatmap(), alpha);
    } else if(cv::draw_songbrowser_menu_background_image.getBool()) {
        // menu-background
        Image *backgroundImage = osu->getSkin()->i_menu_bg;
        if(backgroundImage != nullptr && backgroundImage != MISSING_TEXTURE && backgroundImage->isReady()) {
            const float scale = Osu::getImageScaleToFillResolution(backgroundImage, osu->getVirtScreenSize());

            g->setColor(0xffffffff);
            g->pushTransform();
            {
                g->scale(scale, scale);
                g->translate(osu->getVirtScreenWidth() / 2, osu->getVirtScreenHeight() / 2);
                g->drawImage(backgroundImage);
            }
            g->popTransform();
        }
    }

    {
        f32 mode_osu_scale = SongBrowser::getSkinScale(osu->getSkin()->i_mode_osu);

        g->setColor(0xffffffff);
        if(cv::avoid_flashes.getBool()) {
            g->setAlpha(0.1f);
        } else {
            // XXX: Flash based on song BPM
            g->setAlpha(0.1f);
        }

        g->setBlendMode(DrawBlendMode::BLEND_MODE_ADDITIVE);
        osu->getSkin()->i_mode_osu->drawRaw(vec2(osu->getVirtScreenWidth() / 2, osu->getVirtScreenHeight() / 2),
                                            mode_osu_scale, AnchorPoint::CENTER);
        g->setBlendMode(DrawBlendMode::BLEND_MODE_ALPHA);
    }

    // draw score browser
    this->scoreBrowser->draw();
    this->localBestContainer->draw();

    if(cv::debug_osu.getBool()) {
        this->scoreBrowser->container->draw_debug();
    }

    // draw strain graph of currently selected beatmap
    if(cv::draw_songbrowser_strain_graph.getBool()) {
        const std::vector<f64> &aimStrains = osu->getMapInterface()->getWholeMapPPInfo().aimStrains;
        const std::vector<f64> &speedStrains = osu->getMapInterface()->getWholeMapPPInfo().speedStrains;
        const float speedMultiplier = osu->getMapInterface()->getSpeedMultiplier();

        if(aimStrains.size() > 0 && aimStrains.size() == speedStrains.size()) {
            const float strainStepMS = 400.0f * speedMultiplier;

            const u32 lengthMS = strainStepMS * aimStrains.size();

            // get highest strain values for normalization
            double highestAimStrain = 0.0;
            double highestSpeedStrain = 0.0;
            double highestStrain = 0.0;
            int highestStrainIndex = -1;
            for(int i = 0; i < aimStrains.size(); i++) {
                const double aimStrain = aimStrains[i];
                const double speedStrain = speedStrains[i];
                const double strain = aimStrain + speedStrain;

                if(strain > highestStrain) {
                    highestStrain = strain;
                    highestStrainIndex = i;
                }
                if(aimStrain > highestAimStrain) highestAimStrain = aimStrain;
                if(speedStrain > highestSpeedStrain) highestSpeedStrain = speedStrain;
            }

            // draw strain bar graph
            if(highestAimStrain > 0.0 && highestSpeedStrain > 0.0 && highestStrain > 0.0) {
                const float dpiScale = Osu::getUIScale();

                const float graphWidth = this->scoreBrowser->getSize().x;

                const float msPerPixel = (float)lengthMS / graphWidth;
                const float strainWidth = strainStepMS / msPerPixel;
                const float strainHeightMultiplier = cv::hud_scrubbing_timeline_strains_height.getFloat() * dpiScale;

                McRect graphRect(0, osu->getVirtScreenHeight() - (BottomBar::get_height() + strainHeightMultiplier),
                                 graphWidth, strainHeightMultiplier);

                const float alpha =
                    (graphRect.contains(mouse->getPos()) ? 1.0f : cv::hud_scrubbing_timeline_strains_alpha.getFloat());

                const Color aimStrainColor =
                    argb(alpha, cv::hud_scrubbing_timeline_strains_aim_color_r.getInt() / 255.0f,
                         cv::hud_scrubbing_timeline_strains_aim_color_g.getInt() / 255.0f,
                         cv::hud_scrubbing_timeline_strains_aim_color_b.getInt() / 255.0f);
                const Color speedStrainColor =
                    argb(alpha, cv::hud_scrubbing_timeline_strains_speed_color_r.getInt() / 255.0f,
                         cv::hud_scrubbing_timeline_strains_speed_color_g.getInt() / 255.0f,
                         cv::hud_scrubbing_timeline_strains_speed_color_b.getInt() / 255.0f);

                g->setDepthBuffer(true);
                for(int i = 0; i < aimStrains.size(); i++) {
                    const double aimStrain = (aimStrains[i]) / highestStrain;
                    const double speedStrain = (speedStrains[i]) / highestStrain;
                    // const double strain = (aimStrains[i] + speedStrains[i]) / highestStrain;

                    const double aimStrainHeight = aimStrain * strainHeightMultiplier;
                    const double speedStrainHeight = speedStrain * strainHeightMultiplier;
                    // const double strainHeight = strain * strainHeightMultiplier;

                    if(!keyboard->isShiftDown()) {
                        g->setColor(aimStrainColor);
                        g->fillRect(i * strainWidth,
                                    osu->getVirtScreenHeight() - (BottomBar::get_height() + aimStrainHeight),
                                    std::max(1.0f, std::round(strainWidth + 0.5f)), aimStrainHeight);
                    }

                    if(!keyboard->isControlDown()) {
                        g->setColor(speedStrainColor);
                        g->fillRect(i * strainWidth,
                                    osu->getVirtScreenHeight() -
                                        (BottomBar::get_height() +
                                         ((keyboard->isShiftDown() ? 0 : aimStrainHeight) - speedStrainHeight)),
                                    std::max(1.0f, std::round(strainWidth + 0.5f)), speedStrainHeight + 1);
                    }
                }
                g->setDepthBuffer(false);

                // highlight highest total strain value (+- section block)
                if(highestStrainIndex > -1) {
                    const double aimStrain = (aimStrains[highestStrainIndex]) / highestStrain;
                    const double speedStrain = (speedStrains[highestStrainIndex]) / highestStrain;
                    // const double strain = (aimStrains[i] + speedStrains[i]) / highestStrain;

                    const double aimStrainHeight = aimStrain * strainHeightMultiplier;
                    const double speedStrainHeight = speedStrain * strainHeightMultiplier;
                    // const double strainHeight = strain * strainHeightMultiplier;

                    vec2 topLeftCenter = vec2(
                        highestStrainIndex * strainWidth + strainWidth / 2.0f,
                        osu->getVirtScreenHeight() - (BottomBar::get_height() + aimStrainHeight + speedStrainHeight));

                    const float margin = 5.0f * dpiScale;

                    g->setColor(Color(0xffffffff).setA(alpha));

                    g->drawRect(topLeftCenter.x - margin * strainWidth, topLeftCenter.y - margin * strainWidth,
                                strainWidth * 2 * margin,
                                aimStrainHeight + speedStrainHeight + 2 * margin * strainWidth);
                    g->setAlpha(alpha * 0.5f);
                    g->drawRect(topLeftCenter.x - margin * strainWidth - 2, topLeftCenter.y - margin * strainWidth - 2,
                                strainWidth * 2 * margin + 4,
                                aimStrainHeight + speedStrainHeight + 2 * margin * strainWidth + 4);
                    g->setAlpha(alpha * 0.25f);
                    g->drawRect(topLeftCenter.x - margin * strainWidth - 4, topLeftCenter.y - margin * strainWidth - 4,
                                strainWidth * 2 * margin + 8,
                                aimStrainHeight + speedStrainHeight + 2 * margin * strainWidth + 8);
                }
            }
        }
    }

    // draw beatmap carousel
    this->carousel->draw();

    // draw topbar background
    g->setColor(0xffffffff);
    g->pushTransform();
    {
        auto screen = osu->getVirtScreenSize();
        bool is_widescreen = (screen.x / screen.y) > (4.f / 3.f);

        Image *topbar = osu->getSkin()->i_songselect_top;
        f32 scale = (f32)osu->getVirtScreenWidth() / (f32)topbar->getWidth();
        if(!is_widescreen) scale /= 0.75;  // XXX: stupid

        g->scale(scale, scale);
        g->drawImage(topbar, AnchorPoint::TOP_LEFT);
    }
    g->popTransform();

    // draw bottom bar
    BottomBar::draw();

    // draw top bar
    this->topbarLeft->draw();
    if(cv::debug_osu.getBool()) this->topbarLeft->draw_debug();
    this->topbarRight->draw();
    if(cv::debug_osu.getBool()) this->topbarRight->draw_debug();

    // draw search
    this->search->setSearchString(this->sSearchString, cv::songbrowser_search_hardcoded_filter.getString().c_str());
    this->search->setDrawNumResults(this->bInSearch);
    this->search->setNumFoundResults(this->currentVisibleSearchMatches);
    this->search->setSearching(!this->backgroundSearchMatcher->isDead());
    this->search->draw();

    // NOTE: Intentionally not calling ScreenBackable::draw() here, since we're already drawing
    //       the back button in draw_bottombar().
    OsuScreen::draw();

    // no beatmaps found (osu folder is probably invalid)
    if(db->getBeatmapSets().size() == 0 && !this->bBeatmapRefreshScheduled) {
        UString errorMessage1 = "Invalid osu! folder (or no beatmaps found): ";
        errorMessage1.append(this->sLastOsuFolder);
        UString errorMessage2 = "Go to Options -> osu!folder";

        g->setColor(0xffff0000);
        g->pushTransform();
        {
            g->translate(
                (int)(osu->getVirtScreenWidth() / 2 - osu->getSubTitleFont()->getStringWidth(errorMessage1) / 2),
                (int)(osu->getVirtScreenHeight() / 2 + osu->getSubTitleFont()->getHeight()));
            g->drawString(osu->getSubTitleFont(), errorMessage1);
        }
        g->popTransform();

        g->setColor(0xff00ff00);
        g->pushTransform();
        {
            g->translate(
                (int)(osu->getVirtScreenWidth() / 2 - osu->getSubTitleFont()->getStringWidth(errorMessage2) / 2),
                (int)(osu->getVirtScreenHeight() / 2 + osu->getSubTitleFont()->getHeight() * 2 + 15));
            g->drawString(osu->getSubTitleFont(), errorMessage2);
        }
        g->popTransform();
    }

    // context menu
    this->contextMenu->draw();

    // click pulse animation overlay
    if(this->fPulseAnimation > 0.0f) {
        Color topColor = 0x00ffffff;
        Color bottomColor = argb((int)(25 * this->fPulseAnimation), 255, 255, 255);

        g->fillGradient(0, 0, osu->getVirtScreenWidth(), osu->getVirtScreenHeight(), topColor, topColor, bottomColor,
                        bottomColor);
    }
}

bool SongBrowser::selectBeatmapset(i32 set_id) {
    auto beatmapset = db->getBeatmapSet(set_id);
    if(beatmapset == nullptr) {
        // Pasted from Downloader::download_beatmap
        auto mapset_path = fmt::format(NEOSU_MAPS_PATH "/{}/", set_id);
        db->addBeatmapSet(mapset_path);
        debugLog("Finished loading beatmapset {:d}.", set_id);

        beatmapset = db->getBeatmapSet(set_id);
    }

    if(beatmapset == nullptr) {
        return false;
    }

    // Just picking the hardest diff for now
    DatabaseBeatmap *best_diff = nullptr;
    const std::vector<DatabaseBeatmap *> &diffs = beatmapset->getDifficulties();
    for(auto diff : diffs) {
        if(!best_diff || diff->getStarsNomod() > best_diff->getStarsNomod()) {
            best_diff = diff;
        }
    }

    if(best_diff == nullptr) {
        osu->getNotificationOverlay()->addToast(ULITERAL("Beatmapset has no difficulties"), ERROR_TOAST);
        return false;
    } else {
        this->onSelectionChange(this->hashToDiffButton[best_diff->getMD5()], false);
        this->onDifficultySelected(best_diff, false);
        this->selectSelectedBeatmapSongButton();
        return true;
    }
}

void SongBrowser::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible) return;

    // refresh logic (blocks every other call in the update() function below it!)
    if(this->bBeatmapRefreshScheduled) {
        db->update();  // raw load logic
        // check if we are finished loading
        if(db->isFinished()) {
            this->bBeatmapRefreshScheduled = false;
            this->onDatabaseLoadingFinished();
        } else {
            return;
        }
    }

    this->localBestContainer->mouse_update(propagate_clicks);
    ScreenBackable::mouse_update(propagate_clicks);

    // NOTE: This is placed before BottomBar::update(), otherwise the context menu would close
    //       on a bottombar selector click (yeah, a bit hacky)
    this->contextMenu->mouse_update(propagate_clicks);

    BottomBar::update(propagate_clicks);

    // map star/bpm/other calc
    if(MapCalcThread::is_finished()) {
        MapCalcThread::abort();  // join thread

        auto &maps = db->maps_to_recalc;

        const auto &results = MapCalcThread::get_results();

        {
            Sync::unique_lock lock(db->peppy_overrides_mtx);
            for(int i = 0; i < results.size(); i++) {
                auto map = maps[i];
                auto res = results[i];
                map->iNumCircles = res.nb_circles;
                map->iNumSliders = res.nb_sliders;
                map->iNumSpinners = res.nb_spinners;
                map->fStarsNomod = res.star_rating;
                map->iMinBPM = res.min_bpm;
                map->iMaxBPM = res.max_bpm;
                map->iMostCommonBPM = res.avg_bpm;
                db->peppy_overrides[map->getMD5()] = map->get_overrides();
            }
        }

        maps.clear();
    }

    // auto-download
    if(this->map_autodl) {
        float progress = -1.f;
        auto beatmap = Downloader::download_beatmap(this->map_autodl, this->set_autodl, &progress);
        if(progress == -1.f) {
            auto error_str = UString::format("Failed to download Beatmap #%d :(", this->map_autodl);
            osu->getNotificationOverlay()->addToast(error_str, ERROR_TOAST);
            this->map_autodl = 0;
            this->set_autodl = 0;
        } else if(progress < 1.f) {
            // TODO @kiwec: this notification format is jank & laggy
            auto text = UString::format("Downloading... %.2f%%", progress * 100.f);
            osu->getNotificationOverlay()->addNotification(text);
        } else if(beatmap != nullptr) {
            osu->getSongBrowser()->onDifficultySelected(beatmap, false);
            osu->getSongBrowser()->selectSelectedBeatmapSongButton();
            this->map_autodl = 0;
            this->set_autodl = 0;
        }
    } else if(this->set_autodl) {
        if(this->selectBeatmapset(this->set_autodl)) {
            this->map_autodl = 0;
            this->set_autodl = 0;
        } else {
            float progress = -1.f;
            Downloader::download_beatmapset(this->set_autodl, &progress);
            if(progress == -1.f) {
                auto error_str = UString::format("Failed to download Beatmapset #%d :(", this->set_autodl);
                osu->getNotificationOverlay()->addToast(error_str, ERROR_TOAST);
                this->map_autodl = 0;
                this->set_autodl = 0;
            } else if(progress < 1.f) {
                // TODO @kiwec: this notification format is jank & laggy
                auto text = UString::format("Downloading... %.2f%%", progress * 100.f);
                osu->getNotificationOverlay()->addNotification(text);
            } else {
                this->selectBeatmapset(this->set_autodl);

                this->map_autodl = 0;
                this->set_autodl = 0;
            }
        }
    }

    if(this->score_resort_scheduled) {
        this->rebuildScoreButtons();
        this->score_resort_scheduled = false;
    }

    // update and focus handling
    this->topbarRight->mouse_update(propagate_clicks);
    if(this->localBestButton) this->localBestButton->mouse_update(propagate_clicks);
    this->scoreBrowser->mouse_update(propagate_clicks);
    this->topbarLeft->mouse_update(propagate_clicks);

    this->carousel->mouse_update(propagate_clicks);

    // handle async random beatmap selection
    if(this->bRandomBeatmapScheduled) {
        this->bRandomBeatmapScheduled = false;
        this->selectRandomBeatmap();
    }

    // if cursor is to the left edge of the screen, force center currently selected beatmap/diff
    // but only if the context menu is currently not visible (since we don't want move things while e.g. managing
    // collections etc.)
    // NOTE: it's very slow, so only run it every 10 vsync frames
    if(engine->throttledShouldRun(10) && !osu->getOptionsMenu()->isVisible() &&
       mouse->getPos().x < osu->getVirtScreenWidth() * 0.1f && !this->contextMenu->isVisible()) {
        this->scheduled_scroll_to_selected_button = true;
    }

    // handle searching
    if(this->fSearchWaitTime != 0.0f && engine->getTime() > this->fSearchWaitTime) {
        this->fSearchWaitTime = 0.0f;
        this->onSearchUpdate();
    }

    // handle background search matcher
    {
        if(!this->backgroundSearchMatcher->isDead() && this->backgroundSearchMatcher->isAsyncReady()) {
            // we have the results, now update the UI
            this->rebuildSongButtonsAndVisibleSongButtonsWithSearchMatchSupport(true);
            this->backgroundSearchMatcher->kill();
        }
        if(this->backgroundSearchMatcher->isDead()) {
            if(this->scheduled_scroll_to_selected_button) {
                this->scheduled_scroll_to_selected_button = false;
                this->scrollToBestButton();
            }
        }
    }
}

void SongBrowser::onKeyDown(KeyboardEvent &key) {
    OsuScreen::onKeyDown(key);  // only used for options menu
    if(!this->bVisible || key.isConsumed()) return;

    if(this->bVisible && this->bBeatmapRefreshScheduled &&
       (key == KEY_ESCAPE || key == cv::GAME_PAUSE.getVal<SCANCODE>())) {
        db->cancel();
        key.consume();
        return;
    }

    if(this->bBeatmapRefreshScheduled) return;

    // context menu
    this->contextMenu->onKeyDown(key);
    if(key.isConsumed()) return;

    // searching text delete & escape key handling
    if(!this->sSearchString.isEmpty()) {
        switch(key.getScanCode()) {
            case KEY_DELETE:
            case KEY_BACKSPACE:
                key.consume();
                if(!this->sSearchString.isEmpty()) {
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

                    this->scheduleSearchUpdate(this->sSearchString.length() == 0);
                }
                break;

            case KEY_ESCAPE:
                key.consume();
                this->sSearchString.clear();
                this->scheduleSearchUpdate(true);
                break;
        }
    } else if(!this->contextMenu->isVisible()) {
        if(key == KEY_ESCAPE)  // can't support GAME_PAUSE hotkey here because of text searching
            osu->toggleSongBrowser();
    }

    // paste clipboard support
    if(key == KEY_V) {
        if(keyboard->isControlDown()) {
            const auto &clipstring = env->getClipBoardText();
            if(!clipstring.isEmpty()) {
                this->sSearchString.append(clipstring);
                this->scheduleSearchUpdate(false);
            }
        }
    }

    if(key == KEY_LSHIFT || key == KEY_RSHIFT) this->bShiftPressed = true;

    // function hotkeys
    if((key == KEY_F1 || key == cv::TOGGLE_MODSELECT.getVal<SCANCODE>()) && !this->bF1Pressed) {
        this->bF1Pressed = true;
        BottomBar::press_button(BottomBar::MODS);
    }
    if((key == KEY_F2 || key == cv::RANDOM_BEATMAP.getVal<SCANCODE>()) && !this->bF2Pressed) {
        this->bF2Pressed = true;
        BottomBar::press_button(BottomBar::RANDOM);
    }
    if(key == KEY_F3 && !this->bF3Pressed) {
        this->bF3Pressed = true;
        BottomBar::press_button(BottomBar::OPTIONS);
    }

    if(key == KEY_F5) this->refreshBeatmaps();

    this->carousel->onKeyDown(key);
    //if (key.isConsumed()) return;

    // toggle auto
    if(key == KEY_A && keyboard->isControlDown()) osu->getModSelector()->toggleAuto();

    key.consume();
}

void SongBrowser::onKeyUp(KeyboardEvent &key) {
    // context menu
    this->contextMenu->onKeyUp(key);
    if(key.isConsumed()) return;

    if(key == KEY_LSHIFT || key == KEY_RSHIFT) this->bShiftPressed = false;
    if(key == KEY_LEFT) this->bLeft = false;
    if(key == KEY_RIGHT) this->bRight = false;

    if(key == KEY_F1 || key == cv::TOGGLE_MODSELECT.getVal<SCANCODE>()) this->bF1Pressed = false;
    if(key == KEY_F2 || key == cv::RANDOM_BEATMAP.getVal<SCANCODE>()) this->bF2Pressed = false;
    if(key == KEY_F3) this->bF3Pressed = false;
}

void SongBrowser::onChar(KeyboardEvent &e) {
    // context menu
    this->contextMenu->onChar(e);
    if(e.isConsumed()) return;

    if(e.getCharCode() < 32 || !this->bVisible || this->bBeatmapRefreshScheduled ||
       (keyboard->isControlDown() && !keyboard->isAltDown()))
        return;
    if(this->bF1Pressed || this->bF2Pressed || this->bF3Pressed) return;

    // handle searching
    this->sSearchString.append(e.getCharCode());

    this->scheduleSearchUpdate();
}

void SongBrowser::onResolutionChange(vec2 newResolution) { ScreenBackable::onResolutionChange(newResolution); }

CBaseUIContainer *SongBrowser::setVisible(bool visible) {
    if(BanchoState::spectating && visible) return this;  // don't allow song browser to be visible while spectating
    if(visible == this->bVisible) return this;

    this->bVisible = visible;
    this->bShiftPressed = false;  // seems to get stuck sometimes otherwise

    if(this->bVisible) {
        soundEngine->play(osu->getSkin()->s_expand);
        RichPresence::onSongBrowser();

        this->updateLayout();

        // we have to re-select the current beatmap to start playing music again
        osu->getMapInterface()->selectBeatmap();

        // update user name/stats
        osu->onUserCardChange(BanchoState::get_username().c_str());

        // HACKHACK: workaround for BaseUI framework deficiency (missing mouse events. if a mouse button is being held,
        // and then suddenly a BaseUIElement gets put under it and set visible, and then the mouse button is released,
        // that "incorrectly" fires onMouseUpInside/onClicked/etc.)
        mouse->onButtonChange({Timing::getTicksNS(), MouseButtonFlags::MF_LEFT, false});
        mouse->onButtonChange({Timing::getTicksNS(), MouseButtonFlags::MF_RIGHT, false});

        // For multiplayer: if the host exits song selection without selecting a song, we want to be able to revert
        // to that previous song.
        this->lastSelectedBeatmap = osu->getMapInterface()->getBeatmap();

        // Select button matching current song preview
        this->selectSelectedBeatmapSongButton();
    } else {
        this->contextMenu->setVisible2(false);
    }

    osu->getChat()->updateVisibility();
    return this;
}

void SongBrowser::selectSelectedBeatmapSongButton() {
    DatabaseBeatmap *map = nullptr;
    if(this->hashToDiffButton.empty() || !(map = osu->getMapInterface()->getBeatmap())) return;

    auto it = this->hashToDiffButton.find(map->getMD5());
    if(it == this->hashToDiffButton.end()) {
        debugLog("No song button found for currently selected beatmap...");
        return;
    }

    auto btn = it->second;
    if(btn->getDatabaseBeatmap() != map) {
        debugLog("Found matching beatmap, but not matching difficulty.");
        return;
    }

    btn->deselect();  // if we select() it when already selected, it would start playing!
    btn->select();
}

void SongBrowser::onPlayEnd(bool quit) {
    // update score displays
    if(!quit) {
        this->rebuildScoreButtons();

        auto *selectedSongDiffButton = this->selectedButton->as<SongDifficultyButton>();
        if(selectedSongDiffButton != nullptr) selectedSongDiffButton->updateGrade();
    }

    // update song info
    if(osu->getMapInterface()->getBeatmap() != nullptr) {
        this->songInfo->setFromBeatmap(osu->getMapInterface()->getBeatmap());
    }
}

void SongBrowser::onSelectionChange(CarouselButton *button, bool rebuild) {
    const bool wasSelected = (this->selectedButton == button);
    this->selectedButton = button;
    if(button == nullptr) return;

    this->contextMenu->setVisible2(false);
    if(wasSelected && !rebuild) return;

    // keep track and update all selection states
    // I'm still not happy with this, but at least all state update logic is localized in this function instead of
    // spread across all buttons

    auto *songButtonPointer = button->as<SongButton>();
    auto *songDiffButtonPointer = button->as<SongDifficultyButton>();
    auto *collectionButtonPointer = button->as<CollectionButton>();

    if(songDiffButtonPointer != nullptr) {
        if(this->selectionPreviousSongDiffButton != nullptr &&
           this->selectionPreviousSongDiffButton != songDiffButtonPointer)
            this->selectionPreviousSongDiffButton->deselect();

        // support individual diffs independent from their parent song button container
        {
            // if the new diff has a parent song button, then update its selection state (select it to stay consistent)
            if(!songDiffButtonPointer->getParentSongButton()->isSelected()) {
                songDiffButtonPointer->getParentSongButton()
                    ->sortChildren();  // NOTE: workaround for disabled callback firing in select()
                songDiffButtonPointer->getParentSongButton()->select(false);
                this->onSelectionChange(songDiffButtonPointer->getParentSongButton(), false);  // NOTE: recursive call
            }
        }

        this->selectionPreviousSongDiffButton = songDiffButtonPointer;
    } else if(songButtonPointer != nullptr) {
        if(this->selectionPreviousSongButton != nullptr && this->selectionPreviousSongButton != songButtonPointer)
            this->selectionPreviousSongButton->deselect();
        if(this->selectionPreviousSongDiffButton != nullptr) this->selectionPreviousSongDiffButton->deselect();

        this->selectionPreviousSongButton = songButtonPointer;
    } else if(collectionButtonPointer != nullptr) {
        // TODO: maybe expand this logic with per-group-type last-open-collection memory

        // logic for allowing collections to be deselected by clicking on the same button (contrary to how beatmaps
        // work)
        const bool isTogglingCollection = (this->selectionPreviousCollectionButton != nullptr &&
                                           this->selectionPreviousCollectionButton == collectionButtonPointer);

        if(this->selectionPreviousCollectionButton != nullptr) this->selectionPreviousCollectionButton->deselect();

        this->selectionPreviousCollectionButton = collectionButtonPointer;

        if(isTogglingCollection) this->selectionPreviousCollectionButton = nullptr;
    }

    if(rebuild) this->rebuildSongButtons();
}

void SongBrowser::onDifficultySelected(DatabaseBeatmap *map, bool play) {
    // deselect = unload
    osu->getMapInterface()->deselectBeatmap();

    // select = play preview music
    osu->getMapInterface()->selectBeatmap(map);

    // update song info
    if(map) {
        this->songInfo->setFromBeatmap(map);

        // start playing
        if(play) {
            if(BanchoState::is_in_a_multi_room()) {
                BanchoState::room.map_name =
                    UString::format("%s - %s [%s]", map->getArtistLatin().c_str(), map->getTitleLatin().c_str(),
                                    map->getDifficultyName().c_str());
                BanchoState::room.map_md5 = map->getMD5();
                BanchoState::room.map_id = map->getID();

                Packet packet;
                packet.id = MATCH_CHANGE_SETTINGS;
                BanchoState::room.pack(packet);
                BANCHO::Net::send_packet(packet);

                osu->getRoom()->on_map_change();

                this->setVisible(false);
            } else {
                // CTRL + click = auto
                if(keyboard->isControlDown()) {
                    osu->bModAutoTemp = true;
                    osu->getModSelector()->enableAuto();
                }

                if(osu->getMapInterface()->play()) {
                    this->setVisible(false);
                }
            }
        }
    }

    // animate
    this->fPulseAnimation = 1.0f;
    anim::moveLinear(&this->fPulseAnimation, 0.0f, 0.55f, true);

    // update score display
    this->rebuildScoreButtons();

    // update web button
    this->webButton->setVisible(this->songInfo->getBeatmapID() > 0);
}

void SongBrowser::refreshBeatmaps(bool closeAfterLoading) {
    if(osu->isInPlayMode()) return;

    // reset
    this->checkHandleKillBackgroundSearchMatcher();

    auto map = osu->getMapInterface()->getBeatmap();
    if(map) {
        this->loading_reselect_map.hash = map->getMD5();
        const auto *music = osu->getMapInterface()->getMusic();
        if(music && music->isPlaying()) {
            this->loading_reselect_map.time_when_stopped = Timing::getTicksMS();
            this->loading_reselect_map.musicpos_when_stopped = music->getPositionMS();
        }
    } else {
        this->loading_reselect_map = {};
    }

    // don't pause the music the first time we load the song database
    // TODO: don't do any of this shit
    static bool first_refresh = true;
    if(first_refresh) {
        first_refresh = false;
    } else {
        osu->reloadMapInterface();
    }

    this->selectionPreviousSongButton = nullptr;
    this->selectionPreviousSongDiffButton = nullptr;
    this->selectionPreviousCollectionButton = nullptr;

    // delete local database and UI
    this->carousel->invalidate();

    for(auto &songButton : this->parentButtons) {
        delete songButton;
    }
    this->parentButtons.clear();
    this->hashToDiffButton.clear();
    for(auto &collectionButton : this->collectionButtons) {
        delete collectionButton;
    }
    this->collectionButtons.clear();
    for(auto &artistCollectionButton : this->artistCollectionButtons) {
        delete artistCollectionButton;
    }
    this->artistCollectionButtons.clear();
    for(auto &bpmCollectionButton : this->bpmCollectionButtons) {
        delete bpmCollectionButton;
    }
    this->bpmCollectionButtons.clear();
    for(auto &difficultyCollectionButton : this->difficultyCollectionButtons) {
        delete difficultyCollectionButton;
    }
    this->difficultyCollectionButtons.clear();
    for(auto &creatorCollectionButton : this->creatorCollectionButtons) {
        delete creatorCollectionButton;
    }
    this->creatorCollectionButtons.clear();
    // for(auto &dateaddedCollectionButton : this->dateaddedCollectionButtons) {
    //     delete dateaddedCollectionButton;
    // }
    this->dateaddedCollectionButtons.clear();
    for(auto &lengthCollectionButton : this->lengthCollectionButtons) {
        delete lengthCollectionButton;
    }
    this->lengthCollectionButtons.clear();
    for(auto &titleCollectionButton : this->titleCollectionButtons) {
        delete titleCollectionButton;
    }
    this->titleCollectionButtons.clear();

    this->visibleSongButtons.clear();
    this->previousRandomBeatmaps.clear();

    this->contextMenu->setVisible2(false);

    // clear potentially active search
    this->bInSearch = false;
    this->sSearchString.clear();
    this->sPrevSearchString.clear();
    this->fSearchWaitTime = 0.0f;
    this->searchPrevGroup = std::nullopt;

    // force no grouping
    if(this->curGroup != GroupType::NO_GROUPING) {
        this->onGroupChange("", GroupType::NO_GROUPING);
    } else {
        this->groupByNothingBtn->setTextBrightColor(highlightColor);
    }

    // start loading
    this->bBeatmapRefreshScheduled = true;
    this->bCloseAfterBeatmapRefreshFinished = closeAfterLoading;
    db->load();

    // show loading progress
    // should be *after* this->bBeatmapRefreshScheduled = true
    if(!this->bVisible && !cv::load_db_immediately.getBool()) {
        osu->toggleSongBrowser();
    }
}

void SongBrowser::addBeatmapSet(BeatmapSet *mapset, bool initialSongBrowserLoad) {
    // NOTE: BeatmapSets must always be created with at least itself as a difficulty.
    assert(!mapset->getDifficulties().empty());

    // some invariants are assumed to be true if we are loading for the first time
    if(initialSongBrowserLoad) {
        assert(this->difficultyCollectionButtons.size() == 12);
        assert(this->bpmCollectionButtons.size() == 6);
        assert(this->lengthCollectionButtons.size() == 7);
    } else {
        this->bSongButtonsNeedSorting = true;
    }

    const bool doDiffCollBtns = initialSongBrowserLoad || likely(this->difficultyCollectionButtons.size() == 12);
    const bool doBPMCollBtns = initialSongBrowserLoad || likely(this->bpmCollectionButtons.size() == 6);
    const bool doLengthCollBtns = initialSongBrowserLoad || likely(this->lengthCollectionButtons.size() == 7);

    // always create parent button for the set
    auto *parentButton =
        new SongButton(this->contextMenu, 250, 250 + db->getBeatmapSets().size() * 50, 200, 50, "", mapset);
    this->parentButtons.push_back(parentButton);

    // add mapset to all necessary groups
    this->addSongButtonToAlphanumericGroup(parentButton, this->artistCollectionButtons, mapset->getArtistLatin());
    this->addSongButtonToAlphanumericGroup(parentButton, this->creatorCollectionButtons, mapset->getCreator());
    this->addSongButtonToAlphanumericGroup(parentButton, this->titleCollectionButtons, mapset->getTitleLatin());

    // use parent's children for grouping
    const auto &tempChildrenForGroups =
        reinterpret_cast<const std::vector<SongDifficultyButton *> &>(parentButton->getChildren());

    for(SongDifficultyButton *diff_btn : tempChildrenForGroups) {
        DatabaseBeatmap *diff = diff_btn->getDatabaseBeatmap();
        assert(diff);  // we just added it

        // map each difficulty hash to its button
        this->hashToDiffButton[diff->getMD5()] = diff_btn;

        if(doDiffCollBtns) {
            const float stars_tmp = diff->getStarsNomod();
            const int index = std::clamp<int>(
                (std::isfinite(stars_tmp) && stars_tmp >= static_cast<float>(std::numeric_limits<int>::min()) &&
                 stars_tmp <= static_cast<float>(std::numeric_limits<int>::max()))
                    ? static_cast<int>(stars_tmp)
                    : 0,
                0, 11);
            this->difficultyCollectionButtons[index]->addChild(diff_btn);
        }

        if(doBPMCollBtns) {
            auto bpm = diff->getMostCommonBPM();
            int index;
            if(bpm < 60) {
                index = 0;
            } else if(bpm < 120) {
                index = 1;
            } else if(bpm < 180) {
                index = 2;
            } else if(bpm < 240) {
                index = 3;
            } else if(bpm < 300) {
                index = 4;
            } else {
                index = 5;
            }

            this->bpmCollectionButtons[index]->addChild(diff_btn);
        }

        // dateadded
        {
            // TODO: extremely annoying
        }

        if(doLengthCollBtns) {
            const u32 lengthMS = diff->getLengthMS();

            CollectionButton *btn = nullptr;
            if(lengthMS <= 1000 * 60) {
                btn = this->lengthCollectionButtons[0];
            } else if(lengthMS <= 1000 * 60 * 2) {
                btn = this->lengthCollectionButtons[1];
            } else if(lengthMS <= 1000 * 60 * 3) {
                btn = this->lengthCollectionButtons[2];
            } else if(lengthMS <= 1000 * 60 * 4) {
                btn = this->lengthCollectionButtons[3];
            } else if(lengthMS <= 1000 * 60 * 5) {
                btn = this->lengthCollectionButtons[4];
            } else if(lengthMS <= 1000 * 60 * 10) {
                btn = this->lengthCollectionButtons[5];
            } else {
                btn = this->lengthCollectionButtons[6];
            }

            btn->addChild(diff_btn);
        }
    }
}

void SongBrowser::addSongButtonToAlphanumericGroup(SongButton *sbtn, std::vector<CollectionButton *> &group,
                                                   std::string_view name) {
    if(group.size() != 28) {
        debugLog("Alphanumeric group wasn't initialized!");
        return;
    }

    const char firstChar = name.length() == 0 ? '#' : name[0];
    const bool isNumber = (firstChar >= '0' && firstChar <= '9');
    const bool isLowerCase = (firstChar >= 'a' && firstChar <= 'z');
    const bool isUpperCase = (firstChar >= 'A' && firstChar <= 'Z');

    CollectionButton *cbtn = nullptr;
    if(isNumber) {
        cbtn = group[0];
    } else if(isLowerCase || isUpperCase) {
        const int index = 1 + (25 - (isLowerCase ? 'z' - firstChar : 'Z' - firstChar));
        cbtn = group[index];
    } else {
        cbtn = group[27];
    }

    logIfCV(debug_osu, "Inserting {:s}", name);

    cbtn->addChild(sbtn);
}

void SongBrowser::requestNextScrollToSongButtonJumpFix(SongDifficultyButton *diffButton) {
    if(diffButton == nullptr) return;

    this->bNextScrollToSongButtonJumpFixScheduled = true;

    // use parent position only if the diff is NOT independent
    // (i.e., parent is actually visible and expanded in the carousel)
    this->fNextScrollToSongButtonJumpFixOldRelPosY =
        (diffButton->isIndependentDiffButton() ? diffButton->getRelPos().y
                                               : diffButton->getParentSongButton()->getRelPos().y);

    this->fNextScrollToSongButtonJumpFixOldScrollSizeY = this->carousel->getScrollSize().y;
}

bool SongBrowser::isButtonVisible(CarouselButton *songButton) {
    bool ret = false;

    for(const auto &btn : this->visibleSongButtons) {
        if(btn == songButton) {
            ret = true;
            goto out;
        }
        for(const auto &child : btn->getChildren()) {
            if(child == songButton) {
                ret = true;
                goto out;
            }

            for(const auto &grandchild : child->getChildren()) {
                if(grandchild == songButton) {
                    ret = true;
                    goto out;
                }
            }
        }
    }

out:
    // if(songButton->getDatabaseBeatmap()) {
    //     debugLog("{}: {}", ret ? "VISIBLE" : "NOT VISIBLE", songButton->getDatabaseBeatmap()->getFilePath());
    // }

    return ret;
}

void SongBrowser::scrollToBestButton() {
    for(const auto &collection : this->visibleSongButtons) {
        for(const auto &mapset : collection->getChildren()) {
            for(const auto &diff : mapset->getChildren()) {
                if(diff->isSelected()) {
                    this->scrollToSongButton(diff);
                    return;
                }
            }

            if(mapset->isSelected()) {
                this->scrollToSongButton(mapset);
                return;
            }
        }

        if(collection->isSelected()) {
            this->scrollToSongButton(collection);
            return;
        }
    }

    this->carousel->scrollToTop();
}

void SongBrowser::scrollToSongButton(CarouselButton *songButton, bool alignOnTop) {
    if(songButton == nullptr || !this->isButtonVisible(songButton)) {
        return;
    }

    // NOTE: compensate potential scroll jump due to added/removed elements (feels a lot better this way, also easier on
    // the eyes)
    if(this->bNextScrollToSongButtonJumpFixScheduled) {
        this->bNextScrollToSongButtonJumpFixScheduled = false;

        float delta = 0.0f;
        {
            if(!this->bNextScrollToSongButtonJumpFixUseScrollSizeDelta)
                delta = (songButton->getRelPos().y - this->fNextScrollToSongButtonJumpFixOldRelPosY);  // (default case)
            else
                delta = this->carousel->getScrollSize().y -
                        this->fNextScrollToSongButtonJumpFixOldScrollSizeY;  // technically not correct but feels a
                                                                             // lot better for KEY_LEFT navigation
        }
        this->carousel->scrollToY(this->carousel->getRelPosY() - delta, false);
    }

    this->carousel->scrollToY(-songButton->getRelPos().y +
                              (alignOnTop ? (0) : (this->carousel->getSize().y / 2 - songButton->getSize().y / 2)));
}

void SongBrowser::rebuildSongButtons() {
    this->carousel->invalidate();

    // NOTE: currently supports 3 depth layers (collection > beatmap > diffs)
    for(auto &visibleSongButton : this->visibleSongButtons) {
        CarouselButton *button = visibleSongButton;
        button->resetAnimations();

        if(!(button->isSelected() && button->isHiddenIfSelected())) this->carousel->container->addBaseUIElement(button);

        // children
        if(button->isSelected()) {
            const auto &children = visibleSongButton->getChildren();
            for(auto button2 : children) {
                bool isButton2SearchMatch = false;
                if(button2->getChildren().size() > 0) {
                    const auto &children2 = button2->getChildren();
                    for(auto button3 : children2) {
                        if(button3->isSearchMatch()) {
                            isButton2SearchMatch = true;
                            break;
                        }
                    }
                } else
                    isButton2SearchMatch = button2->isSearchMatch();

                if(this->bInSearch && !isButton2SearchMatch) continue;

                button2->resetAnimations();

                bool addedSingleChild = false;

                if(!(button2->isSelected() && button2->isHiddenIfSelected())) {
                    if(this->getSetVisibility(button2) == SetVisibility::SINGLE_CHILD) {
                        for(auto *child : button2->getChildren()) {
                            if(child->isSearchMatch()) {
                                addedSingleChild = true;
                                child->resetAnimations();

                                this->carousel->container->addBaseUIElement(child);
                                break;  // only one visible child
                            }
                        }
                    } else {
                        this->carousel->container->addBaseUIElement(button2);
                    }
                }

                // child children
                if(!addedSingleChild && button2->isSelected()) {
                    const auto &children2 = button2->getChildren();
                    for(auto button3 : children2) {
                        if(this->bInSearch && !button3->isSearchMatch()) continue;

                        button3->resetAnimations();

                        if(!(button3->isSelected() && button3->isHiddenIfSelected()))
                            this->carousel->container->addBaseUIElement(button3);
                    }
                }
            }
        }
    }

    this->updateSongButtonLayout();
}

void SongBrowser::updateSongButtonLayout() {
    // this rebuilds the entire songButton layout (songButtons in relation to others)
    // only the y axis is set, because the x axis is constantly animated and handled within the button classes
    // themselves

    // all elements must be CarouselButtons, at least
    const auto &elements{
        reinterpret_cast<const std::vector<CarouselButton *> &>(this->carousel->container->getElements())};

    int yCounter = this->carousel->getSize().y / 4;
    if(elements.size() <= 1) yCounter = this->carousel->getSize().y / 2;

    bool isSelected = false;
    bool inOpenCollection = false;
    for(auto &element : elements) {
        auto *songButton = element->as<CarouselButton>();

        if(songButton != nullptr) {
            const auto *diffButtonPointer = songButton->as<const SongDifficultyButton>();

            // depending on the object type, layout differently
            const bool isCollectionButton = songButton->isType<CollectionButton>();
            const bool isDiffButton = diffButtonPointer != nullptr;
            const bool isIndependentDiffButton = isDiffButton && diffButtonPointer->isIndependentDiffButton();

            // give selected items & diffs a bit more spacing, to make them stand out
            if(((songButton->isSelected() && !isCollectionButton) || isSelected ||
                (isDiffButton && !isIndependentDiffButton)))
                yCounter += songButton->getSize().y * 0.1f;

            isSelected = songButton->isSelected() || (isDiffButton && !isIndependentDiffButton);

            // give collections a bit more spacing at start & end
            if((songButton->isSelected() && isCollectionButton)) yCounter += songButton->getSize().y * 0.2f;
            if(inOpenCollection && isCollectionButton && !songButton->isSelected())
                yCounter += songButton->getSize().y * 0.2f;
            if(isCollectionButton) {
                if(songButton->isSelected())
                    inOpenCollection = true;
                else
                    inOpenCollection = false;
            }

            songButton->setTargetRelPosY(yCounter);
            songButton->updateLayoutEx();

            yCounter += songButton->getActualSize().y;
        }
    }
    this->carousel->setScrollSizeToContent(this->carousel->getSize().y / 2);
}

SongBrowser::SetVisibility SongBrowser::getSetVisibility(const SongButton *parent) const {
    if(parent == nullptr) return SetVisibility::HIDDEN;

    // count visible children
    int visibleCount = 0;
    for(const auto *child : parent->getChildren()) {
        if(child->isSearchMatch()) {
            visibleCount++;
        }
        if(visibleCount > 1) break;  // break early
    }

    if(visibleCount == 0) return SetVisibility::HIDDEN;
    if(visibleCount == 1) return SetVisibility::SINGLE_CHILD;
    return SetVisibility::SHOW_PARENT;
}

bool SongBrowser::searchMatcher(const DatabaseBeatmap *databaseBeatmap,
                                const std::vector<std::string> &searchStringTokens) {
    if(databaseBeatmap == nullptr) return false;

    const std::vector<const DatabaseBeatmap *> tmpContainer{databaseBeatmap};
    const auto &diffs = databaseBeatmap->getDifficulties().size() > 0
                            ? databaseBeatmap->getDifficulties<const DatabaseBeatmap>()
                            : tmpContainer;

    auto speed = osu->getMapInterface()->getSpeedMultiplier();

    // TODO: optimize this dumpster fire. can at least cache the parsed tokens and literal strings array instead of
    // parsing every single damn time

    // intelligent search parser
    // all strings which are not expressions get appended with spaces between, then checked with one call to
    // findSubstringInDiff() the rest is interpreted NOTE: this code is quite shitty. the order of the operators
    // array does matter, because find() is used to detect their presence (and '=' would then break '<=' etc.)
    enum operatorId : uint8_t { EQ, LT, GT, LE, GE, NE };

    struct Operator {
        std::string_view str;
        operatorId id;
    };

    static constexpr std::initializer_list<Operator> operators = {
        {.str = "<=", .id = LE}, {.str = ">=", .id = GE}, {.str = "<", .id = LT}, {.str = ">", .id = GT},
        {.str = "!=", .id = NE}, {.str = "==", .id = EQ}, {.str = "=", .id = EQ},
    };

    enum keywordId : uint8_t {
        AR,
        CS,
        OD,
        HP,
        BPM,
        OPM,
        CPM,
        SPM,
        OBJECTS,
        CIRCLES,
        SLIDERS,
        SPINNERS,
        LENGTH,
        STARS,
        CREATOR
    };

    struct Keyword {
        std::string_view str;
        keywordId id;
    };

    static constexpr std::initializer_list<Keyword> keywords = {{.str = "ar", .id = AR},
                                                                {.str = "cs", .id = CS},
                                                                {.str = "od", .id = OD},
                                                                {.str = "hp", .id = HP},
                                                                {.str = "bpm", .id = BPM},
                                                                {.str = "opm", .id = OPM},
                                                                {.str = "cpm", .id = CPM},
                                                                {.str = "spm", .id = SPM},
                                                                {.str = "object", .id = OBJECTS},
                                                                {.str = "objects", .id = OBJECTS},
                                                                {.str = "circle", .id = CIRCLES},
                                                                {.str = "circles", .id = CIRCLES},
                                                                {.str = "slider", .id = SLIDERS},
                                                                {.str = "sliders", .id = SLIDERS},
                                                                {.str = "spinner", .id = SPINNERS},
                                                                {.str = "spinners", .id = SPINNERS},
                                                                {.str = "length", .id = LENGTH},
                                                                {.str = "len", .id = LENGTH},
                                                                {.str = "stars", .id = STARS},
                                                                {.str = "star", .id = STARS},
                                                                {.str = "creator", .id = CREATOR}};

    // split search string into tokens
    // parse over all difficulties
    bool expressionMatches = false;  // if any diff matched all expressions
    std::vector<std::string> literalSearchStrings;
    for(const auto *diff : diffs) {
        bool expressionsMatch = true;  // if the current search string (meaning only the expressions in this case)
                                       // matches the current difficulty

        for(const auto &searchStringToken : searchStringTokens) {
            // debugLog("token[{:d}] = {:s}", i, tokens[i].toUtf8());
            //  determine token type, interpret expression
            bool expression = false;
            for(const auto &[op_str, op_id] : operators) {
                if(searchStringToken.find(op_str) != std::string::npos) {
                    // split expression into left and right parts (only accept singular expressions, things like
                    // "0<bpm<1" will not work with this)
                    // debugLog("splitting by string {:s}", operators[o].first.toUtf8());
                    std::vector<std::string_view> values{SString::split(searchStringToken, op_str)};
                    if(values.size() == 2 && values[0].length() > 0 && values[1].length() > 0) {
                        std::string_view lvalue = values[0];
                        std::string_view rstring = values[1];

                        const auto rvaluePercentIndex = rstring.find('%');
                        const bool rvalueIsPercent = (rvaluePercentIndex != std::string::npos);
                        const float rvalue = [&rstring, rvaluePercentIndex]() -> float {
                            float rvalue_tmp{0.f};
                            const std::string_view rstring_sub = rvaluePercentIndex == std::string::npos
                                                                     ? rstring
                                                                     : rstring.substr(0, rvaluePercentIndex);

                            auto [ptr, ec] = std::from_chars(rstring_sub.data(),
                                                             rstring_sub.data() + rstring_sub.size(), rvalue_tmp);
                            if(ec != std::errc()) return 0.f;
                            return rvalue_tmp;
                        }();  // this must always be a number (at least, assume it is)

                        // find lvalue keyword in array (only continue if keyword exists)
                        for(const auto &[kw_str, kw_id] : keywords) {
                            if(kw_str == lvalue) {
                                expression = true;

                                // we now have a valid expression: the keyword, the operator and the value

                                // solve keyword
                                float compareValue = 5.0f;
                                std::string compareString{};
                                switch(kw_id) {
                                    case AR:
                                        compareValue = diff->getAR();
                                        break;
                                    case CS:
                                        compareValue = diff->getCS();
                                        break;
                                    case OD:
                                        compareValue = diff->getOD();
                                        break;
                                    case HP:
                                        compareValue = diff->getHP();
                                        break;
                                    case BPM:
                                        compareValue = diff->getMostCommonBPM();
                                        break;
                                    case OPM:
                                        compareValue =
                                            (diff->getLengthMS() > 0 ? ((float)diff->getNumObjects() /
                                                                        (float)(diff->getLengthMS() / 1000.0f / 60.0f))
                                                                     : 0.0f) *
                                            speed;
                                        break;
                                    case CPM:
                                        compareValue =
                                            (diff->getLengthMS() > 0 ? ((float)diff->getNumCircles() /
                                                                        (float)(diff->getLengthMS() / 1000.0f / 60.0f))
                                                                     : 0.0f) *
                                            speed;
                                        break;
                                    case SPM:
                                        compareValue =
                                            (diff->getLengthMS() > 0 ? ((float)diff->getNumSliders() /
                                                                        (float)(diff->getLengthMS() / 1000.0f / 60.0f))
                                                                     : 0.0f) *
                                            speed;
                                        break;
                                    case OBJECTS:
                                        compareValue = diff->getNumObjects();
                                        break;
                                    case CIRCLES:
                                        compareValue =
                                            (rvalueIsPercent
                                                 ? ((float)diff->getNumCircles() / (float)diff->getNumObjects()) *
                                                       100.0f
                                                 : diff->getNumCircles());
                                        break;
                                    case SLIDERS:
                                        compareValue =
                                            (rvalueIsPercent
                                                 ? ((float)diff->getNumSliders() / (float)diff->getNumObjects()) *
                                                       100.0f
                                                 : diff->getNumSliders());
                                        break;
                                    case SPINNERS:
                                        compareValue =
                                            (rvalueIsPercent
                                                 ? ((float)diff->getNumSpinners() / (float)diff->getNumObjects()) *
                                                       100.0f
                                                 : diff->getNumSpinners());
                                        break;
                                    case LENGTH:
                                        compareValue = diff->getLengthMS() / 1000.0f;
                                        break;
                                    case STARS:
                                        compareValue = std::round(diff->getStarsNomod() * 100.0f) /
                                                       100.0f;  // round to 2 decimal places
                                        break;
                                    case CREATOR:
                                        compareString = SString::to_lower(diff->getCreator());
                                        break;
                                }

                                // solve operator
                                bool matches = false;
                                switch(op_id) {
                                    case LE:
                                        if(compareValue <= rvalue) matches = true;
                                        break;
                                    case GE:
                                        if(compareValue >= rvalue) matches = true;
                                        break;
                                    case LT:
                                        if(compareValue < rvalue) matches = true;
                                        break;
                                    case GT:
                                        if(compareValue > rvalue) matches = true;
                                        break;
                                    case NE:
                                        if(compareValue != rvalue) matches = true;
                                        break;
                                    case EQ:
                                        if(compareValue == rvalue ||
                                           (!compareString.empty() && compareString == rstring))
                                            matches = true;
                                        break;
                                }

                                // debugLog("comparing {:f} {:s} {:f} (operatorId = {:d}) = {:d}", compareValue,
                                // operators[o].first.toUtf8(), rvalue, (int)operators[o].second, (int)matches);

                                if(!matches)  // if a single expression doesn't match, then the whole diff doesn't match
                                    expressionsMatch = false;

                                break;
                            }
                        }
                    }

                    break;
                }
            }

            // if this is not an expression, add the token to the literalSearchStrings array
            if(!expression) {
                // only add it if it doesn't exist yet
                // this check is only necessary due to multiple redundant parser executions (one per diff!)
                bool exists = false;
                for(const auto &literalSearchString : literalSearchStrings) {
                    if(literalSearchString == searchStringToken) {
                        exists = true;
                        break;
                    }
                }

                if(!exists) {
                    std::string litAdd{searchStringToken};
                    SString::trim_inplace(litAdd);
                    if(!SString::is_wspace_only(litAdd)) literalSearchStrings.push_back(litAdd);
                }
            }
        }

        if(expressionsMatch)  // as soon as one difficulty matches all expressions, we are done here
        {
            expressionMatches = true;
            break;
        }
    }

    // if no diff matched any expression, then we can already stop here
    if(!expressionMatches) return false;

    bool hasAnyValidLiteralSearchString = false;
    for(const auto &literalSearchString : literalSearchStrings) {
        if(literalSearchString.length() > 0) {
            hasAnyValidLiteralSearchString = true;
            break;
        }
    }

    // early return here for literal match/contains
    if(hasAnyValidLiteralSearchString) {
        static constexpr auto findSubstringInDiff = [](const DatabaseBeatmap *diff,
                                                       std::string_view searchString) -> bool {
            return (SString::contains_ncase(diff->getTitleLatin(), searchString)) ||
                   (SString::contains_ncase(diff->getArtistLatin(), searchString)) ||
                   (SString::contains_ncase(diff->getCreator(), searchString)) ||
                   (SString::contains_ncase(diff->getDifficultyName(), searchString)) ||
                   (SString::contains_ncase(diff->getSource(), searchString)) ||
                   (SString::contains_ncase(diff->getTags(), searchString)) ||
                   (diff->getID() > 0 && SString::contains_ncase(std::to_string(diff->getID()), searchString)) ||
                   (diff->getSetID() > 0 && SString::contains_ncase(std::to_string(diff->getSetID()), searchString));
        };

        for(const auto *diff : diffs) {
            bool atLeastOneFullMatch = true;

            for(const auto &literalSearchString : literalSearchStrings) {
                if(!findSubstringInDiff(diff, literalSearchString)) atLeastOneFullMatch = false;
            }

            // as soon as one diff matches all strings, we are done
            if(atLeastOneFullMatch) return true;
        }

        // expression may have matched, but literal didn't match, so the entire beatmap doesn't match
        return false;
    }

    return expressionMatches;
}

void SongBrowser::updateLayout() {
    ScreenBackable::updateLayout();

    const float dpiScale = Osu::getUIScale();
    const int margin = 5 * dpiScale;

    // topbar left
    this->topbarLeft->setSize(SongBrowser::getUIScale(390.f), SongBrowser::getUIScale(145.f));
    this->songInfo->setRelPos(margin, margin);
    this->songInfo->setSize(
        this->topbarLeft->getSize().x - margin,
        std::max(this->topbarLeft->getSize().y * 0.75f, this->songInfo->getMinimumHeight() + margin));

    const int topbarLeftButtonMargin = 5 * dpiScale;
    const int topbarLeftButtonHeight = 30 * dpiScale;
    const int topbarLeftButtonWidth = 55 * dpiScale;
    this->webButton->onResized();  // HACKHACK: framework bug (should update string metrics on setSize())
    this->webButton->setSize(topbarLeftButtonWidth, topbarLeftButtonHeight);
    this->webButton->setRelPos(this->topbarLeft->getSize().x - (topbarLeftButtonMargin + topbarLeftButtonWidth),
                               this->topbarLeft->getSize().y - this->webButton->getSize().y);

    const int dropdowns_width =
        this->topbarLeft->getSize().x - 3 * topbarLeftButtonMargin - (topbarLeftButtonWidth + topbarLeftButtonMargin);
    const int dropdowns_y = this->topbarLeft->getSize().y - this->sortScoresDropdown->getSize().y;

    this->filterScoresDropdown->onResized();  // HACKHACK: framework bug (should update string metrics on setSize())
    this->filterScoresDropdown->setSize(dropdowns_width / 2, topbarLeftButtonHeight);
    this->filterScoresDropdown->setRelPos(topbarLeftButtonMargin, dropdowns_y);

    this->sortScoresDropdown->onResized();  // HACKHACK: framework bug (should update string metrics on setSize())
    this->sortScoresDropdown->setSize(dropdowns_width / 2, topbarLeftButtonHeight);
    this->sortScoresDropdown->setRelPos(topbarLeftButtonMargin + (dropdowns_width / 2), dropdowns_y);

    this->topbarLeft->update_pos();

    // topbar right
    this->topbarRight->setPosX(osu->getVirtScreenWidth() / 2);
    this->topbarRight->setSize(osu->getVirtScreenWidth() - this->topbarRight->getPos().x,
                               SongBrowser::getUIScale(80.f));

    float btn_margin = 10.f * dpiScale;
    this->sortButton->setSize(200.f * dpiScale, 30.f * dpiScale);
    this->sortButton->setRelPos(this->topbarRight->getSize().x - (this->sortButton->getSize().x + btn_margin),
                                btn_margin);

    this->sortLabel->onResized();  // HACKHACK: framework bug (should update string metrics on setSizeToContent())
    this->sortLabel->setSizeToContent(3 * dpiScale);
    this->sortLabel->setRelPos(this->sortButton->getRelPos().x - (this->sortLabel->getSize().x + btn_margin),
                               (this->sortLabel->getSize().y + btn_margin) / 2.f);

    this->groupButton->setSize(this->sortButton->getSize());
    this->groupButton->setRelPos(
        this->sortLabel->getRelPos().x - (this->sortButton->getSize().x + 30.f * dpiScale + btn_margin), btn_margin);

    this->groupLabel->onResized();  // HACKHACK: framework bug (should update string metrics on setSizeToContent())
    this->groupLabel->setSizeToContent(3 * dpiScale);
    this->groupLabel->setRelPos(this->groupButton->getRelPos().x - (this->groupLabel->getSize().x + btn_margin),
                                (this->groupLabel->getSize().y + btn_margin) / 2.f);

    // "hardcoded" group buttons
    const i32 group_btn_width =
        std::clamp<i32>((this->topbarRight->getSize().x - 2 * btn_margin) / 4, 0, 200 * dpiScale);
    this->groupByCollectionBtn->setSize(group_btn_width, 30 * dpiScale);
    this->groupByCollectionBtn->setRelPos(this->topbarRight->getSize().x - (btn_margin + (4 * group_btn_width)),
                                          this->topbarRight->getSize().y - 30 * dpiScale);
    this->groupByArtistBtn->setSize(group_btn_width, 30 * dpiScale);
    this->groupByArtistBtn->setRelPos(this->topbarRight->getSize().x - (btn_margin + (3 * group_btn_width)),
                                      this->topbarRight->getSize().y - 30 * dpiScale);
    this->groupByDifficultyBtn->setSize(group_btn_width, 30 * dpiScale);
    this->groupByDifficultyBtn->setRelPos(this->topbarRight->getSize().x - (btn_margin + (2 * group_btn_width)),
                                          this->topbarRight->getSize().y - 30 * dpiScale);
    this->groupByNothingBtn->setSize(group_btn_width, 30 * dpiScale);
    this->groupByNothingBtn->setRelPos(this->topbarRight->getSize().x - (btn_margin + (1 * group_btn_width)),
                                       this->topbarRight->getSize().y - 30 * dpiScale);

    this->topbarRight->update_pos();

    // score browser
    this->updateScoreBrowserLayout();

    // song browser
    this->carousel->setPos(this->topbarLeft->getPos().x + this->topbarLeft->getSize().x + 1,
                           this->topbarRight->getPos().y + (this->topbarRight->getSize().y * 0.9));
    this->carousel->setSize(
        osu->getVirtScreenWidth() - (this->topbarLeft->getPos().x + this->topbarLeft->getSize().x),
        (osu->getVirtScreenHeight() - this->carousel->getPos().y - (BottomBar::get_min_height() * 0.75f)));

    this->updateSongButtonLayout();

    this->search->setPos(osu->getVirtScreenWidth() / 2, this->topbarRight->getSize().y + 8 * dpiScale);
    this->search->setSize(osu->getVirtScreenWidth() / 2, 20 * dpiScale);
}

void SongBrowser::onBack() { osu->toggleSongBrowser(); }

void SongBrowser::updateScoreBrowserLayout() {
    const float dpiScale = Osu::getUIScale();

    const bool shouldScoreBrowserBeVisible =
        (cv::scores_enabled.getBool() && cv::songbrowser_scorebrowser_enabled.getBool());
    if(shouldScoreBrowserBeVisible != this->scoreBrowser->isVisible())
        this->scoreBrowser->setVisible(shouldScoreBrowserBeVisible);

    const int scoreButtonWidthMax = this->topbarLeft->getSize().x;

    f32 browserHeight = osu->getVirtScreenHeight() -
                        (BottomBar::get_height() + (this->topbarLeft->getPos().y + this->topbarLeft->getSize().y)) +
                        2 * dpiScale;
    this->scoreBrowser->setPos(this->topbarLeft->getPos().x + 2 * dpiScale,
                               this->topbarLeft->getPos().y + this->topbarLeft->getSize().y + 4 * dpiScale);
    this->scoreBrowser->setSize(scoreButtonWidthMax, browserHeight);
    const i32 scoreHeight = SongBrowser::getUIScale(53.f);

    // In stable, even when looking at local scores, there is space where the "local best" would be.
    f32 local_best_size = scoreHeight + SongBrowser::getUIScale(61);
    browserHeight -= local_best_size;
    this->scoreBrowser->setSize(this->scoreBrowser->getSize().x, browserHeight);
    this->scoreBrowser->setScrollSizeToContent();

    if(this->localBestContainer->isVisible()) {
        this->localBestContainer->setPos(this->scoreBrowser->getPos().x,
                                         this->scoreBrowser->getPos().y + this->scoreBrowser->getSize().y);
        this->localBestContainer->setSize(this->scoreBrowser->getPos().x, local_best_size);
        this->localBestLabel->setRelPos(this->scoreBrowser->getPos().x, 0);
        this->localBestLabel->setSize(this->scoreBrowser->getSize().x, 40);
        if(this->localBestButton) {
            this->localBestButton->setRelPos(this->scoreBrowser->getPos().x, 40);
            this->localBestButton->setSize(this->scoreBrowser->getSize().x, scoreHeight);
        }
    }

    const std::vector<CBaseUIElement *> &elements = this->scoreBrowser->container->getElements();
    for(size_t i = 0; i < elements.size(); i++) {
        CBaseUIElement *scoreButton = elements[i];
        scoreButton->setSize(this->scoreBrowser->getSize().x, scoreHeight);
        scoreButton->setRelPos(0, i * scoreButton->getSize().y);
    }
    this->scoreBrowserScoresStillLoadingElement->setSize(this->scoreBrowser->getSize().x * 0.9f, scoreHeight * 0.75f);
    this->scoreBrowserScoresStillLoadingElement->setRelPos(
        this->scoreBrowser->getSize().x / 2 - this->scoreBrowserScoresStillLoadingElement->getSize().x / 2,
        (browserHeight / 2) * 0.65f - this->scoreBrowserScoresStillLoadingElement->getSize().y / 2);
    this->scoreBrowserNoRecordsYetElement->setSize(this->scoreBrowser->getSize().x * 0.9f, scoreHeight * 0.75f);
    if(elements[0] == this->scoreBrowserNoRecordsYetElement) {
        this->scoreBrowserNoRecordsYetElement->setRelPos(
            this->scoreBrowser->getSize().x / 2 - this->scoreBrowserScoresStillLoadingElement->getSize().x / 2,
            (browserHeight / 2) * 0.65f - this->scoreBrowserScoresStillLoadingElement->getSize().y / 2);
    } else {
        this->scoreBrowserNoRecordsYetElement->setRelPos(
            this->scoreBrowser->getSize().x / 2 - this->scoreBrowserScoresStillLoadingElement->getSize().x / 2, 45);
    }
    this->localBestContainer->update_pos();
    this->scoreBrowser->container->update_pos();
    this->scoreBrowser->setScrollSizeToContent();
}

void SongBrowser::rebuildScoreButtons() {
    if(!this->isVisible()) return;

    // XXX: When online, it would be nice to scroll to the current user's highscore

    // reset
    this->scoreBrowser->invalidate();
    this->localBestContainer->invalidate();
    this->localBestContainer->setVisible(false);

    auto *map = osu->getMapInterface()->getBeatmap();
    const bool validBeatmap = !!map;
    const MD5Hash mapHash = validBeatmap ? map->getMD5() : MD5Hash{};

    bool is_online = (BanchoState::is_online() || BanchoState::is_logging_in()) &&
                     cv::songbrowser_scores_filteringtype.getString() != "Local";

    std::vector<FinishedScore> scores;
    if(validBeatmap) {
        Sync::shared_lock lock(db->scores_mtx);

        const FinishedScore *local_best = nullptr;
        const auto &local_scores = db->getScores().find(mapHash);

        if(is_online) {
            if(local_scores != db->getScores().end()) {
                if(const auto &elem = std::ranges::max_element(
                       local_scores->second,
                       [](const FinishedScore &a, const FinishedScore &b) { return a.score < b.score; });
                   elem != local_scores->second.end()) {
                    local_best = &(*elem);
                }
            }

            const auto &search = db->getOnlineScores().find(mapHash);
            if(search != db->getOnlineScores().end()) {
                scores = search->second;

                if(!local_best) {
                    if(!scores.empty()) {
                        // We only want to display "No scores" if there are online scores present
                        // Otherwise, it would be displayed twice
                        SAFE_DELETE(this->localBestButton);
                        this->localBestContainer->addBaseUIElement(this->localBestLabel);
                        this->localBestContainer->addBaseUIElement(this->scoreBrowserNoRecordsYetElement);
                        this->localBestContainer->setVisible(true);
                    }
                } else {
                    SAFE_DELETE(this->localBestButton);
                    this->localBestButton = new ScoreButton(this->contextMenu, 0, 0, 0, 0);
                    this->localBestButton->setClickCallback(SA::MakeDelegate<&SongBrowser::onScoreClicked>(this));
                    this->localBestButton->setScore(*local_best, map);
                    this->localBestButton->resetHighlight();
                    this->localBestButton->grabs_clicks = true;
                    this->localBestContainer->addBaseUIElement(this->localBestLabel);
                    this->localBestContainer->addBaseUIElement(this->localBestButton);
                    this->localBestContainer->setVisible(true);
                }

                // We have already fetched the scores so there's no point in showing "Loading...".
                // When there are no online scores for this map, let's treat it as if we are
                // offline in order to show "No records set!" instead.
                is_online = false;
            } else {
                // We haven't fetched the scores yet, do so now
                BANCHO::Leaderboard::fetch_online_scores(map);

                // Display local best while scores are loading
                if(local_best) {
                    SAFE_DELETE(this->localBestButton);
                    this->localBestButton = new ScoreButton(this->contextMenu, 0, 0, 0, 0);
                    this->localBestButton->setClickCallback(SA::MakeDelegate<&SongBrowser::onScoreClicked>(this));
                    this->localBestButton->setScore(*local_best, map);
                    this->localBestButton->resetHighlight();
                    this->localBestButton->grabs_clicks = true;
                    this->localBestContainer->addBaseUIElement(this->localBestLabel);
                    this->localBestContainer->addBaseUIElement(this->localBestButton);
                    this->localBestContainer->setVisible(true);
                }
            }
        } else {
            if(local_scores != db->getScores().end()) {
                scores = local_scores->second;
            }
        }
    }

    const int numScores = scores.size();

    if(numScores > 0) {
        // sort
        db->sortScoresInPlace(scores, !is_online /* don't lock scores_mtx if we're sorting online scores */);
    }

    // top up cache as necessary
    if(numScores > this->scoreButtonCache.size()) {
        const int numNewButtons = numScores - this->scoreButtonCache.size();
        for(size_t i = 0; std::cmp_less(i, numNewButtons); i++) {
            auto *scoreButton = new ScoreButton(this->contextMenu, 0, 0, 0, 0);
            scoreButton->setClickCallback(SA::MakeDelegate<&SongBrowser::onScoreClicked>(this));
            this->scoreButtonCache.push_back(scoreButton);
        }
    }

    // and build the ui
    if(numScores < 1) {
        if(validBeatmap && is_online) {
            this->scoreBrowser->container->addBaseUIElement(this->scoreBrowserScoresStillLoadingElement,
                                                            this->scoreBrowserScoresStillLoadingElement->getRelPos().x,
                                                            this->scoreBrowserScoresStillLoadingElement->getRelPos().y);
        } else {
            this->scoreBrowser->container->addBaseUIElement(this->scoreBrowserNoRecordsYetElement,
                                                            this->scoreBrowserScoresStillLoadingElement->getRelPos().x,
                                                            this->scoreBrowserScoresStillLoadingElement->getRelPos().y);
        }
    } else {
        // build
        std::vector<ScoreButton *> scoreButtons;
        for(int i = 0; i < numScores; i++) {
            ScoreButton *button = this->scoreButtonCache[i];
            button->setScore(scores[i], map, i + 1);
            scoreButtons.push_back(button);
        }

        // add
        for(int i = 0; i < numScores; i++) {
            scoreButtons[i]->setIndex(i + 1);
            this->scoreBrowser->container->addBaseUIElement(scoreButtons[i]);
        }

        // reset
        for(auto &scoreButton : scoreButtons) {
            scoreButton->resetHighlight();
        }
    }

    // layout
    this->updateScoreBrowserLayout();

    // update grades of songbuttons for current map
    // (weird place for this to be, i think the intent is to update them after you set a score)
    if(validBeatmap) {
        for(auto &visibleSongButton : this->visibleSongButtons) {
            if(visibleSongButton->getDatabaseBeatmap() == map) {
                auto *songButtonPointer = visibleSongButton->as<SongButton>();
                if(songButtonPointer != nullptr) {
                    for(CarouselButton *diffButton : songButtonPointer->getChildren()) {
                        auto *diffButtonPointer = diffButton->as<SongButton>();
                        if(diffButtonPointer != nullptr) diffButtonPointer->updateGrade();
                    }
                }
            }
        }
    }
}

void SongBrowser::scheduleSearchUpdate(bool immediately) {
    this->fSearchWaitTime = engine->getTime() + (immediately ? 0.0f : cv::songbrowser_search_delay.getFloat());
}

void SongBrowser::checkHandleKillBackgroundSearchMatcher() {
    if(!this->backgroundSearchMatcher->isDead()) {
        this->backgroundSearchMatcher->kill();

        const double startTime = Timing::getTimeReal();
        while(!this->backgroundSearchMatcher->isAsyncReady()) {
            if(Timing::getTimeReal() - startTime > 2) {
                debugLog("WARNING: Ignoring stuck SearchMatcher thread!");
                break;
            }
        }
    }
}

void SongBrowser::onDatabaseLoadingFinished() {
    // Extract oszs from neosu's maps/ directory now
    auto oszs = env->getFilesInFolder(NEOSU_MAPS_PATH "/");
    for(const auto &file : oszs) {
        if(env->getFileExtensionFromFilePath(file) != "osz") continue;
        auto path = NEOSU_MAPS_PATH "/" + file;
        bool extracted = env->getEnvInterop().handle_osz(path.c_str());
        if(extracted) env->deleteFile(path);
    }

    Timer t;
    t.start();

    debugLog("Loading {} beatmapsets from database.", db->getBeatmapSets().size());

    // initialize all collection (grouped) buttons
    {
        // artist
        {
            // 0-9
            {
                auto *b =
                    new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", "0-9", std::vector<SongButton *>());
                this->artistCollectionButtons.push_back(b);
            }

            // A-Z
            for(size_t i = 0; i < 26; i++) {
                UString artistCollectionName = UString::format("%c", 'A' + i);

                auto *b = new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", artistCollectionName,
                                               std::vector<SongButton *>());
                this->artistCollectionButtons.push_back(b);
            }

            // Other
            {
                auto *b = new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", "Other",
                                               std::vector<SongButton *>());
                this->artistCollectionButtons.push_back(b);
            }
        }

        // difficulty
        for(size_t i = 0; i < 12; i++) {
            UString difficultyCollectionName = UString::format(i == 1 ? "%i star" : "%i stars", i);
            if(i < 1) difficultyCollectionName = "Below 1 star";
            if(i > 10) difficultyCollectionName = "Above 10 stars";

            std::vector<SongButton *> children;

            auto *b =
                new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", difficultyCollectionName, children);
            this->difficultyCollectionButtons.push_back(b);
        }

        // bpm
        {
            auto *b = new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", "Under 60 BPM",
                                           std::vector<SongButton *>());
            this->bpmCollectionButtons.push_back(b);
            b = new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", "Under 120 BPM",
                                     std::vector<SongButton *>());
            this->bpmCollectionButtons.push_back(b);
            b = new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", "Under 180 BPM",
                                     std::vector<SongButton *>());
            this->bpmCollectionButtons.push_back(b);
            b = new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", "Under 240 BPM",
                                     std::vector<SongButton *>());
            this->bpmCollectionButtons.push_back(b);
            b = new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", "Under 300 BPM",
                                     std::vector<SongButton *>());
            this->bpmCollectionButtons.push_back(b);
            b = new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", "Over 300 BPM",
                                     std::vector<SongButton *>());
            this->bpmCollectionButtons.push_back(b);
        }

        // creator
        {
            // 0-9
            {
                auto *b =
                    new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", "0-9", std::vector<SongButton *>());
                this->creatorCollectionButtons.push_back(b);
            }

            // A-Z
            for(size_t i = 0; i < 26; i++) {
                UString artistCollectionName = UString::format("%c", 'A' + i);

                auto *b = new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", artistCollectionName,
                                               std::vector<SongButton *>());
                this->creatorCollectionButtons.push_back(b);
            }

            // Other
            {
                auto *b = new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", "Other",
                                               std::vector<SongButton *>());
                this->creatorCollectionButtons.push_back(b);
            }
        }

        // dateadded
        {
            // TODO: annoying
        }

        // length
        {
            auto *b = new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", "1 minute or less",
                                           std::vector<SongButton *>());
            this->lengthCollectionButtons.push_back(b);
            b = new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", "2 minutes or less",
                                     std::vector<SongButton *>());
            this->lengthCollectionButtons.push_back(b);
            b = new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", "3 minutes or less",
                                     std::vector<SongButton *>());
            this->lengthCollectionButtons.push_back(b);
            b = new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", "4 minutes or less",
                                     std::vector<SongButton *>());
            this->lengthCollectionButtons.push_back(b);
            b = new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", "5 minutes or less",
                                     std::vector<SongButton *>());
            this->lengthCollectionButtons.push_back(b);
            b = new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", "10 minutes or less",
                                     std::vector<SongButton *>());
            this->lengthCollectionButtons.push_back(b);
            b = new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", "Over 10 minutes",
                                     std::vector<SongButton *>());
            this->lengthCollectionButtons.push_back(b);
        }

        // title
        {
            // 0-9
            {
                auto *b =
                    new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", "0-9", std::vector<SongButton *>());
                this->titleCollectionButtons.push_back(b);
            }

            // A-Z
            for(size_t i = 0; i < 26; i++) {
                UString artistCollectionName = UString::format("%c", 'A' + i);

                auto *b = new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", artistCollectionName,
                                               std::vector<SongButton *>());
                this->titleCollectionButtons.push_back(b);
            }

            // Other
            {
                auto *b = new CollectionButton(this->contextMenu, 250, 250, 200, 50, "", "Other",
                                               std::vector<SongButton *>());
                this->titleCollectionButtons.push_back(b);
            }
        }
    }

    // add all beatmaps (build buttons)
    this->parentButtons.reserve(db->getBeatmapSets().size());
    for(const auto &beatmap : db->getBeatmapSets()) {
        this->addBeatmapSet(beatmap, true /* initial songbrowser load flag (skip some checks) */);
    }
    this->parentButtons.shrink_to_fit();

    // build collections
    this->recreateCollectionsButtons();

    this->onSortChange(cv::songbrowser_sortingtype.getString().c_str());
    this->onSortScoresChange(cv::songbrowser_scores_sortingtype.getString().c_str());

    // update rich presence (discord total pp)
    RichPresence::onSongBrowser();

    // update user name/stats
    osu->onUserCardChange(BanchoState::get_username().c_str());

    if(cv::songbrowser_search_hardcoded_filter.getString().length() > 0) this->onSearchUpdate();

    // ugly hack to transition from preloaded main menu beatmap to database-loaded beatmap without pausing music
    {
        DatabaseBeatmap *reselectMap = nullptr;
        if(this->loading_reselect_map.hash != MD5Hash{}) {
            reselectMap = db->getBeatmapDifficulty(this->loading_reselect_map.hash);
        }

        this->onDifficultySelected(reselectMap, false);  // select even if null (clear existing)
        if(reselectMap) {
            this->selectSelectedBeatmapSongButton();
        }

        // always clear preloaded maps to avoid stale references
        osu->getMainMenu()->clearPreloadedMaps();

        this->loading_reselect_map = {};
    }

    // ok, if we still haven't selected a song, do so now
    if(osu->getMapInterface()->getBeatmap() == nullptr) {
        this->selectRandomBeatmap();
    }

    Sound *music = nullptr;
    if((music = osu->getMapInterface()->getMusic())) {
        // make sure we loop the music, since if we're carrying over from main menu it was set to not-loop
        music->setLoop(cv::beatmap_preview_music_loop.getBool());
    }

    if(this->bCloseAfterBeatmapRefreshFinished) {
        this->setVisible(false);
    }

    t.update();
    debugLog("Took {} seconds.", t.getElapsedTime());

    // Watch for new maps now
    directoryWatcher->watch_directory(NEOSU_MAPS_PATH "/", [](const FileChangeEvent &ev) {
        if(ev.type != FileChangeType::CREATED) return;
        Logger::logRaw("[DirectoryWatcher] Importing new beatmap {}: type {}", ev.path, (u32)ev.type);
        if(env->getFileExtensionFromFilePath(ev.path) != "osz") return;
        const bool extracted = env->getEnvInterop().handle_osz(ev.path.c_str());
        if(extracted) env->deleteFile(ev.path);
    });
}

void SongBrowser::onSearchUpdate() {
    const UString hardcodedFilterString = cv::songbrowser_search_hardcoded_filter.getString().c_str();
    const bool hasHardcodedSearchStringChanged = (this->sPrevHardcodedSearchString != hardcodedFilterString);
    const bool hasSearchStringChanged = (this->sPrevSearchString != this->sSearchString);

    const bool prevInSearch = this->bInSearch;
    this->bInSearch = (!this->sSearchString.isEmpty() || !hardcodedFilterString.isEmpty());
    const bool hasInSearchChanged = (prevInSearch != this->bInSearch);

    if(this->bInSearch) {
        const bool shouldRefreshMatches =
            hasSearchStringChanged || hasHardcodedSearchStringChanged || hasInSearchChanged;

        // GROUP_COLLECTIONS is the only group that can filter beatmaps, so skip some work if we're not switching between that and something else
        this->bShouldRecountMatchesAfterSearch =
            this->bShouldRecountMatchesAfterSearch ||             //
            shouldRefreshMatches ||                               //
            (!this->searchPrevGroup.has_value() ||                //
             (this->curGroup != this->searchPrevGroup.value() &&  //
              (this->curGroup == GroupType::COLLECTIONS || this->searchPrevGroup.value() == GroupType::COLLECTIONS)));

        this->searchPrevGroup = this->curGroup;

        // flag all search matches across entire database
        if(shouldRefreshMatches) {
            // stop potentially running async search
            this->checkHandleKillBackgroundSearchMatcher();

            this->backgroundSearchMatcher->revive();
            this->backgroundSearchMatcher->release();
            this->backgroundSearchMatcher->setSongButtonsAndSearchString(this->parentButtons, this->sSearchString,
                                                                         hardcodedFilterString);

            resourceManager->requestNextLoadAsync();
            resourceManager->loadResource(this->backgroundSearchMatcher);
        } else
            this->rebuildSongButtonsAndVisibleSongButtonsWithSearchMatchSupport(true, true);

        // (results are handled in update() once available)
    } else  // exit search
    {
        // exiting the search does not need any async work, so we can just directly do it in here

        // stop potentially running async search
        this->checkHandleKillBackgroundSearchMatcher();

        // reset container and visible buttons list
        this->carousel->invalidate();
        this->visibleSongButtons.clear();

        // reset all search flags
        for(auto &songButton : this->parentButtons) {
            for(auto &c : songButton->getChildren()) {
                c->setIsSearchMatch(true);
            }
        }

        // remember which tab was selected, instead of defaulting back to no grouping
        // this also rebuilds the visible buttons list
        if(this->searchPrevGroup.has_value()) {
            auto prevgid = this->searchPrevGroup.value();
            this->onGroupChange(GROUP_NAMES[prevgid], prevgid);
        }
    }

    this->sPrevSearchString = this->sSearchString;
    this->sPrevHardcodedSearchString = cv::songbrowser_search_hardcoded_filter.getString().c_str();
}

void SongBrowser::rebuildSongButtonsAndVisibleSongButtonsWithSearchMatchSupport(bool scrollToTop,
                                                                                bool doRebuildSongButtons) {
    // reset container and visible buttons list
    this->carousel->invalidate();
    this->visibleSongButtons.clear();

    // optimization: currently, only grouping by collections can actually filter beatmaps
    // so don't reset search matches if switching between any other grouping mode
    const bool recountMatches = this->bShouldRecountMatchesAfterSearch && this->bInSearch;
    const bool canBreakEarly = !recountMatches;
    if(recountMatches) {
        this->currentVisibleSearchMatches = 0;
        // don't re-count again next time
        this->bShouldRecountMatchesAfterSearch = false;
    }

    // use flagged search matches to rebuild visible song buttons
    if(this->curGroup == GroupType::NO_GROUPING) {
        for(auto *parentButton : this->parentButtons) {
            const SetVisibility visibility = this->getSetVisibility(parentButton);

            switch(visibility) {
                case SetVisibility::HIDDEN:
                    // don't add to carousel
                    break;

                case SetVisibility::SINGLE_CHILD:
                    // add only the visible child
                    for(auto *child : parentButton->getChildren()) {
                        if(child->isSearchMatch()) {
                            if(recountMatches) this->currentVisibleSearchMatches++;
                            this->visibleSongButtons.push_back(child);
                            break;  // only one visible child
                        }
                    }
                    break;

                case SetVisibility::SHOW_PARENT:
                    // add parent (which will expand to show children if selected)
                    if(recountMatches) {
                        for(const auto *child : parentButton->getChildren()) {
                            if(child->isSearchMatch()) {
                                this->currentVisibleSearchMatches++;
                            }
                        }
                    }
                    this->visibleSongButtons.push_back(parentButton);
                    break;
            }
        }
    } else {
        std::vector<CollectionButton *> *groupButtons = getCollectionButtonsForGroup(this->curGroup);

        if(groupButtons != nullptr) {
            for(const auto &groupButton : *groupButtons) {
                bool isAnyMatchInGroup = false;

                const auto &children = groupButton->getChildren();
                for(const auto &c : children) {
                    const auto &childrenChildren = c->getChildren();
                    if(childrenChildren.size() > 0) {
                        for(const auto &cc : childrenChildren) {
                            if(cc->isSearchMatch()) {
                                isAnyMatchInGroup = true;
                                // also count total matching children while we're here
                                // break out early if we're not searching, though
                                if(canBreakEarly)
                                    break;
                                else
                                    this->currentVisibleSearchMatches++;
                            }
                        }

                        if(canBreakEarly && isAnyMatchInGroup) break;
                    } else if(c->isSearchMatch()) {
                        isAnyMatchInGroup = true;
                        if(canBreakEarly)
                            break;
                        else
                            this->currentVisibleSearchMatches++;
                    }
                }

                if(isAnyMatchInGroup || !this->bInSearch) this->visibleSongButtons.push_back(groupButton);
            }
        }
    }

    if(doRebuildSongButtons) this->rebuildSongButtons();

    // scroll to top search result, or auto select the only result
    if(scrollToTop) {
        if(this->visibleSongButtons.size() > 1) {
            this->scrollToSongButton(this->visibleSongButtons[0]);
        } else if(this->visibleSongButtons.size() > 0) {
            this->selectSongButton(this->visibleSongButtons[0]);
        }
    }
}

void SongBrowser::onFilterScoresClicked(CBaseUIButton *button) {
    const std::vector<std::string> filters{"Local", "Global", "Selected mods", "Country", "Friends", "Team"};

    this->contextMenu->setPos(button->getPos());
    this->contextMenu->setRelPos(button->getRelPos());
    this->contextMenu->begin(button->getSize().x);
    {
        if(BanchoState::is_online()) {
            for(const auto &filter : filters) {
                CBaseUIButton *button = this->contextMenu->addButton(filter.c_str());
                if(filter == cv::songbrowser_scores_filteringtype.getString()) {
                    button->setTextBrightColor(0xff00ff00);
                }
            }
        } else {
            CBaseUIButton *button = this->contextMenu->addButton("Local");
            button->setTextBrightColor(0xff00ff00);
        }
    }
    this->contextMenu->end(false, false);
    this->contextMenu->setClickCallback(SA::MakeDelegate<&SongBrowser::onFilterScoresChange>(this));
}

void SongBrowser::onSortScoresClicked(CBaseUIButton *button) {
    this->contextMenu->setPos(button->getPos());
    this->contextMenu->setRelPos(button->getRelPos());
    this->contextMenu->begin(button->getSize().x);
    {
        for(const auto &scoreSortingMethod : Database::SCORE_SORTING_METHODS) {
            CBaseUIButton *button = this->contextMenu->addButton(UString{scoreSortingMethod.name});
            if(scoreSortingMethod.name == cv::songbrowser_scores_sortingtype.getString())
                button->setTextBrightColor(0xff00ff00);
        }
    }
    this->contextMenu->end(false, false);
    this->contextMenu->setClickCallback(SA::MakeDelegate<&SongBrowser::onSortScoresChange>(this));
}

void SongBrowser::onFilterScoresChange(const UString &text, int id) {
    UString text_to_set{text};
    const auto &type_cv = &cv::songbrowser_scores_filteringtype;
    const auto &manual_type_cv = &cv::songbrowser_scores_filteringtype_manual;

    // abusing "id" to determine whether it was a click callback or due to login
    if(id != LOGIN_STATE_FILTER_ID) {
        manual_type_cv->setValue(text);
    }

    // always change for manual setting, otherwise allow login state to affect filtering (if it was never manually set)
    const bool should_change = id != LOGIN_STATE_FILTER_ID || (manual_type_cv->isDefault());
    if(!should_change) {
        text_to_set = UString{manual_type_cv->getString()};
    }
    type_cv->setValue(text_to_set);  // NOTE: remember

    this->filterScoresDropdown->setText(text_to_set);
    db->getOnlineScores().clear();
    this->rebuildScoreButtons();
    this->scoreBrowser->scrollToTop();
}

void SongBrowser::onSortScoresChange(const UString &text, int /*id*/) {
    cv::songbrowser_scores_sortingtype.setValue(text);  // NOTE: remember
    this->sortScoresDropdown->setText(text);
    this->rebuildScoreButtons();
    this->scoreBrowser->scrollToTop();
}

void SongBrowser::onWebClicked(CBaseUIButton * /*button*/) {
    if(this->songInfo->getBeatmapID() > 0) {
        env->openURLInDefaultBrowser(fmt::format("https://osu.ppy.sh/b/{}", this->songInfo->getBeatmapID()));
        osu->getNotificationOverlay()->addNotification("Opening browser, please wait ...", 0xffffffff, false, 0.75f);
    }
}

void SongBrowser::onQuickGroupClicked(CBaseUIButton *button) {
    if(button->getText().isEmpty()) return;
    const std::string_view btntxt = button->getText().utf8View();

    for(int gid = -1; const auto &gname : GROUP_NAMES) {
        ++gid;
        if(btntxt == gname) {
            this->onGroupChange(gname, gid);
            break;
        }
    }
}

void SongBrowser::onGroupClicked(CBaseUIButton *button) {
    this->contextMenu->setPos(button->getPos());
    this->contextMenu->setRelPos(button->getRelPos());
    this->contextMenu->begin(button->getSize().x);
    {
        for(int gid = -1; const auto &gname : GROUP_NAMES) {
            ++gid;
            CBaseUIButton *button = this->contextMenu->addButton(gname, gid);
            if(gid == this->curGroup) button->setTextBrightColor(0xff00ff00);
        }
    }
    this->contextMenu->end(false, false);
    this->contextMenu->setClickCallback(SA::MakeDelegate<&SongBrowser::onGroupChange>(this));
}

std::vector<CollectionButton *> *SongBrowser::getCollectionButtonsForGroup(GroupType group) {
    switch(group) {
        case GroupType::ARTIST:
            return &this->artistCollectionButtons;
        case GroupType::CREATOR:
            return &this->creatorCollectionButtons;
        case GroupType::DIFFICULTY:
            return &this->difficultyCollectionButtons;
        case GroupType::LENGTH:
            return &this->lengthCollectionButtons;
        case GroupType::TITLE:
            return &this->titleCollectionButtons;
        case GroupType::BPM:
            return &this->bpmCollectionButtons;
        // case GROUP::GROUP_DATEADDED:
        //     return &this->dateaddedCollectionButtons;
        case GroupType::COLLECTIONS:
            return &this->collectionButtons;
        case GroupType::NO_GROUPING:
        default:
            return nullptr;
    }
    return nullptr;
}

void SongBrowser::onGroupChange(const UString &text, int id) {
    auto group_id = this->curGroup;
    if(id >= 0 && id < GroupType::MAX) {
        group_id = (GroupType)id;
    } else if(!text.isEmpty()) {
        for(int gid = -1; const auto &gname : GROUP_NAMES) {
            ++gid;
            if(text.utf8View() == gname) {
                group_id = (GroupType)gid;
                break;
            }
        }
    }

    // update group combobox button text
    this->groupButton->setText(GROUP_NAMES[group_id]);

    // set highlighted colour
    this->groupByCollectionBtn->setTextBrightColor(defaultColor);
    this->groupByArtistBtn->setTextBrightColor(defaultColor);
    this->groupByDifficultyBtn->setTextBrightColor(defaultColor);
    this->groupByNothingBtn->setTextBrightColor(defaultColor);

    switch(group_id) {
        case GroupType::ARTIST:
            this->groupByArtistBtn->setTextBrightColor(highlightColor);
            break;
        case GroupType::DIFFICULTY:
            this->groupByDifficultyBtn->setTextBrightColor(highlightColor);
            break;
        case GroupType::COLLECTIONS:
            this->groupByCollectionBtn->setTextBrightColor(highlightColor);
            break;
        case GroupType::NO_GROUPING:
        default:
            this->groupByNothingBtn->setTextBrightColor(highlightColor);
            break;
    }

    // and update the actual songbrowser contents
    rebuildAfterGroupOrSortChange(group_id);
}

void SongBrowser::onSortClicked(CBaseUIButton *button) {
    this->contextMenu->setPos(button->getPos());
    this->contextMenu->setRelPos(button->getRelPos());
    this->contextMenu->begin(button->getSize().x);
    {
        for(int stype = -1; const auto &sortmeth : SORTING_METHODS) {
            ++stype;
            CBaseUIButton *button = this->contextMenu->addButton(sortmeth.name, stype);
            if(stype == this->curSortMethod) button->setTextBrightColor(0xff00ff00);
        }
    }
    this->contextMenu->end(false, false);
    this->contextMenu->setClickCallback(SA::MakeDelegate<&SongBrowser::onSortChange>(this));
}

void SongBrowser::onSortChange(const UString &text, int id) {
    auto sort_id = this->curSortMethod;
    if(id >= 0 && id < SortType::MAX) {
        sort_id = (SortType)id;
    } else if(!text.isEmpty()) {
        for(int sid = -1; const auto &sortmeth : SORTING_METHODS) {
            ++sid;
            if(text.utf8View() == sortmeth.name) {
                sort_id = (SortType)sid;
                break;
            }
        }
    }

    SORTING_METHOD newMethod = SORTING_METHODS[sort_id];

    const bool sortChanged = sort_id != this->curSortMethod;

    this->sortButton->setText(newMethod.name);
    cv::songbrowser_sortingtype.setValue(newMethod.name);

    // reuse the group update logic instead of duplicating it
    if(!sortChanged) {
        this->rebuildAfterGroupOrSortChange(this->curGroup);
    } else {
        this->rebuildAfterGroupOrSortChange(this->curGroup, sort_id);
    }
}

void SongBrowser::rebuildAfterGroupOrSortChange(GroupType group, const std::optional<SortType> &sortMethod) {
    const bool sortingChanged = this->curSortMethod != sortMethod.value_or(this->curSortMethod);
    const bool groupingChanged = this->curGroup != group;

    this->curGroup = group;
    this->curSortMethod = sortMethod.value_or(this->curSortMethod);

    if(this->bSongButtonsNeedSorting || sortingChanged) {
        // the master button list should be sorted for all groupings
        std::ranges::sort(this->parentButtons, SORTING_METHODS[this->curSortMethod].comparator);
        this->bSongButtonsNeedSorting = false;
    }

    this->visibleSongButtons.clear();

    if(group == GroupType::NO_GROUPING) {
        this->visibleSongButtons.reserve(this->parentButtons.size());

        // apply visibility logic
        for(auto *parentButton : this->parentButtons) {
            const SetVisibility visibility = this->getSetVisibility(parentButton);

            switch(visibility) {
                case SetVisibility::HIDDEN:
                    break;

                case SetVisibility::SINGLE_CHILD:
                    for(auto *child : parentButton->getChildren()) {
                        if(child->isSearchMatch()) {
                            this->visibleSongButtons.push_back(child);
                            break;
                        }
                    }
                    break;

                case SetVisibility::SHOW_PARENT:
                    this->visibleSongButtons.push_back(parentButton);
                    break;
            }
        }
    } else {
        auto *groupButtons = this->getCollectionButtonsForGroup(group);
        if(groupButtons != nullptr) {
            this->visibleSongButtons.reserve(groupButtons->size());
            this->visibleSongButtons.insert(this->visibleSongButtons.end(), groupButtons->begin(), groupButtons->end());

            // only sort if switching TO this group/sorting method (not from it)
            if(groupingChanged || sortingChanged) {
                for(auto &groupButton : *groupButtons) {
                    auto &children = groupButton->getChildren();
                    if(!children.empty()) {
                        std::ranges::sort(children, SORTING_METHODS[this->curSortMethod].comparator);
                        groupButton->setChildren(children);
                    }
                }
            }
        }
    }

    this->rebuildSongButtons();

    // keep search state consistent between tab changes
    if(this->bInSearch) this->onSearchUpdate();

    // (can't call it right here because we maybe have async)
    this->scheduled_scroll_to_selected_button = true;
}

void SongBrowser::onSelectionMode() {
    if(cv::mod_fposu.getBool()) {
        cv::mod_fposu.setValue(false);
        osu->getNotificationOverlay()->addToast(ULITERAL("Disabled FPoSu mode."), INFO_TOAST);
    } else {
        cv::mod_fposu.setValue(true);
        osu->getNotificationOverlay()->addToast(ULITERAL("Enabled FPoSu mode."), SUCCESS_TOAST);
    }
}

void SongBrowser::onSelectionMods() {
    soundEngine->play(osu->getSkin()->s_expand);
    osu->toggleModSelection(this->bF1Pressed);
}

void SongBrowser::onSelectionRandom() {
    soundEngine->play(osu->getSkin()->s_click_button);
    if(this->bShiftPressed)
        this->selectPreviousRandomBeatmap();
    else
        this->bRandomBeatmapScheduled = true;
}

void SongBrowser::onSelectionOptions() {
    soundEngine->play(osu->getSkin()->s_click_button);

    if(this->selectedButton != nullptr) {
        this->scrollToSongButton(this->selectedButton);

        const vec2 heuristicSongButtonPositionAfterSmoothScrollFinishes =
            (this->carousel->getPos() + this->carousel->getSize() / 2.f);

        auto *songButtonPointer = this->selectedButton->as<SongButton>();
        auto *collectionButtonPointer = this->selectedButton->as<CollectionButton>();
        if(songButtonPointer != nullptr) {
            songButtonPointer->triggerContextMenu(heuristicSongButtonPositionAfterSmoothScrollFinishes);
        } else if(collectionButtonPointer != nullptr) {
            collectionButtonPointer->triggerContextMenu(heuristicSongButtonPositionAfterSmoothScrollFinishes);
        }
    }
}

void SongBrowser::onScoreClicked(CBaseUIButton *button) {
    auto *scoreButton = (ScoreButton *)button;

    // NOTE: the order of these two calls matters
    osu->getRankingScreen()->setBeatmapInfo(scoreButton->getScore().map);
    osu->getRankingScreen()->setScore(scoreButton->getScore());

    osu->getSongBrowser()->setVisible(false);
    osu->getRankingScreen()->setVisible(true);

    soundEngine->play(osu->getSkin()->s_menu_hit);
}

void SongBrowser::onScoreContextMenu(ScoreButton *scoreButton, int id) {
    // NOTE: see ScoreButton::onContextMenu()

    if(id == 2) {
        db->deleteScore(scoreButton->getScore().beatmap_hash, scoreButton->getScoreUnixTimestamp());

        this->rebuildScoreButtons();
        osu->getUserButton()->updateUserStats();
    }
}

void SongBrowser::onSongButtonContextMenu(SongButton *songButton, const UString &text, int id) {
    // debugLog("SongBrowser::onSongButtonContextMenu({:p}, {:s}, {:d})", songButton, text.toUtf8(), id);

    struct CollectionManagementHelper {
        static std::vector<MD5Hash> getBeatmapSetHashesForSongButton(SongButton *songButton) {
            std::vector<MD5Hash> beatmapSetHashes;
            {
                const auto &songButtonChildren = songButton->getChildren();
                if(songButtonChildren.size() > 0) {
                    for(auto i : songButtonChildren) {
                        beatmapSetHashes.push_back(i->getDatabaseBeatmap()->getMD5());
                    }
                } else {
                    const BeatmapSet *mapset = db->getBeatmapSet(songButton->getDatabaseBeatmap()->getSetID());
                    if(mapset != nullptr) {
                        const std::vector<DatabaseBeatmap *> &diffs = mapset->getDifficulties();
                        for(auto diff : diffs) {
                            beatmapSetHashes.push_back(diff->getMD5());
                        }
                    }
                }
            }
            return beatmapSetHashes;
        }
    };

    bool updateUI = false;
    {
        if(id == 1) {
            // add diff to collection
            std::string name = text.toUtf8();
            auto &collection = Collections::get_or_create_collection(name);
            collection.add_map(songButton->getDatabaseBeatmap()->getMD5());
            Collections::save_collections();
            updateUI = true;
        } else if(id == 2) {
            // add set to collection
            std::string name = text.toUtf8();
            auto &collection = Collections::get_or_create_collection(name);
            const std::vector<MD5Hash> beatmapSetHashes =
                CollectionManagementHelper::getBeatmapSetHashesForSongButton(songButton);
            for(const auto &hash : beatmapSetHashes) {
                collection.add_map(hash);
            }
            Collections::save_collections();
            updateUI = true;
        } else if(id == 3) {
            // remove diff from collection

            // get collection name by selection
            std::string collectionName;
            {
                for(auto &collectionButton : this->collectionButtons) {
                    if(collectionButton->isSelected()) {
                        collectionName = collectionButton->getCollectionName();
                        break;
                    }
                }
            }

            auto &collection = Collections::get_or_create_collection(collectionName);
            collection.remove_map(songButton->getDatabaseBeatmap()->getMD5());
            Collections::save_collections();
            updateUI = true;
        } else if(id == 4) {
            // remove entire set from collection

            // get collection name by selection
            std::string collectionName;
            {
                for(auto &collectionButton : this->collectionButtons) {
                    if(collectionButton->isSelected()) {
                        collectionName = collectionButton->getCollectionName();
                        break;
                    }
                }
            }

            auto &collection = Collections::get_or_create_collection(collectionName);
            const std::vector<MD5Hash> beatmapSetHashes =
                CollectionManagementHelper::getBeatmapSetHashesForSongButton(songButton);
            for(const auto &hash : beatmapSetHashes) {
                collection.remove_map(hash);
            }
            Collections::save_collections();
            updateUI = true;
        } else if(id == -2 || id == -4) {
            // add beatmap(set) to new collection
            std::string name = text.toUtf8();
            auto &collection = Collections::get_or_create_collection(name);

            if(id == -2) {
                // id == -2 means beatmap
                collection.add_map(songButton->getDatabaseBeatmap()->getMD5());
                updateUI = true;
            } else if(id == -4) {
                // id == -4 means beatmapset
                const std::vector<MD5Hash> beatmapSetHashes =
                    CollectionManagementHelper::getBeatmapSetHashesForSongButton(songButton);
                for(const auto &hash : beatmapSetHashes) {
                    collection.add_map(hash);
                }
                updateUI = true;
            }

            Collections::save_collections();
        }
    }

    if(updateUI) {
        const float prevScrollPosY = this->carousel->getRelPosY();  // usability
        const auto previouslySelectedCollectionName =
            (this->selectionPreviousCollectionButton != nullptr
                 ? this->selectionPreviousCollectionButton->getCollectionName()
                 : "");  // usability
        {
            this->recreateCollectionsButtons();
            this->rebuildSongButtonsAndVisibleSongButtonsWithSearchMatchSupport(
                false, false);  // (last false = skipping rebuildSongButtons() here)
            this->onSortChange(
                cv::songbrowser_sortingtype.getString().c_str());  // (because this does the rebuildSongButtons())
        }
        if(previouslySelectedCollectionName.length() > 0) {
            for(auto &collectionButton : this->collectionButtons) {
                if(collectionButton->getCollectionName() == previouslySelectedCollectionName) {
                    collectionButton->select();
                    this->carousel->scrollToY(prevScrollPosY, false);
                    break;
                }
            }
        }
    }
}

void SongBrowser::onCollectionButtonContextMenu(CollectionButton *collectionButton, const UString &text, int id) {
    std::string collection_name = text.toUtf8();

    if(id == 2) {  // delete collection
        if(const auto &it =
               std::ranges::find(this->collectionButtons, collection_name,
                                 [](const auto &collBtn) -> std::string { return collBtn->getCollectionName(); });
           it != this->collectionButtons.end() && Collections::delete_collection(collection_name)) {
            Collections::save_collections();

            // delete UI
            delete *it;
            this->collectionButtons.erase(it);

            // reset UI state
            this->selectionPreviousCollectionButton = nullptr;

            // update UI
            this->rebuildAfterGroupOrSortChange(GroupType::COLLECTIONS);
        }
    } else if(id == 3) {  // rename collection
        const std::string &currentButtonName = collectionButton->getCollectionName();
        if(!currentButtonName.empty()) {
            auto &existingCollection = Collections::get_or_create_collection(currentButtonName);

            if(existingCollection.rename_to(collection_name)) {
                Collections::save_collections();

                // rename button
                if(const auto &it = std::ranges::find(this->collectionButtons, collectionButton);
                   it != this->collectionButtons.end()) {
                    (*it)->setCollectionName(collection_name);

                    // resort collection buttons
                    std::ranges::stable_sort(
                        this->collectionButtons,
                        [](const char *s1, const char *s2) -> bool { return strcasecmp(s1, s2) < 0; },
                        [](const auto &colBtn) -> const char * { return colBtn->getCollectionName().c_str(); });
                }

                // update UI
                this->onSortChange(cv::songbrowser_sortingtype.getString().c_str());
            }
        }
    }
}

void SongBrowser::highlightScore(u64 unixTimestamp) {
    for(auto &i : this->scoreButtonCache) {
        if(i->getScore().unixTimestamp == unixTimestamp) {
            this->scoreBrowser->scrollToElement(i, 0, 10);
            i->highlight();
            break;
        }
    }
}

void SongBrowser::selectSongButton(CarouselButton *songButton) {
    if(songButton != nullptr && !songButton->isSelected()) {
        this->contextMenu->setVisible2(false);
        songButton->select();
    }
}

void SongBrowser::selectRandomBeatmap() {
    // filter songbuttons or independent diffs
    const auto &elements{
        reinterpret_cast<const std::vector<CarouselButton *> &>(this->carousel->container->getElements())};

    std::vector<SongButton *> songButtons;
    for(auto element : elements) {
        auto *songButtonPointer = element->as<SongButton>();
        auto *songDifficultyButtonPointer = element->as<SongDifficultyButton>();

        if(songButtonPointer != nullptr &&
           (songDifficultyButtonPointer == nullptr ||
            songDifficultyButtonPointer->isIndependentDiffButton()))  // only allow songbuttons or independent diffs
            songButtons.push_back(songButtonPointer);
    }

    if(songButtons.size() < 1) return;

    // remember previous
    if(osu->getMapInterface()->getBeatmap() != nullptr && !osu->getMapInterface()->getBeatmap()->do_not_store) {
        this->previousRandomBeatmaps.push_back(osu->getMapInterface()->getBeatmap());
    }

    std::uniform_int_distribution<size_t> rng(0, songButtons.size() - 1);
    size_t randomIndex = rng(this->rngalg);

    auto *songButton = songButtons[randomIndex]->as<SongButton>();
    this->selectSongButton(songButton);
}

void SongBrowser::selectPreviousRandomBeatmap() {
    if(this->previousRandomBeatmaps.size() > 0) {
        const auto *currentRandomBeatmap = this->previousRandomBeatmaps.back();
        if(this->previousRandomBeatmaps.size() > 1 && currentRandomBeatmap == osu->getMapInterface()->getBeatmap())
            this->previousRandomBeatmaps.pop_back();  // deletes the current beatmap which may also be at the top (so
                                                      // we don't switch to ourself)

        // filter songbuttons
        const auto &elements{
            reinterpret_cast<const std::vector<CarouselButton *> &>(this->carousel->container->getElements())};

        std::vector<SongButton *> songButtons;
        for(auto element : elements) {
            auto *songButtonPointer = element->as<SongButton>();

            if(songButtonPointer != nullptr)  // allow ALL songbuttons
                songButtons.push_back(songButtonPointer);
        }

        // select it, if we can find it (and remove it from memory)
        bool foundIt = false;
        const DatabaseBeatmap *previousRandomBeatmap = this->previousRandomBeatmaps.back();
        for(auto &songButton : songButtons) {
            if(songButton->getDatabaseBeatmap() != nullptr &&
               songButton->getDatabaseBeatmap() == previousRandomBeatmap) {
                this->previousRandomBeatmaps.pop_back();
                this->selectSongButton(songButton);
                foundIt = true;
                break;
            }

            const auto &children = songButton->getChildren();
            for(auto c : children) {
                if(c->getDatabaseBeatmap() == previousRandomBeatmap) {
                    this->previousRandomBeatmaps.pop_back();
                    this->selectSongButton(c);
                    foundIt = true;
                    break;
                }
            }

            if(foundIt) break;
        }

        // if we didn't find it then restore the current random beatmap, which got pop_back()'d above (shit logic)
        if(!foundIt) this->previousRandomBeatmaps.push_back(currentRandomBeatmap);
    }
}

void SongBrowser::playSelectedDifficulty() {
    const auto &elements{
        reinterpret_cast<const std::vector<CarouselButton *> &>(this->carousel->container->getElements())};

    for(auto element : elements) {
        auto *songDifficultyButton = element->as<SongDifficultyButton>();
        if(songDifficultyButton != nullptr && songDifficultyButton->isSelected()) {
            songDifficultyButton->select();
            break;
        }
    }
}

void SongBrowser::recreateCollectionsButtons() {
    // reset
    {
        this->selectionPreviousCollectionButton = nullptr;
        for(auto &collectionButton : this->collectionButtons) {
            delete collectionButton;
        }
        this->collectionButtons.clear();

        // sanity
        if(this->curGroup == GroupType::COLLECTIONS) {
            this->carousel->invalidate();
            this->visibleSongButtons.clear();
        }
    }

    Timer t;
    t.start();

    for(const auto &collection : Collections::get_loaded()) {
        const auto &maps = collection.get_maps();
        if(maps.empty()) continue;

        std::vector<SongButton *> folder;
        std::vector<u32> matched_sets;

        for(const auto &map : maps) {
            auto it = this->hashToDiffButton.find(map);
            if(it == this->hashToDiffButton.end()) continue;

            std::vector<SongButton *> matching_diffs;

            auto *song_button = it->second;

            // get parent's children (we always have a parent now)
            auto *parent = song_button->getParentSongButton();
            const std::vector<SongButton *> &songButtonChildren = parent->getChildren();

            // filter to only diffs in this collection
            for(SongButton *sbc : songButtonChildren | std::views::filter([&maps](const auto &child) {
                                      return std::ranges::contains(maps, child->getDatabaseBeatmap()->getMD5());
                                  })) {
                matching_diffs.push_back(sbc);
            }

            const i32 set_id = song_button->getDatabaseBeatmap()->getSetID();

            if(!std::ranges::contains(matched_sets, set_id)) {
                // Mark set as processed so we don't add the diffs from the same set twice
                matched_sets.push_back(set_id);
            } else {
                // We already added the maps from this set to the collection!
                continue;
            }

            if(songButtonChildren.size() == matching_diffs.size()) {
                // all diffs match: add the set button (user added all diffs of beatmap into collection)
                folder.push_back(parent);  // fixed: add parent instead of song_button
            } else {
                // only add matched diff buttons
                folder.insert(folder.end(), matching_diffs.begin(), matching_diffs.end());
            }
        }

        if(!folder.empty()) {
            this->collectionButtons.push_back(new CollectionButton(this->contextMenu, 250,
                                                                   250 + db->getBeatmapSets().size() * 50, 200, 50, "",
                                                                   collection.get_name(), folder));
        }
    }

    // sort buttons by name
    std::ranges::stable_sort(
        this->collectionButtons, [](const char *s1, const char *s2) -> bool { return strcasecmp(s1, s2) < 0; },
        [](const auto &colBtn) -> const char * { return colBtn->getCollectionName().c_str(); });

    t.update();
    debugLog("recreateCollectionsButtons(): {:f} seconds", t.getElapsedTime());
}
