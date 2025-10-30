// Copyright (c) 2015, PG, All rights reserved.
#include "Skin.h"

#include "Archival.h"
#include "BeatmapInterface.h"
#include "ConVar.h"
#include "ConVarHandler.h"
#include "Engine.h"
#include "Environment.h"
#include "File.h"
#include "Database.h"
#include "NotificationOverlay.h"
#include "Osu.h"
#include "Parsing.h"
#include "ResourceManager.h"
#include "SString.h"
#include "SkinImage.h"
#include "SoundEngine.h"
#include "VolumeOverlay.h"
#include "Logging.h"

#include <cstring>
#include <utility>

// Readability
// XXX: change loadSound() interface to use flags instead
#define NOT_OVERLAYABLE false
#define OVERLAYABLE true
#define STREAM false
#define SAMPLE true
#define NOT_LOOPING false
#define LOOPING true

bool Skin::BasicSkinImage::is2x() const {
    if(unlikely(!this->checked_2x)) {
        this->checked_2x = true;
        if(this->img && this->img != MISSING_TEXTURE && !this->img->getFilePath().empty()) {
            this->file_2x = this->img->getFilePath().contains("@2x");
        }
    }

    return this->file_2x;
}

void Skin::unpack(const char *filepath) {
    auto skin_name = env->getFileNameFromFilePath(filepath);
    debugLog("Extracting {:s}...", skin_name.c_str());
    skin_name.erase(skin_name.size() - 4);  // remove .osk extension

    auto skin_root = fmt::format(NEOSU_SKINS_PATH "/{}/", skin_name);

    std::unique_ptr<u8[]> fileBuffer;
    size_t fileSize{0};
    {
        File file(filepath);
        if(!file.canRead() || !(fileSize = file.getFileSize())) {
            debugLog("Failed to read skin file {:s}", filepath);
            return;
        }
        fileBuffer = file.takeFileBuffer();
        // close the file here
    }

    Archive archive(fileBuffer.get(), fileSize);
    if(!archive.isValid()) {
        debugLog("Failed to open .osk file");
        return;
    }

    auto entries = archive.getAllEntries();
    if(entries.empty()) {
        debugLog(".osk file is empty!");
        return;
    }

    if(!env->directoryExists(skin_root)) {
        env->createDirectory(skin_root);
    }

    for(const auto &entry : entries) {
        if(entry.isDirectory()) continue;

        std::string filename = entry.getFilename();
        const auto folders = SString::split(filename, '/');
        std::string file_path = skin_root;

        for(const auto &folder : folders) {
            if(!env->directoryExists(file_path)) {
                env->createDirectory(file_path);
            }

            if(folder == "..") {
                // security check: skip files with path traversal attempts
                goto skip_file;
            } else {
                file_path.push_back('/');
                file_path.append(folder);
            }
        }

        if(!entry.extractToFile(file_path)) {
            debugLog("Failed to extract skin file {:s}", filename.c_str());
        }

    skip_file:;
        // when a file can't be extracted we just ignore it (as long as the archive is valid)
    }
}

Skin::Skin(const UString &name, std::string filepath, bool isDefaultSkin) {
    this->sName = name.utf8View();
    this->sFilePath = std::move(filepath);
    this->bIsDefaultSkin = isDefaultSkin;

    // vars
    this->spinnerApproachCircleColor = 0xffffffff;
    this->spinnerBackgroundColor = 0xffdddddd;  // "by default, tinted grey"
    this->sliderBorderColor = 0xffffffff;
    this->sliderBallColor = 0xffffffff;  // NOTE: 0xff02aaff is a hardcoded special case for osu!'s default skin, but it
                                         // does not apply to user skins

    this->songSelectActiveText = 0xff000000;
    this->songSelectInactiveText = 0xffffffff;
    this->inputOverlayText = 0xff000000;

    // custom
    this->bIsRandom = cv::skin_random.getBool();
    this->bIsRandomElements = cv::skin_random_elements.getBool();

    // load all files
    this->load();
}

Skin::~Skin() {
    for(auto &resource : this->resources) {
        if(resource && resource != (Resource *)MISSING_TEXTURE) resourceManager->destroyResource(resource);
    }
    this->resources.clear();

    for(auto &image : this->images) {
        delete image;
    }
    this->images.clear();

    this->filepathsForExport.clear();
    // sounds are managed by resourcemanager, not unloaded here
}

void Skin::update() {
    // tasks which have to be run after async loading finishes
    if(!this->bReady && this->isReady()) {
        this->bReady = true;

        // force effect volume update
        osu->getVolumeOverlay()->updateEffectVolume(this);
    }

    // shitty check to not animate while paused with hitobjects in background
    if(osu->isInPlayMode() && !osu->getMapInterface()->isPlaying() && !cv::skin_animation_force.getBool()) return;

    const bool useEngineTimeForAnimations = !osu->isInPlayMode();
    const i32 curMusicPos = osu->getMapInterface()->getCurMusicPosWithOffsets();
    for(auto &image : this->images) {
        image->update(this->animationSpeedMultiplier, useEngineTimeForAnimations, curMusicPos);
    }
}

bool Skin::isReady() {
    if(this->bReady) return true;

    // default skin sounds aren't added to the resources vector... so check explicitly for that
    for(auto &sound : this->sounds) {
        if(resourceManager->isLoadingResource(sound)) return false;
    }

    for(auto &resource : this->resources) {
        if(resourceManager->isLoadingResource(resource)) return false;
    }

    for(auto &image : this->images) {
        if(!image->isReady()) return false;
    }

    // (ready is set in update())
    return true;
}

