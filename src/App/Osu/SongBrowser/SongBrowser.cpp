// Copyright (c) 2016, PG, All rights reserved.
#include "BaseEnvironment.h"

#include "AnimationHandler.h"
#include "BackgroundImageHandler.h"
#include "Bancho.h"
#include "BanchoLeaderboard.h"
#include "BanchoNetworking.h"
#include "Beatmap.h"
#include "BeatmapCarousel.h"
#include "BottomBar.h"
#include "CBaseUIContainer.h"
#include "CBaseUILabel.h"
#include "CBaseUIScrollView.h"
#include "Chat.h"
#include "CollectionButton.h"
#include "Collections.h"
#include "ConVar.h"
#include "Database.h"
#include "DatabaseBeatmap.h"
#include "Downloader.h"
#include "Engine.h"
#include "HUD.h"
#include "Icons.h"
#include "InfoLabel.h"
#include "KeyBindings.h"
#include "Keyboard.h"
#include "LeaderboardPPCalcThread.h"
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
#include "SongBrowser.h"
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

#include <algorithm>
#include <chrono>
#include <memory>
#include <utility>
#include <filesystem>

namespace fs = std::filesystem;

const Color highlightColor = argb(255, 0, 255, 0);
const Color defaultColor = argb(255, 255, 255, 255);

class SongBrowserBackgroundSearchMatcher : public Resource {
   public:
    SongBrowserBackgroundSearchMatcher() : Resource() {
        this->bDead = true;  // NOTE: start dead! need to revive() before use
    }

    [[nodiscard]] bool isDead() const { return this->bDead.load(); }
    void kill() { this->bDead = true; }
    void revive() { this->bDead = false; }

    void setSongButtonsAndSearchString(const std::vector<SongButton *> &songButtons, const std::string &searchString,
                                       const std::string &hardcodedSearchString) {
        this->songButtons = songButtons;

        this->sSearchString.clear();
        if(hardcodedSearchString.length() > 0) {
            this->sSearchString.append(hardcodedSearchString);
            this->sSearchString.append(" ");
        }
        this->sSearchString.append(searchString);
        // do case-insensitive searches
        SString::to_lower(this->sSearchString);
        this->sHardcodedSearchString = hardcodedSearchString;
    }

    [[nodiscard]] Type getResType() const override { return APPDEFINED; }  // TODO: handle this better?

   protected:
    void init() override { this->bReady = true; }

    void initAsync() override {
        if(this->bDead.load()) {
            this->bAsyncReady = true;
            return;
        }

        // flag matches across entire database
        const std::vector<std::string> searchStringTokens = SString::split(this->sSearchString, " ");
        for(auto &songButton : this->songButtons) {
            const auto &children = songButton->getChildren();
            if(children.size() > 0) {
                for(auto c : children) {
                    c->setIsSearchMatch(SongBrowser::searchMatcher(c->getDatabaseBeatmap(), searchStringTokens));
                }
            } else
                songButton->setIsSearchMatch(
                    SongBrowser::searchMatcher(songButton->getDatabaseBeatmap(), searchStringTokens));

            // cancellation point
            if(this->bDead.load()) break;
        }

        this->bAsyncReady = true;
    }

    void destroy() override { ; }

   private:
    std::atomic<bool> bDead;

    std::string sSearchString;
    std::string sHardcodedSearchString;
    std::vector<SongButton *> songButtons;
};

