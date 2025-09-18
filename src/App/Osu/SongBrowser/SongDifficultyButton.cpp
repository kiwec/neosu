// Copyright (c) 2016, PG, All rights reserved.
#include "SongDifficultyButton.h"

#include <utility>

#include "ScoreButton.h"
#include "SongBrowser.h"
// ---

#include "AnimationHandler.h"
#include "BackgroundImageHandler.h"
#include "BeatmapInterface.h"
#include "ConVar.h"
#include "Database.h"
#include "DatabaseBeatmap.h"
#include "Engine.h"
#include "LegacyReplay.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "Skin.h"
#include "SoundEngine.h"
#include "Timing.h"

SongDifficultyButton::SongDifficultyButton(SongBrowser* songBrowser, UIContextMenu* contextMenu, float xPos, float yPos,
                                           float xSize, float ySize, UString name, DatabaseBeatmap* map,
                                           SongButton* parentSongButton)
    : SongButton(songBrowser, contextMenu, xPos, yPos, xSize, ySize, std::move(name), nullptr) {
    this->databaseBeatmap = map;  // NOTE: can't use parent constructor for passing this argument, as it would
                                    // otherwise try to build a full button (and not just a diff button)
    this->parentSongButton = parentSongButton;

    this->sMapper = this->databaseBeatmap->getCreator();
    this->sDiff = this->databaseBeatmap->getDifficultyName();

    this->fDiffScale = 0.18f;
    this->fOffsetPercentAnim = (this->parentSongButton != nullptr ? 1.0f : 0.0f);

    this->bUpdateGradeScheduled = true;
    this->bPrevOffsetPercentSelectionState = this->isIndependentDiffButton();

    // settings
    this->setHideIfSelected(false);

    this->updateLayoutEx();
    this->updateGrade();
}

SongDifficultyButton::~SongDifficultyButton() { anim->deleteExistingAnimation(&this->fOffsetPercentAnim); }

void SongDifficultyButton::draw() {
    CarouselButton::draw();
    if(!this->bVisible) return;

    const bool isIndependentDiff = this->isIndependentDiffButton();

    Skin* skin = osu->getSkin();

    // scaling
    const vec2 pos = this->getActualPos();
    const vec2 size = this->getActualSize();

    // draw background image
    this->drawBeatmapBackgroundThumbnail(
        osu->getBackgroundImageHandler()->getLoadBackgroundImage(this->databaseBeatmap));

    if(this->grade != FinishedScore::Grade::N) this->drawGrade();

    this->sTitle = this->databaseBeatmap->getTitle();
    this->drawTitle(!isIndependentDiff ? 0.2f : 1.0f);
    this->sArtist = this->databaseBeatmap->getArtist();
    this->drawSubTitle(!isIndependentDiff ? 0.2f : 1.0f);

    // draw diff name
    const float titleScale = (size.y * this->fTitleScale) / this->font->getHeight();
    const float subTitleScale = (size.y * this->fSubTitleScale) / this->font->getHeight();
    const float diffScale = (size.y * this->fDiffScale) / this->fontBold->getHeight();
    g->setColor(this->bSelected ? skin->getSongSelectActiveText() : skin->getSongSelectInactiveText());
    g->pushTransform();
    {
        g->scale(diffScale, diffScale);
        g->translate(pos.x + this->fTextOffset,
                     pos.y + size.y * this->fTextMarginScale + this->font->getHeight() * titleScale +
                         size.y * this->fTextSpacingScale + this->font->getHeight() * subTitleScale * 0.85f +
                         size.y * this->fTextSpacingScale + this->fontBold->getHeight() * diffScale * 0.8f);
        g->drawString(this->fontBold, this->sDiff.c_str());
    }
    g->popTransform();

    // draw stars
    // NOTE: stars can sometimes be infinity! (e.g. broken osu!.db database)
    float stars = this->databaseBeatmap->getStarsNomod();
    if(stars > 0) {
        const float starOffsetY = (size.y * 0.85);
        const float starWidth = (size.y * 0.2);
        const float starScale = starWidth / skin->getStar()->getHeight();
        const int numFullStars = std::clamp<int>((int)stars, 0, 25);
        const float partialStarScale =
            std::max(0.5f, std::clamp<float>(stars - numFullStars, 0.0f, 1.0f));  // at least 0.5x

        g->setColor(this->bSelected ? skin->getSongSelectActiveText() : skin->getSongSelectInactiveText());

        // full stars
        for(int i = 0; i < numFullStars; i++) {
            const float scale =
                std::min(starScale * 1.175f, starScale + i * 0.015f);  // more stars = getting bigger, up to a limit

            g->pushTransform();
            {
                g->scale(scale, scale);
                g->translate(pos.x + this->fTextOffset + starWidth / 2 + i * starWidth * 1.75f, pos.y + starOffsetY);
                g->drawImage(skin->getStar());
            }
            g->popTransform();
        }

        // partial star
        g->pushTransform();
        {
            g->scale(starScale * partialStarScale, starScale * partialStarScale);
            g->translate(pos.x + this->fTextOffset + starWidth / 2 + numFullStars * starWidth * 1.75f,
                         pos.y + starOffsetY);
            g->drawImage(skin->getStar());
        }
        g->popTransform();

        // fill leftover space up to 10 stars total (background stars)
        g->setColor(0x1effffff);
        const float backgroundStarScale = 0.6f;

        g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_ADDITIVE);
        {
            for(int i = (numFullStars + 1); i < 10; i++) {
                g->pushTransform();
                {
                    g->scale(starScale * backgroundStarScale, starScale * backgroundStarScale);
                    g->translate(pos.x + this->fTextOffset + starWidth / 2 + i * starWidth * 1.75f,
                                 pos.y + starOffsetY);
                    g->drawImage(skin->getStar());
                }
                g->popTransform();
            }
        }
        g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_ALPHA);
    }
}