void Skin::load() {
    resourceManager->setSyncLoadMaxBatchSize(512);
    // random skins
    {
        this->filepathsForRandomSkin.clear();
        if(this->bIsRandom || this->bIsRandomElements) {
            std::vector<std::string> skinNames;

            // regular skins
            {
                std::string skinFolder = cv::osu_folder.getString();
                skinFolder.append(cv::osu_folder_sub_skins.getString());
                std::vector<std::string> skinFolders = env->getFoldersInFolder(skinFolder);

                for(const auto &i : skinFolders) {
                    if(i.compare(".") == 0 ||
                       i.compare("..") == 0)  // is this universal in every file system? too lazy to check.
                                              // should probably fix this in the engine and not here
                        continue;

                    std::string randomSkinFolder = skinFolder;
                    randomSkinFolder.append(i);
                    randomSkinFolder.append("/");

                    this->filepathsForRandomSkin.push_back(randomSkinFolder);
                    skinNames.push_back(i);
                }
            }

            if(this->bIsRandom && this->filepathsForRandomSkin.size() > 0) {
                const int randomIndex = std::rand() % std::min(this->filepathsForRandomSkin.size(), skinNames.size());

                this->sName = skinNames[randomIndex];
                this->sFilePath = this->filepathsForRandomSkin[randomIndex];
            }
        }
    }

    // spinner loading has top priority in async
    this->randomizeFilePath();
    {
        this->checkLoadImage(this->loadingSpinner, "loading-spinner", "SKIN_LOADING_SPINNER");
    }

    // and the cursor comes right after that
    this->randomizeFilePath();
    {
        this->checkLoadImage(this->cursor, "cursor", "SKIN_CURSOR");
        this->checkLoadImage(this->cursorMiddle, "cursormiddle", "SKIN_CURSORMIDDLE", true);
        this->checkLoadImage(this->cursorTrail, "cursortrail", "SKIN_CURSORTRAIL");
        this->checkLoadImage(this->cursorRipple, "cursor-ripple", "SKIN_CURSORRIPPLE");
        this->checkLoadImage(this->cursorSmoke, "cursor-smoke", "SKIN_CURSORSMOKE");

        // special case: if fallback to default cursor, do load cursorMiddle
        if(this->cursor.img == resourceManager->getImage("SKIN_CURSOR_DEFAULT"))
            this->checkLoadImage(this->cursorMiddle, "cursormiddle", "SKIN_CURSORMIDDLE");
    }

    // skin ini
    this->randomizeFilePath();
    this->sSkinIniFilePath = this->sFilePath + "skin.ini";

    bool parseSkinIni1Status = true;
    bool parseSkinIni2Status = true;
    cvars->resetSkinCvars();
    if(!this->parseSkinINI(this->sSkinIniFilePath)) {
        parseSkinIni1Status = false;
        this->sSkinIniFilePath = MCENGINE_IMAGES_PATH "/default/skin.ini";
        cvars->resetSkinCvars();
        parseSkinIni2Status = this->parseSkinINI(this->sSkinIniFilePath);
    }

    // default values, if none were loaded
    if(this->comboColors.size() == 0) {
        this->comboColors.push_back(argb(255, 255, 192, 0));
        this->comboColors.push_back(argb(255, 0, 202, 0));
        this->comboColors.push_back(argb(255, 18, 124, 255));
        this->comboColors.push_back(argb(255, 242, 24, 57));
    }

    // images
    this->randomizeFilePath();
    this->checkLoadImage(this->hitCircle, "hitcircle", "SKIN_HITCIRCLE");
    this->hitCircleOverlay2 = this->createSkinImage("hitcircleoverlay", vec2(128, 128), 64);
    this->hitCircleOverlay2->setAnimationFramerate(2);

    this->randomizeFilePath();
    this->checkLoadImage(this->approachCircle, "approachcircle", "SKIN_APPROACHCIRCLE");
    this->randomizeFilePath();
    this->checkLoadImage(this->reverseArrow, "reversearrow", "SKIN_REVERSEARROW");

    this->randomizeFilePath();
    this->followPoint2 = this->createSkinImage("followpoint", vec2(16, 22), 64);

    this->randomizeFilePath();
    {
        const std::string hitCirclePrefix = this->sHitCirclePrefix.empty() ? "default" : this->sHitCirclePrefix;
        for(int i = 0; i < 10; i++) {
            const std::string resName = fmt::format("SKIN_DEFAULT{}", i);
            this->checkLoadImage(this->defaultNumImgs[i], fmt::format("{}-{}", hitCirclePrefix, i), resName);
            // special cases: fallback to default skin hitcircle numbers if the
            // defined prefix doesn't point to any valid files
            if(this->defaultNumImgs[i].img == MISSING_TEXTURE)
                this->checkLoadImage(this->defaultNumImgs[i], fmt::format("default-{}", i), resName);
        }
    }

    this->randomizeFilePath();
    {
        const std::string scorePrefix = this->sScorePrefix.empty() ? "score" : this->sScorePrefix;
        for(int i = 0; i < 10; i++) {
            const std::string resName = fmt::format("SKIN_SCORE{}", i);
            this->checkLoadImage(this->scoreNumImgs[i], fmt::format("{}-{}", scorePrefix, i), resName);
            // fallback logic
            if(this->scoreNumImgs[i].img == MISSING_TEXTURE)
                this->checkLoadImage(this->scoreNumImgs[i], fmt::format("score-{}", i), resName);
        }

        this->checkLoadImage(this->scoreX, fmt::format("{}-x", scorePrefix), "SKIN_SCOREX");
        // if (this->scoreX == MISSING_TEXTURE) checkLoadImage(m_scoreX, "score-x", "SKIN_SCOREX"); // special
        // case: ScorePrefix'd skins don't get default fallbacks, instead missing extraneous things like the X are
        // simply not drawn
        this->checkLoadImage(this->scorePercent, fmt::format("{}-percent", scorePrefix), "SKIN_SCOREPERCENT");
        this->checkLoadImage(this->scoreDot, fmt::format("{}-dot", scorePrefix), "SKIN_SCOREDOT");
    }

    this->randomizeFilePath();
    {
        // yes, "score" is the default value for the combo prefix
        const std::string comboPrefix = this->sComboPrefix.empty() ? "score" : this->sComboPrefix;
        for(int i = 0; i < 10; i++) {
            const std::string resName = fmt::format("SKIN_COMBO{}", i);
            this->checkLoadImage(this->comboNumImgs[i], fmt::format("{}-{}", comboPrefix, i), resName);
            // fallback logic
            if(this->comboNumImgs[i].img == MISSING_TEXTURE)
                this->checkLoadImage(this->comboNumImgs[i], fmt::format("score-{}", i), resName);
        }

        // special case as above for extras
        this->checkLoadImage(this->comboX, fmt::format("{}-x", comboPrefix), "SKIN_COMBOX");
    }

    this->randomizeFilePath();
    this->playSkip = this->createSkinImage("play-skip", vec2(193, 147), 94);
    this->randomizeFilePath();
    this->checkLoadImage(this->playWarningArrow, "play-warningarrow", "SKIN_PLAYWARNINGARROW");
    this->playWarningArrow2 = this->createSkinImage("play-warningarrow", vec2(167, 129), 128);
    this->randomizeFilePath();
    this->checkLoadImage(this->circularmetre, "circularmetre", "SKIN_CIRCULARMETRE");
    this->randomizeFilePath();
    this->scorebarBg = this->createSkinImage("scorebar-bg", vec2(695, 44), 27.5f);
    this->scorebarColour = this->createSkinImage("scorebar-colour", vec2(645, 10), 6.25f);
    this->scorebarMarker = this->createSkinImage("scorebar-marker", vec2(24, 24), 15.0f);
    this->scorebarKi = this->createSkinImage("scorebar-ki", vec2(116, 116), 72.0f);
    this->scorebarKiDanger = this->createSkinImage("scorebar-kidanger", vec2(116, 116), 72.0f);
    this->scorebarKiDanger2 = this->createSkinImage("scorebar-kidanger2", vec2(116, 116), 72.0f);
    this->randomizeFilePath();
    this->sectionPassImage = this->createSkinImage("section-pass", vec2(650, 650), 400.0f);
    this->randomizeFilePath();
    this->sectionFailImage = this->createSkinImage("section-fail", vec2(650, 650), 400.0f);
    this->randomizeFilePath();
    this->inputoverlayBackground = this->createSkinImage("inputoverlay-background", vec2(193, 55), 34.25f);
    this->inputoverlayKey = this->createSkinImage("inputoverlay-key", vec2(43, 46), 26.75f);

    this->randomizeFilePath();
    this->hit0 = this->createSkinImage("hit0", vec2(128, 128), 42);
    this->hit0->setAnimationFramerate(60);
    this->hit50 = this->createSkinImage("hit50", vec2(128, 128), 42);
    this->hit50->setAnimationFramerate(60);
    this->hit50g = this->createSkinImage("hit50g", vec2(128, 128), 42);
    this->hit50g->setAnimationFramerate(60);
    this->hit50k = this->createSkinImage("hit50k", vec2(128, 128), 42);
    this->hit50k->setAnimationFramerate(60);
    this->hit100 = this->createSkinImage("hit100", vec2(128, 128), 42);
    this->hit100->setAnimationFramerate(60);
    this->hit100g = this->createSkinImage("hit100g", vec2(128, 128), 42);
    this->hit100g->setAnimationFramerate(60);
    this->hit100k = this->createSkinImage("hit100k", vec2(128, 128), 42);
    this->hit100k->setAnimationFramerate(60);
    this->hit300 = this->createSkinImage("hit300", vec2(128, 128), 42);
    this->hit300->setAnimationFramerate(60);
    this->hit300g = this->createSkinImage("hit300g", vec2(128, 128), 42);
    this->hit300g->setAnimationFramerate(60);
    this->hit300k = this->createSkinImage("hit300k", vec2(128, 128), 42);
    this->hit300k->setAnimationFramerate(60);

    this->randomizeFilePath();
    this->checkLoadImage(this->particle50, "particle50", "SKIN_PARTICLE50", true);
    this->checkLoadImage(this->particle100, "particle100", "SKIN_PARTICLE100", true);
    this->checkLoadImage(this->particle300, "particle300", "SKIN_PARTICLE300", true);

    this->randomizeFilePath();
    this->checkLoadImage(this->sliderGradient, "slidergradient", "SKIN_SLIDERGRADIENT");
    this->randomizeFilePath();
    this->sliderb = this->createSkinImage("sliderb", vec2(128, 128), 64, false, "");
    this->sliderb->setAnimationFramerate(/*45.0f*/ 50.0f);
    this->randomizeFilePath();
    this->checkLoadImage(this->sliderScorePoint, "sliderscorepoint", "SKIN_SLIDERSCOREPOINT");
    this->randomizeFilePath();
    this->sliderFollowCircle2 = this->createSkinImage("sliderfollowcircle", vec2(259, 259), 64);
    this->randomizeFilePath();
    this->checkLoadImage(
        this->sliderStartCircle, "sliderstartcircle", "SKIN_SLIDERSTARTCIRCLE",
        !this->bIsDefaultSkin);  // !m_bIsDefaultSkin ensures that default doesn't override user, in these special cases
    this->sliderStartCircle2 = this->createSkinImage("sliderstartcircle", vec2(128, 128), 64, !this->bIsDefaultSkin);
    this->checkLoadImage(this->sliderStartCircleOverlay, "sliderstartcircleoverlay", "SKIN_SLIDERSTARTCIRCLEOVERLAY",
                         !this->bIsDefaultSkin);
    this->sliderStartCircleOverlay2 =
        this->createSkinImage("sliderstartcircleoverlay", vec2(128, 128), 64, !this->bIsDefaultSkin);
    this->sliderStartCircleOverlay2->setAnimationFramerate(2);
    this->randomizeFilePath();
    this->checkLoadImage(this->sliderEndCircle, "sliderendcircle", "SKIN_SLIDERENDCIRCLE", !this->bIsDefaultSkin);
    this->sliderEndCircle2 = this->createSkinImage("sliderendcircle", vec2(128, 128), 64, !this->bIsDefaultSkin);
    this->checkLoadImage(this->sliderEndCircleOverlay, "sliderendcircleoverlay", "SKIN_SLIDERENDCIRCLEOVERLAY",
                         !this->bIsDefaultSkin);
    this->sliderEndCircleOverlay2 =
        this->createSkinImage("sliderendcircleoverlay", vec2(128, 128), 64, !this->bIsDefaultSkin);
    this->sliderEndCircleOverlay2->setAnimationFramerate(2);

    this->randomizeFilePath();
    this->checkLoadImage(this->spinnerBackground, "spinner-background", "SKIN_SPINNERBACKGROUND");
    this->checkLoadImage(this->spinnerCircle, "spinner-circle", "SKIN_SPINNERCIRCLE");
    this->checkLoadImage(this->spinnerApproachCircle, "spinner-approachcircle", "SKIN_SPINNERAPPROACHCIRCLE");
    this->checkLoadImage(this->spinnerBottom, "spinner-bottom", "SKIN_SPINNERBOTTOM");
    this->checkLoadImage(this->spinnerMiddle, "spinner-middle", "SKIN_SPINNERMIDDLE");
    this->checkLoadImage(this->spinnerMiddle2, "spinner-middle2", "SKIN_SPINNERMIDDLE2");
    this->checkLoadImage(this->spinnerTop, "spinner-top", "SKIN_SPINNERTOP");
    this->checkLoadImage(this->spinnerSpin, "spinner-spin", "SKIN_SPINNERSPIN");
    this->checkLoadImage(this->spinnerClear, "spinner-clear", "SKIN_SPINNERCLEAR");
    this->checkLoadImage(this->spinnerMetre, "spinner-metre", "SKIN_SPINNERMETRE");
    this->checkLoadImage(this->spinnerGlow, "spinner-glow", "SKIN_SPINNERGLOW");  // TODO: use
    this->checkLoadImage(this->spinnerOsu, "spinner-osu", "SKIN_SPINNEROSU");     // TODO: use
    this->checkLoadImage(this->spinnerRpm, "spinner-rpm", "SKIN_SPINNERRPM");     // TODO: use

    {
        // cursor loading was here, moved up to improve async usability
    }

    this->randomizeFilePath();
    this->selectionModEasy = this->createSkinImage("selection-mod-easy", vec2(68, 66), 38);
    this->selectionModNoFail = this->createSkinImage("selection-mod-nofail", vec2(68, 66), 38);
    this->selectionModHalfTime = this->createSkinImage("selection-mod-halftime", vec2(68, 66), 38);
    this->selectionModHardRock = this->createSkinImage("selection-mod-hardrock", vec2(68, 66), 38);
    this->selectionModSuddenDeath = this->createSkinImage("selection-mod-suddendeath", vec2(68, 66), 38);
    this->selectionModPerfect = this->createSkinImage("selection-mod-perfect", vec2(68, 66), 38);
    this->selectionModDoubleTime = this->createSkinImage("selection-mod-doubletime", vec2(68, 66), 38);
    this->selectionModNightCore = this->createSkinImage("selection-mod-nightcore", vec2(68, 66), 38);
    this->selectionModDayCore = this->createSkinImage("selection-mod-daycore", vec2(68, 66), 38);
    this->selectionModHidden = this->createSkinImage("selection-mod-hidden", vec2(68, 66), 38);
    this->selectionModFlashlight = this->createSkinImage("selection-mod-flashlight", vec2(68, 66), 38);
    this->selectionModRelax = this->createSkinImage("selection-mod-relax", vec2(68, 66), 38);
    this->selectionModAutopilot = this->createSkinImage("selection-mod-relax2", vec2(68, 66), 38);
    this->selectionModSpunOut = this->createSkinImage("selection-mod-spunout", vec2(68, 66), 38);
    this->selectionModAutoplay = this->createSkinImage("selection-mod-autoplay", vec2(68, 66), 38);
    this->selectionModNightmare = this->createSkinImage("selection-mod-nightmare", vec2(68, 66), 38);
    this->selectionModTarget = this->createSkinImage("selection-mod-target", vec2(68, 66), 38);
    this->selectionModScorev2 = this->createSkinImage("selection-mod-scorev2", vec2(68, 66), 38);
    this->selectionModTD = this->createSkinImage("selection-mod-touchdevice", vec2(68, 66), 38);
    this->selectionModCinema = this->createSkinImage("selection-mod-cinema", vec2(68, 66), 38);

    this->mode_osu = this->createSkinImage("mode-osu", vec2(32, 32), 32);
    this->mode_osu_small = this->createSkinImage("mode-osu-small", vec2(32, 32), 32);

    this->randomizeFilePath();
    this->checkLoadImage(this->pauseContinue, "pause-continue", "SKIN_PAUSE_CONTINUE");
    this->checkLoadImage(this->pauseReplay, "pause-replay", "SKIN_PAUSE_REPLAY");
    this->checkLoadImage(this->pauseRetry, "pause-retry", "SKIN_PAUSE_RETRY");
    this->checkLoadImage(this->pauseBack, "pause-back", "SKIN_PAUSE_BACK");
    this->checkLoadImage(this->pauseOverlay, "pause-overlay", "SKIN_PAUSE_OVERLAY");
    if(this->pauseOverlay.img == MISSING_TEXTURE)
        this->checkLoadImage(this->pauseOverlay, "pause-overlay", "SKIN_PAUSE_OVERLAY", true, "jpg");
    this->checkLoadImage(this->failBackground, "fail-background", "SKIN_FAIL_BACKGROUND");
    if(this->failBackground.img == MISSING_TEXTURE)
        this->checkLoadImage(this->failBackground, "fail-background", "SKIN_FAIL_BACKGROUND", true, "jpg");
    this->checkLoadImage(this->unpause, "unpause", "SKIN_UNPAUSE");

    this->randomizeFilePath();
    this->checkLoadImage(this->buttonLeft, "button-left", "SKIN_BUTTON_LEFT");
    this->checkLoadImage(this->buttonMiddle, "button-middle", "SKIN_BUTTON_MIDDLE");
    this->checkLoadImage(this->buttonRight, "button-right", "SKIN_BUTTON_RIGHT");
    this->randomizeFilePath();
    this->menuBackImg = this->createSkinImage("menu-back", vec2(225, 87), 54);
    this->randomizeFilePath();

    // NOTE: scaling is ignored when drawing this specific element
    this->selectionMode = this->createSkinImage("selection-mode", vec2(90, 90), 38);

    this->selectionModeOver = this->createSkinImage("selection-mode-over", vec2(88, 90), 38);
    this->selectionMods = this->createSkinImage("selection-mods", vec2(74, 90), 38);
    this->selectionModsOver = this->createSkinImage("selection-mods-over", vec2(74, 90), 38);
    this->selectionRandom = this->createSkinImage("selection-random", vec2(74, 90), 38);
    this->selectionRandomOver = this->createSkinImage("selection-random-over", vec2(74, 90), 38);
    this->selectionOptions = this->createSkinImage("selection-options", vec2(74, 90), 38);
    this->selectionOptionsOver = this->createSkinImage("selection-options-over", vec2(74, 90), 38);

    this->randomizeFilePath();
    this->checkLoadImage(this->songSelectTop, "songselect-top", "SKIN_SONGSELECT_TOP");
    this->checkLoadImage(this->songSelectBottom, "songselect-bottom", "SKIN_SONGSELECT_BOTTOM");
    this->randomizeFilePath();
    this->checkLoadImage(this->menuButtonBackground, "menu-button-background", "SKIN_MENU_BUTTON_BACKGROUND");
    this->menuButtonBackground2 = this->createSkinImage("menu-button-background", vec2(699, 103), 64.0f);
    this->randomizeFilePath();
    this->checkLoadImage(this->star, "star", "SKIN_STAR");

    this->randomizeFilePath();
    this->checkLoadImage(this->rankingPanel, "ranking-panel", "SKIN_RANKING_PANEL");
    this->checkLoadImage(this->rankingGraph, "ranking-graph", "SKIN_RANKING_GRAPH");
    this->checkLoadImage(this->rankingTitle, "ranking-title", "SKIN_RANKING_TITLE");
    this->checkLoadImage(this->rankingMaxCombo, "ranking-maxcombo", "SKIN_RANKING_MAXCOMBO");
    this->checkLoadImage(this->rankingAccuracy, "ranking-accuracy", "SKIN_RANKING_ACCURACY");

    this->checkLoadImage(this->rankingA, "ranking-A", "SKIN_RANKING_A");
    this->checkLoadImage(this->rankingB, "ranking-B", "SKIN_RANKING_B");
    this->checkLoadImage(this->rankingC, "ranking-C", "SKIN_RANKING_C");
    this->checkLoadImage(this->rankingD, "ranking-D", "SKIN_RANKING_D");
    this->checkLoadImage(this->rankingS, "ranking-S", "SKIN_RANKING_S");
    this->checkLoadImage(this->rankingSH, "ranking-SH", "SKIN_RANKING_SH");
    this->checkLoadImage(this->rankingX, "ranking-X", "SKIN_RANKING_X");
    this->checkLoadImage(this->rankingXH, "ranking-XH", "SKIN_RANKING_XH");

    this->rankingAsmall = this->createSkinImage("ranking-A-small", vec2(34, 40), 128);
    this->rankingBsmall = this->createSkinImage("ranking-B-small", vec2(34, 40), 128);
    this->rankingCsmall = this->createSkinImage("ranking-C-small", vec2(34, 40), 128);
    this->rankingDsmall = this->createSkinImage("ranking-D-small", vec2(34, 40), 128);
    this->rankingSsmall = this->createSkinImage("ranking-S-small", vec2(34, 40), 128);
    this->rankingSHsmall = this->createSkinImage("ranking-SH-small", vec2(34, 40), 128);
    this->rankingXsmall = this->createSkinImage("ranking-X-small", vec2(34, 40), 128);
    this->rankingXHsmall = this->createSkinImage("ranking-XH-small", vec2(34, 40), 128);

    this->rankingPerfect = this->createSkinImage("ranking-perfect", vec2(478, 150), 128);

    this->randomizeFilePath();
    this->checkLoadImage(this->beatmapImportSpinner, "beatmapimport-spinner", "SKIN_BEATMAP_IMPORT_SPINNER");
    {
        // loading spinner load was here, moved up to improve async usability
    }
    this->checkLoadImage(this->circleEmpty, "circle-empty", "SKIN_CIRCLE_EMPTY");
    this->checkLoadImage(this->circleFull, "circle-full", "SKIN_CIRCLE_FULL");
    this->randomizeFilePath();
    this->checkLoadImage(this->seekTriangle, "seektriangle", "SKIN_SEEKTRIANGLE");
    this->randomizeFilePath();
    this->checkLoadImage(this->userIcon, "user-icon", "SKIN_USER_ICON");
    this->randomizeFilePath();
    this->checkLoadImage(this->backgroundCube, "backgroundcube", "SKIN_FPOSU_BACKGROUNDCUBE", false, "png",
                         true);  // force mipmaps
    this->randomizeFilePath();
    this->checkLoadImage(this->menuBackground, "menu-background", "SKIN_MENU_BACKGROUND", false, "jpg");
    this->randomizeFilePath();
    this->checkLoadImage(this->skybox, "skybox", "SKIN_FPOSU_3D_SKYBOX");

    // slider ticks
    this->loadSound(this->normalSliderTick, "normal-slidertick", "SKIN_NORMALSLIDERTICK_SND",  //
                    OVERLAYABLE, SAMPLE, NOT_LOOPING);                                         //
    this->loadSound(this->softSliderTick, "soft-slidertick", "SKIN_SOFTSLIDERTICK_SND",        //
                    OVERLAYABLE, SAMPLE, NOT_LOOPING);                                         //
    this->loadSound(this->drumSliderTick, "drum-slidertick", "SKIN_DRUMSLIDERTICK_SND",        //
                    OVERLAYABLE, SAMPLE, NOT_LOOPING);                                         //

    // silder slides
    this->loadSound(this->normalSliderSlide, "normal-sliderslide", "SKIN_NORMALSLIDERSLIDE_SND",  //
                    NOT_OVERLAYABLE, SAMPLE, LOOPING);                                            //
    this->loadSound(this->softSliderSlide, "soft-sliderslide", "SKIN_SOFTSLIDERSLIDE_SND",        //
                    NOT_OVERLAYABLE, SAMPLE, LOOPING);                                            //
    this->loadSound(this->drumSliderSlide, "drum-sliderslide", "SKIN_DRUMSLIDERSLIDE_SND",        //
                    NOT_OVERLAYABLE, SAMPLE, LOOPING);                                            //

    // slider whistles
    this->loadSound(this->normalSliderWhistle, "normal-sliderwhistle", "SKIN_NORMALSLIDERWHISTLE_SND",  //
                    NOT_OVERLAYABLE, SAMPLE, LOOPING);                                                  //
    this->loadSound(this->softSliderWhistle, "soft-sliderwhistle", "SKIN_SOFTSLIDERWHISTLE_SND",        //
                    NOT_OVERLAYABLE, SAMPLE, LOOPING);                                                  //
    this->loadSound(this->drumSliderWhistle, "drum-sliderwhistle", "SKIN_DRUMSLIDERWHISTLE_SND",        //
                    NOT_OVERLAYABLE, SAMPLE, LOOPING);                                                  //

    // hitcircle
    this->loadSound(this->normalHitNormal, "normal-hitnormal", "SKIN_NORMALHITNORMAL_SND",     //
                    OVERLAYABLE, SAMPLE, NOT_LOOPING);                                         //
    this->loadSound(this->softHitNormal, "soft-hitnormal", "SKIN_SOFTHITNORMAL_SND",           //
                    OVERLAYABLE, SAMPLE, NOT_LOOPING);                                         //
    this->loadSound(this->drumHitNormal, "drum-hitnormal", "SKIN_DRUMHITNORMAL_SND",           //
                    OVERLAYABLE, SAMPLE, NOT_LOOPING);                                         //
    this->loadSound(this->normalHitWhistle, "normal-hitwhistle", "SKIN_NORMALHITWHISTLE_SND",  //
                    OVERLAYABLE, SAMPLE, NOT_LOOPING);                                         //
    this->loadSound(this->softHitWhistle, "soft-hitwhistle", "SKIN_SOFTHITWHISTLE_SND",        //
                    OVERLAYABLE, SAMPLE, NOT_LOOPING);                                         //
    this->loadSound(this->drumHitWhistle, "drum-hitwhistle", "SKIN_DRUMHITWHISTLE_SND",        //
                    OVERLAYABLE, SAMPLE, NOT_LOOPING);                                         //
    this->loadSound(this->normalHitFinish, "normal-hitfinish", "SKIN_NORMALHITFINISH_SND",     //
                    OVERLAYABLE, SAMPLE, NOT_LOOPING);                                         //
    this->loadSound(this->softHitFinish, "soft-hitfinish", "SKIN_SOFTHITFINISH_SND",           //
                    OVERLAYABLE, SAMPLE, NOT_LOOPING);                                         //
    this->loadSound(this->drumHitFinish, "drum-hitfinish", "SKIN_DRUMHITFINISH_SND",           //
                    OVERLAYABLE, SAMPLE, NOT_LOOPING);                                         //
    this->loadSound(this->normalHitClap, "normal-hitclap", "SKIN_NORMALHITCLAP_SND",           //
                    OVERLAYABLE, SAMPLE, NOT_LOOPING);                                         //
    this->loadSound(this->softHitClap, "soft-hitclap", "SKIN_SOFTHITCLAP_SND",                 //
                    OVERLAYABLE, SAMPLE, NOT_LOOPING);                                         //
    this->loadSound(this->drumHitClap, "drum-hitclap", "SKIN_DRUMHITCLAP_SND",                 //
                    OVERLAYABLE, SAMPLE, NOT_LOOPING);                                         //

    // spinner
    this->loadSound(this->spinnerBonus, "spinnerbonus", "SKIN_SPINNERBONUS_SND", OVERLAYABLE, SAMPLE, NOT_LOOPING);
    this->loadSound(this->spinnerSpinSound, "spinnerspin", "SKIN_SPINNERSPIN_SND", NOT_OVERLAYABLE, SAMPLE, LOOPING);

    // others
    this->loadSound(this->combobreak, "combobreak", "SKIN_COMBOBREAK_SND", true, true);
    this->loadSound(this->failsound, "failsound", "SKIN_FAILSOUND_SND");
    this->loadSound(this->applause, "applause", "SKIN_APPLAUSE_SND");
    this->loadSound(this->menuHit, "menuhit", "SKIN_MENUHIT_SND", true, true);
    this->loadSound(this->menuHover, "menuclick", "SKIN_MENUCLICK_SND", true, true);
    this->loadSound(this->checkOn, "check-on", "SKIN_CHECKON_SND", true, true);
    this->loadSound(this->checkOff, "check-off", "SKIN_CHECKOFF_SND", true, true);
    this->loadSound(this->shutter, "shutter", "SKIN_SHUTTER_SND", true, true);
    this->loadSound(this->sectionPassSound, "sectionpass", "SKIN_SECTIONPASS_SND");
    this->loadSound(this->sectionFailSound, "sectionfail", "SKIN_SECTIONFAIL_SND");

    // UI feedback
    this->loadSound(this->messageSent, "key-confirm", "SKIN_MESSAGE_SENT_SND", true, true, false);
    this->loadSound(this->deletingText, "key-delete", "SKIN_DELETING_TEXT_SND", true, true, false);
    this->loadSound(this->movingTextCursor, "key-movement", "MOVING_TEXT_CURSOR_SND", true, true, false);
    this->loadSound(this->typing1, "key-press-1", "TYPING_1_SND", true, true, false);
    this->loadSound(this->typing2, "key-press-2", "TYPING_2_SND", true, true, false, false);
    this->loadSound(this->typing3, "key-press-3", "TYPING_3_SND", true, true, false, false);
    this->loadSound(this->typing4, "key-press-4", "TYPING_4_SND", true, true, false, false);
    this->loadSound(this->menuBack, "menuback", "MENU_BACK_SND", true, true, false, false);
    this->loadSound(this->closeChatTab, "click-close", "CLOSE_CHAT_TAB_SND", true, true, false, false);
    this->loadSound(this->clickButton, "click-short-confirm", "CLICK_BUTTON_SND", true, true, false, false);
    this->loadSound(this->hoverButton, "click-short", "HOVER_BUTTON_SND", true, true, false, false);
    this->loadSound(this->backButtonClick, "back-button-click", "BACK_BUTTON_CLICK_SND", true, true, false, false);
    this->loadSound(this->backButtonHover, "back-button-hover", "BACK_BUTTON_HOVER_SND", true, true, false, false);
    this->loadSound(this->clickMainMenuCube, "menu-play-click", "CLICK_MAIN_MENU_CUBE_SND", true, true, false, false);
    this->loadSound(this->hoverMainMenuCube, "menu-play-hover", "HOVER_MAIN_MENU_CUBE_SND", true, true, false, false);
    this->loadSound(this->clickSingleplayer, "menu-freeplay-click", "CLICK_SINGLEPLAYER_SND", true, true, false, false);
    this->loadSound(this->hoverSingleplayer, "menu-freeplay-hover", "HOVER_SINGLEPLAYER_SND", true, true, false, false);
    this->loadSound(this->clickMultiplayer, "menu-multiplayer-click", "CLICK_MULTIPLAYER_SND", true, true, false,
                    false);
    this->loadSound(this->hoverMultiplayer, "menu-multiplayer-hover", "HOVER_MULTIPLAYER_SND", true, true, false,
                    false);
    this->loadSound(this->clickOptions, "menu-options-click", "CLICK_OPTIONS_SND", true, true, false, false);
    this->loadSound(this->hoverOptions, "menu-options-hover", "HOVER_OPTIONS_SND", true, true, false, false);
    this->loadSound(this->clickExit, "menu-exit-click", "CLICK_EXIT_SND", true, true, false, false);
    this->loadSound(this->hoverExit, "menu-exit-hover", "HOVER_EXIT_SND", true, true, false, false);
    this->loadSound(this->expand, "select-expand", "EXPAND_SND", true, true, false);
    this->loadSound(this->selectDifficulty, "select-difficulty", "SELECT_DIFFICULTY_SND", true, true, false, false);
    this->loadSound(this->sliderbar, "sliderbar", "DRAG_SLIDER_SND", true, true, false);
    this->loadSound(this->matchConfirm, "match-confirm", "ALL_PLAYERS_READY_SND", true, true, false);
    this->loadSound(this->roomJoined, "match-join", "ROOM_JOINED_SND", true, true, false);
    this->loadSound(this->roomQuit, "match-leave", "ROOM_QUIT_SND", true, true, false);
    this->loadSound(this->roomNotReady, "match-notready", "ROOM_NOT_READY_SND", true, true, false);
    this->loadSound(this->roomReady, "match-ready", "ROOM_READY_SND", true, true, false);
    this->loadSound(this->matchStart, "match-start", "MATCH_START_SND", true, true, false);

    this->loadSound(this->pauseLoop, "pause-loop", "PAUSE_LOOP_SND", NOT_OVERLAYABLE, STREAM, LOOPING, true);
    this->loadSound(this->pauseHover, "pause-hover", "PAUSE_HOVER_SND", OVERLAYABLE, SAMPLE, NOT_LOOPING, false);
    this->loadSound(this->clickPauseBack, "pause-back-click", "CLICK_QUIT_SONG_SND", true, true, false, false);
    this->loadSound(this->hoverPauseBack, "pause-back-hover", "HOVER_QUIT_SONG_SND", true, true, false, false);
    this->loadSound(this->clickPauseContinue, "pause-continue-click", "CLICK_RESUME_SONG_SND", true, true, false,
                    false);
    this->loadSound(this->hoverPauseContinue, "pause-continue-hover", "HOVER_RESUME_SONG_SND", true, true, false,
                    false);
    this->loadSound(this->clickPauseRetry, "pause-retry-click", "CLICK_RETRY_SONG_SND", true, true, false, false);
    this->loadSound(this->hoverPauseRetry, "pause-retry-hover", "HOVER_RETRY_SONG_SND", true, true, false, false);

    if(!this->clickButton) this->clickButton = this->menuHit;
    if(!this->hoverButton) this->hoverButton = this->menuHover;
    if(!this->pauseHover) this->pauseHover = this->hoverButton;
    if(!this->selectDifficulty) this->selectDifficulty = this->clickButton;
    if(!this->typing2) this->typing2 = this->typing1;
    if(!this->typing3) this->typing3 = this->typing2;
    if(!this->typing4) this->typing4 = this->typing3;
    if(!this->backButtonClick) this->backButtonClick = this->clickButton;
    if(!this->backButtonHover) this->backButtonHover = this->hoverButton;
    if(!this->menuBack) this->menuBack = this->clickButton;
    if(!this->closeChatTab) this->closeChatTab = this->clickButton;
    if(!this->clickMainMenuCube) this->clickMainMenuCube = this->clickButton;
    if(!this->hoverMainMenuCube) this->hoverMainMenuCube = this->menuHover;
    if(!this->clickSingleplayer) this->clickSingleplayer = this->clickButton;
    if(!this->hoverSingleplayer) this->hoverSingleplayer = this->menuHover;
    if(!this->clickMultiplayer) this->clickMultiplayer = this->clickButton;
    if(!this->hoverMultiplayer) this->hoverMultiplayer = this->menuHover;
    if(!this->clickOptions) this->clickOptions = this->clickButton;
    if(!this->hoverOptions) this->hoverOptions = this->menuHover;
    if(!this->clickExit) this->clickExit = this->clickButton;
    if(!this->hoverExit) this->hoverExit = this->menuHover;
    if(!this->clickPauseBack) this->clickPauseBack = this->clickButton;
    if(!this->hoverPauseBack) this->hoverPauseBack = this->pauseHover;
    if(!this->clickPauseContinue) this->clickPauseContinue = this->clickButton;
    if(!this->hoverPauseContinue) this->hoverPauseContinue = this->pauseHover;
    if(!this->clickPauseRetry) this->clickPauseRetry = this->clickButton;
    if(!this->hoverPauseRetry) this->hoverPauseRetry = this->pauseHover;

    // custom
    BasicSkinImage defaultCursor{resourceManager->getImage("SKIN_CURSOR_DEFAULT")};
    BasicSkinImage defaultCursor2 = this->cursor;
    if(defaultCursor)
        this->defaultCursor = defaultCursor;
    else if(defaultCursor2)
        this->defaultCursor = defaultCursor2;

    BasicSkinImage defaultButtonLeft{resourceManager->getImage("SKIN_BUTTON_LEFT_DEFAULT")};
    BasicSkinImage defaultButtonLeft2 = this->buttonLeft;
    if(defaultButtonLeft)
        this->defaultButtonLeft = defaultButtonLeft;
    else if(defaultButtonLeft2)
        this->defaultButtonLeft = defaultButtonLeft2;

    BasicSkinImage defaultButtonMiddle{resourceManager->getImage("SKIN_BUTTON_MIDDLE_DEFAULT")};
    BasicSkinImage defaultButtonMiddle2 = this->buttonMiddle;
    if(defaultButtonMiddle)
        this->defaultButtonMiddle = defaultButtonMiddle;
    else if(defaultButtonMiddle2)
        this->defaultButtonMiddle = defaultButtonMiddle2;

    BasicSkinImage defaultButtonRight{resourceManager->getImage("SKIN_BUTTON_RIGHT_DEFAULT")};
    BasicSkinImage defaultButtonRight2 = this->buttonRight;
    if(defaultButtonRight)
        this->defaultButtonRight = defaultButtonRight;
    else if(defaultButtonRight2)
        this->defaultButtonRight = defaultButtonRight2;

    // print some debug info
    debugLog("Skin: Version {:f}", this->fVersion);
    debugLog("Skin: HitCircleOverlap = {:d}", this->iHitCircleOverlap);

    // delayed error notifications due to resource loading potentially blocking engine time
    if(!parseSkinIni1Status && parseSkinIni2Status && cv::skin.getString() != "default")
        osu->getNotificationOverlay()->addNotification("Error: Couldn't load skin.ini!", 0xffff0000);
    else if(!parseSkinIni2Status)
        osu->getNotificationOverlay()->addNotification("Error: Couldn't load DEFAULT skin.ini!!!", 0xffff0000);

    resourceManager->resetSyncLoadMaxBatchSize();
}

