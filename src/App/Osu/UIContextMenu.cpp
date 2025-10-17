// Copyright (c) 2016, PG, All rights reserved.
#include "UIContextMenu.h"

#include <utility>

#include "AnimationHandler.h"
#include "CBaseUIContainer.h"
#include "CBaseUIScrollView.h"
#include "ConVar.h"
#include "Engine.h"
#include "KeyBindings.h"
#include "Mouse.h"
#include "Osu.h"
#include "Skin.h"
#include "SoundEngine.h"
#include "TooltipOverlay.h"

static float button_sound_cooldown = 0.f;

UIContextMenuButton::UIContextMenuButton(float xPos, float yPos, float xSize, float ySize, UString name, UString text,
                                         int id)
    : CBaseUIButton(xPos, yPos, xSize, ySize, std::move(name), std::move(text)) {
    this->iID = id;
}

void UIContextMenuButton::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible) return;
    CBaseUIButton::mouse_update(propagate_clicks);

    if(this->isMouseInside() && this->tooltipTextLines.size() > 0) {
        osu->getTooltipOverlay()->begin();
        {
            for(const auto &tooltipTextLine : this->tooltipTextLines) {
                osu->getTooltipOverlay()->addLine(tooltipTextLine);
            }
        }
        osu->getTooltipOverlay()->end();
    }
}

void UIContextMenuButton::onMouseInside() {
    if(button_sound_cooldown + 0.05f < engine->getTime()) {
        soundEngine->play(osu->getSkin()->getHoverButtonSound());
        button_sound_cooldown = engine->getTime();
    }
}

void UIContextMenuButton::onMouseDownInside(bool /*left*/, bool /*right*/) {
    soundEngine->play(osu->getSkin()->getClickButtonSound());
}

void UIContextMenuButton::setTooltipText(const UString &text) { this->tooltipTextLines = text.split("\n"); }

UIContextMenuTextbox::UIContextMenuTextbox(float xPos, float yPos, float xSize, float ySize, UString name, int id)
    : CBaseUITextbox(xPos, yPos, xSize, ySize, std::move(name)) {
    this->iID = id;
}

UIContextMenu::UIContextMenu(float xPos, float yPos, float xSize, float ySize, const UString &name,
                             CBaseUIScrollView *parent)
    : CBaseUIScrollView(xPos, yPos, xSize, ySize, name) {
    this->parent = parent;

    this->setPos(xPos, yPos);
    this->setSize(xSize, ySize);
    this->setName(name);

    this->setHorizontalScrolling(false);
    this->setDrawBackground(false);
    this->setDrawFrame(false);
    this->setScrollbarSizeMultiplier(0.5f);

    // HACHACK: this->bVisible is always true, since we want to be able to put a context menu in a scrollview.
    //          When scrolling, scrollviews call setVisible(false) to clip items, and that breaks the menu.
    this->bVisible = true;

    this->bBigStyle = false;
    this->bClampUnderflowAndOverflowAndEnableScrollingIfNecessary = false;
}

void UIContextMenu::draw() {
    if(!this->bVisible2) return;

    if(this->fAnimation > 0.0f && this->fAnimation < 1.0f) {
        g->push3DScene(McRect(this->vPos.x,
                              this->vPos.y + ((this->vSize.y / 2.0f) * (this->bInvertAnimation ? 1.0f : -1.0f)),
                              this->vSize.x, this->vSize.y));
        g->rotate3DScene((1.0f - this->fAnimation) * 90.0f * (this->bInvertAnimation ? 1.0f : -1.0f), 0, 0);
    }

    // draw background
    g->setColor(Color(0xff222222).setA(this->fAnimation));

    g->fillRect(this->vPos.x + 1, this->vPos.y + 1, this->vSize.x - 1, this->vSize.y - 1);

    // draw frame
    g->setColor(Color(0xffffffff).setA(this->fAnimation * this->fAnimation));

    g->drawRect(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y);

    CBaseUIScrollView::draw();

    if(this->fAnimation > 0.0f && this->fAnimation < 1.0f) g->pop3DScene();
}