class ScoresStillLoadingElement : public CBaseUILabel {
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

class NoRecordsSetElement : public CBaseUILabel {
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

bool sort_by_difficulty(SongButton const *a, SongButton const *b) {
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

namespace {  // static namespace

// not used anywhere else

bool sort_by_artist(SongButton const *a, SongButton const *b) {
    const auto *aPtr = a->getDatabaseBeatmap(), *bPtr = b->getDatabaseBeatmap();
    if((aPtr == nullptr) || (bPtr == nullptr)) return (aPtr == nullptr) < (bPtr == nullptr);

    const auto &artistA{aPtr->getArtistLatin()};
    const auto &artistB{bPtr->getArtistLatin()};

    i32 cmp = strcasecmp(artistA.c_str(), artistB.c_str());
    if(cmp == 0) return sort_by_difficulty(a, b);
    return cmp < 0;
}

bool sort_by_bpm(SongButton const *a, SongButton const *b) {
    const auto *aPtr = a->getDatabaseBeatmap(), *bPtr = b->getDatabaseBeatmap();
    if((aPtr == nullptr) || (bPtr == nullptr)) return (aPtr == nullptr) < (bPtr == nullptr);

    int bpm1 = aPtr->getMostCommonBPM();
    int bpm2 = bPtr->getMostCommonBPM();
    if(bpm1 == bpm2) return sort_by_difficulty(a, b);
    return bpm1 < bpm2;
}

bool sort_by_creator(SongButton const *a, SongButton const *b) {
    const auto *aPtr = a->getDatabaseBeatmap(), *bPtr = b->getDatabaseBeatmap();
    if((aPtr == nullptr) || (bPtr == nullptr)) return (aPtr == nullptr) < (bPtr == nullptr);

    const auto &creatorA{aPtr->getCreator()};
    const auto &creatorB{bPtr->getCreator()};

    i32 cmp = strcasecmp(creatorA.c_str(), creatorB.c_str());
    if(cmp == 0) return sort_by_difficulty(a, b);
    return cmp < 0;
}

bool sort_by_date_added(SongButton const *a, SongButton const *b) {
    const auto *aPtr = a->getDatabaseBeatmap(), *bPtr = b->getDatabaseBeatmap();
    if((aPtr == nullptr) || (bPtr == nullptr)) return (aPtr == nullptr) < (bPtr == nullptr);

    long long time1 = aPtr->last_modification_time;
    long long time2 = bPtr->last_modification_time;
    if(time1 == time2) return sort_by_difficulty(a, b);
    return time1 > time2;
}

bool sort_by_grade(SongButton const *a, SongButton const *b) {
    if(a->grade == b->grade) return sort_by_difficulty(a, b);
    return a->grade < b->grade;
}

bool sort_by_length(SongButton const *a, SongButton const *b) {
    const auto *aPtr = a->getDatabaseBeatmap(), *bPtr = b->getDatabaseBeatmap();
    if((aPtr == nullptr) || (bPtr == nullptr)) return (aPtr == nullptr) < (bPtr == nullptr);

    unsigned long length1 = aPtr->getLengthMS();
    unsigned long length2 = bPtr->getLengthMS();
    if(length1 == length2) return sort_by_difficulty(a, b);
    return length1 < length2;
}

bool sort_by_title(SongButton const *a, SongButton const *b) {
    const auto *aPtr = a->getDatabaseBeatmap(), *bPtr = b->getDatabaseBeatmap();
    if((aPtr == nullptr) || (bPtr == nullptr)) return (aPtr == nullptr) < (bPtr == nullptr);

    const auto &titleA{aPtr->getTitleLatin()};
    const auto &titleB{bPtr->getTitleLatin()};

    i32 cmp = strcasecmp(titleA.c_str(), titleB.c_str());
    if(cmp == 0) return sort_by_difficulty(a, b);
    return cmp < 0;
}

}  // namespace

SongBrowser::SongBrowser()  // NOLINT(cert-msc51-cpp, cert-msc32-c)
    : ScreenBackable() {
    // random selection algorithm init
    this->rngalg.seed(crypto::rng::get_rand<u64>());

    // sorting/grouping + methods
    this->group = GROUP::GROUP_NO_GROUPING;
    this->groupings = {{{.type = GROUP::GROUP_NO_GROUPING, .name = "No Grouping", .id = 0},
                        {.type = GROUP::GROUP_ARTIST, .name = "By Artist", .id = 1},
                        {.type = GROUP::GROUP_BPM, .name = "By BPM", .id = 2},
                        {.type = GROUP::GROUP_CREATOR, .name = "By Creator", .id = 3},
                        //{GROUP::GROUP_DATEADDED, "By Date Added", 4}); // not yet possible
                        {.type = GROUP::GROUP_DIFFICULTY, .name = "By Difficulty", .id = 5},
                        {.type = GROUP::GROUP_LENGTH, .name = "By Length", .id = 6},
                        {.type = GROUP::GROUP_TITLE, .name = "By Title", .id = 7},
                        {.type = GROUP::GROUP_COLLECTIONS, .name = "Collections", .id = 8}}};

    this->sortingMethod = SORT::SORT_ARTIST;
    this->sortingComparator = sort_by_artist;

    this->sortingMethods = {
        {{.type = SORT::SORT_ARTIST, .name = "By Artist", .comparator = sort_by_artist},
         {.type = SORT::SORT_BPM, .name = "By BPM", .comparator = sort_by_bpm},
         {.type = SORT::SORT_CREATOR, .name = "By Creator", .comparator = sort_by_creator},
         {.type = SORT::SORT_DATEADDED, .name = "By Date Added", .comparator = sort_by_date_added},
         {.type = SORT::SORT_DIFFICULTY, .name = "By Difficulty", .comparator = sort_by_difficulty},
         {.type = SORT::SORT_LENGTH, .name = "By Length", .comparator = sort_by_length},
         {.type = SORT::SORT_TITLE, .name = "By Title", .comparator = sort_by_title},
         {.type = SORT::SORT_RANKACHIEVED, .name = "By Rank Achieved", .comparator = sort_by_grade}}};

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
    this->bPreviousRandomBeatmapScheduled = false;

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
    this->scoreBrowser->getContainer()->addBaseUIElement(this->scoreBrowserNoRecordsYetElement);

    // NOTE: we don't add localBestContainer to the screen; we draw and update it manually so that
    //       it can be drawn under skins which overlay the scores list.
    this->localBestContainer = std::make_unique<CBaseUIContainer>(0.f, 0.f, 0.f, 0.f, "");
    this->localBestContainer->setVisible(false);
    this->localBestLabel = new CBaseUILabel(0, 0, 0, 0, "", "Personal Best (from local scores)");
    this->localBestLabel->setDrawBackground(false);
    this->localBestLabel->setDrawFrame(false);
    this->localBestLabel->setTextJustification(CBaseUILabel::TEXT_JUSTIFICATION::TEXT_JUSTIFICATION_CENTERED);

    // build carousel
    this->carousel = std::make_unique<BeatmapCarousel>(this, 0.f, 0.f, 0.f, 0.f, "Carousel");
    this->thumbnailYRatio = cv::draw_songbrowser_thumbnails.getBool() ? 1.333333f : 0.f;

    // beatmap database
    db = std::make_unique<Database>();
    this->bBeatmapRefreshScheduled = true;

    // behaviour
    this->bHasSelectedAndIsPlaying = false;
    this->fPulseAnimation = 0.0f;
    this->fBackgroundFadeInTime = 0.0f;

    // search
    this->search = new UISearchOverlay(0, 0, 0, 0, "");
    this->search->setOffsetRight(10);
    this->fSearchWaitTime = 0.0f;
    this->bInSearch = (cv::songbrowser_search_hardcoded_filter.getString().length() > 0);
    this->searchPrevGroup = GROUP::GROUP_NO_GROUPING;
    this->backgroundSearchMatcher = new SongBrowserBackgroundSearchMatcher();

    this->beatmap = new Beatmap();

    this->updateLayout();
}

SongBrowser::~SongBrowser() {
    sct_abort();
    lct_set_map(nullptr);
    VolNormalization::abort();
    MapCalcThread::abort();
    this->checkHandleKillBackgroundSearchMatcher();

    resourceManager->destroyResource(this->backgroundSearchMatcher);

    for(auto &songButton : this->songButtons) {
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

    this->scoreBrowser->getContainer()->invalidate();
    for(ScoreButton *button : this->scoreButtonCache) {
        SAFE_DELETE(button);
    }
    this->scoreButtonCache.clear();

    this->localBestContainer->invalidate();  // contained elements freed manually below
    SAFE_DELETE(this->localBestButton);
    SAFE_DELETE(this->localBestLabel);
    SAFE_DELETE(this->scoreBrowserScoresStillLoadingElement);
    SAFE_DELETE(this->scoreBrowserNoRecordsYetElement);

    SAFE_DELETE(this->beatmap);
    SAFE_DELETE(this->contextMenu);
    SAFE_DELETE(this->search);
    SAFE_DELETE(this->topbarLeft);
    SAFE_DELETE(this->topbarRight);
    SAFE_DELETE(this->scoreBrowser);
    db.reset();

    // Memory leak on shutdown, maybe
    this->invalidate();
}

void SongBrowser::draw() {
    if(!this->bVisible) return;

    // draw background
    g->setColor(0xff000000);
    g->fillRect(0, 0, osu->getScreenWidth(), osu->getScreenHeight());

    // refreshing (blocks every other call in draw() below it!)
    if(this->bBeatmapRefreshScheduled) {
        UString loadingMessage = UString::format("Loading beatmaps ... (%i %%)", (int)(db->getProgress() * 100.0f));

        g->setColor(0xffffffff);
        g->pushTransform();
        {
            g->translate((int)(osu->getScreenWidth() / 2 - osu->getSubTitleFont()->getStringWidth(loadingMessage) / 2),
                         osu->getScreenHeight() - 15);
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
            const bool ready = osu->getSelectedBeatmap()->getSelectedDifficulty2() != nullptr &&
                               osu->getBackgroundImageHandler()->getLoadBackgroundImage(
                                   osu->getSelectedBeatmap()->getSelectedDifficulty2()) != nullptr &&
                               osu->getBackgroundImageHandler()
                                   ->getLoadBackgroundImage(osu->getSelectedBeatmap()->getSelectedDifficulty2())
                                   ->isReady();

            if(!ready)
                this->fBackgroundFadeInTime = engine->getTime();
            else if(this->fBackgroundFadeInTime > 0.0f && engine->getTime() > this->fBackgroundFadeInTime) {
                alpha = std::clamp<float>((engine->getTime() - this->fBackgroundFadeInTime) /
                                              cv::songbrowser_background_fade_in_duration.getFloat(),
                                          0.0f, 1.0f);
                alpha = 1.0f - (1.0f - alpha) * (1.0f - alpha);
            }
        }

        drawSelectedBeatmapBackgroundImage(alpha);
    } else if(cv::draw_songbrowser_menu_background_image.getBool()) {
        // menu-background
        Image *backgroundImage = osu->getSkin()->getMenuBackground();
        if(backgroundImage != nullptr && backgroundImage != osu->getSkin()->getMissingTexture() &&
           backgroundImage->isReady()) {
            const float scale = Osu::getImageScaleToFillResolution(backgroundImage, osu->getScreenSize());

            g->setColor(0xffffffff);
            g->pushTransform();
            {
                g->scale(scale, scale);
                g->translate(osu->getScreenWidth() / 2, osu->getScreenHeight() / 2);
                g->drawImage(backgroundImage);
            }
            g->popTransform();
        }
    }

    {
        auto screen = osu->getScreenSize();
        bool is_widescreen = ((i32)(std::max(0, (i32)((screen.x - (screen.y * 4.f / 3.f)) / 2.f))) > 0);
        f32 global_scale = screen.x / (is_widescreen ? 1366.f : 1024.f);
        f32 mode_osu_scale = global_scale * (osu->getSkin()->mode_osu->is_2x ? 0.5f : 1.f);

        g->setColor(0xffffffff);
        if(cv::avoid_flashes.getBool()) {
            g->setAlpha(0.1f);
        } else {
            // XXX: Flash based on song BPM
            g->setAlpha(0.1f);
        }

        g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_ADDITIVE);
        osu->getSkin()->mode_osu->drawRaw(vec2(osu->getScreenWidth() / 2, osu->getScreenHeight() / 2), mode_osu_scale,
                                          AnchorPoint::CENTER);
        g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_ALPHA);
    }

    // draw score browser
    this->scoreBrowser->draw();
    this->localBestContainer->draw();

    if(cv::debug_osu.getBool()) {
        this->scoreBrowser->getContainer()->draw_debug();
    }

    // draw strain graph of currently selected beatmap
    if(cv::draw_songbrowser_strain_graph.getBool()) {
        const std::vector<double> &aimStrains = this->getSelectedBeatmap()->aimStrains;
        const std::vector<double> &speedStrains = this->getSelectedBeatmap()->speedStrains;
        const float speedMultiplier = this->getSelectedBeatmap()->getSpeedMultiplier();

        if(aimStrains.size() > 0 && aimStrains.size() == speedStrains.size()) {
            const float strainStepMS = 400.0f * speedMultiplier;

            const unsigned long lengthMS = strainStepMS * aimStrains.size();

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

                McRect graphRect(0, osu->getScreenHeight() - (get_bottombar_height() + strainHeightMultiplier),
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
                                    osu->getScreenHeight() - (get_bottombar_height() + aimStrainHeight),
                                    std::max(1.0f, std::round(strainWidth + 0.5f)), aimStrainHeight);
                    }

                    if(!keyboard->isControlDown()) {
                        g->setColor(speedStrainColor);
                        g->fillRect(i * strainWidth,
                                    osu->getScreenHeight() -
                                        (get_bottombar_height() +
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

                    vec2 topLeftCenter =
                        vec2(highestStrainIndex * strainWidth + strainWidth / 2.0f,
                             osu->getScreenHeight() - (get_bottombar_height() + aimStrainHeight + speedStrainHeight));

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
        auto screen = osu->getScreenSize();
        bool is_widescreen = ((i32)(std::max(0, (i32)((screen.x - (screen.y * 4.f / 3.f)) / 2.f))) > 0);

        Image *topbar = osu->getSkin()->songSelectTop;
        f32 scale = (f32)osu->getScreenWidth() / (f32)topbar->getWidth();
        if(!is_widescreen) scale /= 0.75;

        g->scale(scale, scale);
        g->drawImage(topbar, AnchorPoint::TOP_LEFT);
    }
    g->popTransform();

    // draw bottom bar
    draw_bottombar();

    // draw top bar
    this->topbarLeft->draw();
    if(cv::debug_osu.getBool()) this->topbarLeft->draw_debug();
    this->topbarRight->draw();
    if(cv::debug_osu.getBool()) this->topbarRight->draw_debug();

    // draw search
    this->search->setSearchString(this->sSearchString, cv::songbrowser_search_hardcoded_filter.getString().c_str());
    this->search->setDrawNumResults(this->bInSearch);
    this->search->setNumFoundResults(this->visibleSongButtons.size());
    this->search->setSearching(!this->backgroundSearchMatcher->isDead());
    this->search->draw();

    // NOTE: Intentionally not calling ScreenBackable::draw() here, since we're already drawing
    //       the back button in draw_bottombar().
    OsuScreen::draw();

    // no beatmaps found (osu folder is probably invalid)
    if(this->beatmaps.size() == 0 && !this->bBeatmapRefreshScheduled) {
        UString errorMessage1 = "Invalid osu! folder (or no beatmaps found): ";
        errorMessage1.append(this->sLastOsuFolder);
        UString errorMessage2 = "Go to Options -> osu!folder";

        g->setColor(0xffff0000);
        g->pushTransform();
        {
            g->translate((int)(osu->getScreenWidth() / 2 - osu->getSubTitleFont()->getStringWidth(errorMessage1) / 2),
                         (int)(osu->getScreenHeight() / 2 + osu->getSubTitleFont()->getHeight()));
            g->drawString(osu->getSubTitleFont(), errorMessage1);
        }
        g->popTransform();

        g->setColor(0xff00ff00);
        g->pushTransform();
        {
            g->translate((int)(osu->getScreenWidth() / 2 - osu->getSubTitleFont()->getStringWidth(errorMessage2) / 2),
                         (int)(osu->getScreenHeight() / 2 + osu->getSubTitleFont()->getHeight() * 2 + 15));
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

        g->fillGradient(0, 0, osu->getScreenWidth(), osu->getScreenHeight(), topColor, topColor, bottomColor,
                        bottomColor);
    }
}

void SongBrowser::drawSelectedBeatmapBackgroundImage(float alpha) {
    if(osu->getSelectedBeatmap()->getSelectedDifficulty2() != nullptr) {
        Image *backgroundImage = osu->getBackgroundImageHandler()->getLoadBackgroundImage(
            osu->getSelectedBeatmap()->getSelectedDifficulty2());
        if(backgroundImage != nullptr && backgroundImage->isReady()) {
            const float scale = Osu::getImageScaleToFillResolution(backgroundImage, osu->getScreenSize());

            g->setColor(Color(0xff999999).setA(alpha));

            g->pushTransform();
            {
                g->scale(scale, scale);
                g->translate(osu->getScreenWidth() / 2, osu->getScreenHeight() / 2);
                g->drawImage(backgroundImage);
            }
            g->popTransform();
        }
    }
}

bool SongBrowser::selectBeatmapset(i32 set_id) {
    auto beatmapset = db->getBeatmapSet(set_id);
    if(beatmapset == nullptr) {
        // Pasted from Downloader::download_beatmap
        auto mapset_path = UString::format(MCENGINE_DATA_DIR "maps/%d/", set_id);
        db->addBeatmapSet(mapset_path.toUtf8());
        debugLog("Finished loading beatmapset {:d}.\n", set_id);

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
        osu->notificationOverlay->addToast("Beatmapset has no difficulties", ERROR_TOAST);
        return false;
    } else {
        this->onSelectionChange(this->hashToSongButton[best_diff->getMD5Hash()], false);
        this->onDifficultySelected(best_diff, false);
        this->selectSelectedBeatmapSongButton();
        return true;
    }
}

void SongBrowser::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible) return;

    this->localBestContainer->mouse_update(propagate_clicks);
    ScreenBackable::mouse_update(propagate_clicks);

    // NOTE: This is placed before update_bottombar(), otherwise the context menu would close
    //       on a bottombar selector click (yeah, a bit hacky)
    this->contextMenu->mouse_update(propagate_clicks);

    update_bottombar(propagate_clicks);

    // refresh logic (blocks every other call in the update() function below it!)
    if(this->bBeatmapRefreshScheduled) {
        db->update();  // raw load logic
        // check if we are finished loading
        if(db->isFinished()) {
            this->bBeatmapRefreshScheduled = false;
            this->onDatabaseLoadingFinished();
        }
        return;
    }

    // map star/bpm/other calc
    if(MapCalcThread::is_finished()) {
        MapCalcThread::abort();  // join thread

        auto &maps = db->maps_to_recalc;

        const auto &results = MapCalcThread::get_results();

        db->peppy_overrides_mtx.lock();
        for(int i = 0; i < results.size(); i++) {
            auto diff = maps[i];
            auto res = results[i];
            diff->iNumCircles = res.nb_circles;
            diff->iNumSliders = res.nb_sliders;
            diff->iNumSpinners = res.nb_spinners;
            diff->fStarsNomod = res.star_rating;
            diff->iMinBPM = res.min_bpm;
            diff->iMaxBPM = res.max_bpm;
            diff->iMostCommonBPM = res.avg_bpm;
            db->peppy_overrides[diff->sMD5Hash] = diff->get_overrides();
        }
        db->peppy_overrides_mtx.unlock();

        maps.clear();
    }

    // auto-download
    if(this->map_autodl) {
        float progress = -1.f;
        auto beatmap = Downloader::download_beatmap(this->map_autodl, this->set_autodl, &progress);
        if(progress == -1.f) {
            auto error_str = UString::format("Failed to download Beatmap #%d :(", this->map_autodl);
            osu->notificationOverlay->addToast(error_str, ERROR_TOAST);
            this->map_autodl = 0;
            this->set_autodl = 0;
        } else if(progress < 1.f) {
            // TODO @kiwec: this notification format is jank & laggy
            auto text = UString::format("Downloading... %.2f%%", progress * 100.f);
            osu->notificationOverlay->addNotification(text);
        } else if(beatmap != nullptr) {
            osu->songBrowser2->onDifficultySelected(beatmap, false);
            osu->songBrowser2->selectSelectedBeatmapSongButton();
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
                osu->notificationOverlay->addToast(error_str, ERROR_TOAST);
                this->map_autodl = 0;
                this->set_autodl = 0;
            } else if(progress < 1.f) {
                // TODO @kiwec: this notification format is jank & laggy
                auto text = UString::format("Downloading... %.2f%%", progress * 100.f);
                osu->notificationOverlay->addNotification(text);
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
    if(this->bPreviousRandomBeatmapScheduled) {
        this->bPreviousRandomBeatmapScheduled = false;
        this->selectPreviousRandomBeatmap();
    }

    // if cursor is to the left edge of the screen, force center currently selected beatmap/diff
    // but only if the context menu is currently not visible (since we don't want move things while e.g. managing
    // collections etc.)
    // NOTE: it's very slow, so only run it every 10 vsync frames
    if(engine->throttledShouldRun(10) && !osu->getOptionsMenu()->isVisible() &&
       mouse->getPos().x < osu->getScreenWidth() * 0.1f && !this->contextMenu->isVisible()) {
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
       (key == KEY_ESCAPE || key == (KEYCODE)cv::GAME_PAUSE.getInt())) {
        db->cancel();
        key.consume();
        return;
    }

    if(this->bBeatmapRefreshScheduled) return;

    // context menu
    this->contextMenu->onKeyDown(key);
    if(key.isConsumed()) return;

    // searching text delete & escape key handling
    if(this->sSearchString.length() > 0) {
        switch(key.getKeyCode()) {
            case KEY_DELETE:
            case KEY_BACKSPACE:
                key.consume();
                if(this->sSearchString.length() > 0) {
                    if(keyboard->isControlDown()) {
                        // delete everything from the current caret position to the left, until after the first
                        // non-space character (but including it)
                        bool foundNonSpaceChar = false;
                        while(this->sSearchString.length() > 0) {
                            std::string curChar = this->sSearchString.substr(this->sSearchString.length() - 1, 1);

                            if(foundNonSpaceChar && SString::whitespace_only(curChar)) break;

                            if(!SString::whitespace_only(curChar)) foundNonSpaceChar = true;

                            this->sSearchString.erase(this->sSearchString.length() - 1, 1);
                        }
                    } else
                        this->sSearchString = this->sSearchString.substr(0, this->sSearchString.length() - 1);

                    this->scheduleSearchUpdate(this->sSearchString.length() == 0);
                }
                break;

            case KEY_ESCAPE:
                key.consume();
                this->sSearchString = "";
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
            const std::string clipstring{env->getClipBoardText().toUtf8()};
            if(clipstring.length() > 0) {
                this->sSearchString.append(clipstring);
                this->scheduleSearchUpdate(false);
            }
        }
    }

    if(key == KEY_LSHIFT || key == KEY_RSHIFT) this->bShiftPressed = true;

    // function hotkeys
    if((key == KEY_F1 || key == (KEYCODE)cv::TOGGLE_MODSELECT.getInt()) && !this->bF1Pressed) {
        this->bF1Pressed = true;
        press_bottombar_button(1);
    }
    if((key == KEY_F2 || key == (KEYCODE)cv::RANDOM_BEATMAP.getInt()) && !this->bF2Pressed) {
        this->bF2Pressed = true;
        press_bottombar_button(2);
    }
    if(key == KEY_F3 && !this->bF3Pressed) {
        this->bF3Pressed = true;
        press_bottombar_button(3);
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

    if(key == KEY_F1 || key == (KEYCODE)cv::TOGGLE_MODSELECT.getInt()) this->bF1Pressed = false;
    if(key == KEY_F2 || key == (KEYCODE)cv::RANDOM_BEATMAP.getInt()) this->bF2Pressed = false;
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
    this->sSearchString.append(std::string{static_cast<char>(e.getCharCode())});

    this->scheduleSearchUpdate();
}

void SongBrowser::onResolutionChange(vec2 newResolution) { ScreenBackable::onResolutionChange(newResolution); }

CBaseUIContainer *SongBrowser::setVisible(bool visible) {
    if(BanchoState::spectating && visible) return this;  // don't allow song browser to be visible while spectating
    if(visible == this->bVisible) return this;

    this->bVisible = visible;
    this->bShiftPressed = false;  // seems to get stuck sometimes otherwise

    if(this->bVisible) {
        soundEngine->play(osu->getSkin()->getExpandSound());
        RichPresence::onSongBrowser();

        this->updateLayout();

        // we have to re-select the current beatmap to start playing music again
        if(this->beatmap != nullptr) this->beatmap->select();

        this->bHasSelectedAndIsPlaying = false;  // sanity

        // try another refresh, maybe the osu!folder has changed
        if(this->beatmaps.size() == 0) {
            this->refreshBeatmaps();
        }

        // update user name/stats
        osu->onUserCardChange(BanchoState::get_username().c_str());

        // HACKHACK: workaround for BaseUI framework deficiency (missing mouse events. if a mouse button is being held,
        // and then suddenly a BaseUIElement gets put under it and set visible, and then the mouse button is released,
        // that "incorrectly" fires onMouseUpInside/onClicked/etc.)
        mouse->onButtonChange(ButtonIndex::BUTTON_LEFT, false);
        mouse->onButtonChange(ButtonIndex::BUTTON_RIGHT, false);

        if(this->beatmap != nullptr) {
            // For multiplayer: if the host exits song selection without selecting a song, we want to be able to revert
            // to that previous song.
            this->lastSelectedBeatmap = this->beatmap->getSelectedDifficulty2();

            // Select button matching current song preview
            this->selectSelectedBeatmapSongButton();
        }
    } else {
        this->contextMenu->setVisible2(false);
    }

    osu->chat->updateVisibility();
    return this;
}

void SongBrowser::selectSelectedBeatmapSongButton() {
    if(this->beatmap == nullptr) return;

    auto diff = this->beatmap->getSelectedDifficulty2();
    if(diff == nullptr) return;

    auto it = this->hashToSongButton.find(diff->getMD5Hash());
    if(it == this->hashToSongButton.end()) {
        debugLog("No song button found for currently selected beatmap...\n");
        return;
    }

    auto btn = it->second;
    for(auto sub_btn : btn->getChildren()) {
        // hashToSongButton points to the *beatmap* song button.
        // We want to select the *difficulty* song button.
        if(sub_btn->getDatabaseBeatmap() == diff) {
            btn = sub_btn;
            break;
        }
    }

    if(btn->getDatabaseBeatmap() != diff) {
        debugLog("Found matching beatmap, but not matching difficulty.\n");
        return;
    }

    btn->deselect();  // if we select() it when already selected, it would start playing!
    btn->select();
}

void SongBrowser::onPlayEnd(bool quit) {
    this->bHasSelectedAndIsPlaying = false;

    // update score displays
    if(!quit) {
        this->rebuildScoreButtons();

        auto *selectedSongDiffButton = dynamic_cast<SongDifficultyButton *>(this->selectedButton);
        if(selectedSongDiffButton != nullptr) selectedSongDiffButton->updateGrade();
    }

    // update song info
    if(this->beatmap != nullptr && this->beatmap->getSelectedDifficulty2() != nullptr) {
        this->songInfo->setFromBeatmap(this->beatmap, this->beatmap->getSelectedDifficulty2());
    }
}

void SongBrowser::onSelectionChange(CarouselButton *button, bool rebuild) {
    this->selectedButton = button;
    if(button == nullptr) return;

    this->contextMenu->setVisible2(false);

    // keep track and update all selection states
    // I'm still not happy with this, but at least all state update logic is localized in this function instead of
    // spread across all buttons

    auto *songButtonPointer = dynamic_cast<SongButton *>(button);
    auto *songDiffButtonPointer = dynamic_cast<SongDifficultyButton *>(button);
    auto *collectionButtonPointer = dynamic_cast<CollectionButton *>(button);

    if(songDiffButtonPointer != nullptr) {
        if(this->selectionPreviousSongDiffButton != nullptr &&
           this->selectionPreviousSongDiffButton != songDiffButtonPointer)
            this->selectionPreviousSongDiffButton->deselect();

        // support individual diffs independent from their parent song button container
        {
            // if the new diff has a parent song button, then update its selection state (select it to stay consistent)
            if(songDiffButtonPointer->getParentSongButton() != nullptr &&
               !songDiffButtonPointer->getParentSongButton()->isSelected()) {
                songDiffButtonPointer->getParentSongButton()
                    ->sortChildren();  // NOTE: workaround for disabled callback firing in select()
                songDiffButtonPointer->getParentSongButton()->select(false);
                this->onSelectionChange(songDiffButtonPointer->getParentSongButton(), false);  // NOTE: recursive call
            }

            // if the new diff does not have a parent song button, but the previous diff had, then update the previous
            // diff parent song button selection state (to deselect it)
            if(songDiffButtonPointer->getParentSongButton() == nullptr) {
                if(this->selectionPreviousSongDiffButton != nullptr &&
                   this->selectionPreviousSongDiffButton->getParentSongButton() != nullptr)
                    this->selectionPreviousSongDiffButton->getParentSongButton()->deselect();
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

void SongBrowser::onDifficultySelected(DatabaseBeatmap *diff2, bool play) {
    // deselect = unload
    this->beatmap->deselect();

    // select = play preview music
    this->beatmap->selectDifficulty2(diff2);

    // update song info
    this->songInfo->setFromBeatmap(this->beatmap, diff2);

    // start playing
    if(play) {
        if(BanchoState::is_in_a_multi_room()) {
            BanchoState::room.map_name =
                UString::format("%s - %s [%s]", diff2->getArtistLatin().c_str(), diff2->getTitleLatin().c_str(),
                                diff2->getDifficultyName().c_str());
            BanchoState::room.map_md5 = diff2->getMD5Hash();
            BanchoState::room.map_id = diff2->getID();

            Packet packet;
            packet.id = MATCH_CHANGE_SETTINGS;
            BanchoState::room.pack(&packet);
            BANCHO::Net::send_packet(packet);

            osu->room->on_map_change();

            this->setVisible(false);
        } else {
            // CTRL + click = auto
            if(keyboard->isControlDown()) {
                osu->bModAutoTemp = true;
                osu->getModSelector()->enableAuto();
            }

            if(this->beatmap->play()) {
                this->setVisible(false);
            }
        }
    }

    // animate
    this->fPulseAnimation = 1.0f;
    anim->moveLinear(&this->fPulseAnimation, 0.0f, 0.55f, true);

    // update score display
    this->rebuildScoreButtons();

    // update web button
    this->webButton->setVisible(this->songInfo->getBeatmapID() > 0);
}

void SongBrowser::refreshBeatmaps() {
    if(this->bHasSelectedAndIsPlaying) return;

    if(!this->bVisible) {
        // XXX: We're forcing visibility here to show the reload progress
        //      But ideally, we should be able to load in the background (assuming no data races...)
        //      That would let us load databases immediately on game start in the background
        osu->toggleSongBrowser();
    }

    // reset
    this->checkHandleKillBackgroundSearchMatcher();

    // don't pause the music the first time we load the song database
    static bool first_refresh = true;
    if(first_refresh) {
        this->beatmap->music = nullptr;
        first_refresh = false;
    }

    auto diff2 = this->beatmap->getSelectedDifficulty2();
    if(diff2) {
        this->beatmap_to_reselect_after_db_load = diff2->getMD5Hash();
    }

    this->beatmap->pausePreviewMusic();
    this->beatmap->deselect();
    SAFE_DELETE(this->beatmap);
    this->beatmap = new Beatmap();

    this->selectionPreviousSongButton = nullptr;
    this->selectionPreviousSongDiffButton = nullptr;
    this->selectionPreviousCollectionButton = nullptr;

    // delete local database and UI
    this->carousel->getContainer()->invalidate();

    for(auto &songButton : this->songButtons) {
        delete songButton;
    }
    this->songButtons.clear();
    this->hashToSongButton.clear();
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
    this->beatmaps.clear();
    this->previousRandomBeatmaps.clear();

    this->contextMenu->setVisible2(false);

    // clear potentially active search
    this->bInSearch = false;
    this->sSearchString = "";
    this->sPrevSearchString = "";
    this->fSearchWaitTime = 0.0f;
    this->searchPrevGroup = GROUP::GROUP_NO_GROUPING;

    // force no grouping
    if(this->group != GROUP::GROUP_NO_GROUPING) {
        this->onGroupChange("", 0);
    } else {
        this->groupByNothingBtn->setTextBrightColor(highlightColor);
    }

    // start loading
    this->bBeatmapRefreshScheduled = true;
    db->load();
}

void SongBrowser::addBeatmapSet(BeatmapSet *mapset) {
    if(mapset->getDifficulties().size() < 1) return;

    SongButton *songButton;
    if(mapset->getDifficulties().size() > 1) {
        songButton =
            new SongButton(this, this->contextMenu, 250, 250 + this->beatmaps.size() * 50, 200, 50, "", mapset);
    } else {
        songButton = new SongDifficultyButton(this, this->contextMenu, 250, 250 + this->beatmaps.size() * 50, 200, 50,
                                              "", mapset->getDifficulties()[0], nullptr);
    }

    this->songButtons.push_back(songButton);
    for(auto diff : mapset->getDifficulties()) {
        this->hashToSongButton[diff->getMD5Hash()] = songButton;
    }

    // prebuild temporary list of all relevant buttons, used by some groups
    std::vector<SongButton *> tempChildrenForGroups;
    {
        if(songButton->getChildren().size() > 0) {
            for(SongButton *child : songButton->getChildren()) {
                tempChildrenForGroups.push_back(child);
            }
        } else {
            tempChildrenForGroups.push_back(songButton);
        }
    }

    // add mapset to all necessary groups
    {
        this->addSongButtonToAlphanumericGroup(songButton, this->artistCollectionButtons, mapset->getArtistLatin());
        this->addSongButtonToAlphanumericGroup(songButton, this->creatorCollectionButtons, mapset->getCreator());
        this->addSongButtonToAlphanumericGroup(songButton, this->titleCollectionButtons, mapset->getTitleLatin());

        // difficulty
        if(this->difficultyCollectionButtons.size() == 12) {
            for(SongButton *diff_btn : tempChildrenForGroups) {
                const auto &stars_tmp = diff_btn->getDatabaseBeatmap()->getStarsNomod();
                const int index = std::clamp<int>(
                    (std::isfinite(stars_tmp) && stars_tmp >= static_cast<float>(std::numeric_limits<int>::min()) &&
                     stars_tmp <= static_cast<float>(std::numeric_limits<int>::max()))
                        ? static_cast<int>(stars_tmp)
                        : 0,
                    0, 11);
                auto &children = this->difficultyCollectionButtons[index]->getChildren();
                children.push_back(diff_btn);
            }
        }

        // bpm
        if(this->bpmCollectionButtons.size() == 6) {
            for(SongButton *diff_btn : tempChildrenForGroups) {
                auto bpm = diff_btn->getDatabaseBeatmap()->getMostCommonBPM();
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
                auto &children = this->bpmCollectionButtons[index]->getChildren();
                children.push_back(diff_btn);
            }
        }

        // dateadded
        {
            // TODO: extremely annoying
        }

        // length
        if(this->lengthCollectionButtons.size() == 7) {
            for(auto diff_btn : tempChildrenForGroups) {
                const u32 lengthMS = diff_btn->getDatabaseBeatmap()->getLengthMS();

                std::vector<SongButton *> *children = nullptr;
                if(lengthMS <= 1000 * 60) {
                    children = &this->lengthCollectionButtons[0]->getChildren();
                } else if(lengthMS <= 1000 * 60 * 2) {
                    children = &this->lengthCollectionButtons[1]->getChildren();
                } else if(lengthMS <= 1000 * 60 * 3) {
                    children = &this->lengthCollectionButtons[2]->getChildren();
                } else if(lengthMS <= 1000 * 60 * 4) {
                    children = &this->lengthCollectionButtons[3]->getChildren();
                } else if(lengthMS <= 1000 * 60 * 5) {
                    children = &this->lengthCollectionButtons[4]->getChildren();
                } else if(lengthMS <= 1000 * 60 * 10) {
                    children = &this->lengthCollectionButtons[5]->getChildren();
                } else {
                    children = &this->lengthCollectionButtons[6]->getChildren();
                }

                children->push_back(diff_btn);
            }
        }
    }
}

void SongBrowser::addSongButtonToAlphanumericGroup(SongButton *btn, std::vector<CollectionButton *> &group,
                                                   const std::string &name) {
    if(group.size() != 28) {
        debugLog("Alphanumeric group wasn't initialized!\n");
        return;
    }

    const char firstChar = name.length() == 0 ? '#' : name[0];
    const bool isNumber = (firstChar >= '0' && firstChar <= '9');
    const bool isLowerCase = (firstChar >= 'a' && firstChar <= 'z');
    const bool isUpperCase = (firstChar >= 'A' && firstChar <= 'Z');

    std::vector<SongButton *> *children = nullptr;
    if(isNumber) {
        children = &group[0]->getChildren();
    } else if(isLowerCase || isUpperCase) {
        const int index = 1 + (25 - (isLowerCase ? 'z' - firstChar : 'Z' - firstChar));
        children = &group[index]->getChildren();
    } else {
        children = &group[27]->getChildren();
    }

    if(cv::debug_osu.getBool()) {
        debugLog("Inserting {:s}\n", name.c_str());
    }

    children->push_back(btn);
}

void SongBrowser::requestNextScrollToSongButtonJumpFix(SongDifficultyButton *diffButton) {
    if(diffButton == nullptr) return;

    this->bNextScrollToSongButtonJumpFixScheduled = true;
    this->fNextScrollToSongButtonJumpFixOldRelPosY =
        (diffButton->getParentSongButton() != nullptr ? diffButton->getParentSongButton()->getRelPos().y
                                                      : diffButton->getRelPos().y);
    this->fNextScrollToSongButtonJumpFixOldScrollSizeY = this->carousel->getScrollSize().y;
}

bool SongBrowser::isButtonVisible(CarouselButton *songButton) {
    for(const auto &btn : this->visibleSongButtons) {
        if(btn == songButton) {
            return true;
        }
        for(const auto &child : btn->getChildren()) {
            if(child == songButton) {
                return true;
            }

            for(const auto &grandchild : child->getChildren()) {
                if(grandchild == songButton) {
                    return true;
                }
            }
        }
    }

    return false;
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
    this->carousel->getContainer()->invalidate();

    // NOTE: currently supports 3 depth layers (collection > beatmap > diffs)
    for(auto &visibleSongButton : this->visibleSongButtons) {
        CarouselButton *button = visibleSongButton;
        button->resetAnimations();

        if(!(button->isSelected() && button->isHiddenIfSelected()))
            this->carousel->getContainer()->addBaseUIElement(button);

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

                if(!(button2->isSelected() && button2->isHiddenIfSelected()))
                    this->carousel->getContainer()->addBaseUIElement(button2);

                // child children
                if(button2->isSelected()) {
                    const auto &children2 = button2->getChildren();
                    for(auto button3 : children2) {
                        if(this->bInSearch && !button3->isSearchMatch()) continue;

                        button3->resetAnimations();

                        if(!(button3->isSelected() && button3->isHiddenIfSelected()))
                            this->carousel->getContainer()->addBaseUIElement(button3);
                    }
                }
            }
        }
    }

    // TODO: regroup diffs which are next to each other into one song button (parent button)
    // TODO: regrouping is non-deterministic, depending on the searching method used.
    //       meaning that any number of "clusters" of diffs belonging to the same beatmap could build, requiring
    //       multiple song "parent" buttons for the same beatmap (if touching group size >= 2)
    //       when regrouping, these "fake" parent buttons have to be deleted on every reload. this means that the
    //       selection state logic has to be kept cleared of any invalid pointers!
    //       (including everything else which would rely on having a permanent pointer to an SongButton)

    this->updateSongButtonLayout();
}

void SongBrowser::updateSongButtonLayout() {
    // this rebuilds the entire songButton layout (songButtons in relation to others)
    // only the y axis is set, because the x axis is constantly animated and handled within the button classes
    // themselves
    const std::vector<CBaseUIElement *> &elements = this->carousel->getContainer()->getElements();

    int yCounter = this->carousel->getSize().y / 4;
    if(elements.size() <= 1) yCounter = this->carousel->getSize().y / 2;

    bool isSelected = false;
    bool inOpenCollection = false;
    for(auto &element : elements) {
        auto *songButton = dynamic_cast<CarouselButton *>(element);

        if(songButton != nullptr) {
            const auto *diffButtonPointer = dynamic_cast<const SongDifficultyButton *>(songButton);

            // depending on the object type, layout differently
            const bool isCollectionButton = dynamic_cast<CollectionButton *>(songButton) != nullptr;
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

bool SongBrowser::searchMatcher(const DatabaseBeatmap *databaseBeatmap,
                                const std::vector<std::string> &searchStringTokens) {
    if(databaseBeatmap == nullptr) return false;

    const std::vector<const DatabaseBeatmap *> tmpContainer{databaseBeatmap};
    const auto &diffs = databaseBeatmap->getDifficulties().size() > 0
                            ? databaseBeatmap->getDifficulties<const DatabaseBeatmap>()
                            : tmpContainer;

    auto speed = osu->getSelectedBeatmap()->getSpeedMultiplier();

    // TODO: optimize this dumpster fire. can at least cache the parsed tokens and literal strings array instead of
    // parsing every single damn time

    // intelligent search parser
    // all strings which are not expressions get appended with spaces between, then checked with one call to
    // findSubstringInDiff() the rest is interpreted NOTE: this code is quite shitty. the order of the operators
    // array does matter, because find() is used to detect their presence (and '=' would then break '<=' etc.)
    enum operatorId : uint8_t { EQ, LT, GT, LE, GE, NE };
    static constexpr auto operators = std::array{
        std::pair<std::string_view, operatorId>("<=", LE), std::pair<std::string_view, operatorId>(">=", GE),
        std::pair<std::string_view, operatorId>("<", LT),  std::pair<std::string_view, operatorId>(">", GT),
        std::pair<std::string_view, operatorId>("!=", NE), std::pair<std::string_view, operatorId>("==", EQ),
        std::pair<std::string_view, operatorId>("=", EQ),
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
    static constexpr auto keywords = std::array{std::pair<std::string_view, keywordId>("ar", AR),
                                                std::pair<std::string_view, keywordId>("cs", CS),
                                                std::pair<std::string_view, keywordId>("od", OD),
                                                std::pair<std::string_view, keywordId>("hp", HP),
                                                std::pair<std::string_view, keywordId>("bpm", BPM),
                                                std::pair<std::string_view, keywordId>("opm", OPM),
                                                std::pair<std::string_view, keywordId>("cpm", CPM),
                                                std::pair<std::string_view, keywordId>("spm", SPM),
                                                std::pair<std::string_view, keywordId>("object", OBJECTS),
                                                std::pair<std::string_view, keywordId>("objects", OBJECTS),
                                                std::pair<std::string_view, keywordId>("circle", CIRCLES),
                                                std::pair<std::string_view, keywordId>("circles", CIRCLES),
                                                std::pair<std::string_view, keywordId>("slider", SLIDERS),
                                                std::pair<std::string_view, keywordId>("sliders", SLIDERS),
                                                std::pair<std::string_view, keywordId>("spinner", SPINNERS),
                                                std::pair<std::string_view, keywordId>("spinners", SPINNERS),
                                                std::pair<std::string_view, keywordId>("length", LENGTH),
                                                std::pair<std::string_view, keywordId>("len", LENGTH),
                                                std::pair<std::string_view, keywordId>("stars", STARS),
                                                std::pair<std::string_view, keywordId>("star", STARS),
                                                std::pair<std::string_view, keywordId>("creator", CREATOR)};

    // split search string into tokens
    // parse over all difficulties
    bool expressionMatches = false;  // if any diff matched all expressions
    std::vector<std::string> literalSearchStrings;
    for(const auto *diff : diffs) {
        bool expressionsMatch = true;  // if the current search string (meaning only the expressions in this case)
                                       // matches the current difficulty

        for(const auto &searchStringToken : searchStringTokens) {
            // debugLog("token[{:d}] = {:s}\n", i, tokens[i].toUtf8());
            //  determine token type, interpret expression
            bool expression = false;
            for(const auto &o : operators) {
                if(searchStringToken.find(o.first) != std::string::npos) {
                    // split expression into left and right parts (only accept singular expressions, things like
                    // "0<bpm<1" will not work with this)
                    // debugLog("splitting by string {:s}\n", operators[o].first.toUtf8());
                    std::vector<std::string> values{SString::split(searchStringToken, o.first)};
                    if(values.size() == 2 && values[0].length() > 0 && values[1].length() > 0) {
                        const std::string &lvalue = values[0];
                        const std::string &rstring = values[1];
                        const auto rvaluePercentIndex = rstring.find('%');
                        const bool rvalueIsPercent = (rvaluePercentIndex != std::string::npos);
                        const float rvalue =
                            (rvaluePercentIndex == std::string::npos
                                 ? std::strtof(rstring.c_str(), nullptr)
                                 : std::strtof(rstring.substr(0, rvaluePercentIndex).c_str(),
                                               nullptr));  // this must always be a number (at least, assume it is)

                        // find lvalue keyword in array (only continue if keyword exists)
                        for(const auto &keyword : keywords) {
                            if(keyword.first == lvalue) {
                                expression = true;

                                // we now have a valid expression: the keyword, the operator and the value

                                // solve keyword
                                float compareValue = 5.0f;
                                std::string compareString{};
                                switch(keyword.second) {
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
                                        compareString = SString::lower(diff->getCreator());
                                        break;
                                }

                                // solve operator
                                bool matches = false;
                                switch(o.second) {
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

                                // debugLog("comparing {:f} {:s} {:f} (operatorId = {:d}) = {:d}\n", compareValue,
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
                    SString::trim(&litAdd);
                    if(!SString::whitespace_only(litAdd)) literalSearchStrings.push_back(litAdd);
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
                                                       const std::string &searchString) -> bool {
            if(!diff->getTitleLatin().empty() && SString::contains_ncase(diff->getTitleLatin(), searchString))
                return true;
            if(!diff->getArtistLatin().empty() && SString::contains_ncase(diff->getArtistLatin(), searchString))
                return true;
            if(!diff->getCreator().empty() && SString::contains_ncase(diff->getCreator(), searchString)) return true;
            if(!diff->getDifficultyName().empty() && SString::contains_ncase(diff->getDifficultyName(), searchString))
                return true;
            if(!diff->getSource().empty() && SString::contains_ncase(diff->getSource(), searchString)) return true;
            if(!diff->getTags().empty() && SString::contains_ncase(diff->getTags(), searchString)) return true;

            if(diff->getID() > 0 && SString::contains_ncase(std::to_string(diff->getID()), searchString)) return true;
            if(diff->getSetID() > 0 && SString::contains_ncase(std::to_string(diff->getSetID()), searchString))
                return true;

            return false;
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

    auto screen = osu->getScreenSize();
    bool is_widescreen = ((i32)(std::max(0, (i32)((screen.x - (screen.y * 4.f / 3.f)) / 2.f))) > 0);
    f32 global_scale = screen.x / (is_widescreen ? 1366.f : 1024.f);

    /* TODO: use?
     * const float uiScale = cv::ui_scale.getFloat();
     */

    const float dpiScale = Osu::getUIScale();

    const int margin = 5 * dpiScale;

    // topbar left
    // TODO @kiwec: move score sorting method to settings menu. instead make the dropdown
    //              select between Local/Country/Friends/Global
    // TODO @kiwec: have checkbox in settings to toggle displaying non-vanilla scores
    // TODO @kiwec: have checkbox in settings to toggle only showing selected mods (?)

    this->topbarLeft->setSize(global_scale * 390.f, global_scale * 145.f);
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
    this->topbarRight->setPosX(osu->getScreenWidth() / 2);
    this->topbarRight->setSize(osu->getScreenWidth() - this->topbarRight->getPos().x, 80.f * global_scale);

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
        osu->getScreenWidth() - (this->topbarLeft->getPos().x + this->topbarLeft->getSize().x),
        (osu->getScreenHeight() - this->carousel->getPos().y - (bottombar_get_hardcoded_height() * 0.75f)));

    this->updateSongButtonLayout();

    this->search->setPos(osu->getScreenWidth() / 2, this->topbarRight->getSize().y + 8 * dpiScale);
    this->search->setSize(osu->getScreenWidth() / 2, 20 * dpiScale);
}

void SongBrowser::onBack() { osu->toggleSongBrowser(); }

void SongBrowser::updateScoreBrowserLayout() {
    const float dpiScale = Osu::getUIScale();

    auto screen = osu->getScreenSize();
    bool is_widescreen = ((i32)(std::max(0, (i32)((screen.x - (screen.y * 4.f / 3.f)) / 2.f))) > 0);
    f32 global_scale = screen.x / (is_widescreen ? 1366.f : 1024.f);

    const bool shouldScoreBrowserBeVisible =
        (cv::scores_enabled.getBool() && cv::songbrowser_scorebrowser_enabled.getBool());
    if(shouldScoreBrowserBeVisible != this->scoreBrowser->isVisible())
        this->scoreBrowser->setVisible(shouldScoreBrowserBeVisible);

    const int scoreButtonWidthMax = this->topbarLeft->getSize().x;

    /* TODO: use?
     * f32 back_btn_height = osu->getSkin()->getMenuBack2()->getSize().y;
     */
    f32 browserHeight = osu->getScreenHeight() -
                        (get_bottombar_height() + (this->topbarLeft->getPos().y + this->topbarLeft->getSize().y)) +
                        2 * dpiScale;
    this->scoreBrowser->setPos(this->topbarLeft->getPos().x + 2 * dpiScale,
                               this->topbarLeft->getPos().y + this->topbarLeft->getSize().y + 4 * dpiScale);
    this->scoreBrowser->setSize(scoreButtonWidthMax, browserHeight);
    const i32 scoreHeight = 53.f * global_scale;

    // In stable, even when looking at local scores, there is space where the "local best" would be.
    f32 local_best_size = scoreHeight + 61 * global_scale;
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

    const std::vector<CBaseUIElement *> &elements = this->scoreBrowser->getContainer()->getElements();
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
    this->scoreBrowser->getContainer()->update_pos();
    this->scoreBrowser->setScrollSizeToContent();
}

void SongBrowser::rebuildScoreButtons() {
    if(!this->isVisible()) return;

    // XXX: When online, it would be nice to scroll to the current user's highscore

    // reset
    this->scoreBrowser->getContainer()->invalidate();
    this->localBestContainer->invalidate();
    this->localBestContainer->setVisible(false);

    const bool validBeatmap = (this->beatmap != nullptr && this->beatmap->getSelectedDifficulty2() != nullptr);
    bool is_online = cv::songbrowser_scores_filteringtype.getString() != "Local";

    std::vector<FinishedScore> scores;
    if(validBeatmap) {
        std::scoped_lock lock(db->scores_mtx);
        auto diff2 = this->beatmap->getSelectedDifficulty2();
        auto local_scores = db->scores[diff2->getMD5Hash()];
        auto local_best = std::ranges::max_element(
            local_scores, [](FinishedScore const &a, FinishedScore const &b) { return a.score < b.score; });

        if(is_online) {
            auto search = db->online_scores.find(diff2->getMD5Hash());
            if(search != db->online_scores.end()) {
                scores = search->second;

                if(local_best == local_scores.end()) {
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
                    this->localBestButton->map_hash = diff2->getMD5Hash();
                    this->localBestButton->setScore(*local_best, diff2);
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
                BANCHO::Leaderboard::fetch_online_scores(diff2);

                // Display local best while scores are loading
                if(local_best != local_scores.end()) {
                    SAFE_DELETE(this->localBestButton);
                    this->localBestButton = new ScoreButton(this->contextMenu, 0, 0, 0, 0);
                    this->localBestButton->setClickCallback(SA::MakeDelegate<&SongBrowser::onScoreClicked>(this));
                    this->localBestButton->map_hash = diff2->getMD5Hash();
                    this->localBestButton->setScore(*local_best, diff2);
                    this->localBestButton->resetHighlight();
                    this->localBestButton->grabs_clicks = true;
                    this->localBestContainer->addBaseUIElement(this->localBestLabel);
                    this->localBestContainer->addBaseUIElement(this->localBestButton);
                    this->localBestContainer->setVisible(true);
                }
            }
        } else {
            scores = local_scores;
        }
    }

    const int numScores = scores.size();

    // top up cache as necessary
    if(numScores > this->scoreButtonCache.size()) {
        const int numNewButtons = numScores - this->scoreButtonCache.size();
        for(size_t i = 0; i < numNewButtons; i++) {
            auto *scoreButton = new ScoreButton(this->contextMenu, 0, 0, 0, 0);
            scoreButton->setClickCallback(SA::MakeDelegate<&SongBrowser::onScoreClicked>(this));
            this->scoreButtonCache.push_back(scoreButton);
        }
    }

    // and build the ui
    if(numScores < 1) {
        if(validBeatmap && is_online) {
            this->scoreBrowser->getContainer()->addBaseUIElement(
                this->scoreBrowserScoresStillLoadingElement, this->scoreBrowserScoresStillLoadingElement->getRelPos().x,
                this->scoreBrowserScoresStillLoadingElement->getRelPos().y);
        } else {
            this->scoreBrowser->getContainer()->addBaseUIElement(
                this->scoreBrowserNoRecordsYetElement, this->scoreBrowserScoresStillLoadingElement->getRelPos().x,
                this->scoreBrowserScoresStillLoadingElement->getRelPos().y);
        }
    } else {
        // sort
        db->sortScoresInPlace(scores);

        // build
        std::vector<ScoreButton *> scoreButtons;
        for(size_t i = 0; i < numScores; i++) {
            ScoreButton *button = this->scoreButtonCache[i];
            button->map_hash = this->beatmap->getSelectedDifficulty2()->getMD5Hash();
            button->setScore(scores[i], this->beatmap->getSelectedDifficulty2(), i + 1);
            scoreButtons.push_back(button);
        }

        // add
        for(size_t i = 0; i < numScores; i++) {
            scoreButtons[i]->setIndex(i + 1);
            this->scoreBrowser->getContainer()->addBaseUIElement(scoreButtons[i]);
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
            if(visibleSongButton->getDatabaseBeatmap() == this->beatmap->getSelectedDifficulty2()) {
                auto *songButtonPointer = dynamic_cast<SongButton *>(visibleSongButton);
                if(songButtonPointer != nullptr) {
                    for(CarouselButton *diffButton : songButtonPointer->getChildren()) {
                        auto *diffButtonPointer = dynamic_cast<SongButton *>(diffButton);
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
                debugLog("WARNING: Ignoring stuck SearchMatcher thread!\n");
                break;
            }
        }
    }
}

void SongBrowser::onDatabaseLoadingFinished() {
    Timer t;
    t.start();

    // having a copy of the vector in here is actually completely unnecessary
    this->beatmaps = db->getDatabaseBeatmaps();

    // If any .osz files are in "maps", extract and add them to the DB.
    // Very helpful for users that don't have an osu!stable folder (and don't want to have one).
    for(const auto &entry : fs::directory_iterator{MCENGINE_DATA_DIR "maps"}) {
        if(!entry.is_regular_file() || entry.path().extension() != ".osz") continue;
        debugLog("Auto-importing .osz file found in maps dir: {}\n", entry.path().c_str());
        env->getEnvInterop().handle_osz(entry.path().c_str());
        fs::remove(entry);  // delete when done
    }

    debugLog("Loading {} beatmapsets from database.\n", this->beatmaps.size());

    // initialize all collection (grouped) buttons
    {
        // artist
        {
            // 0-9
            {
                auto *b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", "0-9",
                                               std::vector<SongButton *>());
                this->artistCollectionButtons.push_back(b);
            }

            // A-Z
            for(size_t i = 0; i < 26; i++) {
                UString artistCollectionName = UString::format("%c", 'A' + i);

                auto *b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", artistCollectionName,
                                               std::vector<SongButton *>());
                this->artistCollectionButtons.push_back(b);
            }

            // Other
            {
                auto *b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", "Other",
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

            auto *b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", difficultyCollectionName,
                                           children);
            this->difficultyCollectionButtons.push_back(b);
        }

        // bpm
        {
            auto *b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", "Under 60 BPM",
                                           std::vector<SongButton *>());
            this->bpmCollectionButtons.push_back(b);
            b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", "Under 120 BPM",
                                     std::vector<SongButton *>());
            this->bpmCollectionButtons.push_back(b);
            b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", "Under 180 BPM",
                                     std::vector<SongButton *>());
            this->bpmCollectionButtons.push_back(b);
            b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", "Under 240 BPM",
                                     std::vector<SongButton *>());
            this->bpmCollectionButtons.push_back(b);
            b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", "Under 300 BPM",
                                     std::vector<SongButton *>());
            this->bpmCollectionButtons.push_back(b);
            b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", "Over 300 BPM",
                                     std::vector<SongButton *>());
            this->bpmCollectionButtons.push_back(b);
        }

        // creator
        {
            // 0-9
            {
                auto *b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", "0-9",
                                               std::vector<SongButton *>());
                this->creatorCollectionButtons.push_back(b);
            }

            // A-Z
            for(size_t i = 0; i < 26; i++) {
                UString artistCollectionName = UString::format("%c", 'A' + i);

                auto *b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", artistCollectionName,
                                               std::vector<SongButton *>());
                this->creatorCollectionButtons.push_back(b);
            }

            // Other
            {
                auto *b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", "Other",
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
            auto *b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", "1 minute or less",
                                           std::vector<SongButton *>());
            this->lengthCollectionButtons.push_back(b);
            b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", "2 minutes or less",
                                     std::vector<SongButton *>());
            this->lengthCollectionButtons.push_back(b);
            b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", "3 minutes or less",
                                     std::vector<SongButton *>());
            this->lengthCollectionButtons.push_back(b);
            b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", "4 minutes or less",
                                     std::vector<SongButton *>());
            this->lengthCollectionButtons.push_back(b);
            b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", "5 minutes or less",
                                     std::vector<SongButton *>());
            this->lengthCollectionButtons.push_back(b);
            b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", "10 minutes or less",
                                     std::vector<SongButton *>());
            this->lengthCollectionButtons.push_back(b);
            b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", "Over 10 minutes",
                                     std::vector<SongButton *>());
            this->lengthCollectionButtons.push_back(b);
        }

        // title
        {
            // 0-9
            {
                auto *b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", "0-9",
                                               std::vector<SongButton *>());
                this->titleCollectionButtons.push_back(b);
            }

            // A-Z
            for(size_t i = 0; i < 26; i++) {
                UString artistCollectionName = UString::format("%c", 'A' + i);

                auto *b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", artistCollectionName,
                                               std::vector<SongButton *>());
                this->titleCollectionButtons.push_back(b);
            }

            // Other
            {
                auto *b = new CollectionButton(this, this->contextMenu, 250, 250, 200, 50, "", "Other",
                                               std::vector<SongButton *>());
                this->titleCollectionButtons.push_back(b);
            }
        }
    }

    // add all beatmaps (build buttons)
    for(const auto &beatmap : this->beatmaps) {
        this->addBeatmapSet(beatmap);
    }

    // build collections
    this->recreateCollectionsButtons();

    this->onSortChange(cv::songbrowser_sortingtype.getString().c_str());
    this->onSortScoresChange(cv::songbrowser_scores_sortingtype.getString().c_str());

    // update rich presence (discord total pp)
    RichPresence::onSongBrowser();

    // update user name/stats
    osu->onUserCardChange(BanchoState::get_username().c_str());

    if(cv::songbrowser_search_hardcoded_filter.getString().length() > 0) this->onSearchUpdate();

    if(this->beatmap_to_reselect_after_db_load.hash[0] != 0) {
        auto beatmap = db->getBeatmapDifficulty(this->beatmap_to_reselect_after_db_load);
        if(beatmap) {
            this->onDifficultySelected(beatmap, false);
            this->selectSelectedBeatmapSongButton();
        }

        SAFE_DELETE(osu->mainMenu->preloaded_beatmapset);
    }

    // ok, if we still haven't selected a song, do so now
    if(this->beatmap->getSelectedDifficulty2() == nullptr) {
        this->selectRandomBeatmap();
    }

    t.update();
    debugLog("Took {} seconds.\n", t.getElapsedTime());
}

void SongBrowser::onSearchUpdate() {
    const bool hasHardcodedSearchStringChanged =
        (this->sPrevHardcodedSearchString != cv::songbrowser_search_hardcoded_filter.getString().c_str());
    const bool hasSearchStringChanged = (this->sPrevSearchString != this->sSearchString);

    const bool prevInSearch = this->bInSearch;
    this->bInSearch =
        (this->sSearchString.length() > 0 || cv::songbrowser_search_hardcoded_filter.getString().length() > 0);
    const bool hasInSearchChanged = (prevInSearch != this->bInSearch);

    if(this->bInSearch) {
        this->searchPrevGroup = this->group;

        // flag all search matches across entire database
        if(hasSearchStringChanged || hasHardcodedSearchStringChanged || hasInSearchChanged) {
            // stop potentially running async search
            this->checkHandleKillBackgroundSearchMatcher();

            this->backgroundSearchMatcher->revive();
            this->backgroundSearchMatcher->release();
            this->backgroundSearchMatcher->setSongButtonsAndSearchString(
                this->songButtons, this->sSearchString, cv::songbrowser_search_hardcoded_filter.getString().c_str());

            resourceManager->requestNextLoadAsync();
            resourceManager->loadResource(this->backgroundSearchMatcher);
        } else
            this->rebuildSongButtonsAndVisibleSongButtonsWithSearchMatchSupport(true);

        // (results are handled in update() once available)
    } else  // exit search
    {
        // exiting the search does not need any async work, so we can just directly do it in here

        // stop potentially running async search
        this->checkHandleKillBackgroundSearchMatcher();

        // reset container and visible buttons list
        this->carousel->getContainer()->invalidate();
        this->visibleSongButtons.clear();

        // reset all search flags
        for(auto &songButton : this->songButtons) {
            const auto &children = songButton->getChildren();
            if(children.size() > 0) {
                for(auto &c : children) {
                    c->setIsSearchMatch(true);
                }
            } else
                songButton->setIsSearchMatch(true);
        }

        // remember which tab was selected, instead of defaulting back to no grouping
        // this also rebuilds the visible buttons list
        for(auto &grouping : this->groupings) {
            if(grouping.type == this->searchPrevGroup) this->onGroupChange("", grouping.id);
        }
    }

    this->sPrevSearchString = this->sSearchString;
    this->sPrevHardcodedSearchString = cv::songbrowser_search_hardcoded_filter.getString().c_str();
}

void SongBrowser::rebuildSongButtonsAndVisibleSongButtonsWithSearchMatchSupport(bool scrollToTop,
                                                                                bool doRebuildSongButtons) {
    // reset container and visible buttons list
    this->carousel->getContainer()->invalidate();
    this->visibleSongButtons.clear();

    // use flagged search matches to rebuild visible song buttons
    {
        if(this->group == GROUP::GROUP_NO_GROUPING) {
            for(auto &songButton : this->songButtons) {
                const auto &children = songButton->getChildren();
                if(children.size() > 0) {
                    // if all children match, then we still want to display the parent wrapper button (without expanding
                    // all diffs)
                    bool allChildrenMatch = true;
                    for(const auto &c : children) {
                        if(!c->isSearchMatch()) allChildrenMatch = false;
                    }

                    if(allChildrenMatch)
                        this->visibleSongButtons.push_back(songButton);
                    else {
                        // rip matching children from parent
                        for(const auto &c : children) {
                            if(c->isSearchMatch()) this->visibleSongButtons.push_back(c);
                        }
                    }
                } else if(songButton->isSearchMatch())
                    this->visibleSongButtons.push_back(songButton);
            }
        } else {
            std::vector<CollectionButton *> *groupButtons = getCollectionButtonsForGroup(this->group);

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
                                    break;
                                }
                            }

                            if(isAnyMatchInGroup) break;
                        } else if(c->isSearchMatch()) {
                            isAnyMatchInGroup = true;
                            break;
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
}

void SongBrowser::onFilterScoresClicked(CBaseUIButton *button) {
    const std::vector<std::string> filters{"Local", "Global", "Selected mods", "Country", "Friends"};

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
        for(const auto &scoreSortingMethod : db->getScoreSortingMethods()) {
            CBaseUIButton *button = this->contextMenu->addButton(UString{scoreSortingMethod.name});
            if(scoreSortingMethod.name == cv::songbrowser_scores_sortingtype.getString())
                button->setTextBrightColor(0xff00ff00);
        }
    }
    this->contextMenu->end(false, false);
    this->contextMenu->setClickCallback(SA::MakeDelegate<&SongBrowser::onSortScoresChange>(this));
}

void SongBrowser::onFilterScoresChange(const UString &text, int /*id*/) {
    cv::songbrowser_scores_filteringtype.setValue(text);  // NOTE: remember
    this->filterScoresDropdown->setText(text);
    db->online_scores.clear();
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
        osu->notificationOverlay->addNotification("Opening browser, please wait ...", 0xffffffff, false, 0.75f);
    }
}

void SongBrowser::onQuickGroupClicked(CBaseUIButton *button) {
    for(const auto &group : this->groupings) {
        if(group.name == button->getText()) {
            this->onGroupChange(group.name, group.id);
            return;
        }
    }
}

void SongBrowser::onGroupClicked(CBaseUIButton *button) {
    this->contextMenu->setPos(button->getPos());
    this->contextMenu->setRelPos(button->getRelPos());
    this->contextMenu->begin(button->getSize().x);
    {
        for(auto &grouping : this->groupings) {
            CBaseUIButton *button = this->contextMenu->addButton(grouping.name, grouping.id);
            if(grouping.type == this->group) button->setTextBrightColor(0xff00ff00);
        }
    }
    this->contextMenu->end(false, false);
    this->contextMenu->setClickCallback(SA::MakeDelegate<&SongBrowser::onGroupChange>(this));
}

std::vector<CollectionButton *> *SongBrowser::getCollectionButtonsForGroup(GROUP group) {
    switch(group) {
        case GROUP::GROUP_NO_GROUPING:
            return nullptr;
        case GROUP::GROUP_ARTIST:
            return &this->artistCollectionButtons;
        case GROUP::GROUP_CREATOR:
            return &this->creatorCollectionButtons;
        case GROUP::GROUP_DIFFICULTY:
            return &this->difficultyCollectionButtons;
        case GROUP::GROUP_LENGTH:
            return &this->lengthCollectionButtons;
        case GROUP::GROUP_TITLE:
            return &this->titleCollectionButtons;
        case GROUP::GROUP_BPM:
            return &this->bpmCollectionButtons;
        // case GROUP::GROUP_DATEADDED:
        //     return &this->dateaddedCollectionButtons;
        case GROUP::GROUP_COLLECTIONS:
            return &this->collectionButtons;
        default:
            return nullptr;
    }
    return nullptr;
}

void SongBrowser::onGroupChange(const UString &text, int id) {
    GROUPING *grouping = &this->groupings[0];
    for(auto &groupingI : this->groupings) {
        if(groupingI.id == id || (text.length() > 1 && groupingI.name == text)) {
            grouping = &groupingI;
            break;
        }
    }
    if(grouping == nullptr) return;

    // update group combobox button text
    this->groupButton->setText(grouping->name);

    // set highlighted colour
    this->groupByCollectionBtn->setTextBrightColor(defaultColor);
    this->groupByArtistBtn->setTextBrightColor(defaultColor);
    this->groupByDifficultyBtn->setTextBrightColor(defaultColor);
    this->groupByNothingBtn->setTextBrightColor(defaultColor);

    switch(grouping->type) {
        case GROUP::GROUP_ARTIST:
            this->groupByArtistBtn->setTextBrightColor(highlightColor);
            break;
        case GROUP::GROUP_DIFFICULTY:
            this->groupByDifficultyBtn->setTextBrightColor(highlightColor);
            break;
        case GROUP::GROUP_COLLECTIONS:
            this->groupByCollectionBtn->setTextBrightColor(highlightColor);
            break;
        case GROUP::GROUP_NO_GROUPING:
        default:
            this->groupByNothingBtn->setTextBrightColor(highlightColor);
            break;
    }

    // and update the actual songbrowser contents
    rebuildAfterGroupOrSortChange(grouping->type);
}

void SongBrowser::onSortClicked(CBaseUIButton *button) {
    this->contextMenu->setPos(button->getPos());
    this->contextMenu->setRelPos(button->getRelPos());
    this->contextMenu->begin(button->getSize().x);
    {
        for(auto &sortingMethod : this->sortingMethods) {
            CBaseUIButton *button = this->contextMenu->addButton(sortingMethod.name);
            if(sortingMethod.type == this->sortingMethod) button->setTextBrightColor(0xff00ff00);
        }
    }
    this->contextMenu->end(false, false);
    this->contextMenu->setClickCallback(SA::MakeDelegate<&SongBrowser::onSortChange>(this));
}

void SongBrowser::onSortChange(const UString &text, int /*id*/) { this->onSortChangeInt(text); }

void SongBrowser::onSortChangeInt(const UString &text) {
    SORTING_METHOD *sortingMethod = nullptr;
    for(auto &sortingMethodI : this->sortingMethods) {
        if(sortingMethodI.name == text) {
            sortingMethod = &sortingMethodI;
            break;
        }
    }
    if(sortingMethod == nullptr) return;

    SORT previousSort = this->sortingMethod;

    this->sortingMethod = sortingMethod->type;
    this->sortButton->setText(sortingMethod->name);
    cv::songbrowser_sortingtype.setValue(sortingMethod->name);

    // always sort the master list (needed for all views)
    std::ranges::sort(this->songButtons, sortingMethod->comparator);

    // reuse the group update logic instead of duplicating it
    this->rebuildAfterGroupOrSortChange(this->group,
                                        previousSort == this->sortingMethod ? nullptr : sortingMethod->comparator);
}

void SongBrowser::rebuildAfterGroupOrSortChange(GROUP group, SORTING_COMPARATOR sortComp) {
    GROUP previousGroup = this->group;
    this->group = group;

    this->visibleSongButtons.clear();

    if(group == GROUP::GROUP_NO_GROUPING) {
        this->visibleSongButtons.reserve(this->songButtons.size());
        this->visibleSongButtons.insert(this->visibleSongButtons.end(), this->songButtons.begin(),
                                        this->songButtons.end());
    } else {
        auto *groupButtons = this->getCollectionButtonsForGroup(group);
        if(groupButtons != nullptr) {
            this->visibleSongButtons.reserve(groupButtons->size());
            this->visibleSongButtons.insert(this->visibleSongButtons.end(), groupButtons->begin(), groupButtons->end());

            // only sort if switching TO this group/sorting method (not from it)
            if(previousGroup != group || sortComp != nullptr) {
                // collections are always sorted alphabetically
                if(group == GROUP::GROUP_COLLECTIONS) {
                    std::ranges::sort(
                        *groupButtons, [](const char *b1, const char *b2) -> bool { return strcasecmp(b1, b2) < 0; },
                        [](const CollectionButton *btn) -> const char * { return btn->getCollectionName().c_str(); });
                }

                // sort children only if needed (defer until group is active)
                if(!sortComp) {
                    // need to get the current sorting method if we're called from onGroupChange...
                    SORTING_METHOD *currentSortMethod = nullptr;
                    for(auto &sortingMethodI : this->sortingMethods) {
                        if(sortingMethodI.type == this->sortingMethod) {
                            currentSortMethod = &sortingMethodI;
                            break;
                        }
                    }
                    if(currentSortMethod != nullptr) sortComp = currentSortMethod->comparator;
                }
                if(sortComp) {
                    for(auto &groupButton : *groupButtons) {
                        auto &children = groupButton->getChildren();
                        if(!children.empty()) {
                            std::ranges::sort(children, sortComp);
                            groupButton->setChildren(children);
                        }
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
        osu->notificationOverlay->addToast("Disabled FPoSu mode.", INFO_TOAST);
    } else {
        cv::mod_fposu.setValue(true);
        osu->notificationOverlay->addToast("Enabled FPoSu mode.", SUCCESS_TOAST);
    }
}

void SongBrowser::onSelectionMods() {
    soundEngine->play(osu->getSkin()->getExpandSound());
    osu->toggleModSelection(this->bF1Pressed);
}

void SongBrowser::onSelectionRandom() {
    soundEngine->play(osu->getSkin()->getClickButtonSound());
    if(this->bShiftPressed)
        this->bPreviousRandomBeatmapScheduled = true;
    else
        this->bRandomBeatmapScheduled = true;
}

void SongBrowser::onSelectionOptions() {
    soundEngine->play(osu->getSkin()->getClickButtonSound());

    if(this->selectedButton != nullptr) {
        this->scrollToSongButton(this->selectedButton);

        const vec2 heuristicSongButtonPositionAfterSmoothScrollFinishes =
            (this->carousel->getPos() + this->carousel->getSize() / 2.f);

        auto *songButtonPointer = dynamic_cast<SongButton *>(this->selectedButton);
        auto *collectionButtonPointer = dynamic_cast<CollectionButton *>(this->selectedButton);
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
    osu->getRankingScreen()->setScore(scoreButton->getScore());
    osu->getRankingScreen()->setBeatmapInfo(this->beatmap, this->beatmap->getSelectedDifficulty2());

    osu->getSongBrowser()->setVisible(false);
    osu->getRankingScreen()->setVisible(true);

    soundEngine->play(osu->getSkin()->getMenuHit());
}

void SongBrowser::onScoreContextMenu(ScoreButton *scoreButton, int id) {
    // NOTE: see ScoreButton::onContextMenu()

    if(id == 2) {
        db->deleteScore(scoreButton->map_hash, scoreButton->getScoreUnixTimestamp());

        this->rebuildScoreButtons();
        osu->userButton->updateUserStats();
    }
}

void SongBrowser::onSongButtonContextMenu(SongButton *songButton, const UString &text, int id) {
    // debugLog("SongBrowser::onSongButtonContextMenu({:p}, {:s}, {:d})\n", songButton, text.toUtf8(), id);

    struct CollectionManagementHelper {
        static std::vector<MD5Hash> getBeatmapSetHashesForSongButton(SongButton *songButton) {
            std::vector<MD5Hash> beatmapSetHashes;
            {
                const auto &songButtonChildren = songButton->getChildren();
                if(songButtonChildren.size() > 0) {
                    for(auto i : songButtonChildren) {
                        beatmapSetHashes.push_back(i->getDatabaseBeatmap()->getMD5Hash());
                    }
                } else {
                    const DatabaseBeatmap *beatmap =
                        db->getBeatmapDifficulty(songButton->getDatabaseBeatmap()->getMD5Hash());
                    if(beatmap != nullptr) {
                        const std::vector<DatabaseBeatmap *> &diffs = beatmap->getDifficulties();
                        for(auto diff : diffs) {
                            beatmapSetHashes.push_back(diff->getMD5Hash());
                        }
                    }
                }
            }
            return beatmapSetHashes;
        }
    };

    bool updateUIScheduled = false;
    {
        if(id == 1) {
            // add diff to collection
            std::string name = text.toUtf8();
            auto collection = get_or_create_collection(name);
            collection->add_map(songButton->getDatabaseBeatmap()->getMD5Hash());
            save_collections();
            updateUIScheduled = true;
        } else if(id == 2) {
            // add set to collection
            std::string name = text.toUtf8();
            auto collection = get_or_create_collection(name);
            const std::vector<MD5Hash> beatmapSetHashes =
                CollectionManagementHelper::getBeatmapSetHashesForSongButton(songButton);
            for(auto hash : beatmapSetHashes) {
                collection->add_map(hash);
            }
            save_collections();
            updateUIScheduled = true;
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

            auto collection = get_or_create_collection(collectionName);
            collection->remove_map(songButton->getDatabaseBeatmap()->getMD5Hash());
            save_collections();
            updateUIScheduled = true;
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

            auto collection = get_or_create_collection(collectionName);
            const std::vector<MD5Hash> beatmapSetHashes =
                CollectionManagementHelper::getBeatmapSetHashesForSongButton(songButton);
            for(auto hash : beatmapSetHashes) {
                collection->remove_map(hash);
            }
            save_collections();
            updateUIScheduled = true;
        } else if(id == -2 || id == -4) {
            // add beatmap(set) to new collection
            std::string name = text.toUtf8();
            auto collection = get_or_create_collection(name);

            if(id == -2) {
                // id == -2 means beatmap
                collection->add_map(songButton->getDatabaseBeatmap()->getMD5Hash());
                updateUIScheduled = true;
            } else if(id == -4) {
                // id == -4 means beatmapset
                const std::vector<MD5Hash> beatmapSetHashes =
                    CollectionManagementHelper::getBeatmapSetHashesForSongButton(songButton);
                for(const auto &beatmapSetHashe : beatmapSetHashes) {
                    collection->add_map(beatmapSetHashe);
                }
                updateUIScheduled = true;
            }

            save_collections();
        }
    }

    if(updateUIScheduled) {
        const float prevScrollPosY = this->carousel->getRelPosY();  // usability
        const auto previouslySelectedCollectionName =
            (this->selectionPreviousCollectionButton != nullptr
                 ? this->selectionPreviousCollectionButton->getCollectionName()
                 : "");  // usability
        {
            this->recreateCollectionsButtons();
            this->rebuildSongButtonsAndVisibleSongButtonsWithSearchMatchSupport(
                false, false);  // (last false = skipping rebuildSongButtons() here)
            this->onSortChangeInt(
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

void SongBrowser::onCollectionButtonContextMenu(CollectionButton * /*collectionButton*/, const UString &text, int id) {
    std::string collection_name = text.toUtf8();

    if(id == 2) {  // delete collection
        for(size_t i = 0; i < this->collectionButtons.size(); i++) {
            if(this->collectionButtons[i]->getCollectionName() == collection_name) {
                // delete UI
                delete this->collectionButtons[i];
                this->collectionButtons.erase(this->collectionButtons.begin() + i);

                // reset UI state
                this->selectionPreviousCollectionButton = nullptr;

                auto collection = get_or_create_collection(collection_name);
                collection->delete_collection();
                save_collections();

                // update UI
                this->rebuildAfterGroupOrSortChange(GROUP::GROUP_COLLECTIONS);

                break;
            }
        }
    } else if(id == 3) {  // collection has been renamed
        // update UI
        this->onSortChangeInt(cv::songbrowser_sortingtype.getString().c_str());
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
    const std::vector<CBaseUIElement *> &elements = this->carousel->getContainer()->getElements();
    std::vector<SongButton *> songButtons;
    for(auto element : elements) {
        auto *songButtonPointer = dynamic_cast<SongButton *>(element);
        auto *songDifficultyButtonPointer = dynamic_cast<SongDifficultyButton *>(element);

        if(songButtonPointer != nullptr &&
           (songDifficultyButtonPointer == nullptr ||
            songDifficultyButtonPointer->isIndependentDiffButton()))  // only allow songbuttons or independent diffs
            songButtons.push_back(songButtonPointer);
    }

    if(songButtons.size() < 1) return;

    // remember previous
    if(this->beatmap != nullptr && this->beatmap->getSelectedDifficulty2() != nullptr) {
        this->previousRandomBeatmaps.push_back(this->beatmap->getSelectedDifficulty2());
    }

    std::uniform_int_distribution<size_t> rng(0, songButtons.size() - 1);
    size_t randomIndex = rng(this->rngalg);

    auto *songButton = dynamic_cast<SongButton *>(songButtons[randomIndex]);
    this->selectSongButton(songButton);
}

void SongBrowser::selectPreviousRandomBeatmap() {
    if(this->previousRandomBeatmaps.size() > 0) {
        DatabaseBeatmap *currentRandomBeatmap = this->previousRandomBeatmaps.back();
        if(this->previousRandomBeatmaps.size() > 1 && this->beatmap != nullptr &&
           this->previousRandomBeatmaps[this->previousRandomBeatmaps.size() - 1] ==
               this->beatmap->getSelectedDifficulty2())
            this->previousRandomBeatmaps.pop_back();  // deletes the current beatmap which may also be at the top (so
                                                      // we don't switch to ourself)

        // filter songbuttons
        const std::vector<CBaseUIElement *> &elements = this->carousel->getContainer()->getElements();
        std::vector<SongButton *> songButtons;
        for(auto element : elements) {
            auto *songButtonPointer = dynamic_cast<SongButton *>(element);

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
    const std::vector<CBaseUIElement *> &elements = this->carousel->getContainer()->getElements();
    for(auto element : elements) {
        auto *songDifficultyButton = dynamic_cast<SongDifficultyButton *>(element);
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
        if(this->group == GROUP::GROUP_COLLECTIONS) {
            this->carousel->getContainer()->invalidate();
            this->visibleSongButtons.clear();
        }
    }

    Timer t;
    t.start();

    for(auto collection : collections) {
        if(collection->maps.empty()) continue;

        std::vector<SongButton *> folder;
        std::vector<u32> matched_sets;

        for(auto &map : collection->maps) {
            auto it = this->hashToSongButton.find(map);
            if(it == this->hashToSongButton.end()) continue;

            std::vector<SongButton *> matching_diffs;

            auto song_button = it->second;
            i32 set_id = song_button->getDatabaseBeatmap()->getSetID();

            const auto &songButtonChildren = song_button->getChildren();
            if(songButtonChildren.empty()) {
                // button is a difficulty, not a set
                matching_diffs.push_back(song_button);
            } else {
                for(SongButton *sbc : songButtonChildren) {
                    for(auto &map2 : collection->maps) {
                        if(sbc->getDatabaseBeatmap()->getMD5Hash() == map2) {
                            matching_diffs.push_back(sbc);
                        }
                    }
                }
            }

            auto set = std::ranges::find(matched_sets, set_id);
            if(set == matched_sets.end()) {
                // Mark set as processed so we don't add the diffs from the same set twice
                matched_sets.push_back(set_id);
            } else {
                // We already added the maps from this set to the collection!
                continue;
            }

            if(songButtonChildren.size() == matching_diffs.size()) {
                // all diffs match: add the set button (user added all diffs of beatmap into collection)
                folder.push_back(song_button);
            } else {
                // only add matched diff buttons
                folder.insert(folder.end(), matching_diffs.begin(), matching_diffs.end());
            }
        }

        if(!folder.empty()) {
            UString uname = collection->name.c_str();
            this->collectionButtons.push_back(new CollectionButton(
                this, this->contextMenu, 250, 250 + this->beatmaps.size() * 50, 200, 50, "", uname, folder));
        }
    }

    t.update();
    debugLog("recreateCollectionsButtons(): {:f} seconds\n", t.getElapsedTime());
}
