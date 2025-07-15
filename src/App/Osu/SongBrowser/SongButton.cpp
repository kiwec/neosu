#include "SongButton.h"

#include <utility>

#include "CollectionButton.h"
#include "ScoreButton.h"
#include "SongBrowser.h"
#include "SongDifficultyButton.h"
// ---

#include "BackgroundImageHandler.h"
#include "Collections.h"
#include "ConVar.h"
#include "DatabaseBeatmap.h"
#include "Engine.h"
#include "Mouse.h"
#include "NotificationOverlay.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "Skin.h"
#include "SkinImage.h"
#include "UIContextMenu.h"

SongButton::SongButton(SongBrowser *songBrowser, CBaseUIScrollView *view, UIContextMenu *contextMenu, float xPos,
                       float yPos, float xSize, float ySize, UString name, DatabaseBeatmap *databaseBeatmap)
    : Button(songBrowser, view, contextMenu, xPos, yPos, xSize, ySize, std::move(name)) {
    this->databaseBeatmap = databaseBeatmap;

    // settings
    this->setHideIfSelected(true);

    // labels
    this->fThumbnailFadeInTime = 0.0f;
    this->fTextOffset = 0.0f;
    this->fGradeOffset = 0.0f;
    this->fTextSpacingScale = 0.075f;
    this->fTextMarginScale = 0.075f;
    this->fTitleScale = 0.22f;
    this->fSubTitleScale = 0.14f;
    this->fGradeScale = 0.45f;

    // build children
    if(this->databaseBeatmap != NULL) {
        const std::vector<DatabaseBeatmap *> &difficulties = this->databaseBeatmap->getDifficulties();

        // and add them
        for(int i = 0; i < difficulties.size(); i++) {
            SongButton *songButton = new SongDifficultyButton(this->songBrowser, this->view, this->contextMenu, 0, 0, 0,
                                                              0, "", difficulties[i], this);

            this->children.push_back(songButton);
        }
    }

    this->updateLayoutEx();
}

SongButton::~SongButton() {
    for(int i = 0; i < this->children.size(); i++) {
        delete this->children[i];
    }
}

void SongButton::draw() {
    if(!this->bVisible) return;
    if(this->vPos.y + this->vSize.y < 0) return;
    if(this->vPos.y > osu->getScreenHeight()) return;

    Button::draw();

    // draw background image
    this->sortChildren();
    if(this->databaseBeatmap != NULL && this->children.size() > 0) {
        // use the bottom child (hardest diff, assuming default sorting, and respecting the current search matches)
        for(int i = this->children.size() - 1; i >= 0; i--) {
            // NOTE: if no search is active, then all search matches return true by default
            if(this->children[i]->isSearchMatch()) {
                auto representative_beatmap = dynamic_cast<SongButton *>(this->children[i])->getDatabaseBeatmap();

                this->sTitle = representative_beatmap->getTitle();
                this->sArtist = representative_beatmap->getArtist();
                this->sMapper = representative_beatmap->getCreator();

                this->drawBeatmapBackgroundThumbnail(
                    osu->getBackgroundImageHandler()->getLoadBackgroundImage(representative_beatmap));

                break;
            }
        }
    }

    if(this->grade != FinishedScore::Grade::N) this->drawGrade();
    this->drawTitle();
    this->drawSubTitle();
}