void UIContextMenu::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible2) return;
    CBaseUIScrollView::mouse_update(propagate_clicks);

    if(this->containedTextbox != nullptr) {
        if(this->containedTextbox->hitEnter()) this->onHitEnter(this->containedTextbox);
    }

    // HACKHACK: mouse wheel handling order
    if(this->bClampUnderflowAndOverflowAndEnableScrollingIfNecessary) {
        if(this->isMouseInside()) mouse->resetWheelDelta();
    }

    if(this->selfDeletionCrashWorkaroundScheduledElementDeleteHack.size() > 0) {
        for(auto &i : this->selfDeletionCrashWorkaroundScheduledElementDeleteHack) {
            delete i;
        }

        this->selfDeletionCrashWorkaroundScheduledElementDeleteHack.clear();
    }
}

void UIContextMenu::onKeyUp(KeyboardEvent &e) {
    if(!this->bVisible2) return;

    CBaseUIScrollView::onKeyUp(e);
}

void UIContextMenu::onKeyDown(KeyboardEvent &e) {
    if(!this->bVisible2) return;

    CBaseUIScrollView::onKeyDown(e);

    // also force ENTER event if context menu textbox has lost focus (but context menu is still visible, e.g. if the
    // user clicks inside the context menu but outside the textbox)
    if(this->containedTextbox != nullptr) {
        if(e == KEY_ENTER || e == KEY_NUMPAD_ENTER) {
            e.consume();
            this->onHitEnter(this->containedTextbox);
        }
    }

    // hide on ESC
    if(!e.isConsumed()) {
        if(e == KEY_ESCAPE || e == cv::GAME_PAUSE.getVal<KEYCODE>()) {
            e.consume();
            this->setVisible2(false);
        }
    }
}

void UIContextMenu::onChar(KeyboardEvent &e) {
    if(!this->bVisible2) return;

    CBaseUIScrollView::onChar(e);
}

void UIContextMenu::begin(int minWidth, bool bigStyle) {
    this->iWidthCounter = minWidth;
    this->bBigStyle = bigStyle;

    this->iYCounter = 0;
    this->clickCallback = {};

    this->setSizeX(this->iWidthCounter);

    // HACKHACK: bad design workaround.
    // HACKHACK: callbacks from the same context menu which call begin() to create a new context menu may crash because
    // begin() deletes the object the callback is currently being called from HACKHACK: so, instead we just keep a list
    // of things to delete whenever we get to the next update() tick
    {
        const std::vector<CBaseUIElement *> &oldElementsWeCanNotDeleteYet = this->getContainer()->getElements();
        this->selfDeletionCrashWorkaroundScheduledElementDeleteHack.insert(
            this->selfDeletionCrashWorkaroundScheduledElementDeleteHack.end(), oldElementsWeCanNotDeleteYet.begin(),
            oldElementsWeCanNotDeleteYet.end());
        this->invalidate();  // ensure nothing is deleted yet
    }

    this->freeElements();

    this->containedTextbox = nullptr;
}

UIContextMenuButton *UIContextMenu::addButtonJustified(const UString &text, bool left, int id) {
    const int buttonHeight = 30 * Osu::getUIScale() * (this->bBigStyle ? 1.27f : 1.0f);
    const int margin = 9 * Osu::getUIScale();

    auto *button = new UIContextMenuButton(margin, this->iYCounter + margin, 0, buttonHeight, text, text, id);
    {
        if(this->bBigStyle) button->setFont(osu->getSubTitleFont());

        button->setClickCallback(SA::MakeDelegate<&UIContextMenu::onClick>(this));
        button->setWidthToContent(3 * Osu::getUIScale());
        button->setTextLeft(left);
        button->setDrawFrame(false);
        button->setDrawBackground(false);
    }
    this->getContainer()->addBaseUIElement(button);

    if(button->getSize().x + 2 * margin > this->iWidthCounter) {
        this->iWidthCounter = button->getSize().x + 2 * margin;
        this->setSizeX(this->iWidthCounter);
    }

    this->iYCounter += buttonHeight;
    this->setSizeY(this->iYCounter + 2 * margin);

    return button;
}

