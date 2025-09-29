#pragma once
// Copyright (c) 2016, PG, All rights reserved.
#include "MD5Hash.h"
#include "ScreenBackable.h"

#include <random>

class BeatmapCarousel;
class Database;
class DatabaseBeatmap;
typedef DatabaseBeatmap BeatmapDifficulty;
typedef DatabaseBeatmap BeatmapSet;
class SkinImage;

class UIContextMenu;
class UISearchOverlay;
class InfoLabel;
class ScoreButton;
class CarouselButton;
class SongButton;
class SongDifficultyButton;
class CollectionButton;

class CBaseUIContainer;
class CBaseUIImageButton;
class CBaseUIScrollView;
class CBaseUIButton;
class CBaseUILabel;

class McFont;
class ConVar;

class SongBrowserBackgroundSearchMatcher;

typedef bool (*SORTING_COMPARATOR)(const SongButton *a, const SongButton *b);
bool sort_by_difficulty(const SongButton *a, const SongButton *b);

class SongBrowser final : public ScreenBackable {
   public:
    static void drawSelectedBeatmapBackgroundImage(float alpha = 1.0f);

    static f32 getUIScale();
    static i32 getUIScale(f32 m) { return (i32)(m * getUIScale()); }
    static f32 getUIScale2();
    static i32 getUIScale2(f32 m) { return (i32)(m * getUIScale2()); }
    static f32 getSkinScale(SkinImage *img);
    static f32 getSkinScale2(SkinImage *img);
    static vec2 getSkinDimensions(SkinImage *img);

    enum class GROUP : uint8_t {
        GROUP_NO_GROUPING,
        GROUP_ARTIST,
        GROUP_BPM,
        GROUP_CREATOR,
        // GROUP_DATEADDED, // unimpl
        GROUP_DIFFICULTY,
        GROUP_LENGTH,
        GROUP_TITLE,
        GROUP_COLLECTIONS,
        GROUP_MAX
    };

    friend class SongBrowserBackgroundSearchMatcher;
    friend class BeatmapCarousel;

    SongBrowser();
    ~SongBrowser() override;

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;

    void onKeyDown(KeyboardEvent &e) override;
    void onKeyUp(KeyboardEvent &e) override;
    void onChar(KeyboardEvent &e) override;

    void onResolutionChange(vec2 newResolution) override;

    CBaseUIContainer *setVisible(bool visible) override;

    bool selectBeatmapset(i32 set_id);
    void selectSelectedBeatmapSongButton();
    void onPlayEnd(bool quit = true);  // called when a beatmap is finished playing (or the player quit)

    void onSelectionChange(CarouselButton *button, bool rebuild);
    void onDifficultySelected(DatabaseBeatmap *map, bool play = false);

    void onScoreContextMenu(ScoreButton *scoreButton, int id);
    void onSongButtonContextMenu(SongButton *songButton, const UString &text, int id);
    void onCollectionButtonContextMenu(CollectionButton *collectionButton, const UString &text, int id);

    void highlightScore(u64 unixTimestamp);
    void selectRandomBeatmap();
    void playNextRandomBeatmap() {
        this->selectRandomBeatmap();
        this->playSelectedDifficulty();
    }

    void refreshBeatmaps(bool closeAfterLoading = false);
    void addBeatmapSet(BeatmapSet *beatmap);
    void addSongButtonToAlphanumericGroup(SongButton *btn, std::vector<CollectionButton *> &group,
                                          const std::string &name);

    void requestNextScrollToSongButtonJumpFix(SongDifficultyButton *diffButton);
    bool isButtonVisible(CarouselButton *songButton);
    void scrollToBestButton();
    void scrollToSongButton(CarouselButton *songButton, bool alignOnTop = false);
    void rebuildSongButtons();
    void recreateCollectionsButtons();
    void rebuildScoreButtons();
    void updateSongButtonLayout();

    [[nodiscard]] inline const std::vector<CollectionButton *> &getCollectionButtons() const {
        return this->collectionButtons;
    }

    [[nodiscard]] inline const std::unique_ptr<BeatmapCarousel> &getCarousel() const { return this->carousel; }

    [[nodiscard]] inline bool isInSearch() const { return this->bInSearch; }
    [[nodiscard]] inline bool isRightClickScrolling() const { return this->bSongBrowserRightClickScrolling; }

    inline InfoLabel *getInfoLabel() { return this->songInfo; }

    [[nodiscard]] inline GROUP getGroupingMode() const { return this->group; }

    enum class SORT : uint8_t {
        SORT_ARTIST,
        SORT_BPM,
        SORT_CREATOR,
        SORT_DATEADDED,
        SORT_DIFFICULTY,
        SORT_LENGTH,
        SORT_TITLE,
        SORT_RANKACHIEVED,
        SORT_MAX
    };

    struct SORTING_METHOD {
        SORT type;
        UString name;
        SORTING_COMPARATOR comparator;
    };

    struct GROUPING {
        GROUP type;
        UString name;
        int id;
    };

    static bool searchMatcher(const DatabaseBeatmap *databaseBeatmap,
                              const std::vector<std::string> &searchStringTokens);

    void updateLayout() override;
    void onBack() override;

    void updateScoreBrowserLayout();

    void scheduleSearchUpdate(bool immediately = false);
    void checkHandleKillBackgroundSearchMatcher();

    void onDatabaseLoadingFinished();

    void onSearchUpdate();
    void rebuildSongButtonsAndVisibleSongButtonsWithSearchMatchSupport(bool scrollToTop,
                                                                       bool doRebuildSongButtons = true);

