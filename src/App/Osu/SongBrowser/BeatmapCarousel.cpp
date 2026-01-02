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
#include "Engine.h"
#include "Mouse.h"
#include "Keyboard.h"

using namespace neosu::sbr;

BeatmapCarousel::~BeatmapCarousel() {
    // elements are free'd manually/externally by SongBrowser, so invalidate the container to avoid double-free
    // TODO: factor this out from SongBrowser
    this->invalidate();
}

void BeatmapCarousel::draw() { CBaseUIScrollView::draw(); }

void BeatmapCarousel::mouse_update(bool *propagate_clicks) {
    CBaseUIScrollView::mouse_update(propagate_clicks);
    if(!this->isVisible()) {
        // just reset this as a precaution
        this->rightClickScrollRelYVelocity = 0.0;
        return;
    }

    this->container->update_pos();  // necessary due to constant animations

    // handle right click absolute scrolling
    {
        if(mouse->isRightDown() && !g_songbrowser->contextMenu->isMouseInside()) {
            if(!g_songbrowser->bSongBrowserRightClickScrollCheck) {
                g_songbrowser->bSongBrowserRightClickScrollCheck = true;

                bool isMouseInsideAnySongButton = false;
                {
                    const std::vector<CBaseUIElement *> &elements = this->container->getElements();
                    for(CBaseUIElement *songButton : elements) {
                        if(songButton->isMouseInside()) {
                            isMouseInsideAnySongButton = true;
                            break;
                        }
                    }
                }

                if(this->isMouseInside() && !osu->getOptionsMenu()->isMouseInside() && !isMouseInsideAnySongButton)
                    g_songbrowser->bSongBrowserRightClickScrolling = true;
                else
                    g_songbrowser->bSongBrowserRightClickScrolling = false;
            }
        } else {
            g_songbrowser->bSongBrowserRightClickScrollCheck = false;
            g_songbrowser->bSongBrowserRightClickScrolling = false;
        }

        if(g_songbrowser->bSongBrowserRightClickScrolling) {
            const int scrollingTo =
                -((mouse->getPos().y - 2 - this->getPos().y) / this->getSize().y) * this->getScrollSize().y;
            this->scrollToY(scrollingTo);
        }
    }

    // update right click scrolling relative velocity, as a cache for isActuallyRightClickScrolling
    f64 curRightClickScrollRelYVelocity = 0.0;
    do {
        if(!g_songbrowser->isRightClickScrolling()) {
            curRightClickScrollRelYVelocity = 0.0;  // if we are not right click scrolling then there is no velocity
            break;
        }

        // this case never seems to be hit for the carousel, not sure why... probably because of the hacky way the
        // scrollview is used
        if(this->isScrolling()) {
            curRightClickScrollRelYVelocity = 1.0;
            break;
        }

        const f64 scrollSizeY = std::abs(this->getScrollSize().y);
        if(scrollSizeY == 0.0) {
            curRightClickScrollRelYVelocity = 0.0;
            break;
        }

        const f64 absYVelocity = std::abs(this->getVelocity().y);
        if(absYVelocity == 0.0) {
            curRightClickScrollRelYVelocity = 0.0;
            break;
        }

        // we are truly scrolling, calculate the relative velocity
        curRightClickScrollRelYVelocity = absYVelocity / scrollSizeY;
    } while(false);

    this->rightClickScrollRelYVelocity = curRightClickScrollRelYVelocity;
}

void BeatmapCarousel::onKeyUp(KeyboardEvent & /*e*/) { /*this->container->onKeyUp(e);*/ ; }