void Skin::loadBeatmapOverride(const std::string & /*filepath*/) {
    // debugLog("Skin::loadBeatmapOverride( {:s} )", filepath.c_str());
    //  TODO: beatmap skin support
}

void Skin::reloadSounds() {
    std::vector<Resource *> soundResources;
    soundResources.reserve(this->sounds.size());

    for(auto &sound : this->sounds) {
        soundResources.push_back(sound);
    }

    resourceManager->reloadResources(soundResources, cv::skin_async.getBool());
}

bool Skin::parseSkinINI(std::string filepath) {
    std::string fileContent;

    size_t fileSize{0};
    {
        File file(filepath);
        if(!file.canRead() || !(fileSize = file.getFileSize())) {
            debugLog("OsuSkin Error: Couldn't load {:s}", filepath);
            return false;
        }
        // convert possible non-UTF8 file to UTF8
        UString uFileContent{file.readToString().c_str(), static_cast<int>(fileSize)};
        // then store the resulting std::string
        fileContent = uFileContent.toUtf8();
        // close the file here
    }

    enum class SkinSection : u8 {
        GENERAL,
        COLOURS,
        FONTS,
        NEOSU,
    };

    bool hasNonEmptyLines = false;

    std::array<std::optional<Color>, 8> tempColors;

    // osu! defaults to [General] and loads properties even before the actual section start
    SkinSection curBlock = SkinSection::GENERAL;
    using enum SkinSection;

    for(auto curLineUnstripped : SString::split(fileContent, '\n')) {
        SString::trim_inplace(curLineUnstripped);
        // ignore comments, but only if at the beginning of a line
        if(curLineUnstripped.empty() || curLineUnstripped.starts_with("//")) continue;
        hasNonEmptyLines = true;

        const auto curLine = curLineUnstripped;  // don't want to accidentally modify it somewhere later

        // section detection
        if(curLine.find("[General]") != std::string::npos)
            curBlock = GENERAL;
        else if(curLine.find("[Colours]") != std::string::npos || curLine.find("[Colors]") != std::string::npos)
            curBlock = COLOURS;
        else if(curLine.find("[Fonts]") != std::string::npos)
            curBlock = FONTS;
        else if(curLine.find("[neosu]") != std::string::npos)
            curBlock = NEOSU;

        switch(curBlock) {
            case GENERAL: {
                std::string version;
                if(Parsing::parse(curLine, "Version", ':', &version)) {
                    if((version.find("latest") != std::string::npos) || (version.find("User") != std::string::npos)) {
                        this->fVersion = 2.5f;
                    } else {
                        Parsing::parse(curLine, "Version", ':', &this->fVersion);
                    }
                }

                Parsing::parse(curLine, "CursorRotate", ':', &this->bCursorRotate);
                Parsing::parse(curLine, "CursorCentre", ':', &this->bCursorCenter);
                Parsing::parse(curLine, "CursorExpand", ':', &this->bCursorExpand);
                Parsing::parse(curLine, "LayeredHitSounds", ':', &this->bLayeredHitSounds);
                Parsing::parse(curLine, "SliderBallFlip", ':', &this->bSliderBallFlip);
                Parsing::parse(curLine, "AllowSliderBallTint", ':', &this->bAllowSliderBallTint);
                Parsing::parse(curLine, "HitCircleOverlayAboveNumber", ':', &this->bHitCircleOverlayAboveNumber);
                Parsing::parse(curLine, "SpinnerFadePlayfield", ':', &this->bSpinnerFadePlayfield);
                Parsing::parse(curLine, "SpinnerFrequencyModulate", ':', &this->bSpinnerFrequencyModulate);
                Parsing::parse(curLine, "SpinnerNoBlink", ':', &this->bSpinnerNoBlink);

                // https://osu.ppy.sh/community/forums/topics/314209
                Parsing::parse(curLine, "HitCircleOverlayAboveNumer", ':', &this->bHitCircleOverlayAboveNumber);

                if(Parsing::parse(curLine, "SliderStyle", ':', &this->iSliderStyle)) {
                    if(this->iSliderStyle != 1 && this->iSliderStyle != 2) this->iSliderStyle = 2;
                }

                if(Parsing::parse(curLine, "AnimationFramerate", ':', &this->fAnimationFramerate)) {
                    if(this->fAnimationFramerate < 0.f) this->fAnimationFramerate = 0.f;
                }

                break;
            }

            case COLOURS: {
                u8 comboNum;
                u8 r, g, b;

                if(Parsing::parse(curLine, "Combo", &comboNum, ':', &r, ',', &g, ',', &b)) {
                    if(comboNum >= 1 && comboNum <= 8) {
                        tempColors[comboNum - 1] = rgb(r, g, b);
                    }
                } else if(Parsing::parse(curLine, "SpinnerApproachCircle", ':', &r, ',', &g, ',', &b))
                    this->spinnerApproachCircleColor = rgb(r, g, b);
                else if(Parsing::parse(curLine, "SpinnerBackground", ':', &r, ',', &g, ',', &b))
                    this->spinnerBackgroundColor = rgb(r, g, b);
                else if(Parsing::parse(curLine, "SliderBall", ':', &r, ',', &g, ',', &b))
                    this->sliderBallColor = rgb(r, g, b);
                else if(Parsing::parse(curLine, "SliderBorder", ':', &r, ',', &g, ',', &b))
                    this->sliderBorderColor = rgb(r, g, b);
                else if(Parsing::parse(curLine, "SliderTrackOverride", ':', &r, ',', &g, ',', &b)) {
                    this->sliderTrackOverride = rgb(r, g, b);
                    this->bSliderTrackOverride = true;
                } else if(Parsing::parse(curLine, "SongSelectActiveText", ':', &r, ',', &g, ',', &b))
                    this->songSelectActiveText = rgb(r, g, b);
                else if(Parsing::parse(curLine, "SongSelectInactiveText", ':', &r, ',', &g, ',', &b))
                    this->songSelectInactiveText = rgb(r, g, b);
                else if(Parsing::parse(curLine, "InputOverlayText", ':', &r, ',', &g, ',', &b))
                    this->inputOverlayText = rgb(r, g, b);

                break;
            }

            case FONTS: {
                Parsing::parse(curLine, "ComboOverlap", ':', &this->iComboOverlap);
                Parsing::parse(curLine, "ScoreOverlap", ':', &this->iScoreOverlap);
                Parsing::parse(curLine, "HitCircleOverlap", ':', &this->iHitCircleOverlap);

                if(Parsing::parse(curLine, "ComboPrefix", ':', &this->sComboPrefix)) {
                    // XXX: jank path normalization
                    std::ranges::replace(this->sComboPrefix, '\\', '/');
                }

                if(Parsing::parse(curLine, "ScorePrefix", ':', &this->sScorePrefix)) {
                    // XXX: jank path normalization
                    std::ranges::replace(this->sScorePrefix, '\\', '/');
                }

                if(Parsing::parse(curLine, "HitCirclePrefix", ':', &this->sHitCirclePrefix)) {
                    // XXX: jank path normalization
                    std::ranges::replace(this->sHitCirclePrefix, '\\', '/');
                }

                break;
            }

            case NEOSU: {
                size_t pos = curLine.find(':');
                if(pos == std::string::npos) break;

                bool shouldParse = true;
                std::string name, value;
                shouldParse &= !!Parsing::parse(curLine.substr(0, pos), &name);
                shouldParse &= !!Parsing::parse(curLine.substr(pos + 1), &value);

                // XXX: shouldn't be setting cvars directly in parsing method
                if(shouldParse) {
                    auto cvar = cvars->getConVarByName(name, false);
                    if(cvar) {
                        cvar->setValue(value, true, ConVar::CvarEditor::SKIN);
                    } else {
                        debugLog("Skin wanted to set cvar '{}' to '{}', but it doesn't exist!", name, value);
                    }
                }

                break;
            }
        }
    }

    if(!hasNonEmptyLines) return false;

    for(const auto &tempCol : tempColors) {
        if(tempCol.has_value()) {
            this->comboColors.push_back(tempCol.value());
        }
    }

    return true;
}

