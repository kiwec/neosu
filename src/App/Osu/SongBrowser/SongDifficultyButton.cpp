// Copyright (c) 2016, PG, All rights reserved.
#include "SongDifficultyButton.h"

#include <utility>

#include "Font.h"
#include "ScoreButton.h"
#include "SongBrowser.h"
// ---

#include "AnimationHandler.h"
#include "BackgroundImageHandler.h"
#include "BeatmapInterface.h"
#include "OsuConVars.h"
#include "Database.h"
#include "DatabaseBeatmap.h"
#include "Engine.h"
#include "LegacyReplay.h"
#include "Osu.h"
#include "Skin.h"
#include "SoundEngine.h"

SongDifficultyButton::SongDifficultyButton(UIContextMenu* contextMenu, float xPos, float yPos, float xSize, float ySize,
                                           UString name, BeatmapDifficulty* diff, SongButton* parentSongButton)
    : SongButton(contextMenu, xPos, yPos, xSize, ySize, std::move(name)) {
    // must exist and be a difficulty
    assert(diff && diff->getDifficulties().empty());

    this->databaseBeatmap = diff;
    this->parentSongButton = parentSongButton;

    this->sMapper = this->databaseBeatmap->getCreator();
    this->sDiff = this->databaseBeatmap->getDifficultyName();

    const bool independentDiff = this->isIndependentDiffButton();

    this->fDiffScale = 0.18f;
    this->fOffsetPercentAnim = independentDiff ? 1.0f : 0.f;

    this->bUpdateGradeScheduled = true;
    this->bPrevOffsetPercentSelectionState = independentDiff;

    // settings
    this->setHideIfSelected(false);

    this->updateLayoutEx();
    this->updateGrade();
}

SongDifficultyButton::~SongDifficultyButton() { anim::deleteExistingAnimation(&this->fOffsetPercentAnim); }

void SongDifficultyButton::draw() {
    if(!this->bVisible || this->vPos.y + this->vSize.y < 0 || this->vPos.y > osu->getVirtScreenHeight()) {
        return;
    }
    CarouselButton::draw();

    const bool isIndependentDiff = this->isIndependentDiffButton();

    const auto& skin = osu->getSkin();

    // scaling
    const vec2 pos = this->getActualPos();
    const vec2 size = this->getActualSize();

    // draw background image
    // delay requesting the image itself a bit
    if(this->fVisibleFor >= ((std::clamp<f32>(cv::background_image_loading_delay.getFloat(), 0.f, 2.f)) / 4.f)) {
        this->drawBeatmapBackgroundThumbnail(
            osu->getBackgroundImageHandler()->getLoadBackgroundImage(this->databaseBeatmap));
    }

    if(this->grade != ScoreGrade::N) this->drawGrade();

    this->sTitle = this->databaseBeatmap->getTitle();
    this->drawTitle(!isIndependentDiff ? 0.2f : 1.0f);
    this->sArtist = this->databaseBeatmap->getArtist();
    this->drawSubTitle(!isIndependentDiff ? 0.2f : 1.0f);

    // draw diff name
    const float titleScale = (size.y * this->fTitleScale) / this->font->getHeight();
    const float subTitleScale = (size.y * this->fSubTitleScale) / this->font->getHeight();
    const float diffScale = (size.y * this->fDiffScale) / this->fontBold->getHeight();
    g->setColor(this->bSelected ? skin->c_song_select_active_text : skin->c_song_select_inactive_text);
    g->pushTransform();
    {
        g->scale(diffScale, diffScale);
        g->translate(pos.x + this->fTextOffset,
                     pos.y + size.y * this->fTextMarginScale + this->font->getHeight() * titleScale +
                         size.y * this->fTextSpacingScale + this->font->getHeight() * subTitleScale * 0.85f +
                         size.y * this->fTextSpacingScale + this->fontBold->getHeight() * diffScale * 0.8f);
        g->drawString(this->fontBold, this->sDiff);
    }
    g->popTransform();

    // draw stars
    // NOTE: stars can sometimes be infinity! (e.g. broken osu!.db database)
    float stars = this->databaseBeatmap->getStarsNomod();
    if(stars > 0) {
        const float starOffsetY = (size.y * 0.85);
        const float starWidth = (size.y * 0.2);
        const float starScale = starWidth / skin->i_star->getHeight();
        const int numFullStars = std::clamp<int>((int)stars, 0, 25);
        const float partialStarScale =
            std::max(0.5f, std::clamp<float>(stars - numFullStars, 0.0f, 1.0f));  // at least 0.5x

        g->setColor(this->bSelected ? skin->c_song_select_active_text : skin->c_song_select_inactive_text);

        // full stars
        for(int i = 0; i < numFullStars; i++) {
            const float scale =
                std::min(starScale * 1.175f, starScale + i * 0.015f);  // more stars = getting bigger, up to a limit

            g->pushTransform();
            {
                g->scale(scale, scale);
                g->translate(pos.x + this->fTextOffset + starWidth / 2 + i * starWidth * 1.75f, pos.y + starOffsetY);
                g->drawImage(skin->i_star);
            }
            g->popTransform();
        }

        // partial star
        g->pushTransform();
        {
            g->scale(starScale * partialStarScale, starScale * partialStarScale);
            g->translate(pos.x + this->fTextOffset + starWidth / 2 + numFullStars * starWidth * 1.75f,
                         pos.y + starOffsetY);
            g->drawImage(skin->i_star);
        }
        g->popTransform();

        // fill leftover space up to 10 stars total (background stars)
        g->setColor(0x1effffff);
        const float backgroundStarScale = 0.6f;

        g->setBlendMode(DrawBlendMode::BLEND_MODE_ADDITIVE);
        {
            for(int i = (numFullStars + 1); i < 10; i++) {
                g->pushTransform();
                {
                    g->scale(starScale * backgroundStarScale, starScale * backgroundStarScale);
                    g->translate(pos.x + this->fTextOffset + starWidth / 2 + i * starWidth * 1.75f,
                                 pos.y + starOffsetY);
                    g->drawImage(skin->i_star);
                }
                g->popTransform();
            }
        }
        g->setBlendMode(DrawBlendMode::BLEND_MODE_ALPHA);
    }
}

