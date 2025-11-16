#pragma once
// Copyright (c) 2016, PG, All rights reserved.

#include "CarouselButton.h"

class CollectionButton final : public CarouselButton {
   public:
    DEF_BUTTON_TYPE(CollectionButton, CollectionButton_, CarouselButton)
   public:
    CollectionButton(SongBrowser *songBrowser, UIContextMenu *contextMenu, float xPos, float yPos, float xSize,
                     float ySize, UString name, const UString &collectionName, std::vector<SongButton *> children);

    void draw() override;

    void triggerContextMenu(vec2 pos);

    [[nodiscard]] Color getActiveBackgroundColor() const override;
    [[nodiscard]] Color getInactiveBackgroundColor() const override;

    [[nodiscard]] const std::string &getCollectionName() const { return this->sCollectionName; }

   private:
    void onSelected(bool wasSelected, bool autoSelectBottomMostChild, bool wasParentSelected) override;
    void onRightMouseUpInside() override;

    void onContextMenu(const UString &text, int id = -1);
    void onRenameCollectionConfirmed(const UString &text, int id = -1);
    void onDeleteCollectionConfirmed(const UString &text, int id = -1);

    std::string sCollectionName;

    float fTitleScale;
};