Color Skin::getComboColorForCounter(int i, int offset) {
    i += cv::skin_color_index_add.getInt();
    i = std::max(i, 0);

    if(this->beatmapComboColors.size() > 0 && !cv::ignore_beatmap_combo_colors.getBool())
        return this->beatmapComboColors[(i + offset) % this->beatmapComboColors.size()];
    else if(this->comboColors.size() > 0)
        return this->comboColors[i % this->comboColors.size()];
    else
        return argb(255, 0, 255, 0);
}

void Skin::setBeatmapComboColors(std::vector<Color> colors) { this->beatmapComboColors = std::move(colors); }

void Skin::playSpinnerSpinSound() {
    if(this->spinnerSpinSound == nullptr) return;

    if(!this->spinnerSpinSound->isPlaying()) {
        soundEngine->play(this->spinnerSpinSound);
    }
}

void Skin::playSpinnerBonusSound() { soundEngine->play(this->spinnerBonus); }

void Skin::stopSpinnerSpinSound() {
    if(this->spinnerSpinSound == nullptr) return;

    if(this->spinnerSpinSound->isPlaying()) soundEngine->stop(this->spinnerSpinSound);
}

void Skin::randomizeFilePath() {
    if(this->bIsRandomElements && this->filepathsForRandomSkin.size() > 0)
        this->sFilePath = this->filepathsForRandomSkin[rand() % this->filepathsForRandomSkin.size()];
}

