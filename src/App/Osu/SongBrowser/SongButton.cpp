// Copyright (c) 2016, PG, All rights reserved.
#include "SongButton.h"

#include <algorithm>
#include <utility>

#include "Font.h"
#include "ScoreButton.h"
#include "SongBrowser.h"
#include "SongDifficultyButton.h"
#include "BeatmapCarousel.h"

// ---

#include "BackgroundImageHandler.h"
#include "Collections.h"
#include "OsuConVars.h"
#include "DatabaseBeatmap.h"
#include "MakeDelegateWrapper.h"
#include "Environment.h"
#include "Engine.h"
#include "Mouse.h"
#include "Osu.h"
#include "Skin.h"
#include "SkinImage.h"
#include "UIContextMenu.h"

// passthrough for SongDifficultyButton
SongButton::SongButton(UIContextMenu *contextMenu, float xPos, float yPos, float xSize, float ySize, UString name)
    : CarouselButton(contextMenu, xPos, yPos, xSize, ySize, std::move(name)) {}

SongButton::SongButton(UIContextMenu *contextMenu, float xPos, float yPos, float xSize, float ySize, UString name,
                       DatabaseBeatmap *databaseBeatmap)
    : SongButton(contextMenu, xPos, yPos, xSize, ySize, std::move(name)) {
    assert(databaseBeatmap && !databaseBeatmap->getDifficulties().empty());

    this->databaseBeatmap = databaseBeatmap;

    // settings
    this->setHideIfSelected(true);

    // build and add children
    const auto &diffs = this->databaseBeatmap->getDifficulties();

    const bool isDifficultySingular = diffs.size() == 1;
    this->children.reserve(diffs.size());
    for(auto &diff : diffs) {
        this->children.emplace_back(
            new SongDifficultyButton(this->contextMenu, 0, 0, 0, 0, "", diff.get(), this, isDifficultySingular));
    }

    this->updateLayoutEx();
}

SongButton::~SongButton() {
    for(auto &i : this->children) {
        delete i;
    }
}

void SongButton::draw() {
    if(!this->bVisible) {
        return;
    }

    CarouselButton::draw();

    const auto &carousel = osu->getSongBrowser()->carousel;

    // don't try to load images while right click scrolling to avoid lag
    if(!carousel->isActuallyRightClickScrolling() && this->representativeBeatmap &&
       // delay requesting the image itself a bit
       this->fVisibleFor >= ((std::clamp<f32>(cv::background_image_loading_delay.getFloat(), 0.f, 2.f)) / 4.f)) {
        // draw background image
        this->drawBeatmapBackgroundThumbnail(
            osu->getBackgroundImageHandler()->getLoadBackgroundImage(this->representativeBeatmap));
    }

    if(this->grade != ScoreGrade::N) this->drawGrade();
    this->drawTitle();
    this->drawSubTitle();
}

void SongButton::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible) {
        this->fVisibleFor = 0.f;
        return;
    }

    this->fVisibleFor += engine->getFrameTime();
    CarouselButton::mouse_update(propagate_clicks);

    if(this->children.empty()) return;

    // HACKHACK: calling these two every frame is a bit insane, but too lazy to write delta detection logic atm. (UI
    // desync is not a problem since parent buttons are invisible while selected, so no resorting happens in that state)
    this->sortChildren();

    SongDifficultyButton *bottomChild = nullptr;
    // use the bottom child (hardest diff, assuming default sorting, and respecting the current search matches)
    for(auto *child : this->childDiffBtns() | std::views::reverse) {
        // NOTE: if no search is active, then all search matches return true by default
        if(!child->isSearchMatch()) continue;
        bottomChild = child;
        break;
    }
    // no children visible
    if(!bottomChild) return;

    const auto *currentRepresentativeBeatmap = this->representativeBeatmap;
    auto *newRepresentativeBeatmap = bottomChild->getDatabaseBeatmap();

    if(currentRepresentativeBeatmap == nullptr || currentRepresentativeBeatmap != newRepresentativeBeatmap) {
        this->representativeBeatmap = newRepresentativeBeatmap;
    }
}

