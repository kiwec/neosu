// Copyright (c) 2025, WH, All rights reserved.

#include "Logging.h"
#include "Osu.h"
#include "BeatmapCarousel.h"
#include "CollectionButton.h"
#include "SongBrowser.h"
#include "CarouselButton.h"
#include "SongButton.h"
#include "SongDifficultyButton.h"
#include "UIContextMenu.h"
#include "OptionsMenu.h"
#include "CBaseUIContainer.h"
#include "Engine.h"
#include "Mouse.h"
#include "Keyboard.h"

BeatmapCarousel::~BeatmapCarousel() {
    // elements are free'd manually/externally by SongBrowser, so invalidate the container to avoid double-free
    // TODO: factor this out from SongBrowser
    this->invalidate();
}

void BeatmapCarousel::draw() { CBaseUIScrollView::draw(); }

void BeatmapCarousel::mouse_update(bool *propagate_clicks) {
    CBaseUIScrollView::mouse_update(propagate_clicks);
    if(!this->isVisible()) return;
    this->getContainer()->update_pos();  // necessary due to constant animations

    // handle right click absolute scrolling
    {
        if(mouse->isRightDown() && !this->browser_ptr->contextMenu->isMouseInside()) {
            if(!this->browser_ptr->bSongBrowserRightClickScrollCheck) {
                this->browser_ptr->bSongBrowserRightClickScrollCheck = true;

                bool isMouseInsideAnySongButton = false;
                {
                    const std::vector<CBaseUIElement *> &elements = this->getContainer()->getElements();
                    for(CBaseUIElement *songButton : elements) {
                        if(songButton->isMouseInside()) {
                            isMouseInsideAnySongButton = true;
                            break;
                        }
                    }
                }

                if(this->isMouseInside() && !osu->getOptionsMenu()->isMouseInside() && !isMouseInsideAnySongButton)
                    this->browser_ptr->bSongBrowserRightClickScrolling = true;
                else
                    this->browser_ptr->bSongBrowserRightClickScrolling = false;
            }
        } else {
            this->browser_ptr->bSongBrowserRightClickScrollCheck = false;
            this->browser_ptr->bSongBrowserRightClickScrolling = false;
        }

        if(this->browser_ptr->bSongBrowserRightClickScrolling) {
            const int scrollingTo =
                -((mouse->getPos().y - 2 - this->getPos().y) / this->getSize().y) * this->getScrollSize().y;
            this->scrollToY(scrollingTo);
        }
    }
}

void BeatmapCarousel::onKeyUp(KeyboardEvent & /*e*/) { /*this->getContainer()->onKeyUp(e);*/ ; }