void SongDifficultyButton::mouse_update(bool* propagate_clicks) {
    if(!this->bVisible) return;
    SongButton::mouse_update(propagate_clicks);

    // dynamic settings (moved from constructor to here)
    const bool newOffsetPercentSelectionState = (this->bSelected || !this->isIndependentDiffButton());
    if(newOffsetPercentSelectionState != this->bPrevOffsetPercentSelectionState) {
        this->bPrevOffsetPercentSelectionState = newOffsetPercentSelectionState;
        anim->moveQuadOut(&this->fOffsetPercentAnim, newOffsetPercentSelectionState ? 1.0f : 0.0f,
                          0.25f * (1.0f - this->fOffsetPercentAnim), true);
    }
    this->setOffsetPercent(std::lerp(0.0f, 0.075f, this->fOffsetPercentAnim));

    if(this->bUpdateGradeScheduled) {
        this->bUpdateGradeScheduled = false;
        this->updateGrade();
    }
}

void SongDifficultyButton::onClicked(bool left, bool right) {
    soundEngine->play(osu->getSkin()->getSelectDifficultySound());

    // NOTE: Intentionally not calling Button::onClicked(left, right), since that one plays another sound
    CBaseUIButton::onClicked(left, right);

    this->select(true, true);
}

void SongDifficultyButton::onSelected(bool wasSelected, bool autoSelectBottomMostChild, bool wasParentSelected) {
    CarouselButton::onSelected(wasSelected, autoSelectBottomMostChild, wasParentSelected);

    const bool wasParentActuallySelected = (this->parentSongButton != nullptr && wasParentSelected);

    this->updateGrade();

    if(!wasParentActuallySelected) this->songBrowser->requestNextScrollToSongButtonJumpFix(this);

    this->songBrowser->onSelectionChange(this, true);
    this->songBrowser->onDifficultySelected(this->databaseBeatmap, wasSelected);
    this->songBrowser->scrollToSongButton(this);
}

void SongDifficultyButton::updateGrade() {
    if(!cv::scores_enabled.getBool()) {
        return;
    }

    std::scoped_lock lock(db->scores_mtx);
    if(!db->getScores()) {
        return;
    }
    const auto& db_scores = db->getScores();
    for(const auto& score : (*db_scores)[this->databaseBeatmap->getMD5Hash()]) {
        if(score.grade < this->grade) {
            this->grade = score.grade;

            if(this->parentSongButton != nullptr && this->parentSongButton->grade > this->grade) {
                this->parentSongButton->grade = this->grade;
            }
        }
    }
}

bool SongDifficultyButton::isIndependentDiffButton() const {
    return (this->parentSongButton == nullptr || !this->parentSongButton->isSelected());
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