void SongButton::drawBeatmapBackgroundThumbnail(const Image *image) {
    if(!cv::draw_songbrowser_thumbnails.getBool() || osu->getSkin()->version < 2.2f) return;

    float alpha = 1.0f;
    if(cv::songbrowser_thumbnail_fade_in_duration.getFloat() > 0.0f) {
        if(image == nullptr || !image->isReady())
            this->fThumbnailFadeInTime = engine->getTime();
        else if(this->fThumbnailFadeInTime > 0.0f && engine->getTime() > this->fThumbnailFadeInTime) {
            alpha = std::clamp<float>((engine->getTime() - this->fThumbnailFadeInTime) /
                                          cv::songbrowser_thumbnail_fade_in_duration.getFloat(),
                                      0.0f, 1.0f);
            alpha = 1.0f - (1.0f - alpha) * (1.0f - alpha);
        }
    }

    if(image == nullptr || !image->isReady()) return;

    // scaling
    const vec2 pos = this->getActualPos();
    const vec2 size = this->getActualSize();

    const f32 thumbnailYRatio = osu->getSongBrowser()->thumbnailYRatio;
    const f32 beatmapBackgroundScale =
        Osu::getImageScaleToFillResolution(image, vec2(size.y * thumbnailYRatio, size.y)) * 1.05f;

    vec2 centerOffset = vec2((size.y * thumbnailYRatio) / 2.0f, size.y / 2.0f);
    McRect clipRect = McRect(pos.x - 2, pos.y + 1, (size.y * thumbnailYRatio) + 5, size.y + 2);

    g->setColor(argb(alpha, 1.f, 1.f, 1.f));
    g->pushTransform();
    {
        g->scale(beatmapBackgroundScale, beatmapBackgroundScale);
        g->translate(pos.x + centerOffset.x, pos.y + centerOffset.y);
        // draw with smooth edge clipping
        g->drawImage(image, {}, 1.f, clipRect);
    }
    g->popTransform();

    // debug cliprect bounding box
    if(cv::debug_osu.getBool()) {
        vec2 clipRectPos = vec2(clipRect.getX(), clipRect.getY() - 1);
        vec2 clipRectSize = vec2(clipRect.getWidth(), clipRect.getHeight());

        g->setColor(0xffffff00);
        g->drawLine(clipRectPos.x, clipRectPos.y, clipRectPos.x + clipRectSize.x, clipRectPos.y);
        g->drawLine(clipRectPos.x, clipRectPos.y, clipRectPos.x, clipRectPos.y + clipRectSize.y);
        g->drawLine(clipRectPos.x, clipRectPos.y + clipRectSize.y, clipRectPos.x + clipRectSize.x,
                    clipRectPos.y + clipRectSize.y);
        g->drawLine(clipRectPos.x + clipRectSize.x, clipRectPos.y, clipRectPos.x + clipRectSize.x,
                    clipRectPos.y + clipRectSize.y);
    }
}

void SongButton::drawGrade() {
    // scaling
    const vec2 pos = this->getActualPos();
    const vec2 size = this->getActualSize();

    const auto &gradeImg = osu->getSkin()->getGradeImageSmall(this->grade);
    g->pushTransform();
    {
        const float scale = this->calculateGradeScale();
        g->setColor(0xffffffff);
        gradeImg->drawRaw(vec2(pos.x + this->fGradeOffset, pos.y + size.y / 2), scale, AnchorPoint::LEFT);
    }
    g->popTransform();
}