// don't consume keys, we are not a keyboard listener, but called from SongBrowser::onKeyDown manually
void BeatmapCarousel::onKeyDown(KeyboardEvent &key) {
    /*this->container->onKeyDown(e);*/

    // all elements must be CarouselButtons, at least
    const auto &elements{this->container->getElements<CarouselButton>()};

    // selection move
    if(!keyboard->isAltDown() && key == KEY_DOWN) {
        // get bottom selection
        int selectedIndex = -1;
        for(int i = 0; i < elements.size(); i++) {
            if(elements[i]->isSelected()) selectedIndex = i;
        }

        // select +1
        if(selectedIndex > -1 && selectedIndex + 1 < elements.size()) {
            int nextSelectionIndex = selectedIndex + 1;
            auto *nextButton = elements[nextSelectionIndex];

            nextButton->select({.noSelectBottomChild = true});

            auto *songButton = nextButton->as<SongButton>();

            // if this is a song button, select top child
            if(songButton != nullptr) {
                const auto &children = songButton->getChildren();
                if(children.size() > 0 && !children[0]->isSelected())
                    children[0]->select({.noSelectBottomChild = true, .parentUnselected = true});
            }
        }
    }

    if(!keyboard->isAltDown() && key == KEY_UP) {
        // get bottom selection
        int selectedIndex = -1;
        for(int i = 0; i < elements.size(); i++) {
            if(elements[i]->isSelected()) selectedIndex = i;
        }

        // select -1
        if(selectedIndex > -1 && selectedIndex - 1 > -1) {
            int nextSelectionIndex = selectedIndex - 1;
            auto *nextButton = elements[nextSelectionIndex];

            nextButton->select();
            const bool isCollectionButton = nextButton->isType<CollectionButton>();

            // automatically open collection on top of this one and go to bottom child
            if(isCollectionButton && nextSelectionIndex - 1 > -1) {
                nextSelectionIndex = nextSelectionIndex - 1;
                auto *nextCollectionButton = elements[nextSelectionIndex]->as<CollectionButton>();
                if(nextCollectionButton != nullptr) {
                    nextCollectionButton->select();

                    const auto &children = nextCollectionButton->getChildren();
                    if(children.size() > 0 && !children.back()->isSelected()) children.back()->select();
                }
            }
        }
    }

    if(key == KEY_LEFT && !g_songbrowser->bLeft) {
        g_songbrowser->bLeft = true;

        const bool jumpToNextGroup = keyboard->isShiftDown();

        bool foundSelected = false;
        for(sSz i = elements.size() - 1; i >= 0; i--) {
            const auto *diffButtonPointer = elements[i]->as<const SongDifficultyButton>();
            const auto *collectionButtonPointer = elements[i]->as<const CollectionButton>();

            auto *button = elements[i]->as<CarouselButton>();
            const bool isSongDifficultyButtonAndNotIndependent =
                (diffButtonPointer != nullptr && !diffButtonPointer->isIndependentDiffButton());

            if(foundSelected && button != nullptr && !button->isSelected() &&
               !isSongDifficultyButtonAndNotIndependent && (!jumpToNextGroup || collectionButtonPointer != nullptr)) {
                g_songbrowser->bNextScrollToSongButtonJumpFixUseScrollSizeDelta = true;
                {
                    button->select();

                    if(!jumpToNextGroup || collectionButtonPointer == nullptr) {
                        // automatically open collection below and go to bottom child
                        auto *collectionButton = elements[i]->as<CollectionButton>();
                        if(collectionButton != nullptr) {
                            const auto &children = collectionButton->getChildren();
                            if(children.size() > 0 && !children.back()->isSelected()) children.back()->select();
                        }
                    }
                }
                g_songbrowser->bNextScrollToSongButtonJumpFixUseScrollSizeDelta = false;

                break;
            }

            if(button != nullptr && button->isSelected()) foundSelected = true;
        }
    }

    if(key == KEY_RIGHT && !g_songbrowser->bRight) {
        g_songbrowser->bRight = true;

        const bool jumpToNextGroup = keyboard->isShiftDown();

        // get bottom selection
        int selectedIndex = -1;
        for(int i = 0; i < elements.size(); i++) {
            if(elements[i]->isSelected()) selectedIndex = i;
        }

        if(selectedIndex > -1) {
            for(size_t i = selectedIndex; i < elements.size(); i++) {
                const auto *diffButtonPointer = elements[i]->as<const SongDifficultyButton>();
                const auto *collectionButtonPointer = elements[i]->as<const CollectionButton>();

                auto *button = elements[i]->as<CarouselButton>();
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
            const auto *collectionButtonPointer = element->as<const CollectionButton>();

            auto *button = element->as<CarouselButton>();

            if(collectionButtonPointer != nullptr && button != nullptr && button->isSelected()) {
                button->select();  // deselect
                g_songbrowser->scrollToSongButton(button);
                break;
            }
        }
    }

    // selection select
    if((key == KEY_ENTER || key == KEY_NUMPAD_ENTER) && !keyboard->isShiftDown())
        g_songbrowser->playSelectedDifficulty();
}

void BeatmapCarousel::onChar(KeyboardEvent & /*e*/) { /*this->container->onChar(e);*/ ; }