    void onFilterScoresClicked(CBaseUIButton *button);
    static constexpr int LOGIN_STATE_FILTER_ID{100};
    void onFilterScoresChange(const UString &text, int id = -1);
    void onSortScoresClicked(CBaseUIButton *button);
    void onSortScoresChange(const UString &text, int id = -1);
    void onWebClicked(CBaseUIButton *button);

    void onQuickGroupClicked(CBaseUIButton *button);
    void onGroupClicked(CBaseUIButton *button);
    void onGroupChange(const UString &text, int id = -1);

    void onSortClicked(CBaseUIButton *button);
    void onSortChange(const UString &text, int id = -1);
    void onSortChangeInt(const UString &text);

    void rebuildAfterGroupOrSortChange(GROUP group, SORTING_COMPARATOR sortComp = nullptr);

    void onSelectionMode();
    void onSelectionMods();
    void onSelectionRandom();
    void onSelectionOptions();

    void onScoreClicked(CBaseUIButton *button);

    void selectSongButton(CarouselButton *songButton);
    void selectPreviousRandomBeatmap();
    void playSelectedDifficulty();

    std::mt19937_64 rngalg;

    GROUP group;
    std::array<GROUPING, static_cast<size_t>(GROUP::GROUP_MAX)> groupings;

    SORTING_COMPARATOR sortingComparator;
    SORT sortingMethod;
    std::array<SORTING_METHOD, static_cast<size_t>(SORT::SORT_MAX)> sortingMethods;

    // top bar left
    CBaseUIContainer *topbarLeft;
    InfoLabel *songInfo;
    CBaseUIButton *filterScoresDropdown;
    CBaseUIButton *sortScoresDropdown;
    CBaseUIButton *webButton;

    // top bar right
    CBaseUIContainer *topbarRight;
    CBaseUILabel *groupLabel;
    CBaseUIButton *groupButton;
    CBaseUILabel *sortLabel;
    CBaseUIButton *sortButton;
    UIContextMenu *contextMenu;

    CBaseUIButton *groupByCollectionBtn;
    CBaseUIButton *groupByArtistBtn;
    CBaseUIButton *groupByDifficultyBtn;
    CBaseUIButton *groupByNothingBtn;

    // score browser
    std::vector<ScoreButton *> scoreButtonCache;
    CBaseUIScrollView *scoreBrowser;
    CBaseUIElement *scoreBrowserScoresStillLoadingElement;
    CBaseUIElement *scoreBrowserNoRecordsYetElement;
    std::unique_ptr<CBaseUIContainer> localBestContainer{nullptr};
    CBaseUILabel *localBestLabel;
    ScoreButton *localBestButton = nullptr;
    bool score_resort_scheduled = false;

    // song carousel
    std::unique_ptr<BeatmapCarousel> carousel{nullptr};
    CarouselButton *selectedButton = nullptr;
    bool bSongBrowserRightClickScrollCheck;
    bool bSongBrowserRightClickScrolling;
    bool bNextScrollToSongButtonJumpFixScheduled;
    bool bNextScrollToSongButtonJumpFixUseScrollSizeDelta;
    bool scheduled_scroll_to_selected_button = false;
    float fNextScrollToSongButtonJumpFixOldRelPosY;
    float fNextScrollToSongButtonJumpFixOldScrollSizeY;
    f32 thumbnailYRatio = 0.f;

    // song browser selection state logic
    SongButton *selectionPreviousSongButton;
    SongDifficultyButton *selectionPreviousSongDiffButton;
    CollectionButton *selectionPreviousCollectionButton;

    // beatmap database
    std::vector<DatabaseBeatmap *> beatmapsets;
    std::vector<SongButton *> songButtons;
    std::vector<CarouselButton *> visibleSongButtons;
    std::vector<CollectionButton *> collectionButtons;
    std::vector<CollectionButton *> artistCollectionButtons;
    std::vector<CollectionButton *> difficultyCollectionButtons;
    std::vector<CollectionButton *> bpmCollectionButtons;
    std::vector<CollectionButton *> creatorCollectionButtons;
    std::vector<CollectionButton *> dateaddedCollectionButtons;
    std::vector<CollectionButton *> lengthCollectionButtons;
    std::vector<CollectionButton *> titleCollectionButtons;
    std::unordered_map<MD5Hash, SongButton *> hashToSongButton;
    bool bBeatmapRefreshScheduled;
    bool bCloseAfterBeatmapRefreshFinished;
    UString sLastOsuFolder;
    // i hate this
    struct {
        MD5Hash hash{};
        u64 time_when_stopped{0};
        u32 musicpos_when_stopped{0};
    } loading_reselect_map;

    // keys
    bool bF1Pressed;
    bool bF2Pressed;
    bool bF3Pressed;
    bool bShiftPressed;
    bool bLeft;
    bool bRight;
    bool bRandomBeatmapScheduled;
    bool bPreviousRandomBeatmapScheduled;

    // behaviour
    DatabaseBeatmap *lastSelectedBeatmap = nullptr;
    bool bHasSelectedAndIsPlaying;
    float fPulseAnimation;
    float fBackgroundFadeInTime;
    std::vector<DatabaseBeatmap *> previousRandomBeatmaps;

    // map auto-download
    i32 map_autodl = 0;
    i32 set_autodl = 0;

    // search
    UISearchOverlay *search;
    std::string sSearchString;
    std::string sPrevSearchString;
    std::string sPrevHardcodedSearchString;
    float fSearchWaitTime;
    bool bInSearch;
    bool bShouldRecountMatchesAfterSearch{true};
    i32 currentVisibleSearchMatches{0};
    GROUP searchPrevGroup;
    SongBrowserBackgroundSearchMatcher *backgroundSearchMatcher;

   private:
    std::vector<CollectionButton *> *getCollectionButtonsForGroup(GROUP group);
};