void SongButton::drawTitle(float deselectedAlpha, bool forceSelectedStyle) {
    // scaling
    const vec2 pos = this->getActualPos();
    const vec2 size = this->getActualSize();

    const float titleScale = (size.y * this->fTitleScale) / this->font->getHeight();
    g->setColor((this->bSelected || forceSelectedStyle) ? osu->getSkin()->c_song_select_active_text
                                                        : osu->getSkin()->c_song_select_inactive_text);
    if(!(this->bSelected || forceSelectedStyle)) g->setAlpha(deselectedAlpha);

    const UString title{this->representativeBeatmap ? this->representativeBeatmap->getTitle()
                        : this->databaseBeatmap     ? this->databaseBeatmap->getTitle()
                                                    : ULITERAL("")};

    g->pushTransform();
    {
        g->scale(titleScale, titleScale);
        g->translate(pos.x + this->fTextOffset,
                     pos.y + size.y * this->fTextMarginScale + this->font->getHeight() * titleScale);
        g->drawString(this->font, title);
    }
    g->popTransform();
}

void SongButton::drawSubTitle(float deselectedAlpha, bool forceSelectedStyle) {
    // scaling
    const vec2 pos = this->getActualPos();
    const vec2 size = this->getActualSize();

    const float titleScale = (size.y * this->fTitleScale) / this->font->getHeight();
    const float subTitleScale = (size.y * this->fSubTitleScale) / this->font->getHeight();
    g->setColor((this->bSelected || forceSelectedStyle) ? osu->getSkin()->c_song_select_active_text
                                                        : osu->getSkin()->c_song_select_inactive_text);
    if(!(this->bSelected || forceSelectedStyle)) g->setAlpha(deselectedAlpha);

    const std::string &artist{this->representativeBeatmap
                                  ? this->representativeBeatmap->getTitle()
                                  : (this->databaseBeatmap ? this->databaseBeatmap->getTitle() : "")};

    const std::string &mapper{this->representativeBeatmap
                                  ? this->representativeBeatmap->getCreator()
                                  : (this->databaseBeatmap ? this->databaseBeatmap->getCreator() : "")};

    g->pushTransform();
    {
        const UString subTitleString{fmt::format("{} // {}", artist, mapper)};

        g->scale(subTitleScale, subTitleScale);
        g->translate(pos.x + this->fTextOffset,
                     pos.y + size.y * this->fTextMarginScale + this->font->getHeight() * titleScale +
                         size.y * this->fTextSpacingScale + this->font->getHeight() * subTitleScale * 0.85f);
        g->drawString(this->font, subTitleString);
    }
    g->popTransform();
}

bool SongButton::sortChildren() {
    if(this->bChildrenNeedSorting) {
        this->bChildrenNeedSorting = false;
        std::ranges::sort(this->children, SongBrowser::sort_by_difficulty);
        return true;
    } else {
        return false;
    }
}

void SongButton::updateLayoutEx() {
    CarouselButton::updateLayoutEx();

    // scaling
    const vec2 size = this->getActualSize();

    this->fTextOffset = 0.0f;
    this->fGradeOffset = 0.0f;

    if(this->grade != ScoreGrade::N) this->fTextOffset += this->calculateGradeWidth();

    if(osu->getSkin()->version < 2.2f) {
        this->fTextOffset += size.x * 0.02f * 2.0f;
    } else {
        const f32 thumbnailYRatio = osu->getSongBrowser()->thumbnailYRatio;
        this->fTextOffset += size.y * thumbnailYRatio + size.x * 0.02f;
        this->fGradeOffset += size.y * thumbnailYRatio + size.x * 0.0125f;
    }
}

