// Copyright (c) 2015, PG, All rights reserved.
#include "Skin.h"

#include <cstring>
#include <utility>

#include "Archival.h"
#include "BeatmapInterface.h"
#include "ConVar.h"
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

// Readability
// XXX: change loadSound() interface to use flags instead
#define NOT_OVERLAYABLE false
#define OVERLAYABLE true
#define STREAM false
#define SAMPLE true
#define NOT_LOOPING false
#define LOOPING true

void Skin::unpack(const char *filepath) {
    auto skin_name = env->getFileNameFromFilePath(filepath);
    debugLog("Extracting {:s}...\n", skin_name.c_str());
    skin_name.erase(skin_name.size() - 4);  // remove .osk extension

    auto skin_root = std::string(MCENGINE_DATA_DIR "skins/");
    skin_root.append(skin_name);
    skin_root.append("/");

    std::vector<u8> fileBuffer;
    size_t fileSize{0};
    {
        File file(filepath);
        if(!file.canRead() || !(fileSize = file.getFileSize())) {
            debugLog("Failed to read skin file {:s}\n", filepath);
            return;
        }
        fileBuffer = file.takeFileBuffer();
        // close the file here
    }

    Archive archive(fileBuffer.data(), fileSize);
    if(!archive.isValid()) {
        debugLog("Failed to open .osk file\n");
        return;
    }

    auto entries = archive.getAllEntries();
    if(entries.empty()) {
        debugLog(".osk file is empty!\n");
        return;
    }

    if(!env->directoryExists(skin_root)) {
        env->createDirectory(skin_root);
    }

    for(const auto &entry : entries) {
        if(entry.isDirectory()) continue;

        std::string filename = entry.getFilename();
        const auto folders = SString::split(filename, "/");
        std::string file_path = skin_root;

        for(const auto &folder : folders) {
            if(!env->directoryExists(file_path)) {
                env->createDirectory(file_path);
            }

            if(folder == "..") {
                // security check: skip files with path traversal attempts
                goto skip_file;
            } else {
                file_path.append("/");
                file_path.append(folder.c_str());
            }
        }

        if(!entry.extractToFile(file_path)) {
            debugLog("Failed to extract skin file {:s}\n", filename.c_str());
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
    const long curMusicPos = osu->getMapInterface()->getCurMusicPosWithOffsets();
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
        this->checkLoadImage(&this->loadingSpinner, "loading-spinner", "SKIN_LOADING_SPINNER");
    }

    // and the cursor comes right after that
    this->randomizeFilePath();
    {
        this->checkLoadImage(&this->cursor, "cursor", "SKIN_CURSOR");
        this->checkLoadImage(&this->cursorMiddle, "cursormiddle", "SKIN_CURSORMIDDLE", true);
        this->checkLoadImage(&this->cursorTrail, "cursortrail", "SKIN_CURSORTRAIL");
        this->checkLoadImage(&this->cursorRipple, "cursor-ripple", "SKIN_CURSORRIPPLE");
        this->checkLoadImage(&this->cursorSmoke, "cursor-smoke", "SKIN_CURSORSMOKE");

        // special case: if fallback to default cursor, do load cursorMiddle
        if(this->cursor == resourceManager->getImage("SKIN_CURSOR_DEFAULT"))
            this->checkLoadImage(&this->cursorMiddle, "cursormiddle", "SKIN_CURSORMIDDLE");
    }

    // skin ini
    this->randomizeFilePath();
    this->sSkinIniFilePath = this->sFilePath + "skin.ini";

    bool parseSkinIni1Status = true;
    bool parseSkinIni2Status = true;
    cvars->resetSkinCvars();
    if(!this->parseSkinINI(this->sSkinIniFilePath)) {
        parseSkinIni1Status = false;
        this->sSkinIniFilePath = MCENGINE_DATA_DIR "materials/default/skin.ini";
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
    this->checkLoadImage(&this->hitCircle, "hitcircle", "SKIN_HITCIRCLE");
    this->hitCircleOverlay2 = this->createSkinImage("hitcircleoverlay", vec2(128, 128), 64);
    this->hitCircleOverlay2->setAnimationFramerate(2);

    this->randomizeFilePath();
    this->checkLoadImage(&this->approachCircle, "approachcircle", "SKIN_APPROACHCIRCLE");
    this->randomizeFilePath();
    this->checkLoadImage(&this->reverseArrow, "reversearrow", "SKIN_REVERSEARROW");

    this->randomizeFilePath();
    this->followPoint2 = this->createSkinImage("followpoint", vec2(16, 22), 64);

    this->randomizeFilePath();
    {
        std::string hitCirclePrefix = this->sHitCirclePrefix.length() > 0 ? this->sHitCirclePrefix : "default";
        std::string hitCircleStringFinal = hitCirclePrefix;
        hitCircleStringFinal.append("-0");
        this->checkLoadImage(&this->default0, hitCircleStringFinal, "SKIN_DEFAULT0");
        if(this->default0 == MISSING_TEXTURE)
            this->checkLoadImage(&this->default0, "default-0",
                                 "SKIN_DEFAULT0");  // special cases: fallback to default skin hitcircle numbers if
                                                    // the defined prefix doesn't point to any valid files
        hitCircleStringFinal = hitCirclePrefix;
        hitCircleStringFinal.append("-1");
        this->checkLoadImage(&this->default1, hitCircleStringFinal, "SKIN_DEFAULT1");
        if(this->default1 == MISSING_TEXTURE) this->checkLoadImage(&this->default1, "default-1", "SKIN_DEFAULT1");
        hitCircleStringFinal = hitCirclePrefix;
        hitCircleStringFinal.append("-2");
        this->checkLoadImage(&this->default2, hitCircleStringFinal, "SKIN_DEFAULT2");
        if(this->default2 == MISSING_TEXTURE) this->checkLoadImage(&this->default2, "default-2", "SKIN_DEFAULT2");
        hitCircleStringFinal = hitCirclePrefix;
        hitCircleStringFinal.append("-3");
        this->checkLoadImage(&this->default3, hitCircleStringFinal, "SKIN_DEFAULT3");
        if(this->default3 == MISSING_TEXTURE) this->checkLoadImage(&this->default3, "default-3", "SKIN_DEFAULT3");
        hitCircleStringFinal = hitCirclePrefix;
        hitCircleStringFinal.append("-4");
        this->checkLoadImage(&this->default4, hitCircleStringFinal, "SKIN_DEFAULT4");
        if(this->default4 == MISSING_TEXTURE) this->checkLoadImage(&this->default4, "default-4", "SKIN_DEFAULT4");
        hitCircleStringFinal = hitCirclePrefix;
        hitCircleStringFinal.append("-5");
        this->checkLoadImage(&this->default5, hitCircleStringFinal, "SKIN_DEFAULT5");
        if(this->default5 == MISSING_TEXTURE) this->checkLoadImage(&this->default5, "default-5", "SKIN_DEFAULT5");
        hitCircleStringFinal = hitCirclePrefix;
        hitCircleStringFinal.append("-6");
        this->checkLoadImage(&this->default6, hitCircleStringFinal, "SKIN_DEFAULT6");
        if(this->default6 == MISSING_TEXTURE) this->checkLoadImage(&this->default6, "default-6", "SKIN_DEFAULT6");
        hitCircleStringFinal = hitCirclePrefix;
        hitCircleStringFinal.append("-7");
        this->checkLoadImage(&this->default7, hitCircleStringFinal, "SKIN_DEFAULT7");
        if(this->default7 == MISSING_TEXTURE) this->checkLoadImage(&this->default7, "default-7", "SKIN_DEFAULT7");
        hitCircleStringFinal = hitCirclePrefix;
        hitCircleStringFinal.append("-8");
        this->checkLoadImage(&this->default8, hitCircleStringFinal, "SKIN_DEFAULT8");
        if(this->default8 == MISSING_TEXTURE) this->checkLoadImage(&this->default8, "default-8", "SKIN_DEFAULT8");
        hitCircleStringFinal = hitCirclePrefix;
        hitCircleStringFinal.append("-9");
        this->checkLoadImage(&this->default9, hitCircleStringFinal, "SKIN_DEFAULT9");
        if(this->default9 == MISSING_TEXTURE) this->checkLoadImage(&this->default9, "default-9", "SKIN_DEFAULT9");
    }

    this->randomizeFilePath();
    {
        std::string scorePrefix = this->sScorePrefix.length() > 0 ? this->sScorePrefix : "score";
        std::string scoreStringFinal = scorePrefix;
        scoreStringFinal.append("-0");
        this->checkLoadImage(&this->score0, scoreStringFinal, "SKIN_SCORE0");
        if(this->score0 == MISSING_TEXTURE)
            this->checkLoadImage(&this->score0, "score-0",
                                 "SKIN_SCORE0");  // special cases: fallback to default skin score numbers if the
                                                  // defined prefix doesn't point to any valid files
        scoreStringFinal = scorePrefix;
        scoreStringFinal.append("-1");
        this->checkLoadImage(&this->score1, scoreStringFinal, "SKIN_SCORE1");
        if(this->score1 == MISSING_TEXTURE) this->checkLoadImage(&this->score1, "score-1", "SKIN_SCORE1");
        scoreStringFinal = scorePrefix;
        scoreStringFinal.append("-2");
        this->checkLoadImage(&this->score2, scoreStringFinal, "SKIN_SCORE2");
        if(this->score2 == MISSING_TEXTURE) this->checkLoadImage(&this->score2, "score-2", "SKIN_SCORE2");
        scoreStringFinal = scorePrefix;
        scoreStringFinal.append("-3");
        this->checkLoadImage(&this->score3, scoreStringFinal, "SKIN_SCORE3");
        if(this->score3 == MISSING_TEXTURE) this->checkLoadImage(&this->score3, "score-3", "SKIN_SCORE3");
        scoreStringFinal = scorePrefix;
        scoreStringFinal.append("-4");
        this->checkLoadImage(&this->score4, scoreStringFinal, "SKIN_SCORE4");
        if(this->score4 == MISSING_TEXTURE) this->checkLoadImage(&this->score4, "score-4", "SKIN_SCORE4");
        scoreStringFinal = scorePrefix;
        scoreStringFinal.append("-5");
        this->checkLoadImage(&this->score5, scoreStringFinal, "SKIN_SCORE5");
        if(this->score5 == MISSING_TEXTURE) this->checkLoadImage(&this->score5, "score-5", "SKIN_SCORE5");
        scoreStringFinal = scorePrefix;
        scoreStringFinal.append("-6");
        this->checkLoadImage(&this->score6, scoreStringFinal, "SKIN_SCORE6");
        if(this->score6 == MISSING_TEXTURE) this->checkLoadImage(&this->score6, "score-6", "SKIN_SCORE6");
        scoreStringFinal = scorePrefix;
        scoreStringFinal.append("-7");
        this->checkLoadImage(&this->score7, scoreStringFinal, "SKIN_SCORE7");
        if(this->score7 == MISSING_TEXTURE) this->checkLoadImage(&this->score7, "score-7", "SKIN_SCORE7");
        scoreStringFinal = scorePrefix;
        scoreStringFinal.append("-8");
        this->checkLoadImage(&this->score8, scoreStringFinal, "SKIN_SCORE8");
        if(this->score8 == MISSING_TEXTURE) this->checkLoadImage(&this->score8, "score-8", "SKIN_SCORE8");
        scoreStringFinal = scorePrefix;
        scoreStringFinal.append("-9");
        this->checkLoadImage(&this->score9, scoreStringFinal, "SKIN_SCORE9");
        if(this->score9 == MISSING_TEXTURE) this->checkLoadImage(&this->score9, "score-9", "SKIN_SCORE9");

        scoreStringFinal = scorePrefix;
        scoreStringFinal.append("-x");
        this->checkLoadImage(&this->scoreX, scoreStringFinal, "SKIN_SCOREX");
        // if (this->scoreX == MISSING_TEXTURE) checkLoadImage(&m_scoreX, "score-x", "SKIN_SCOREX"); // special
        // case: ScorePrefix'd skins don't get default fallbacks, instead missing extraneous things like the X are
        // simply not drawn
        scoreStringFinal = scorePrefix;
        scoreStringFinal.append("-percent");
        this->checkLoadImage(&this->scorePercent, scoreStringFinal, "SKIN_SCOREPERCENT");
        // if (this->scorePercent == MISSING_TEXTURE) checkLoadImage(&m_scorePercent, "score-percent",
        // "SKIN_SCOREPERCENT"); // special case: ScorePrefix'd skins don't get default fallbacks, instead missing
        // extraneous things like the X are simply not drawn
        scoreStringFinal = scorePrefix;
        scoreStringFinal.append("-dot");
        this->checkLoadImage(&this->scoreDot, scoreStringFinal, "SKIN_SCOREDOT");
        // if (this->scoreDot == MISSING_TEXTURE) checkLoadImage(&m_scoreDot, "score-dot", "SKIN_SCOREDOT"); //
        // special case: ScorePrefix'd skins don't get default fallbacks, instead missing extraneous things like the X
        // are simply not drawn
    }

    this->randomizeFilePath();
    {
        std::string comboPrefix = this->sComboPrefix.length() > 0
                                      ? this->sComboPrefix
                                      : "score";  // yes, "score" is the default value for the combo prefix
        std::string comboStringFinal = comboPrefix;
        comboStringFinal.append("-0");
        this->checkLoadImage(&this->combo0, comboStringFinal, "SKIN_COMBO0");
        if(this->combo0 == MISSING_TEXTURE)
            this->checkLoadImage(&this->combo0, "score-0",
                                 "SKIN_COMBO0");  // special cases: fallback to default skin combo numbers if the
                                                  // defined prefix doesn't point to any valid files
        comboStringFinal = comboPrefix;
        comboStringFinal.append("-1");
        this->checkLoadImage(&this->combo1, comboStringFinal, "SKIN_COMBO1");
        if(this->combo1 == MISSING_TEXTURE) this->checkLoadImage(&this->combo1, "score-1", "SKIN_COMBO1");
        comboStringFinal = comboPrefix;
        comboStringFinal.append("-2");
        this->checkLoadImage(&this->combo2, comboStringFinal, "SKIN_COMBO2");
        if(this->combo2 == MISSING_TEXTURE) this->checkLoadImage(&this->combo2, "score-2", "SKIN_COMBO2");
        comboStringFinal = comboPrefix;
        comboStringFinal.append("-3");
        this->checkLoadImage(&this->combo3, comboStringFinal, "SKIN_COMBO3");
        if(this->combo3 == MISSING_TEXTURE) this->checkLoadImage(&this->combo3, "score-3", "SKIN_COMBO3");
        comboStringFinal = comboPrefix;
        comboStringFinal.append("-4");
        this->checkLoadImage(&this->combo4, comboStringFinal, "SKIN_COMBO4");
        if(this->combo4 == MISSING_TEXTURE) this->checkLoadImage(&this->combo4, "score-4", "SKIN_COMBO4");
        comboStringFinal = comboPrefix;
        comboStringFinal.append("-5");
        this->checkLoadImage(&this->combo5, comboStringFinal, "SKIN_COMBO5");
        if(this->combo5 == MISSING_TEXTURE) this->checkLoadImage(&this->combo5, "score-5", "SKIN_COMBO5");
        comboStringFinal = comboPrefix;
        comboStringFinal.append("-6");
        this->checkLoadImage(&this->combo6, comboStringFinal, "SKIN_COMBO6");
        if(this->combo6 == MISSING_TEXTURE) this->checkLoadImage(&this->combo6, "score-6", "SKIN_COMBO6");
        comboStringFinal = comboPrefix;
        comboStringFinal.append("-7");
        this->checkLoadImage(&this->combo7, comboStringFinal, "SKIN_COMBO7");
        if(this->combo7 == MISSING_TEXTURE) this->checkLoadImage(&this->combo7, "score-7", "SKIN_COMBO7");
        comboStringFinal = comboPrefix;
        comboStringFinal.append("-8");
        this->checkLoadImage(&this->combo8, comboStringFinal, "SKIN_COMBO8");
        if(this->combo8 == MISSING_TEXTURE) this->checkLoadImage(&this->combo8, "score-8", "SKIN_COMBO8");
        comboStringFinal = comboPrefix;
        comboStringFinal.append("-9");
        this->checkLoadImage(&this->combo9, comboStringFinal, "SKIN_COMBO9");
        if(this->combo9 == MISSING_TEXTURE) this->checkLoadImage(&this->combo9, "score-9", "SKIN_COMBO9");

        comboStringFinal = comboPrefix;
        comboStringFinal.append("-x");
        this->checkLoadImage(&this->comboX, comboStringFinal, "SKIN_COMBOX");
        // if (this->comboX == MISSING_TEXTURE) m_comboX = m_scoreX; // special case: ComboPrefix'd skins don't get
        // default fallbacks, instead missing extraneous things like the X are simply not drawn
    }

    this->randomizeFilePath();
    this->playSkip = this->createSkinImage("play-skip", vec2(193, 147), 94);
    this->randomizeFilePath();
    this->checkLoadImage(&this->playWarningArrow, "play-warningarrow", "SKIN_PLAYWARNINGARROW");
    this->playWarningArrow2 = this->createSkinImage("play-warningarrow", vec2(167, 129), 128);
    this->randomizeFilePath();
    this->checkLoadImage(&this->circularmetre, "circularmetre", "SKIN_CIRCULARMETRE");
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
    this->checkLoadImage(&this->particle50, "particle50", "SKIN_PARTICLE50", true);
    this->checkLoadImage(&this->particle100, "particle100", "SKIN_PARTICLE100", true);
    this->checkLoadImage(&this->particle300, "particle300", "SKIN_PARTICLE300", true);

    this->randomizeFilePath();
    this->checkLoadImage(&this->sliderGradient, "slidergradient", "SKIN_SLIDERGRADIENT");
    this->randomizeFilePath();
    this->sliderb = this->createSkinImage("sliderb", vec2(128, 128), 64, false, "");
    this->sliderb->setAnimationFramerate(/*45.0f*/ 50.0f);
    this->randomizeFilePath();
    this->checkLoadImage(&this->sliderScorePoint, "sliderscorepoint", "SKIN_SLIDERSCOREPOINT");
    this->randomizeFilePath();
    this->sliderFollowCircle2 = this->createSkinImage("sliderfollowcircle", vec2(259, 259), 64);
    this->randomizeFilePath();
    this->checkLoadImage(
        &this->sliderStartCircle, "sliderstartcircle", "SKIN_SLIDERSTARTCIRCLE",
        !this->bIsDefaultSkin);  // !m_bIsDefaultSkin ensures that default doesn't override user, in these special cases
    this->sliderStartCircle2 = this->createSkinImage("sliderstartcircle", vec2(128, 128), 64, !this->bIsDefaultSkin);
    this->checkLoadImage(&this->sliderStartCircleOverlay, "sliderstartcircleoverlay", "SKIN_SLIDERSTARTCIRCLEOVERLAY",
                         !this->bIsDefaultSkin);
    this->sliderStartCircleOverlay2 =
        this->createSkinImage("sliderstartcircleoverlay", vec2(128, 128), 64, !this->bIsDefaultSkin);
    this->sliderStartCircleOverlay2->setAnimationFramerate(2);
    this->randomizeFilePath();
    this->checkLoadImage(&this->sliderEndCircle, "sliderendcircle", "SKIN_SLIDERENDCIRCLE", !this->bIsDefaultSkin);
    this->sliderEndCircle2 = this->createSkinImage("sliderendcircle", vec2(128, 128), 64, !this->bIsDefaultSkin);
    this->checkLoadImage(&this->sliderEndCircleOverlay, "sliderendcircleoverlay", "SKIN_SLIDERENDCIRCLEOVERLAY",
                         !this->bIsDefaultSkin);
    this->sliderEndCircleOverlay2 =
        this->createSkinImage("sliderendcircleoverlay", vec2(128, 128), 64, !this->bIsDefaultSkin);
    this->sliderEndCircleOverlay2->setAnimationFramerate(2);

    this->randomizeFilePath();
    this->checkLoadImage(&this->spinnerBackground, "spinner-background", "SKIN_SPINNERBACKGROUND");
    this->checkLoadImage(&this->spinnerCircle, "spinner-circle", "SKIN_SPINNERCIRCLE");
    this->checkLoadImage(&this->spinnerApproachCircle, "spinner-approachcircle", "SKIN_SPINNERAPPROACHCIRCLE");
    this->checkLoadImage(&this->spinnerBottom, "spinner-bottom", "SKIN_SPINNERBOTTOM");
    this->checkLoadImage(&this->spinnerMiddle, "spinner-middle", "SKIN_SPINNERMIDDLE");
    this->checkLoadImage(&this->spinnerMiddle2, "spinner-middle2", "SKIN_SPINNERMIDDLE2");
    this->checkLoadImage(&this->spinnerTop, "spinner-top", "SKIN_SPINNERTOP");
    this->checkLoadImage(&this->spinnerSpin, "spinner-spin", "SKIN_SPINNERSPIN");
    this->checkLoadImage(&this->spinnerClear, "spinner-clear", "SKIN_SPINNERCLEAR");

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
    this->checkLoadImage(&this->pauseContinue, "pause-continue", "SKIN_PAUSE_CONTINUE");
    this->checkLoadImage(&this->pauseReplay, "pause-replay", "SKIN_PAUSE_REPLAY");
    this->checkLoadImage(&this->pauseRetry, "pause-retry", "SKIN_PAUSE_RETRY");
    this->checkLoadImage(&this->pauseBack, "pause-back", "SKIN_PAUSE_BACK");
    this->checkLoadImage(&this->pauseOverlay, "pause-overlay", "SKIN_PAUSE_OVERLAY");
    if(this->pauseOverlay == MISSING_TEXTURE)
        this->checkLoadImage(&this->pauseOverlay, "pause-overlay", "SKIN_PAUSE_OVERLAY", true, "jpg");
    this->checkLoadImage(&this->failBackground, "fail-background", "SKIN_FAIL_BACKGROUND");
    if(this->failBackground == MISSING_TEXTURE)
        this->checkLoadImage(&this->failBackground, "fail-background", "SKIN_FAIL_BACKGROUND", true, "jpg");
    this->checkLoadImage(&this->unpause, "unpause", "SKIN_UNPAUSE");

    this->randomizeFilePath();
    this->checkLoadImage(&this->buttonLeft, "button-left", "SKIN_BUTTON_LEFT");
    this->checkLoadImage(&this->buttonMiddle, "button-middle", "SKIN_BUTTON_MIDDLE");
    this->checkLoadImage(&this->buttonRight, "button-right", "SKIN_BUTTON_RIGHT");
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
    this->checkLoadImage(&this->songSelectTop, "songselect-top", "SKIN_SONGSELECT_TOP");
    this->checkLoadImage(&this->songSelectBottom, "songselect-bottom", "SKIN_SONGSELECT_BOTTOM");
    this->randomizeFilePath();
    this->checkLoadImage(&this->menuButtonBackground, "menu-button-background", "SKIN_MENU_BUTTON_BACKGROUND");
    this->menuButtonBackground2 = this->createSkinImage("menu-button-background", vec2(699, 103), 64.0f);
    this->randomizeFilePath();
    this->checkLoadImage(&this->star, "star", "SKIN_STAR");

    this->randomizeFilePath();
    this->checkLoadImage(&this->rankingPanel, "ranking-panel", "SKIN_RANKING_PANEL");
    this->checkLoadImage(&this->rankingGraph, "ranking-graph", "SKIN_RANKING_GRAPH");
    this->checkLoadImage(&this->rankingTitle, "ranking-title", "SKIN_RANKING_TITLE");
    this->checkLoadImage(&this->rankingMaxCombo, "ranking-maxcombo", "SKIN_RANKING_MAXCOMBO");
    this->checkLoadImage(&this->rankingAccuracy, "ranking-accuracy", "SKIN_RANKING_ACCURACY");

    this->checkLoadImage(&this->rankingA, "ranking-A", "SKIN_RANKING_A");
    this->checkLoadImage(&this->rankingB, "ranking-B", "SKIN_RANKING_B");
    this->checkLoadImage(&this->rankingC, "ranking-C", "SKIN_RANKING_C");
    this->checkLoadImage(&this->rankingD, "ranking-D", "SKIN_RANKING_D");
    this->checkLoadImage(&this->rankingS, "ranking-S", "SKIN_RANKING_S");
    this->checkLoadImage(&this->rankingSH, "ranking-SH", "SKIN_RANKING_SH");
    this->checkLoadImage(&this->rankingX, "ranking-X", "SKIN_RANKING_X");
    this->checkLoadImage(&this->rankingXH, "ranking-XH", "SKIN_RANKING_XH");

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
    this->checkLoadImage(&this->beatmapImportSpinner, "beatmapimport-spinner", "SKIN_BEATMAP_IMPORT_SPINNER");
    {
        // loading spinner load was here, moved up to improve async usability
    }
    this->checkLoadImage(&this->circleEmpty, "circle-empty", "SKIN_CIRCLE_EMPTY");
    this->checkLoadImage(&this->circleFull, "circle-full", "SKIN_CIRCLE_FULL");
    this->randomizeFilePath();
    this->checkLoadImage(&this->seekTriangle, "seektriangle", "SKIN_SEEKTRIANGLE");
    this->randomizeFilePath();
    this->checkLoadImage(&this->userIcon, "user-icon", "SKIN_USER_ICON");
    this->randomizeFilePath();
    this->checkLoadImage(&this->backgroundCube, "backgroundcube", "SKIN_FPOSU_BACKGROUNDCUBE", false, "png",
                         true);  // force mipmaps
    this->randomizeFilePath();
    this->checkLoadImage(&this->menuBackground, "menu-background", "SKIN_MENU_BACKGROUND", false, "jpg");
    this->randomizeFilePath();
    this->checkLoadImage(&this->skybox, "skybox", "SKIN_FPOSU_3D_SKYBOX");

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

    if(this->clickButton == nullptr) this->clickButton = this->menuHit;
    if(this->hoverButton == nullptr) this->hoverButton = this->menuHover;
    if(this->pauseHover == nullptr) this->pauseHover = this->hoverButton;
    if(this->selectDifficulty == nullptr) this->selectDifficulty = this->clickButton;
    if(this->typing2 == nullptr) this->typing2 = this->typing1;
    if(this->typing3 == nullptr) this->typing3 = this->typing2;
    if(this->typing4 == nullptr) this->typing4 = this->typing3;
    if(this->backButtonClick == nullptr) this->backButtonClick = this->clickButton;
    if(this->backButtonHover == nullptr) this->backButtonHover = this->hoverButton;
    if(this->menuBack == nullptr) this->menuBack = this->clickButton;
    if(this->closeChatTab == nullptr) this->closeChatTab = this->clickButton;
    if(this->clickMainMenuCube == nullptr) this->clickMainMenuCube = this->clickButton;
    if(this->hoverMainMenuCube == nullptr) this->hoverMainMenuCube = this->menuHover;
    if(this->clickSingleplayer == nullptr) this->clickSingleplayer = this->clickButton;
    if(this->hoverSingleplayer == nullptr) this->hoverSingleplayer = this->menuHover;
    if(this->clickMultiplayer == nullptr) this->clickMultiplayer = this->clickButton;
    if(this->hoverMultiplayer == nullptr) this->hoverMultiplayer = this->menuHover;
    if(this->clickOptions == nullptr) this->clickOptions = this->clickButton;
    if(this->hoverOptions == nullptr) this->hoverOptions = this->menuHover;
    if(this->clickExit == nullptr) this->clickExit = this->clickButton;
    if(this->hoverExit == nullptr) this->hoverExit = this->menuHover;
    if(this->clickPauseBack == nullptr) this->clickPauseBack = this->clickButton;
    if(this->hoverPauseBack == nullptr) this->hoverPauseBack = this->pauseHover;
    if(this->clickPauseContinue == nullptr) this->clickPauseContinue = this->clickButton;
    if(this->hoverPauseContinue == nullptr) this->hoverPauseContinue = this->pauseHover;
    if(this->clickPauseRetry == nullptr) this->clickPauseRetry = this->clickButton;
    if(this->hoverPauseRetry == nullptr) this->hoverPauseRetry = this->pauseHover;

    // scaling
    // HACKHACK: this is pure cancer
    if(this->cursor != nullptr && this->cursor->getFilePath().find("@2x") != -1) this->bCursor2x = true;
    if(this->cursorTrail != nullptr && this->cursorTrail->getFilePath().find("@2x") != -1) this->bCursorTrail2x = true;
    if(this->cursorRipple != nullptr && this->cursorRipple->getFilePath().find("@2x") != -1)
        this->bCursorRipple2x = true;
    if(this->cursorSmoke != nullptr && this->cursorSmoke->getFilePath().find("@2x") != -1) this->bCursorSmoke2x = true;
    if(this->approachCircle != nullptr && this->approachCircle->getFilePath().find("@2x") != -1)
        this->bApproachCircle2x = true;
    if(this->reverseArrow != nullptr && this->reverseArrow->getFilePath().find("@2x") != -1)
        this->bReverseArrow2x = true;
    if(this->hitCircle != nullptr && this->hitCircle->getFilePath().find("@2x") != -1) this->bHitCircle2x = true;
    if(this->default0 != nullptr && this->default0->getFilePath().find("@2x") != -1) this->bIsDefault02x = true;
    if(this->default1 != nullptr && this->default1->getFilePath().find("@2x") != -1) this->bIsDefault12x = true;
    if(this->score0 != nullptr && this->score0->getFilePath().find("@2x") != -1) this->bIsScore02x = true;
    if(this->combo0 != nullptr && this->combo0->getFilePath().find("@2x") != -1) this->bIsCombo02x = true;
    if(this->spinnerApproachCircle != nullptr && this->spinnerApproachCircle->getFilePath().find("@2x") != -1)
        this->bSpinnerApproachCircle2x = true;
    if(this->spinnerBottom != nullptr && this->spinnerBottom->getFilePath().find("@2x") != -1)
        this->bSpinnerBottom2x = true;
    if(this->spinnerCircle != nullptr && this->spinnerCircle->getFilePath().find("@2x") != -1)
        this->bSpinnerCircle2x = true;
    if(this->spinnerTop != nullptr && this->spinnerTop->getFilePath().find("@2x") != -1) this->bSpinnerTop2x = true;
    if(this->spinnerMiddle != nullptr && this->spinnerMiddle->getFilePath().find("@2x") != -1)
        this->bSpinnerMiddle2x = true;
    if(this->spinnerMiddle2 != nullptr && this->spinnerMiddle2->getFilePath().find("@2x") != -1)
        this->bSpinnerMiddle22x = true;
    if(this->sliderScorePoint != nullptr && this->sliderScorePoint->getFilePath().find("@2x") != -1)
        this->bSliderScorePoint2x = true;
    if(this->sliderStartCircle != nullptr && this->sliderStartCircle->getFilePath().find("@2x") != -1)
        this->bSliderStartCircle2x = true;
    if(this->sliderEndCircle != nullptr && this->sliderEndCircle->getFilePath().find("@2x") != -1)
        this->bSliderEndCircle2x = true;

    if(this->circularmetre != nullptr && this->circularmetre->getFilePath().find("@2x") != -1)
        this->bCircularmetre2x = true;

    if(this->pauseContinue != nullptr && this->pauseContinue->getFilePath().find("@2x") != -1)
        this->bPauseContinue2x = true;

    if(this->menuButtonBackground != nullptr && this->menuButtonBackground->getFilePath().find("@2x") != -1)
        this->bMenuButtonBackground2x = true;
    if(this->star != nullptr && this->star->getFilePath().find("@2x") != -1) this->bStar2x = true;
    if(this->rankingPanel != nullptr && this->rankingPanel->getFilePath().find("@2x") != -1)
        this->bRankingPanel2x = true;
    if(this->rankingMaxCombo != nullptr && this->rankingMaxCombo->getFilePath().find("@2x") != -1)
        this->bRankingMaxCombo2x = true;
    if(this->rankingAccuracy != nullptr && this->rankingAccuracy->getFilePath().find("@2x") != -1)
        this->bRankingAccuracy2x = true;
    if(this->rankingA != nullptr && this->rankingA->getFilePath().find("@2x") != -1) this->bRankingA2x = true;
    if(this->rankingB != nullptr && this->rankingB->getFilePath().find("@2x") != -1) this->bRankingB2x = true;
    if(this->rankingC != nullptr && this->rankingC->getFilePath().find("@2x") != -1) this->bRankingC2x = true;
    if(this->rankingD != nullptr && this->rankingD->getFilePath().find("@2x") != -1) this->bRankingD2x = true;
    if(this->rankingS != nullptr && this->rankingS->getFilePath().find("@2x") != -1) this->bRankingS2x = true;
    if(this->rankingSH != nullptr && this->rankingSH->getFilePath().find("@2x") != -1) this->bRankingSH2x = true;
    if(this->rankingX != nullptr && this->rankingX->getFilePath().find("@2x") != -1) this->bRankingX2x = true;
    if(this->rankingXH != nullptr && this->rankingXH->getFilePath().find("@2x") != -1) this->bRankingXH2x = true;

    // HACKHACK: all of the <>2 loads are temporary fixes until I fix the checkLoadImage() function logic

    // custom
    Image *defaultCursor = resourceManager->getImage("SKIN_CURSOR_DEFAULT");
    Image *defaultCursor2 = this->cursor;
    if(defaultCursor != nullptr)
        this->defaultCursor = defaultCursor;
    else if(defaultCursor2 != nullptr)
        this->defaultCursor = defaultCursor2;

    Image *defaultButtonLeft = resourceManager->getImage("SKIN_BUTTON_LEFT_DEFAULT");
    Image *defaultButtonLeft2 = this->buttonLeft;
    if(defaultButtonLeft != nullptr)
        this->defaultButtonLeft = defaultButtonLeft;
    else if(defaultButtonLeft2 != nullptr)
        this->defaultButtonLeft = defaultButtonLeft2;

    Image *defaultButtonMiddle = resourceManager->getImage("SKIN_BUTTON_MIDDLE_DEFAULT");
    Image *defaultButtonMiddle2 = this->buttonMiddle;
    if(defaultButtonMiddle != nullptr)
        this->defaultButtonMiddle = defaultButtonMiddle;
    else if(defaultButtonMiddle2 != nullptr)
        this->defaultButtonMiddle = defaultButtonMiddle2;

    Image *defaultButtonRight = resourceManager->getImage("SKIN_BUTTON_RIGHT_DEFAULT");
    Image *defaultButtonRight2 = this->buttonRight;
    if(defaultButtonRight != nullptr)
        this->defaultButtonRight = defaultButtonRight;
    else if(defaultButtonRight2 != nullptr)
        this->defaultButtonRight = defaultButtonRight2;

    // print some debug info
    debugLog("Skin: Version {:f}\n", this->fVersion);
    debugLog("Skin: HitCircleOverlap = {:d}\n", this->iHitCircleOverlap);

    // delayed error notifications due to resource loading potentially blocking engine time
    if(!parseSkinIni1Status && parseSkinIni2Status && cv::skin.getString() != "default")
        osu->getNotificationOverlay()->addNotification("Error: Couldn't load skin.ini!", 0xffff0000);
    else if(!parseSkinIni2Status)
        osu->getNotificationOverlay()->addNotification("Error: Couldn't load DEFAULT skin.ini!!!", 0xffff0000);

    resourceManager->resetSyncLoadMaxBatchSize();
}

void Skin::loadBeatmapOverride(const std::string & /*filepath*/) {
    // debugLog("Skin::loadBeatmapOverride( {:s} )\n", filepath.c_str());
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
    std::vector<u8> rawData;
    size_t fileSize{0};
    {
        File file(filepath);
        if(!file.canRead() || !(fileSize = file.getFileSize())) {
            debugLog("OsuSkin Error: Couldn't load {:s}\n", filepath);
            return false;
        }
        rawData = file.takeFileBuffer();
        // close the file here
    }

    UString fileContent;

    // check for UTF-16 LE BOM and convert if needed
    if(fileSize >= 2 && rawData[0] == 0xFF && rawData[1] == 0xFE) {
        // convert UTF-16 LE to UTF-8
        std::string utf8Result;
        utf8Result.reserve(fileSize);

        size_t utf16Size = fileSize - 2;  // skip BOM
        const u8 *utf16Data = rawData.data() + 2;

        for(size_t i = 0; i < utf16Size - 1; i += 2) {
            uint16_t utf16Char = utf16Data[i] | (utf16Data[i + 1] << 8);

            if(utf16Char < 0x80)
                utf8Result += static_cast<char>(utf16Char);
            else if(utf16Char < 0x800) {
                utf8Result += static_cast<char>(0xC0 | (utf16Char >> 6));
                utf8Result += static_cast<char>(0x80 | (utf16Char & 0x3F));
            } else {
                utf8Result += static_cast<char>(0xE0 | (utf16Char >> 12));
                utf8Result += static_cast<char>(0x80 | ((utf16Char >> 6) & 0x3F));
                utf8Result += static_cast<char>(0x80 | (utf16Char & 0x3F));
            }
        }

        fileContent = UString(utf8Result.c_str());
    } else {
        // assume UTF-8/ASCII
        fileContent = UString(reinterpret_cast<char *>(rawData.data()), static_cast<int>(fileSize));
    }

    // process content line by line, handling different line endings
    int nonEmptyLineCounter = 0;

    enum class SkinSection {
        GENERAL,
        COLOURS,
        FONTS,
        NEOSU,
    };

    // osu! defaults to [General] and loads properties even before the actual section start
    SkinSection curBlock = SkinSection::GENERAL;

    UString currentLine;
    for(int i = 0; i < fileContent.length(); i++) {
        wchar_t ch = fileContent[i];

        // accumulate characters until we hit a line ending
        if(ch != L'\r' && ch != L'\n') {
            currentLine += ch;
            continue;
        }

        // handle CRLF, CR, or LF
        if(ch == L'\r' && i + 1 < fileContent.length() && fileContent[i + 1] == L'\n') i++;  // skip LF in CRLF

        std::string curLine{currentLine.toUtf8()};
        currentLine.clear();  // reset for next line

        if(!curLine.empty()) nonEmptyLineCounter++;

        // skip comments
        size_t firstNonSpace = curLine.find_first_not_of(" \t");
        if(firstNonSpace != std::string::npos && curLine.substr(firstNonSpace).starts_with("//")) continue;

        // section detection
        if(curLine.find("[General]") != std::string::npos)
            curBlock = SkinSection::GENERAL;
        else if(curLine.find("[Colours]") != std::string::npos || curLine.find("[Colors]") != std::string::npos)
            curBlock = SkinSection::COLOURS;
        else if(curLine.find("[Fonts]") != std::string::npos)
            curBlock = SkinSection::FONTS;
        else if(curLine.find("[neosu]") != std::string::npos)
            curBlock = SkinSection::NEOSU;

        auto curLineChar = curLine.c_str();

        switch(curBlock) {
            case SkinSection::GENERAL: {
                std::string version;
                if(Parsing::parse(curLineChar, "Version", ':', &version)) {
                    if((version.find("latest") != std::string::npos) || (version.find("User") != std::string::npos)) {
                        this->fVersion = 2.5f;
                    } else {
                        Parsing::parse(curLineChar, "Version", ':', &this->fVersion);
                    }
                }

                Parsing::parse(curLineChar, "CursorRotate", ':', &this->bCursorRotate);
                Parsing::parse(curLineChar, "CursorCentre", ':', &this->bCursorCenter);
                Parsing::parse(curLineChar, "CursorExpand", ':', &this->bCursorExpand);
                Parsing::parse(curLineChar, "LayeredHitSounds", ':', &this->bLayeredHitSounds);
                Parsing::parse(curLineChar, "SliderBallFlip", ':', &this->bSliderBallFlip);
                Parsing::parse(curLineChar, "AllowSliderBallTint", ':', &this->bAllowSliderBallTint);
                Parsing::parse(curLineChar, "HitCircleOverlayAboveNumber", ':', &this->bHitCircleOverlayAboveNumber);

                // https://osu.ppy.sh/community/forums/topics/314209
                Parsing::parse(curLineChar, "HitCircleOverlayAboveNumer", ':', &this->bHitCircleOverlayAboveNumber);

                if(Parsing::parse(curLineChar, "SliderStyle", ':', &this->iSliderStyle)) {
                    if(this->iSliderStyle != 1 && this->iSliderStyle != 2) this->iSliderStyle = 2;
                }

                if(Parsing::parse(curLineChar, "AnimationFramerate", ':', &this->fAnimationFramerate)) {
                    if(this->fAnimationFramerate < 0.f) this->fAnimationFramerate = 0.f;
                }

                break;
            }

            case SkinSection::COLOURS: {
                u8 comboNum;
                u8 r, g, b;

                // FIXME: actually use comboNum for ordering
                if(Parsing::parse(curLineChar, "Combo", &comboNum, ':', &r, ',', &g, ',', &b)) {
                    if(comboNum >= 1 && comboNum <= 8) {
                        this->comboColors.push_back(rgb(r, g, b));
                    }
                } else if(Parsing::parse(curLineChar, "SpinnerApproachCircle", ':', &r, ',', &g, ',', &b))
                    this->spinnerApproachCircleColor = rgb(r, g, b);
                else if(Parsing::parse(curLineChar, "SliderBall", ':', &r, ',', &g, ',', &b))
                    this->sliderBallColor = rgb(r, g, b);
                else if(Parsing::parse(curLineChar, "SliderBorder", ':', &r, ',', &g, ',', &b))
                    this->sliderBorderColor = rgb(r, g, b);
                else if(Parsing::parse(curLineChar, "SliderTrackOverride", ':', &r, ',', &g, ',', &b)) {
                    this->sliderTrackOverride = rgb(r, g, b);
                    this->bSliderTrackOverride = true;
                } else if(Parsing::parse(curLineChar, "SongSelectActiveText", ':', &r, ',', &g, ',', &b))
                    this->songSelectActiveText = rgb(r, g, b);
                else if(Parsing::parse(curLineChar, "SongSelectInactiveText", ':', &r, ',', &g, ',', &b))
                    this->songSelectInactiveText = rgb(r, g, b);
                else if(Parsing::parse(curLineChar, "InputOverlayText", ':', &r, ',', &g, ',', &b))
                    this->inputOverlayText = rgb(r, g, b);

                break;
            }

            case SkinSection::FONTS: {
                Parsing::parse(curLineChar, "ComboOverlap", ':', &this->iComboOverlap);
                Parsing::parse(curLineChar, "ScoreOverlap", ':', &this->iScoreOverlap);
                Parsing::parse(curLineChar, "HitCircleOverlap", ':', &this->iHitCircleOverlap);

                if(Parsing::parse(curLineChar, "ComboPrefix", ':', &this->sComboPrefix)) {
                    // XXX: jank path normalization
                    for(int i = 0; i < this->sComboPrefix.length(); i++) {
                        if(this->sComboPrefix[i] == '\\') {
                            this->sComboPrefix.erase(i, 1);
                            this->sComboPrefix.insert(i, "/");
                        }
                    }
                }

                if(Parsing::parse(curLineChar, "ScorePrefix", ':', &this->sScorePrefix)) {
                    // XXX: jank path normalization
                    for(int i = 0; i < this->sScorePrefix.length(); i++) {
                        if(this->sScorePrefix[i] == '\\') {
                            this->sScorePrefix.erase(i, 1);
                            this->sScorePrefix.insert(i, "/");
                        }
                    }
                }

                if(Parsing::parse(curLineChar, "HitCirclePrefix", ':', &this->sHitCirclePrefix)) {
                    // XXX: jank path normalization
                    for(int i = 0; i < this->sHitCirclePrefix.length(); i++) {
                        if(this->sHitCirclePrefix[i] == '\\') {
                            this->sHitCirclePrefix.erase(i, 1);
                            this->sHitCirclePrefix.insert(i, "/");
                        }
                    }
                }

                break;
            }

            case SkinSection::NEOSU: {
                size_t pos = curLine.find(':');
                if(pos == std::string::npos) break;

                std::string name, value;
                Parsing::parse(curLine.substr(0, pos), &name);
                Parsing::parse(curLine.substr(pos + 1), &value);

                // XXX: shouldn't be setting cvars directly in parsing method
                auto cvar = cvars->getConVarByName(name, false);
                if(cvar) {
                    cvar->setValue(value, true, ConVar::CvarEditor::SKIN);
                } else {
                    debugLog("Skin wanted to set cvar '{}' to '{}', but it doesn't exist!", name, value);
                }

                break;
            }
        }
    }

    // process the last line if it doesn't end with a line terminator
    if(!currentLine.isEmpty()) nonEmptyLineCounter++;

    if(nonEmptyLineCounter < 1) return false;

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

void Skin::checkLoadImage(Image **addressOfPointer, const std::string &skinElementName, const std::string &resourceName,
                          bool ignoreDefaultSkin, const std::string &fileExtension, bool forceLoadMipmaps) {
    if(*addressOfPointer != MISSING_TEXTURE) return;  // we are already loaded

    // NOTE: only the default skin is loaded with a resource name (it must never be unloaded by other instances), and it
    // is NOT added to the resources vector

    std::string defaultFilePath1 = MCENGINE_DATA_DIR "materials/default/";
    defaultFilePath1.append(skinElementName);
    defaultFilePath1.append("@2x.");
    defaultFilePath1.append(fileExtension);

    std::string defaultFilePath2 = MCENGINE_DATA_DIR "materials/default/";
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

                *addressOfPointer = resourceManager->loadImageAbs(defaultFilePath1, defaultResourceName,
                                                                  cv::skin_mipmaps.getBool() || forceLoadMipmaps);
            } else {
                // fallback to @1x
                if(existsDefaultFilePath2) {
                    std::string defaultResourceName = resourceName;
                    defaultResourceName.append("_DEFAULT");  // so we don't load the default skin twice

                    if(cv::skin_async.getBool()) resourceManager->requestNextLoadAsync();

                    *addressOfPointer = resourceManager->loadImageAbs(defaultFilePath2, defaultResourceName,
                                                                      cv::skin_mipmaps.getBool() || forceLoadMipmaps);
                }
            }
        }

        // load user skin
        if(existsFilepath1) {
            if(cv::skin_async.getBool()) resourceManager->requestNextLoadAsync();

            *addressOfPointer =
                resourceManager->loadImageAbs(filepath1, "", cv::skin_mipmaps.getBool() || forceLoadMipmaps);
            this->resources.push_back(*addressOfPointer);

            // export
            if(existsFilepath1) this->filepathsForExport.push_back(filepath1);
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

            *addressOfPointer = resourceManager->loadImageAbs(defaultFilePath2, defaultResourceName,
                                                              cv::skin_mipmaps.getBool() || forceLoadMipmaps);
        }
    }

    // load user skin
    if(existsFilepath2) {
        if(cv::skin_async.getBool()) resourceManager->requestNextLoadAsync();

        *addressOfPointer =
            resourceManager->loadImageAbs(filepath2, "", cv::skin_mipmaps.getBool() || forceLoadMipmaps);
        this->resources.push_back(*addressOfPointer);
    }

    // export
    if(existsFilepath1) this->filepathsForExport.push_back(filepath1);
    if(existsFilepath2) this->filepathsForExport.push_back(filepath2);

    if(!existsFilepath1 && !existsFilepath2) {
        if(existsDefaultFilePath1) this->filepathsForExport.push_back(defaultFilePath1);
        if(existsDefaultFilePath2) this->filepathsForExport.push_back(defaultFilePath2);
    }
}