void SongButton::drawBeatmapBackgroundThumbnail(Image *image) {
    if(!cv::draw_songbrowser_thumbnails.getBool() || osu->getSkin()->getVersion() < 2.2f) return;

    float alpha = 1.0f;
    if(cv::songbrowser_thumbnail_fade_in_duration.getFloat() > 0.0f) {
        if(image == NULL || !image->isReady())
            this->fThumbnailFadeInTime = engine->getTime();
        else if(this->fThumbnailFadeInTime > 0.0f && engine->getTime() > this->fThumbnailFadeInTime) {
            alpha = std::clamp<float>(
                (engine->getTime() - this->fThumbnailFadeInTime) / cv::songbrowser_thumbnail_fade_in_duration.getFloat(),
                0.0f, 1.0f);
            alpha = 1.0f - (1.0f - alpha) * (1.0f - alpha);
        }
    }

    if(image == NULL || !image->isReady()) return;

    // scaling
    const Vector2 pos = this->getActualPos();
    const Vector2 size = this->getActualSize();

    const f32 thumbnailYRatio = osu->getSongBrowser()->thumbnailYRatio;
    const f32 beatmapBackgroundScale =
        Osu::getImageScaleToFillResolution(image, Vector2(size.y * thumbnailYRatio, size.y)) * 1.05f;

    Vector2 centerOffset = Vector2(std::round((size.y * thumbnailYRatio) / 2.0f), std::round(size.y / 2.0f));
    McRect clipRect = McRect(pos.x - 2, pos.y + 1, (int)(size.y * thumbnailYRatio) + 5, size.y + 2);

    g->setColor(0xffffffff);
    g->setAlpha(alpha);
    g->pushTransform();
    {
        g->scale(beatmapBackgroundScale, beatmapBackgroundScale);
        g->translate(pos.x + (int)centerOffset.x, pos.y + (int)centerOffset.y);
        g->pushClipRect(clipRect);
        { g->drawImage(image); }
        g->popClipRect();
    }
    g->popTransform();

    // debug cliprect bounding box
    if(cv::debug.getBool()) {
        Vector2 clipRectPos = Vector2(clipRect.getX(), clipRect.getY() - 1);
        Vector2 clipRectSize = Vector2(clipRect.getWidth(), clipRect.getHeight());

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
    const Vector2 pos = this->getActualPos();
    const Vector2 size = this->getActualSize();

    SkinImage *grade = ScoreButton::getGradeImage(this->grade);
    g->pushTransform();
    {
        const float scale = this->calculateGradeScale();
        g->setColor(0xffffffff);
        grade->drawRaw(Vector2(pos.x + this->fGradeOffset, pos.y + size.y / 2), scale, AnchorPoint::LEFT);
    }
    g->popTransform();
}

void SongButton::drawTitle(float deselectedAlpha, bool forceSelectedStyle) {
    // scaling
    const Vector2 pos = this->getActualPos();
    const Vector2 size = this->getActualSize();

    const float titleScale = (size.y * this->fTitleScale) / this->font->getHeight();
    g->setColor((this->bSelected || forceSelectedStyle) ? osu->getSkin()->getSongSelectActiveText()
                                                        : osu->getSkin()->getSongSelectInactiveText());
    if(!(this->bSelected || forceSelectedStyle)) g->setAlpha(deselectedAlpha);

    g->pushTransform();
    {
        UString title = this->sTitle.c_str();
        g->scale(titleScale, titleScale);
        g->translate(pos.x + this->fTextOffset,
                     pos.y + size.y * this->fTextMarginScale + this->font->getHeight() * titleScale);
        g->drawString(this->font, title);
    }
    g->popTransform();
}

void SongButton::drawSubTitle(float deselectedAlpha, bool forceSelectedStyle) {
    // scaling
    const Vector2 pos = this->getActualPos();
    const Vector2 size = this->getActualSize();

    const float titleScale = (size.y * this->fTitleScale) / this->font->getHeight();
    const float subTitleScale = (size.y * this->fSubTitleScale) / this->font->getHeight();
    g->setColor((this->bSelected || forceSelectedStyle) ? osu->getSkin()->getSongSelectActiveText()
                                                        : osu->getSkin()->getSongSelectInactiveText());
    if(!(this->bSelected || forceSelectedStyle)) g->setAlpha(deselectedAlpha);

    g->pushTransform();
    {
        UString subTitleString = this->sArtist.c_str();
        subTitleString.append(" // ");
        subTitleString.append(this->sMapper.c_str());

        g->scale(subTitleScale, subTitleScale);
        g->translate(pos.x + this->fTextOffset,
                     pos.y + size.y * this->fTextMarginScale + this->font->getHeight() * titleScale +
                         size.y * this->fTextSpacingScale + this->font->getHeight() * subTitleScale * 0.85f);
        g->drawString(this->font, subTitleString);
    }
    g->popTransform();
}

void SongButton::sortChildren() { std::sort(this->children.begin(), this->children.end(), sort_by_difficulty); }

void SongButton::updateLayoutEx() {
    Button::updateLayoutEx();

    // scaling
    const Vector2 size = this->getActualSize();

    this->fTextOffset = 0.0f;
    this->fGradeOffset = 0.0f;

    if(this->grade != FinishedScore::Grade::N) this->fTextOffset += this->calculateGradeWidth();

    if(osu->getSkin()->getVersion() < 2.2f) {
        this->fTextOffset += size.x * 0.02f * 2.0f;
    } else {
        const f32 thumbnailYRatio = osu->getSongBrowser()->thumbnailYRatio;
        this->fTextOffset += size.y * thumbnailYRatio + size.x * 0.02f;
        this->fGradeOffset += size.y * thumbnailYRatio + size.x * 0.0125f;
    }
}

void SongButton::onSelected(bool wasSelected, bool autoSelectBottomMostChild, bool wasParentSelected) {
    Button::onSelected(wasSelected, autoSelectBottomMostChild, wasParentSelected);

    // resort children (since they might have been updated in the meantime)
    this->sortChildren();

    // update grade on child
    for(int c = 0; c < this->children.size(); c++) {
        SongDifficultyButton *child = (SongDifficultyButton *)this->children[c];
        child->updateGrade();
    }

    this->songBrowser->onSelectionChange(this, false);

    // now, automatically select the bottom child (hardest diff, assuming default sorting, and respecting the current
    // search matches)
    if(autoSelectBottomMostChild) {
        for(int i = this->children.size() - 1; i >= 0; i--) {
            if(this->children[i]
                   ->isSearchMatch())  // NOTE: if no search is active, then all search matches return true by default
            {
                this->children[i]->select(true, false, wasSelected);
                break;
            }
        }
    }
}

void SongButton::onRightMouseUpInside() { this->triggerContextMenu(mouse->getPos()); }

void SongButton::triggerContextMenu(Vector2 pos) {
    if(this->contextMenu != NULL) {
        this->contextMenu->setPos(pos);
        this->contextMenu->setRelPos(pos);
        this->contextMenu->begin(0, true);
        {
            if(this->databaseBeatmap != NULL && this->databaseBeatmap->getDifficulties().size() < 1)
                this->contextMenu->addButton("[+]         Add to Collection", 1);

            this->contextMenu->addButton("[+Set]   Add to Collection", 2);

            if(osu->getSongBrowser()->getGroupingMode() == SongBrowser::GROUP::GROUP_COLLECTIONS) {
                CBaseUIButton *spacer = this->contextMenu->addButton("---");
                spacer->setTextLeft(false);
                spacer->setEnabled(false);
                spacer->setTextColor(0xff888888);
                spacer->setTextDarkColor(0xff000000);

                if(this->databaseBeatmap == NULL || this->databaseBeatmap->getDifficulties().size() < 1) {
                    this->contextMenu->addButton("[-]          Remove from Collection", 3);
                }

                this->contextMenu->addButton("[-Set]    Remove from Collection", 4);
            }
        }
        this->contextMenu->end(false, false);
        this->contextMenu->setClickCallback(fastdelegate::MakeDelegate(this, &SongButton::onContextMenu));
        UIContextMenu::clampToRightScreenEdge(this->contextMenu);
        UIContextMenu::clampToBottomScreenEdge(this->contextMenu);
    }
}

void SongButton::onContextMenu(const UString& text, int id) {
    if(id == 1 || id == 2) {
        // 1 = add map to collection
        // 2 = add set to collection
        this->contextMenu->begin(0, true);
        {
            this->contextMenu->addButton("[+]   Create new Collection?", -id * 2);

            for(auto collection : collections) {
                if(!collection->maps.empty()) {
                    CBaseUIButton *spacer = this->contextMenu->addButton("---");
                    spacer->setTextLeft(false);
                    spacer->setEnabled(false);
                    spacer->setTextColor(0xff888888);
                    spacer->setTextDarkColor(0xff000000);
                    break;
                }
            }

            auto map_hash = this->databaseBeatmap->getMD5Hash();
            for(auto collection : collections) {
                if(collection->maps.empty()) continue;

                bool can_add_to_collection = true;

                if(id == 1) {
                    auto it = std::find(collection->maps.begin(), collection->maps.end(), map_hash);
                    if(it != collection->maps.end()) {
                        // Map already is present in the collection
                        can_add_to_collection = false;
                    }
                }

                if(id == 2) {
                    // XXX: Don't mark as valid if the set is fully present in the collection
                }

                auto collectionButton = this->contextMenu->addButton(UString(collection->name.c_str()), id);
                if(!can_add_to_collection) {
                    collectionButton->setEnabled(false);
                    collectionButton->setTextColor(0xff555555);
                    collectionButton->setTextDarkColor(0xff000000);
                }
            }
        }
        this->contextMenu->end(false, true);
        this->contextMenu->setClickCallback(fastdelegate::MakeDelegate(this, &SongButton::onAddToCollectionConfirmed));
        UIContextMenu::clampToRightScreenEdge(this->contextMenu);
        UIContextMenu::clampToBottomScreenEdge(this->contextMenu);
    } else if(id == 3 || id == 4) {
        // 3 = remove map from collection
        // 4 = remove set from collection
        osu->getSongBrowser()->onSongButtonContextMenu(this, text, id);
    }
}

void SongButton::onAddToCollectionConfirmed(const UString& text, int id) {
    if(id == -2 || id == -4) {
        this->contextMenu->begin(0, true);
        {
            CBaseUIButton *label = this->contextMenu->addButton("Enter Collection Name:");
            label->setTextLeft(false);
            label->setEnabled(false);

            CBaseUIButton *spacer = this->contextMenu->addButton("---");
            spacer->setTextLeft(false);
            spacer->setEnabled(false);
            spacer->setTextColor(0xff888888);
            spacer->setTextDarkColor(0xff000000);

            this->contextMenu->addTextbox("", id);

            spacer = this->contextMenu->addButton("---");
            spacer->setTextLeft(false);
            spacer->setEnabled(false);
            spacer->setTextColor(0xff888888);
            spacer->setTextDarkColor(0xff000000);

            label = this->contextMenu->addButton("(Press ENTER to confirm.)", id);
            label->setTextLeft(false);
            label->setTextColor(0xff555555);
            label->setTextDarkColor(0xff000000);
        }
        this->contextMenu->end(false, false);
        this->contextMenu->setClickCallback(
            fastdelegate::MakeDelegate(this, &SongButton::onCreateNewCollectionConfirmed));
        UIContextMenu::clampToRightScreenEdge(this->contextMenu);
        UIContextMenu::clampToBottomScreenEdge(this->contextMenu);
    } else {
        // just forward it
        osu->getSongBrowser()->onSongButtonContextMenu(this, text, id);
    }
}

void SongButton::onCreateNewCollectionConfirmed(const UString& text, int id) {
    if(id == -2 || id == -4) {
        // just forward it
        osu->getSongBrowser()->onSongButtonContextMenu(this, text, id);
    }
}

float SongButton::calculateGradeScale() {
    const Vector2 size = this->getActualSize();
    SkinImage *grade = ScoreButton::getGradeImage(this->grade);
    return Osu::getImageScaleToFitResolution(grade->getSizeBaseRaw(), Vector2(size.x, size.y * this->fGradeScale));
}

float SongButton::calculateGradeWidth() {
    SkinImage *grade = ScoreButton::getGradeImage(this->grade);
    return grade->getSizeBaseRaw().x * this->calculateGradeScale();
}