void SongButton::onSelected(bool wasSelected, SelOpts opts) {
    CarouselButton::onSelected(wasSelected, opts);

    // resort children (since they might have been updated in the meantime)
    if(this->sortChildren()) {
        // update button positions so the resort is actually applied
        // XXX: we shouldn't be updating ALL of the buttons
        osu->getSongBrowser()->updateSongButtonLayout();
    }

    // update grade on child
    for(auto *child : this->childDiffBtns()) {
        child->updateGrade();
    }

    osu->getSongBrowser()->onSelectionChange(this, false);

    // now, automatically select the bottom child (hardest diff, assuming default sorting, and respecting the current
    // search matches)
    if(!opts.noSelectBottomChild) {
        for(auto *child : this->children | std::views::reverse) {
            // NOTE: if no search is active, then all search matches return true by default
            if(!child->isSearchMatch()) continue;
            SelOpts childOpts{.noSelectBottomChild = true, .parentUnselected = !wasSelected};
            child->select(childOpts);
            break;
        }
    }
}

void SongButton::onRightMouseUpInside() { this->triggerContextMenu(mouse->getPos()); }

void SongButton::triggerContextMenu(vec2 pos) {
    if(this->contextMenu != nullptr) {
        this->contextMenu->setPos(pos);
        this->contextMenu->setRelPos(pos);
        this->contextMenu->begin(0, true);
        {
            if(this->databaseBeatmap)
                this->contextMenu->addButtonJustified("[...] Open Beatmap Folder", TEXT_JUSTIFICATION::LEFT, 0)
                    ->setClickCallback(SA::MakeDelegate<&SongButton::onOpenBeatmapFolderClicked>(this));

            if(this->databaseBeatmap != nullptr && this->databaseBeatmap->getDifficulties().size() < 1)
                this->contextMenu->addButtonJustified("[+] Add to Collection", TEXT_JUSTIFICATION::LEFT, 1);

            this->contextMenu->addButtonJustified("[+Set] Add to Collection", TEXT_JUSTIFICATION::LEFT, 2);

            if(osu->getSongBrowser()->getGroupingMode() == SongBrowser::GroupType::COLLECTIONS) {
                CBaseUIButton *spacer = this->contextMenu->addButtonJustified("---", TEXT_JUSTIFICATION::CENTERED);
                spacer->setEnabled(false);
                spacer->setTextColor(0xff888888);
                spacer->setTextDarkColor(0xff000000);

                if(this->databaseBeatmap == nullptr || this->databaseBeatmap->getDifficulties().size() < 1) {
                    this->contextMenu->addButtonJustified("[-] Remove from Collection", TEXT_JUSTIFICATION::LEFT, 3);
                }

                this->contextMenu->addButtonJustified("[-Set] Remove from Collection", TEXT_JUSTIFICATION::LEFT, 4);
            }
        }
        this->contextMenu->end(false, false);
        this->contextMenu->setClickCallback(SA::MakeDelegate<&SongButton::onContextMenu>(this));
        UIContextMenu::clampToRightScreenEdge(this->contextMenu);
        UIContextMenu::clampToBottomScreenEdge(this->contextMenu);
    }
}