void SongDifficultyButton::mouse_update(bool* propagate_clicks) {
    if(!this->bVisible) {
        this->fVisibleFor = 0.f;
        return;
    }
    CarouselButton::mouse_update(propagate_clicks);

    this->fVisibleFor += engine->getFrameTime();

    // dynamic settings (moved from constructor to here)
    const bool newOffsetPercentSelectionState = (this->bSelected || !this->isIndependentDiffButton());
    if(newOffsetPercentSelectionState != this->bPrevOffsetPercentSelectionState) {
        this->bPrevOffsetPercentSelectionState = newOffsetPercentSelectionState;
        anim::moveQuadOut(&this->fOffsetPercentAnim, newOffsetPercentSelectionState ? 1.0f : 0.0f,
                          0.25f * (1.0f - this->fOffsetPercentAnim), true);
    }
    this->setOffsetPercent(std::lerp(0.0f, 0.075f, this->fOffsetPercentAnim));

    if(this->bUpdateGradeScheduled) {
        this->bUpdateGradeScheduled = false;
        this->updateGrade();
    }
}

void SongDifficultyButton::onClicked(bool left, bool right) {
    soundEngine->play(osu->getSkin()->s_select_difficulty);

    // NOTE: Intentionally not calling Button::onClicked(left, right), since that one plays another sound
    CBaseUIButton::onClicked(left, right);

    this->select();
}

void SongDifficultyButton::onSelected(bool wasSelected, SelOpts opts) {
    CarouselButton::onSelected(wasSelected, opts);

    const bool wasParentActuallySelected = (!this->isIndependentDiffButton() && !!this->parentSongButton &&
                                            !(opts.parentUnselected) && this->parentSongButton->isSelected());

    this->updateGrade();

    // debugLog(
    //     "wasParentActuallySelected {} isIndependentDiffButton {} parentSongButton {} wasParentSelected {} "
    //     "parentSongButton->isSelected {}",
    //     wasParentSelected, isIndependentDiffButton(), !!parentSongButton, wasParentSelected,
    //     parentSongButton->isSelected());

    auto* sb = osu->getSongBrowser();
    if(!wasParentActuallySelected) {
        // debugLog("running scroll jump fix for {}",
        //          this->databaseBeatmap ? this->databaseBeatmap->getFilePath() : "???");
        sb->requestNextScrollToSongButtonJumpFix(this);
    }

    sb->onSelectionChange(this, true);
    sb->onDifficultySelected(this->databaseBeatmap, wasSelected);
    sb->scrollToSongButton(this);
}

void SongDifficultyButton::updateGrade() {
    if(!cv::scores_enabled.getBool()) {
        return;
    }

    Sync::shared_lock lock(db->scores_mtx);
    const auto& dbScoreIt = db->getScores().find(this->databaseBeatmap->getMD5());
    if(dbScoreIt == db->getScores().end()) {
        return;
    }

    for(const auto& score : dbScoreIt->second) {
        if(score.grade < this->grade) {
            this->grade = score.grade;

            if(this->parentSongButton != nullptr && this->parentSongButton->grade > this->grade) {
                this->parentSongButton->grade = this->grade;
            }
        }
    }
}

bool SongDifficultyButton::isIndependentDiffButton() const {
    if(!this->parentSongButton->isSelected()) return true;

    // check if this is the only visible sibling
    int visibleSiblings = 0;
    for(const auto& sibling : this->parentSongButton->getChildren()) {
        if(sibling->isSearchMatch()) {
            visibleSiblings++;
            if(visibleSiblings > 1) return false;  // early exit
        }
    }

    return (visibleSiblings == 1);
}

Color SongDifficultyButton::getInactiveBackgroundColor() const {
    if(this->isIndependentDiffButton())
        return SongButton::getInactiveBackgroundColor();
    else
        return argb(std::clamp<int>(cv::songbrowser_button_difficulty_inactive_color_a.getInt(), 0, 255),
                    std::clamp<int>(cv::songbrowser_button_difficulty_inactive_color_r.getInt(), 0, 255),
                    std::clamp<int>(cv::songbrowser_button_difficulty_inactive_color_g.getInt(), 0, 255),
                    std::clamp<int>(cv::songbrowser_button_difficulty_inactive_color_b.getInt(), 0, 255));
}