void Skin::loadSound(Sound *&sndRef, const std::string &skinElementName, std::string resourceName, bool isOverlayable,
                     bool isSample, bool loop, bool fallback_to_default) {
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
                if(default_skin && existing_sound) return existing_sound;

                was_first_load = true;

                // user skin, destroy old sound resource (blocking)
                if(existing_sound) resourceManager->destroyResource(existing_sound, true);

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
        std::string defaultpath = MCENGINE_DATA_DIR "materials/default/";
        std::string defaultResourceName = std::move(resourceName);
        defaultResourceName.append("_DEFAULT");
        sndRef = try_load_sound(defaultpath, skinElementName, loop, defaultResourceName, true);
    }

    // failed both default and user
    if(sndRef == nullptr) {
        debugLog("Skin Warning: NULL sound {:s}!\n", skinElementName.c_str());
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

bool Skin::compareFilenameWithSkinElementName(const std::string &filename, const std::string &skinElementName) {
    if(filename.length() == 0 || skinElementName.length() == 0) return false;
    return filename.substr(0, filename.find_last_of('.')) == skinElementName;
}

Sound *Skin::getSpinnerBonus() const {
    //
    return this->spinnerBonus;
}

Sound *Skin::getSpinnerSpinSound() const {
    //
    return this->spinnerSpinSound;
}

Sound *Skin::getCombobreak() const {
    //
    return this->combobreak;
}

Sound *Skin::getFailsound() const {
    //
    return this->failsound;
}

Sound *Skin::getApplause() const {
    //
    return this->applause;
}

Sound *Skin::getMenuHit() const {
    //
    return this->menuHit;
}

Sound *Skin::getMenuHover() const {
    //
    return this->menuHover;
}

Sound *Skin::getCheckOn() const {
    //
    return this->checkOn;
}

Sound *Skin::getCheckOff() const {
    //
    return this->checkOff;
}

Sound *Skin::getShutter() const {
    //
    return this->shutter;
}

Sound *Skin::getSectionPassSound() const {
    //
    return this->sectionPassSound;
}

Sound *Skin::getSectionFailSound() const {
    //
    return this->sectionFailSound;
}

Sound *Skin::getExpandSound() const {
    //
    return this->expand;
}

Sound *Skin::getMessageSentSound() const {
    //
    return this->messageSent;
}

Sound *Skin::getDeletingTextSound() const {
    //
    return this->deletingText;
}

Sound *Skin::getMovingTextCursorSound() const {
    //
    return this->movingTextCursor;
}

Sound *Skin::getTyping1Sound() const {
    //
    return this->typing1;
}

Sound *Skin::getTyping2Sound() const {
    //
    return this->typing2;
}

Sound *Skin::getTyping3Sound() const {
    //
    return this->typing3;
}

Sound *Skin::getTyping4Sound() const {
    //
    return this->typing4;
}

Sound *Skin::getMenuBackSound() const {
    //
    return this->menuBack;
}

Sound *Skin::getCloseChatTabSound() const {
    //
    return this->closeChatTab;
}

Sound *Skin::getHoverButtonSound() const {
    //
    return this->hoverButton;
}

Sound *Skin::getClickButtonSound() const {
    //
    return this->clickButton;
}

Sound *Skin::getClickMainMenuCubeSound() const {
    //
    return this->clickMainMenuCube;
}

Sound *Skin::getHoverMainMenuCubeSound() const {
    //
    return this->hoverMainMenuCube;
}

Sound *Skin::getClickSingleplayerSound() const {
    //
    return this->clickSingleplayer;
}

Sound *Skin::getHoverSingleplayerSound() const {
    //
    return this->hoverSingleplayer;
}

Sound *Skin::getClickMultiplayerSound() const {
    //
    return this->clickMultiplayer;
}

Sound *Skin::getHoverMultiplayerSound() const {
    //
    return this->hoverMultiplayer;
}

Sound *Skin::getClickOptionsSound() const {
    //
    return this->clickOptions;
}

Sound *Skin::getHoverOptionsSound() const {
    //
    return this->hoverOptions;
}

Sound *Skin::getClickExitSound() const {
    //
    return this->clickExit;
}

Sound *Skin::getHoverExitSound() const {
    //
    return this->hoverExit;
}

Sound *Skin::getPauseLoopSound() const {
    //
    return this->pauseLoop;
}

Sound *Skin::getPauseHoverSound() const {
    //
    return this->pauseHover;
}

Sound *Skin::getClickPauseBackSound() const {
    //
    return this->clickPauseBack;
}

Sound *Skin::getHoverPauseBackSound() const {
    //
    return this->hoverPauseBack;
}

Sound *Skin::getClickPauseContinueSound() const {
    //
    return this->clickPauseContinue;
}

Sound *Skin::getHoverPauseContinueSound() const {
    //
    return this->hoverPauseContinue;
}

Sound *Skin::getClickPauseRetrySound() const {
    //
    return this->clickPauseRetry;
}

Sound *Skin::getHoverPauseRetrySound() const {
    //
    return this->hoverPauseRetry;
}

Sound *Skin::getBackButtonClickSound() const {
    //
    return this->backButtonClick;
}

Sound *Skin::getBackButtonHoverSound() const {
    //
    return this->backButtonHover;
}

Sound *Skin::getSelectDifficultySound() const {
    //
    return this->selectDifficulty;
}

Sound *Skin::getSliderbarSound() const {
    //
    return this->sliderbar;
}

Sound *Skin::getMatchConfirmSound() const {
    //
    return this->matchConfirm;
}

Sound *Skin::getRoomJoinedSound() const {
    //
    return this->roomJoined;
}

Sound *Skin::getRoomQuitSound() const {
    //
    return this->roomQuit;
}

Sound *Skin::getRoomNotReadySound() const {
    //
    return this->roomNotReady;
}

Sound *Skin::getRoomReadySound() const {
    //
    return this->roomReady;
}

Sound *Skin::getMatchStartSound() const {
    //
    return this->matchStart;
}
