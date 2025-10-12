// Copyright (c) 2016, PG, All rights reserved.
#include "CollectionButton.h"

#include <utility>

#include "SongBrowser.h"
#include "SongButton.h"
// ---

#include "Collections.h"
#include "ConVar.h"
#include "Database.h"
#include "Engine.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "NotificationOverlay.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "Skin.h"
#include "UIContextMenu.h"

CollectionButton::CollectionButton(SongBrowser *songBrowser, UIContextMenu *contextMenu,
                                   float xPos, float yPos, float xSize, float ySize, UString name,
                                   const UString &collectionName, std::vector<SongButton *> children)
    : CarouselButton(songBrowser, contextMenu, xPos, yPos, xSize, ySize, std::move(name)) {
    this->sCollectionName = collectionName.utf8View();
    this->children = std::move(children);

    this->fTitleScale = 0.35f;

    // settings
    this->setOffsetPercent(0.075f * 0.5f);
}

void CollectionButton::draw() {
    CarouselButton::draw();
    if(!this->bVisible) return;

    const auto &skin = osu->getSkin();

    // scaling
    const vec2 pos = this->getActualPos();
    const vec2 size = this->getActualSize();

    // draw title
    UString titleString = this->sCollectionName.c_str();
    int numChildren = 0;
    {
        for(auto &c : this->children) {
            const auto &childrenChildren = c->getChildren();
            if(childrenChildren.size() > 0) {
                for(auto cc : childrenChildren) {
                    if(cc->isSearchMatch()) numChildren++;
                }
            } else if(c->isSearchMatch())
                numChildren++;
        }
    }
    titleString.append(UString::format((numChildren == 1 ? " (%i map)" : " (%i maps)"), numChildren));

    int textXOffset = size.x * 0.02f;
    float titleScale = (size.y * this->fTitleScale) / this->font->getHeight();
    g->setColor(this->bSelected ? skin->getSongSelectActiveText() : skin->getSongSelectInactiveText());
    g->pushTransform();
    {
        g->scale(titleScale, titleScale);
        g->translate(pos.x + textXOffset, pos.y + size.y / 2 + (this->font->getHeight() * titleScale) / 2);
        g->drawString(this->font, titleString);
    }
    g->popTransform();
}

void CollectionButton::onSelected(bool wasSelected, bool autoSelectBottomMostChild, bool wasParentSelected) {
    CarouselButton::onSelected(wasSelected, autoSelectBottomMostChild, wasParentSelected);

    this->songBrowser->onSelectionChange(this, true);
    this->songBrowser->scrollToSongButton(this, true);
}

void CollectionButton::onRightMouseUpInside() { this->triggerContextMenu(mouse->getPos()); }

void CollectionButton::triggerContextMenu(vec2 pos) {
    if(osu->getSongBrowser()->getGroupingMode() != SongBrowser::GROUP_ENUM::COLLECTIONS) return;

    if(this->contextMenu != nullptr) {
        this->contextMenu->setPos(pos);
        this->contextMenu->setRelPos(pos);
        this->contextMenu->begin(0, true);
        {
            this->contextMenu->addButton("[...]      Rename Collection", 1);

            CBaseUIButton *spacer = this->contextMenu->addButton("---");
            spacer->setTextLeft(false);
            spacer->setEnabled(false);
            spacer->setTextColor(0xff888888);
            spacer->setTextDarkColor(0xff000000);

            this->contextMenu->addButton("[-]         Delete Collection", 2);
        }
        this->contextMenu->end(false, false);
        this->contextMenu->setClickCallback(SA::MakeDelegate<&CollectionButton::onContextMenu>(this));
        UIContextMenu::clampToRightScreenEdge(this->contextMenu);
        UIContextMenu::clampToBottomScreenEdge(this->contextMenu);
    }
}

void CollectionButton::onContextMenu(const UString &text, int id) {
    if(id == 1) {
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

            this->contextMenu->addTextbox(this->sCollectionName.c_str(), id)->setCursorPosRight();

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
        this->contextMenu->setClickCallback(SA::MakeDelegate<&CollectionButton::onRenameCollectionConfirmed>(this));
        UIContextMenu::clampToRightScreenEdge(this->contextMenu);
        UIContextMenu::clampToBottomScreenEdge(this->contextMenu);
    } else if(id == 2) {
        if(keyboard->isShiftDown())
            this->onDeleteCollectionConfirmed(text, id);
        else {
            this->contextMenu->begin(0, true);
            {
                this->contextMenu->addButton("Really delete collection?")->setEnabled(false);
                CBaseUIButton *spacer = this->contextMenu->addButton("---");
                spacer->setTextLeft(false);
                spacer->setEnabled(false);
                spacer->setTextColor(0xff888888);
                spacer->setTextDarkColor(0xff000000);
                this->contextMenu->addButton("Yes", 2)->setTextLeft(false);
                this->contextMenu->addButton("No")->setTextLeft(false);
            }
            this->contextMenu->end(false, false);
            this->contextMenu->setClickCallback(SA::MakeDelegate<&CollectionButton::onDeleteCollectionConfirmed>(this));
            UIContextMenu::clampToRightScreenEdge(this->contextMenu);
            UIContextMenu::clampToBottomScreenEdge(this->contextMenu);
        }
    }
}

void CollectionButton::onRenameCollectionConfirmed(const UString &text, int /*id*/) {
    if(text.length() > 0) {
        std::string new_name = text.toUtf8();
        auto collection = get_or_create_collection(this->sCollectionName);
        collection->rename_to(new_name);
        save_collections();

        // (trigger re-sorting of collection buttons)
        osu->getSongBrowser()->onCollectionButtonContextMenu(this, this->sCollectionName.c_str(), 3);
    }
}

void CollectionButton::onDeleteCollectionConfirmed(const UString & /*text*/, int id) {
    if(id != 2) return;

    // just forward it
    osu->getSongBrowser()->onCollectionButtonContextMenu(this, this->sCollectionName.c_str(), id);
}

Color CollectionButton::getActiveBackgroundColor() const {
    return argb(std::clamp<int>(cv::songbrowser_button_collection_active_color_a.getInt(), 0, 255),
                std::clamp<int>(cv::songbrowser_button_collection_active_color_r.getInt(), 0, 255),
                std::clamp<int>(cv::songbrowser_button_collection_active_color_g.getInt(), 0, 255),
                std::clamp<int>(cv::songbrowser_button_collection_active_color_b.getInt(), 0, 255));
}

Color CollectionButton::getInactiveBackgroundColor() const {
    return argb(std::clamp<int>(cv::songbrowser_button_collection_inactive_color_a.getInt(), 0, 255),
                std::clamp<int>(cv::songbrowser_button_collection_inactive_color_r.getInt(), 0, 255),
                std::clamp<int>(cv::songbrowser_button_collection_inactive_color_g.getInt(), 0, 255),
                std::clamp<int>(cv::songbrowser_button_collection_inactive_color_b.getInt(), 0, 255));
}