SkinImage *Skin::createSkinImage(const std::string &skinElementName, vec2 baseSizeForScaling2x, float osuSize,
                                 bool ignoreDefaultSkin, const std::string &animationSeparator) {
    auto *skinImage =
        new SkinImage(this, skinElementName, baseSizeForScaling2x, osuSize, animationSeparator, ignoreDefaultSkin);
    this->images.push_back(skinImage);

    const std::vector<std::string> &filepathsForExport = skinImage->getFilepathsForExport();
    this->filepathsForExport.insert(this->filepathsForExport.end(), filepathsForExport.begin(),
                                    filepathsForExport.end());

    return skinImage;
}

void Skin::checkLoadImage(BasicSkinImage &imgRef, const std::string &skinElementName, const std::string &resourceName,
                          bool ignoreDefaultSkin, const std::string &fileExtension, bool forceLoadMipmaps) {
    if(imgRef.img != MISSING_TEXTURE) return;  // we are already loaded

    // NOTE: only the default skin is loaded with a resource name (it must never be unloaded by other instances), and it
    // is NOT added to the resources vector

    std::string defaultFilePath1 = MCENGINE_IMAGES_PATH "/default/";
    defaultFilePath1.append(skinElementName);
    defaultFilePath1.append("@2x.");
    defaultFilePath1.append(fileExtension);

    std::string defaultFilePath2 = MCENGINE_IMAGES_PATH "/default/";
    defaultFilePath2.append(skinElementName);
    defaultFilePath2.append(".");
    defaultFilePath2.append(fileExtension);

    std::string filepath1 = this->sFilePath;
    filepath1.append(skinElementName);
    filepath1.append("@2x.");
    filepath1.append(fileExtension);

    std::string filepath2 = this->sFilePath;
    filepath2.append(skinElementName);
    filepath2.append(".");
    filepath2.append(fileExtension);

    const bool existsDefaultFilePath1 = env->fileExists(defaultFilePath1);
    const bool existsDefaultFilePath2 = env->fileExists(defaultFilePath2);
    const bool existsFilepath1 = env->fileExists(filepath1);
    const bool existsFilepath2 = env->fileExists(filepath2);

    // check if an @2x version of this image exists
    if(cv::skin_hd.getBool()) {
        // load default skin
        if(!ignoreDefaultSkin) {
            if(existsDefaultFilePath1) {
                std::string defaultResourceName = resourceName;
                defaultResourceName.append("_DEFAULT");  // so we don't load the default skin twice

                if(cv::skin_async.getBool()) resourceManager->requestNextLoadAsync();

                imgRef = {resourceManager->loadImageAbs(defaultFilePath1, defaultResourceName,
                                                        cv::skin_mipmaps.getBool() || forceLoadMipmaps)};
            } else {
                // fallback to @1x
                if(existsDefaultFilePath2) {
                    std::string defaultResourceName = resourceName;
                    defaultResourceName.append("_DEFAULT");  // so we don't load the default skin twice

                    if(cv::skin_async.getBool()) resourceManager->requestNextLoadAsync();

                    imgRef = {resourceManager->loadImageAbs(defaultFilePath2, defaultResourceName,
                                                            cv::skin_mipmaps.getBool() || forceLoadMipmaps)};
                }
            }
        }

        // load user skin
        if(existsFilepath1) {
            if(cv::skin_async.getBool()) resourceManager->requestNextLoadAsync();

            imgRef = {resourceManager->loadImageAbs(filepath1, "", cv::skin_mipmaps.getBool() || forceLoadMipmaps)};
            this->resources.push_back(imgRef.img);

            // export
            this->filepathsForExport.push_back(filepath1);
            if(existsFilepath2) this->filepathsForExport.push_back(filepath2);

            if(!existsFilepath1 && !existsFilepath2) {
                if(existsDefaultFilePath1) this->filepathsForExport.push_back(defaultFilePath1);
                if(existsDefaultFilePath2) this->filepathsForExport.push_back(defaultFilePath2);
            }

            return;  // nothing more to do here
        }
    }

    // else load normal @1x version

    // load default skin
    if(!ignoreDefaultSkin) {
        if(existsDefaultFilePath2) {
            std::string defaultResourceName = resourceName;
            defaultResourceName.append("_DEFAULT");  // so we don't load the default skin twice

            if(cv::skin_async.getBool()) resourceManager->requestNextLoadAsync();

            imgRef = {resourceManager->loadImageAbs(defaultFilePath2, defaultResourceName,
                                                    cv::skin_mipmaps.getBool() || forceLoadMipmaps)};
        }
    }

    // load user skin
    if(existsFilepath2) {
        if(cv::skin_async.getBool()) resourceManager->requestNextLoadAsync();

        imgRef = {resourceManager->loadImageAbs(filepath2, "", cv::skin_mipmaps.getBool() || forceLoadMipmaps)};
        this->resources.push_back(imgRef.img);
    }

    // export
    if(existsFilepath1) this->filepathsForExport.push_back(filepath1);
    if(existsFilepath2) this->filepathsForExport.push_back(filepath2);

    if(!existsFilepath1 && !existsFilepath2) {
        if(existsDefaultFilePath1) this->filepathsForExport.push_back(defaultFilePath1);
        if(existsDefaultFilePath2) this->filepathsForExport.push_back(defaultFilePath2);
    }
}