// don't consume keys, we are not a keyboard listener, but called from SongBrowser::onKeyDown manually
void BeatmapCarousel::onKeyDown(KeyboardEvent &key) {
    /*this->getContainer()->onKeyDown(e);*/

    const std::vector<CBaseUIElement *> &elements{this->getContainer()->getElements()};

    // selection move
    if(!keyboard->isAltDown() && key == KEY_DOWN) {
        // get bottom selection
        int selectedIndex = -1;
        for(int i = 0; i < elements.size(); i++) {
            auto *button = dynamic_cast<CarouselButton *>(elements[i]);
            if(button != nullptr && button->isSelected()) selectedIndex = i;
        }

        // select +1
        if(selectedIndex > -1 && selectedIndex + 1 < elements.size()) {
            int nextSelectionIndex = selectedIndex + 1;
            auto *nextButton = dynamic_cast<CarouselButton *>(elements[nextSelectionIndex]);
            auto *songButton = dynamic_cast<SongButton *>(elements[nextSelectionIndex]);
            if(nextButton != nullptr) {
                nextButton->select(true, false);

                // if this is a song button, select top child
                if(songButton != nullptr) {
                    const auto &children = songButton->getChildren();
                    if(children.size() > 0 && !children[0]->isSelected()) children[0]->select(true, false, false);
                }
            }
        }
    }

    if(!keyboard->isAltDown() && key == KEY_UP) {
        // get bottom selection
        int selectedIndex = -1;
        for(int i = 0; i < elements.size(); i++) {
            auto *button = dynamic_cast<CarouselButton *>(elements[i]);
            if(button != nullptr && button->isSelected()) selectedIndex = i;
        }

        // select -1
        if(selectedIndex > -1 && selectedIndex - 1 > -1) {
            int nextSelectionIndex = selectedIndex - 1;
            auto *nextButton = dynamic_cast<CarouselButton *>(elements[nextSelectionIndex]);
            bool isCollectionButton = dynamic_cast<CollectionButton *>(elements[nextSelectionIndex]);

            if(nextButton != nullptr) {
                nextButton->select();

                // automatically open collection on top of this one and go to bottom child
                if(isCollectionButton && nextSelectionIndex - 1 > -1) {
                    nextSelectionIndex = nextSelectionIndex - 1;
                    auto *nextCollectionButton = dynamic_cast<CollectionButton *>(elements[nextSelectionIndex]);
                    if(nextCollectionButton != nullptr) {
                        nextCollectionButton->select();

                        const auto &children = nextCollectionButton->getChildren();
                        if(children.size() > 0 && !children[children.size() - 1]->isSelected())
                            children[children.size() - 1]->select();
                    }
                }
            }
        }
    }

    if(key == KEY_LEFT && !this->browser_ptr->bLeft) {
        this->browser_ptr->bLeft = true;

        const bool jumpToNextGroup = keyboard->isShiftDown();

        bool foundSelected = false;
        for(sSz i = elements.size() - 1; i >= 0; i--) {
            const auto *diffButtonPointer = dynamic_cast<const SongDifficultyButton *>(elements[i]);
            const auto *collectionButtonPointer = dynamic_cast<const CollectionButton *>(elements[i]);

            auto *button = dynamic_cast<CarouselButton *>(elements[i]);
            const bool isSongDifficultyButtonAndNotIndependent =
                (diffButtonPointer != nullptr && !diffButtonPointer->isIndependentDiffButton());

            if(foundSelected && button != nullptr && !button->isSelected() &&
               !isSongDifficultyButtonAndNotIndependent && (!jumpToNextGroup || collectionButtonPointer != nullptr)) {
                this->browser_ptr->bNextScrollToSongButtonJumpFixUseScrollSizeDelta = true;
                {
                    button->select();

                    if(!jumpToNextGroup || collectionButtonPointer == nullptr) {
                        // automatically open collection below and go to bottom child
                        auto *collectionButton = dynamic_cast<CollectionButton *>(elements[i]);
                        if(collectionButton != nullptr) {
                            const auto &children = collectionButton->getChildren();
                            if(children.size() > 0 && !children[children.size() - 1]->isSelected())
                                children[children.size() - 1]->select();
                        }
                    }
                }
                this->browser_ptr->bNextScrollToSongButtonJumpFixUseScrollSizeDelta = false;

                break;
            }

            if(button != nullptr && button->isSelected()) foundSelected = true;
        }
    }

    if(key == KEY_RIGHT && !this->browser_ptr->bRight) {
        this->browser_ptr->bRight = true;

        const bool jumpToNextGroup = keyboard->isShiftDown();

        // get bottom selection
        sSz selectedIndex = -1;
        for(size_t i = 0; i < elements.size(); i++) {
            const auto *button = dynamic_cast<const CarouselButton *>(elements[i]);
            if(button != nullptr && button->isSelected()) selectedIndex = i;
        }

        if(selectedIndex > -1) {
            for(size_t i = selectedIndex; i < elements.size(); i++) {
                const auto *diffButtonPointer = dynamic_cast<const SongDifficultyButton *>(elements[i]);
                const auto *collectionButtonPointer = dynamic_cast<const CollectionButton *>(elements[i]);

                auto *button = dynamic_cast<CarouselButton *>(elements[i]);
                const bool isSongDifficultyButtonAndNotIndependent =
                    (diffButtonPointer != nullptr && !diffButtonPointer->isIndependentDiffButton());

                if(button != nullptr && !button->isSelected() && !isSongDifficultyButtonAndNotIndependent &&
                   (!jumpToNextGroup || collectionButtonPointer != nullptr)) {
                    button->select();
                    break;
                }
            }
        }
    }

    if(key == KEY_PAGEUP) this->scrollY(this->getSize().y);
    if(key == KEY_PAGEDOWN) this->scrollY(-this->getSize().y);

    // group open/close
    // NOTE: only closing works atm (no "focus" state on buttons yet)
    if((key == KEY_ENTER || key == KEY_NUMPAD_ENTER) && keyboard->isShiftDown()) {
        for(auto element : elements) {
            const auto *collectionButtonPointer = dynamic_cast<const CollectionButton *>(element);

            auto *button = dynamic_cast<CarouselButton *>(element);

            if(collectionButtonPointer != nullptr && button != nullptr && button->isSelected()) {
                button->select();  // deselect
                this->browser_ptr->scrollToSongButton(button);
                break;
            }
        }
    }

    // selection select
    if((key == KEY_ENTER || key == KEY_NUMPAD_ENTER) && !keyboard->isShiftDown())
        this->browser_ptr->playSelectedDifficulty();
}

void BeatmapCarousel::onChar(KeyboardEvent & /*e*/) { /*this->getContainer()->onChar(e);*/ ; }
