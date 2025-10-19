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

namespace SortTypes {
enum type : i8 { ARTIST, BPM, CREATOR, DATEADDED, DIFFICULTY, LENGTH, TITLE, RANKACHIEVED, MAX };
};

namespace GroupTypes {
enum type : i8 {
    ARTIST,
    BPM,
    CREATOR,
    DATEADDED,  // unimpl
    DIFFICULTY,
    LENGTH,
    TITLE,
    COLLECTIONS,
    NO_GROUPING,
    MAX
};
};

class SongBrowser final : public ScreenBackable {
    NOCOPY_NOMOVE(SongBrowser)
   private:
    // not used anywhere else
    static bool sort_by_artist(SongButton const *a, SongButton const *b);
    static bool sort_by_bpm(SongButton const *a, SongButton const *b);
    static bool sort_by_creator(SongButton const *a, SongButton const *b);
    static bool sort_by_date_added(SongButton const *a, SongButton const *b);
    static bool sort_by_grade(SongButton const *a, SongButton const *b);
    static bool sort_by_length(SongButton const *a, SongButton const *b);
    static bool sort_by_title(SongButton const *a, SongButton const *b);

   public:
    using SORT_ENUM = SortTypes::type;
    using GROUP_ENUM = GroupTypes::type;

    // used also by SongButton
    static bool sort_by_difficulty(SongButton const *a, SongButton const *b);

    static f32 getUIScale();
    static i32 getUIScale(f32 m) { return (i32)(m * getUIScale()); }
    static f32 getUIScale2();
    static i32 getUIScale2(f32 m) { return (i32)(m * getUIScale2()); }
    static f32 getSkinScale(SkinImage *img);
    static f32 getSkinScale2(SkinImage *img);
    static vec2 getSkinDimensions(SkinImage *img);

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
                                          std::string_view name);

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

    using SORTING_COMPARATOR = bool (*)(const SongButton *a, const SongButton *b);
    struct SORTING_METHOD {
        std::string_view name;
        SORTING_COMPARATOR comparator;
        SORT_ENUM type;
    };

    struct GROUPING {
        std::string_view name;
        GROUP_ENUM type;
    };

    static constexpr std::array<SORTING_METHOD, SORT_ENUM::MAX> SORTING_METHODS{
        {{"By Artist", sort_by_artist, SORT_ENUM::ARTIST},
         {"By BPM", sort_by_bpm, SORT_ENUM::BPM},
         {"By Creator", sort_by_creator, SORT_ENUM::CREATOR},
         {"By Date Added", sort_by_date_added, SORT_ENUM::DATEADDED},
         {"By Difficulty", sort_by_difficulty, SORT_ENUM::DIFFICULTY},
         {"By Length", sort_by_length, SORT_ENUM::LENGTH},
         {"By Title", sort_by_title, SORT_ENUM::TITLE},
         {"By Rank Achieved", sort_by_grade, SORT_ENUM::RANKACHIEVED}}};

    static constexpr std::array<GROUPING, GROUP_ENUM::MAX> GROUPINGS{
        {{"By Artist", GROUP_ENUM::ARTIST},
         {"By BPM", GROUP_ENUM::BPM},
         {"By Creator", GROUP_ENUM::CREATOR},
         {"By Date", GROUP_ENUM::DATEADDED},  // not yet possible
         {"By Difficulty", GROUP_ENUM::DIFFICULTY},
         {"By Length", GROUP_ENUM::LENGTH},
         {"By Title", GROUP_ENUM::TITLE},
         {"Collections", GROUP_ENUM::COLLECTIONS},
         {"No Grouping", GROUP_ENUM::NO_GROUPING}}};

    [[nodiscard]] inline GROUP_ENUM getGroupingMode() const { return this->curGroup.type; }

    static bool searchMatcher(const DatabaseBeatmap *databaseBeatmap,
                              const std::vector<std::string_view> &searchStringTokens);

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

    void rebuildAfterGroupOrSortChange(const GROUPING &group,
                                       const std::optional<SORTING_METHOD> &sortMethod = std::nullopt);

    void onSelectionMode();
    void onSelectionMods();
    void onSelectionRandom();
    void onSelectionOptions();

    void onScoreClicked(CBaseUIButton *button);

    void selectSongButton(CarouselButton *songButton);
    void selectPreviousRandomBeatmap();
    void playSelectedDifficulty();

    std::mt19937_64 rngalg;

    GROUPING curGroup{GROUPINGS[GROUP_ENUM::NO_GROUPING]};
    SORTING_METHOD curSortMethod{SORTING_METHODS[SORT_ENUM::ARTIST]};

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
    bool bSongButtonsNeedSorting{false};
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
    bool bCloseAfterBeatmapRefreshFinished{false};
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
    const DatabaseBeatmap *lastSelectedBeatmap{nullptr};
    bool bHasSelectedAndIsPlaying;
    float fPulseAnimation;
    float fBackgroundFadeInTime;
    std::vector<const DatabaseBeatmap *> previousRandomBeatmaps;

    // map auto-download
    i32 map_autodl = 0;
    i32 set_autodl = 0;

    // search
    UISearchOverlay *search;
    UString sSearchString{u""};
    UString sPrevSearchString{u""};
    UString sPrevHardcodedSearchString{u""};
    float fSearchWaitTime;
    bool bInSearch;
    bool bShouldRecountMatchesAfterSearch{true};
    i32 currentVisibleSearchMatches{0};
    std::optional<GROUPING> searchPrevGroup{std::nullopt};
    SongBrowserBackgroundSearchMatcher *backgroundSearchMatcher;

   private:
    std::vector<CollectionButton *> *getCollectionButtonsForGroup(GroupTypes::type group);
};