void SongButton::onContextMenu(const UString &text, int id) {
    if(id == 1 || id == 2) {
        // 1 = add map to collection
        // 2 = add set to collection
        this->contextMenu->begin(0, true);
        {
            this->contextMenu->addButtonJustified("[+] Create new Collection?", TEXT_JUSTIFICATION::LEFT, -id * 2);

            auto sorted_collections = Collections::get_loaded();  // sort by name

            std::ranges::stable_sort(
                sorted_collections, [](const char *s1, const char *s2) -> bool { return strcasecmp(s1, s2) < 0; },
                [](const auto &col) -> const char * { return col.get_name().c_str(); });

            for(const auto &collection : sorted_collections) {
                if(!collection.get_maps().empty()) {
                    CBaseUIButton *spacer = this->contextMenu->addButtonJustified("---", TEXT_JUSTIFICATION::CENTERED);
                    spacer->setEnabled(false);
                    spacer->setTextColor(0xff888888);
                    spacer->setTextDarkColor(0xff000000);

                    break;
                }
            }

            auto map_hash = this->databaseBeatmap->getMD5();
            for(const auto &collection : sorted_collections) {
                if(collection.get_maps().empty()) continue;

                bool can_add_to_collection = true;

                if(id == 1) {
                    if(std::ranges::contains(collection.get_maps(), map_hash)) {
                        // Map already is present in the collection
                        can_add_to_collection = false;
                    }
                }

                if(id == 2) {
                    // XXX: Don't mark as valid if the set is fully present in the collection
                }

                auto collectionButton =
                    this->contextMenu->addButtonJustified(collection.get_name(), TEXT_JUSTIFICATION::CENTERED, id);
                if(!can_add_to_collection) {
                    collectionButton->setEnabled(false);
                    collectionButton->setTextColor(0xff555555);
                    collectionButton->setTextDarkColor(0xff000000);
                }
            }
        }
        this->contextMenu->end(false, true);
        this->contextMenu->setClickCallback(SA::MakeDelegate<&SongButton::onAddToCollectionConfirmed>(this));
        UIContextMenu::clampToRightScreenEdge(this->contextMenu);
        UIContextMenu::clampToBottomScreenEdge(this->contextMenu);
    } else if(id == 3 || id == 4) {
        // 3 = remove map from collection
        // 4 = remove set from collection
        osu->getSongBrowser()->onSongButtonContextMenu(this, text, id);
    }
}

void SongButton::onAddToCollectionConfirmed(const UString &text, int id) {
    if(id == -2 || id == -4) {
        this->contextMenu->begin(0, true);
        {
            CBaseUIButton *label =
                this->contextMenu->addButtonJustified("Enter Collection Name:", TEXT_JUSTIFICATION::CENTERED);
            label->setEnabled(false);

            CBaseUIButton *spacer = this->contextMenu->addButtonJustified("---", TEXT_JUSTIFICATION::CENTERED);
            spacer->setEnabled(false);
            spacer->setTextColor(0xff888888);
            spacer->setTextDarkColor(0xff000000);

            this->contextMenu->addTextbox("", id);

            spacer = this->contextMenu->addButtonJustified("---", TEXT_JUSTIFICATION::CENTERED);
            spacer->setEnabled(false);
            spacer->setTextColor(0xff888888);
            spacer->setTextDarkColor(0xff000000);

            label =
                this->contextMenu->addButtonJustified("(Press ENTER to confirm.)", TEXT_JUSTIFICATION::CENTERED, id);
            label->setTextColor(0xff555555);
            label->setTextDarkColor(0xff000000);
        }
        this->contextMenu->end(false, false);
        this->contextMenu->setClickCallback(SA::MakeDelegate<&SongButton::onCreateNewCollectionConfirmed>(this));
        UIContextMenu::clampToRightScreenEdge(this->contextMenu);
        UIContextMenu::clampToBottomScreenEdge(this->contextMenu);
    } else {
        // just forward it
        osu->getSongBrowser()->onSongButtonContextMenu(this, text, id);
    }
}

void SongButton::onCreateNewCollectionConfirmed(const UString &text, int id) {
    if(id == -2 || id == -4) {
        // just forward it
        osu->getSongBrowser()->onSongButtonContextMenu(this, text, id);
    }
}

float SongButton::calculateGradeScale() {
    const vec2 size = this->getActualSize();
    const auto &gradeImg = osu->getSkin()->getGradeImageSmall(this->grade);
    return Osu::getImageScaleToFitResolution(gradeImg->getSizeBaseRaw(), vec2(size.x, size.y * this->fGradeScale));
}

float SongButton::calculateGradeWidth() {
    const auto &gradeImg = osu->getSkin()->getGradeImageSmall(this->grade);
    return gradeImg->getSizeBaseRaw().x * this->calculateGradeScale();
}

void SongButton::onOpenBeatmapFolderClicked() {
    this->contextMenu->setVisible2(false);  // why is this manual setVisible not required in mcosu?
    if(!this->databaseBeatmap) return;
    env->openFileBrowser(this->databaseBeatmap->getFolder());
}