void Skin::loadSound(Sound *&sndRef, const std::string &skinElementName, const std::string &resourceName,
                     bool isOverlayable, bool isSample, bool loop, bool fallback_to_default) {
    if(sndRef != nullptr) return;  // we are already loaded

    // random skin support
    this->randomizeFilePath();

    bool was_first_load = false;

    auto try_load_sound = [isSample, isOverlayable, &was_first_load](
                              const std::string &base_path, const std::string &filename, bool loop,
                              const std::string &resource_name, bool default_skin) -> Sound * {
        const char *extensions[] = {".wav", ".mp3", ".ogg", ".flac"};
        for(auto &extension : extensions) {
            std::string fn = filename;
            fn.append(extension);

            std::string path = base_path;
            path.append(fn);

            // this check will fix up the filename casing
            if(env->fileExists(path)) {
                Sound *existing_sound = resourceManager->getSound(resource_name);

                // default already loaded, just return it
                if(default_skin && existing_sound) {
                    // check if it's actually a default skin, though, since we no longer add a "_DEFAULT"
                    // to the resource name to differentiate it
                    // this avoids thinking that we have a loaded default skin element when it was actually from a previous (non-default) skin
                    const std::string &existing_path = existing_sound->getFilePath();
                    if(!existing_path.empty() && existing_path.contains(MCENGINE_IMAGES_PATH "/default/")) {
                        return existing_sound;
                    }
                }

                was_first_load = true;

                // user skin, rebuild with new path
                if(existing_sound) {
                    existing_sound->rebuild(path, cv::skin_async.getBool());
                    return existing_sound;
                }

                if(cv::skin_async.getBool()) {
                    resourceManager->requestNextLoadAsync();
                }

                // load sound here
                return resourceManager->loadSoundAbs(path, resource_name, !isSample, isOverlayable, loop);
            }
        }
        return nullptr;
    };

    // load user skin
    bool loaded_user_skin = false;
    if(cv::skin_use_skin_hitsounds.getBool() || !isSample) {
        sndRef = try_load_sound(this->sFilePath, skinElementName, loop, resourceName, false);
        loaded_user_skin = (sndRef != nullptr);
    }

    if(fallback_to_default && !loaded_user_skin) {
        std::string defaultpath = MCENGINE_IMAGES_PATH "/default/";
        sndRef = try_load_sound(defaultpath, skinElementName, loop, resourceName, true);
    }

    // failed both default and user
    if(sndRef == nullptr) {
        debugLog("Skin Warning: NULL sound {:s}!", skinElementName.c_str());
        return;
    }

    // force reload default skin sound anyway if the custom skin does not include it (e.g. audio device change)
    if(!loaded_user_skin && !was_first_load) {
        resourceManager->reloadResource(sndRef, cv::skin_async.getBool());
    }

    this->sounds.push_back(sndRef);

    // export
    this->filepathsForExport.push_back(sndRef->getFilePath());
}