UIContextMenuTextbox *UIContextMenu::addTextbox(const UString &text, int id) {
    const int buttonHeight = 30 * Osu::getUIScale() * (this->bBigStyle ? 1.27f : 1.0f);
    const int margin = 9 * Osu::getUIScale();

    auto *textbox = new UIContextMenuTextbox(margin, this->iYCounter + margin, 0, buttonHeight, text, id);
    {
        textbox->setText(text);

        if(this->bBigStyle) textbox->setFont(osu->getSubTitleFont());

        textbox->setActive(true);
    }
    this->getContainer()->addBaseUIElement(textbox);

    this->iYCounter += buttonHeight;
    this->setSizeY(this->iYCounter + 2 * margin);

    // NOTE: only one single textbox is supported currently
    this->containedTextbox = textbox;

    return textbox;
}

void UIContextMenu::end(bool invertAnimation, bool clampUnderflowAndOverflowAndEnableScrollingIfNecessary) {
    this->bInvertAnimation = invertAnimation;
    this->bClampUnderflowAndOverflowAndEnableScrollingIfNecessary =
        clampUnderflowAndOverflowAndEnableScrollingIfNecessary;

    const int margin = 9 * Osu::getUIScale();

    const std::vector<CBaseUIElement *> &elements = this->getContainer()->getElements();
    if(elements.empty()) return;

    for(auto element : elements) {
        element->setSizeX(this->iWidthCounter - 2 * margin);
    }

    // scrollview handling and edge cases
    {
        this->setVerticalScrolling(false);

        if(this->bClampUnderflowAndOverflowAndEnableScrollingIfNecessary) {
            if(this->vPos.y < 0) {
                const float underflow = std::abs(this->vPos.y);

                this->setRelPosY(this->vPos.y + underflow);
                this->setPosY(this->vPos.y + underflow);
                this->setSizeY(this->vSize.y - underflow);

                this->setVerticalScrolling(true);
            }

            if(this->vPos.y + this->vSize.y > osu->getVirtScreenHeight()) {
                const float overflow = std::abs(this->vPos.y + this->vSize.y - osu->getVirtScreenHeight());

                this->setSizeY(this->vSize.y - overflow - 1);

                this->setVerticalScrolling(true);
            }
        }

        this->setScrollSizeToContent();
    }

    this->setVisible2(true);

    this->fAnimation = 0.001f;
    anim->moveQuartOut(&this->fAnimation, 1.0f, 0.15f, true);

    soundEngine->play(osu->getSkin()->getExpandSound());
}

void UIContextMenu::setVisible2(bool visible2) {
    this->bVisible2 = visible2;

    if(!this->bVisible2) this->setSize(1, 1);  // reset size

    if(this->parent != nullptr) this->parent->setScrollSizeToContent();  // and update parent scroll size
}

void UIContextMenu::onMouseDownOutside(bool /*left*/, bool /*right*/) { this->setVisible2(false); }

void UIContextMenu::onClick(CBaseUIButton *button) {
    this->setVisible2(false);

    if(this->clickCallback != nullptr) {
        // special case: if text input exists, then override with its text
        if(this->containedTextbox != nullptr)
            this->clickCallback(this->containedTextbox->getText(), ((UIContextMenuButton *)button)->getID());
        else
            this->clickCallback(button->getName(), ((UIContextMenuButton *)button)->getID());
    }
}

void UIContextMenu::onHitEnter(UIContextMenuTextbox *textbox) {
    this->setVisible2(false);

    if(this->clickCallback != nullptr) this->clickCallback(textbox->getText(), textbox->getID());
}

void UIContextMenu::clampToBottomScreenEdge(UIContextMenu *menu) {
    if(menu->getRelPos().y + menu->getSize().y > osu->getVirtScreenHeight()) {
        int newRelPosY = osu->getVirtScreenHeight() - menu->getSize().y - 1;
        menu->setRelPosY(newRelPosY);
        menu->setPosY(newRelPosY);
    }
}

void UIContextMenu::clampToRightScreenEdge(UIContextMenu *menu) {
    if(menu->getRelPos().x + menu->getSize().x > osu->getVirtScreenWidth()) {
        const int newRelPosX = osu->getVirtScreenWidth() - menu->getSize().x - 1;
        menu->setRelPosX(newRelPosX);
        menu->setPosX(newRelPosX);
    }
}
