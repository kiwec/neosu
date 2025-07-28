// TODO: support SHIFT + LEFT/RIGHT selection adjustments
// TODO: support CTRL + LEFT/RIGHT word caret jumping (to next space)
// TODO: support both SHIFT + CTRL + LEFT/RIGHT selection word jumping
// TODO: make scrolling anims fps independent

#include "CBaseUITextbox.h"

#include <utility>

#include "App.h"
#include "ConVar.h"
#include "Cursors.h"
#include "Engine.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "Skin.h"
#include "SoundEngine.h"



CBaseUITextbox::CBaseUITextbox(float xPos, float yPos, float xSize, float ySize, UString name)
    : CBaseUIElement(xPos, yPos, xSize, ySize, std::move(name)) {
    this->setKeepActive(true);

    this->grabs_clicks = true;

    this->font = resourceManager->getFont("FONT_DEFAULT");

    this->textColor = this->frameColor = this->caretColor = 0xffffffff;
    this->backgroundColor = 0xff000000;
    this->frameBrightColor = 0;
    this->frameDarkColor = 0;

    this->bDrawFrame = true;
    this->bDrawBackground = true;

    this->iTextJustification = 0;
    this->iTextAddX = cv::ui_textbox_text_offset_x.getInt();
    this->iTextAddY = 0;
    this->fTextScrollAddX = 0;
    this->iSelectX = 0;
    this->iCaretX = 0;
    this->iCaretWidth = 2;

    this->bHitenter = false;
    this->bContextMouse = false;
    this->bBlockMouse = false;
    this->bCatchMouse = false;
    this->bSelectCheck = false;
    this->iSelectStart = 0;
    this->iSelectEnd = 0;

    this->bLine = false;
    this->fLinetime = 0.0f;
    this->fTextWidth = 0.0f;
    this->iCaretPosition = 0;
}

UString CBaseUITextbox::getVisibleText() {
    if(this->is_password) {
        UString stars;
        for(int i = 0; i < this->sText.length(); i++) {
            stars.append(L'*');
        }
        return stars;
    } else {
        return this->sText;
    }
}

void CBaseUITextbox::draw() {
    if(!this->bVisible) return;

    const float dpiScale = ((float)this->font->getDPI() / 96.0f);  // NOTE: abusing font dpi

    // draw background
    if(this->bDrawBackground) {
        g->setColor(this->backgroundColor);
        g->fillRect(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y);
    }

    // draw base frame
    if(this->bDrawFrame) {
        if(this->frameDarkColor != 0 || this->frameBrightColor != 0)
            g->drawRect(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y, this->frameDarkColor,
                        this->frameBrightColor, this->frameBrightColor, this->frameDarkColor);
        else {
            g->setColor(this->frameColor);
            g->drawRect(this->vPos.x, this->vPos.y, this->vSize.x, this->vSize.y);
        }
    }

    // draw text

    if(this->font == NULL) return;

    g->pushClipRect(McRect(this->vPos.x + 1, this->vPos.y + 1, this->vSize.x - 1, this->vSize.y));
    {
        // draw selection box
        if(this->hasSelectedText()) {
            const int xpos1 = this->vPos.x + this->iTextAddX + this->iCaretX + this->fTextScrollAddX;
            const int xpos2 = this->vPos.x + this->iSelectX + this->iTextAddX + this->fTextScrollAddX;

            g->setColor(0xff56bcff);
            if(xpos1 > xpos2)
                g->fillRect(xpos2, this->vPos.y + 1, xpos1 - xpos2 + 2, this->vSize.y - 1);
            else
                g->fillRect(xpos1, this->vPos.y + 1, xpos2 - xpos1, this->vSize.y - 1);
        }

        this->drawText();

        // draw caret
        if(this->bActive && this->bLine) {
            const int caretWidth = std::round((float)this->iCaretWidth * dpiScale);
            const int height = std::round(this->vSize.y - 2 * 3 * dpiScale);
            const float yadd = std::round(height / 2.0f);

            g->setColor(this->caretColor);
            g->fillRect((int)(this->vPos.x + this->iTextAddX + this->iCaretX + this->fTextScrollAddX),
                        (int)(this->vPos.y + this->vSize.y / 2.0f - yadd), caretWidth, height);
        }
    }
    g->popClipRect();
}

void CBaseUITextbox::drawText() {
    g->setColor(this->textColor);
    g->pushTransform();
    {
        g->translate((int)(this->vPos.x + this->iTextAddX + this->fTextScrollAddX),
                     (int)(this->vPos.y + this->iTextAddY));
        g->drawString(this->font, this->getVisibleText());
    }
    g->popTransform();
}

void CBaseUITextbox::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible) return;
    bool was_active = this->bActive;
    CBaseUIElement::mouse_update(propagate_clicks);

    // Steal focus from all other Textboxes
    if(!was_active && this->bActive) {
        app->stealFocus();
        this->bActive = true;
    }

    const Vector2 mousepos = mouse->getPos();
    const bool mleft = mouse->isLeftDown();
    const bool mright = mouse->isRightDown();

    // HACKHACK: should do this with the proper events! this will only work properly though if we can event.consume()
    // charDown's
    if(!this->bEnabled && this->bActive && mleft && !this->bMouseInside) this->bActive = false;

    if((this->bMouseInside || (this->bBusy && (mleft || mright))) && (this->bActive || (!mleft && !mright)) &&
       this->bEnabled)
        env->setCursor(CURSORTYPE::CURSOR_TEXT);

    // update caret blinking
    {
        if(!this->bActive) {
            this->bLine = true;
            this->fLinetime = engine->getTime();
        } else {
            if((engine->getTime() - this->fLinetime) >= cv::ui_textbox_caret_blink_time.getFloat() && !this->bLine) {
                this->bLine = true;
                this->fLinetime = engine->getTime();
            }

            if((engine->getTime() - this->fLinetime) >= cv::ui_textbox_caret_blink_time.getFloat() && this->bLine) {
                this->bLine = false;
                this->fLinetime = engine->getTime();
            }
        }
    }

    // handle mouse input
    {
        if(!this->bMouseInside && mleft && !this->bActive) this->bBlockMouse = true;

        if(this->bMouseInside && (mleft || mright) && this->bActive) {
            this->bCatchMouse = true;
            this->tickCaret();

            if(mright) this->bContextMouse = true;
        }

        if(!mleft) {
            this->bCatchMouse = false;
            this->bBlockMouse = false;
            this->bSelectCheck = false;
        }
    }

    // handle selecting and scrolling
    if(this->bBusy && this->bActive && (mleft || mright) && !this->bBlockMouse && this->sText.length() > 0) {
        this->tickCaret();

        const int mouseX = mousepos.x - this->vPos.x;
        auto visible_text = this->getVisibleText();

        // handle scrolling
        if(mleft) {
            if(this->fTextWidth > this->vSize.x) {
                if(mouseX < this->vSize.x * 0.15f) {
                    const int scrollspeed = mouseX < 0 ? std::abs(mouseX) / 2 + 1 : 3;

                    // TODO: animations which don't suck for usability
                    this->fTextScrollAddX =
                        std::clamp<int>(this->fTextScrollAddX + scrollspeed, 0,
                                   this->fTextWidth - this->vSize.x + cv::ui_textbox_text_offset_x.getInt() * 2);
                    /// animation->moveSmoothEnd(&m_fTextScrollAddX, clampi(this->fTextScrollAddX+scrollspeed, 0,
                    /// m_fTextWidth-m_vSize.x+cv::ui_textbox_text_offset_x.getInt()*2), 1);
                }

                if(mouseX > this->vSize.x * 0.85f) {
                    const int scrollspeed = mouseX > this->vSize.x ? std::abs(mouseX - this->vSize.x) / 2 + 1 : 1;

                    // TODO: animations which don't suck for usability
                    this->fTextScrollAddX =
                        std::clamp<int>(this->fTextScrollAddX - scrollspeed, 0,
                                   this->fTextWidth - this->vSize.x + cv::ui_textbox_text_offset_x.getInt() * 2);
                    /// animation->moveSmoothEnd(&m_fTextScrollAddX, clampi(this->fTextScrollAddX-scrollspeed, 0,
                    /// m_fTextWidth-m_vSize.x+cv::ui_textbox_text_offset_x.getInt()*2), 1);
                }
            }

            // handle selecting begin, once per grab
            if(!this->bSelectCheck) {
                this->bSelectCheck = true;
                for(int i = 0; i <= visible_text.length(); i++) {
                    const float curGlyphWidth =
                        this->font->getStringWidth(visible_text.substr(i - 1 > 0 ? i - 1 : 0, 1)) / 2;
                    if(mouseX >= this->font->getStringWidth(visible_text.substr(0, i)) + this->iTextAddX +
                                     this->fTextScrollAddX - curGlyphWidth)
                        this->iSelectStart = i;
                }
                this->iSelectX = this->font->getStringWidth(visible_text.substr(0, this->iSelectStart));
            }

            // handle selecting end
            this->iSelectEnd = 0;
            for(int i = 0; i <= visible_text.length(); i++) {
                const float curGlyphWidth =
                    this->font->getStringWidth(visible_text.substr(i - 1 > 0 ? i - 1 : 0, 1)) / 2;
                if(mouseX >= this->font->getStringWidth(visible_text.substr(0, i)) + this->iTextAddX +
                                 this->fTextScrollAddX - curGlyphWidth)
                    this->iSelectEnd = i;
            }
            this->iCaretPosition = this->iSelectEnd;
        } else {
            if(!this->hasSelectedText()) {
                for(int i = 0; i <= visible_text.length(); i++) {
                    const float curGlyphWidth =
                        this->font->getStringWidth(visible_text.substr(i - 1 > 0 ? i - 1 : 0, 1)) / 2;
                    if(mouseX >= this->font->getStringWidth(visible_text.substr(0, i)) + this->iTextAddX +
                                     this->fTextScrollAddX - curGlyphWidth)
                        this->iCaretPosition = i;
                }
            }
        }

        // update caretx
        this->updateCaretX();
    }

    // handle context menu
    if(!mright && this->bContextMouse && this->isMouseInside()) {
        this->bContextMouse = false;
    }
}

void CBaseUITextbox::onFocusStolen() { this->deselectText(); }

void CBaseUITextbox::onKeyDown(KeyboardEvent &e) {
    if(!this->bActive || !this->bVisible) return;

    e.consume();

    switch(e.getKeyCode()) {
        case KEY_DELETE:
            soundEngine->play(osu->getSkin()->deletingText);
            if(this->sText.length() > 0) {
                if(this->hasSelectedText())
                    this->handleDeleteSelectedText();
                else if(this->iCaretPosition < this->sText.length()) {
                    this->sText.erase(this->iCaretPosition, 1);

                    this->setText(this->sText);
                    this->updateTextPos();
                }
            }
            this->tickCaret();
            break;

        case KEY_ENTER:
        case KEY_NUMPAD_ENTER:
            this->bHitenter = true;
            break;

        case KEY_ESCAPE:
            this->bActive = false;
            break;

        case KEY_BACKSPACE:
            soundEngine->play(osu->getSkin()->deletingText);
            if(this->sText.length() > 0) {
                if(this->hasSelectedText())
                    this->handleDeleteSelectedText();
                else if(this->iCaretPosition - 1 >= 0) {
                    if(keyboard->isControlDown()) {
                        if(this->is_password) {
                            this->setText("");
                        }

                        // delete everything from the current caret position to the left, until after the first
                        // non-space character (but including it)
                        bool foundNonSpaceChar = false;
                        while(this->sText.length() > 0 && (this->iCaretPosition - 1) >= 0) {
                            UString curChar = this->sText.substr(this->iCaretPosition - 1, 1);

                            if(foundNonSpaceChar && curChar.isWhitespaceOnly()) break;

                            if(!curChar.isWhitespaceOnly()) foundNonSpaceChar = true;

                            this->sText.erase(this->iCaretPosition - 1, 1);
                            this->iCaretPosition--;
                        }
                    } else {
                        this->sText.erase(this->iCaretPosition - 1, 1);
                        this->iCaretPosition--;
                    }

                    this->setText(this->sText);
                    this->updateTextPos();
                }
            }
            this->tickCaret();
            break;

        case KEY_LEFT: {
            const bool hadSelectedText = this->hasSelectedText();
            const int prevSelectPos = std::min(this->iSelectStart, this->iSelectEnd);

            this->deselectText();

            if(!hadSelectedText)
                this->iCaretPosition = std::clamp<int>(this->iCaretPosition - 1, 0, this->sText.length());
            else
                this->iCaretPosition = std::clamp<int>(prevSelectPos, 0, this->sText.length());

            this->tickCaret();
            this->handleCaretKeyboardMove();
            this->updateCaretX();

            soundEngine->play(osu->getSkin()->movingTextCursor);
        } break;

        case KEY_RIGHT: {
            const bool hadSelectedText = this->hasSelectedText();
            const int prevSelectPos = std::max(this->iSelectStart, this->iSelectEnd);

            this->deselectText();

            if(!hadSelectedText)
                this->iCaretPosition = std::clamp<int>(this->iCaretPosition + 1, 0, this->sText.length());
            else
                this->iCaretPosition = std::clamp<int>(prevSelectPos, 0, this->sText.length());

            this->tickCaret();
            this->handleCaretKeyboardMove();
            this->updateCaretX();

            soundEngine->play(osu->getSkin()->movingTextCursor);
        } break;

        case KEY_C:
            if(keyboard->isControlDown()) env->setClipBoardText(this->getSelectedText());
            break;

        case KEY_V:
            if(keyboard->isControlDown()) this->insertTextFromClipboard();
            break;

        case KEY_A:
            if(keyboard->isControlDown()) {
                // HACKHACK: make proper setSelectedText() function
                this->iSelectStart = 0;
                this->iSelectEnd = this->sText.length();

                this->iCaretPosition = this->iSelectEnd;
                this->iSelectX = this->font->getStringWidth(this->getVisibleText());
                this->iCaretX = 0;
                this->fTextScrollAddX = this->fTextWidth < this->vSize.x ? 0
                                                                         : this->fTextWidth - this->vSize.x +
                                                                               cv::ui_textbox_text_offset_x.getInt() * 2;
            }
            break;

        case KEY_X:
            if(keyboard->isControlDown() && this->hasSelectedText()) {
                soundEngine->play(osu->getSkin()->deletingText);
                env->setClipBoardText(this->getSelectedText());
                this->handleDeleteSelectedText();
            }
            break;

        case KEY_HOME:
            this->deselectText();
            this->iCaretPosition = 0;
            this->tickCaret();
            this->handleCaretKeyboardMove();
            this->updateCaretX();

            soundEngine->play(osu->getSkin()->movingTextCursor);
            break;

        case KEY_END:
            this->deselectText();
            this->iCaretPosition = this->sText.length();
            this->tickCaret();
            this->handleCaretKeyboardMove();
            this->updateCaretX();

            soundEngine->play(osu->getSkin()->movingTextCursor);
            break;
    }
}

void CBaseUITextbox::onChar(KeyboardEvent &e) {
    if(!this->bActive || !this->bVisible) return;

    e.consume();

    // ignore any control characters, we only want text
    // funny story: Windows 10 still has this bug even today, where when editing the name of any shortcut/folder on the
    // desktop, hitting CTRL + BACKSPACE will insert an invalid character
    if(e.getCharCode() < 32 || (keyboard->isControlDown() && !keyboard->isAltDown())) return;

    // Linux inserts a weird character when pressing the delete key
    if(e.getCharCode() == 127) return;

    // delete any potentially selected text
    this->handleDeleteSelectedText();

    // add the pressed letter to the text
    {
        const KEYCODE charCode = e.getCharCode();

        this->sText.insert(this->iCaretPosition, (wchar_t)charCode);
        this->iCaretPosition++;

        this->setText(this->sText);
    }

    this->tickCaret();

    Sound *sounds[] = {osu->getSkin()->typing1, osu->getSkin()->typing2, osu->getSkin()->typing3,
                       osu->getSkin()->typing4};
    soundEngine->play(sounds[rand() % 4]);
}

void CBaseUITextbox::handleCaretKeyboardMove() {
    const int caretPosition = this->iTextAddX +
                              this->font->getStringWidth(this->getVisibleText().substr(0, this->iCaretPosition)) +
                              this->fTextScrollAddX;
    if(caretPosition < 0)
        this->fTextScrollAddX += std::abs(caretPosition) + cv::ui_textbox_text_offset_x.getInt();
    else if(caretPosition > (this->vSize.x - cv::ui_textbox_text_offset_x.getInt()))
        this->fTextScrollAddX -= std::abs(caretPosition - this->vSize.x) + cv::ui_textbox_text_offset_x.getInt();
}

void CBaseUITextbox::handleCaretKeyboardDelete() {
    if(this->fTextWidth > this->vSize.x) {
        const int caretPosition = this->iTextAddX +
                                  this->font->getStringWidth(this->getVisibleText().substr(0, this->iCaretPosition)) +
                                  this->fTextScrollAddX;
        if(caretPosition < (this->vSize.x - cv::ui_textbox_text_offset_x.getInt()))
            this->fTextScrollAddX += std::abs(this->vSize.x - cv::ui_textbox_text_offset_x.getInt() - caretPosition);
    }
}

void CBaseUITextbox::tickCaret() {
    this->bLine = true;
    this->fLinetime = engine->getTime();
}

bool CBaseUITextbox::hitEnter() {
    if(this->bHitenter) {
        this->bHitenter = false;
        return true;
    } else
        return false;
}

bool CBaseUITextbox::hasSelectedText() const { return ((this->iSelectStart - this->iSelectEnd) != 0); }

void CBaseUITextbox::clear() {
    this->deselectText();
    this->setText("");
}

void CBaseUITextbox::focus(bool move_caret) {
    this->bActive = true;
    this->bBusy = true;
    this->bMouseInside = true;

    if(move_caret) {
        this->setCursorPosRight();
    }
}

CBaseUITextbox *CBaseUITextbox::setFont(McFont *font) {
    this->font = font;
    this->setText(this->sText);

    return this;
}

CBaseUITextbox *CBaseUITextbox::setText(UString text) {
    this->sText = text;
    this->iCaretPosition = std::clamp<int>(this->iCaretPosition, 0, text.length());

    // handle text justification
    this->fTextWidth = this->font->getStringWidth(this->getVisibleText());
    switch(this->iTextJustification) {
        case 0:  // left
            this->iTextAddX = cv::ui_textbox_text_offset_x.getInt();
            break;

        case 1:  // middle
            this->iTextAddX = -(this->fTextWidth - this->vSize.x) / 2.0f;
            this->iTextAddX = this->iTextAddX > 0 ? this->iTextAddX : cv::ui_textbox_text_offset_x.getInt();
            break;

        case 2:  // right
            this->iTextAddX = (this->vSize.x - this->fTextWidth) - cv::ui_textbox_text_offset_x.getInt();
            this->iTextAddX = this->iTextAddX > 0 ? this->iTextAddX : cv::ui_textbox_text_offset_x.getInt();
            break;
    }

    // handle over-text
    if(this->fTextWidth > this->vSize.x) {
        this->iTextAddX -= this->fTextWidth - this->vSize.x + cv::ui_textbox_text_offset_x.getInt() * 2;
        this->handleCaretKeyboardMove();
    } else
        this->fTextScrollAddX = 0;

    // TODO: force stop animation, it will fuck shit up
    /// animation->moveSmoothEnd(&m_fTextScrollAddX, this->fTextScrollAddX, 0.1f);

    // center vertically
    const float addY = std::round(this->vSize.y / 2.0f + this->font->getHeight() / 2.0f);
    this->iTextAddY = addY > 0 ? addY : 0;

    this->updateCaretX();

    return this;
}

void CBaseUITextbox::setCursorPosRight() {
    this->iCaretPosition = this->sText.length();
    {
        this->updateCaretX();
        this->tickCaret();
    }
    this->fTextScrollAddX = 0;
}

void CBaseUITextbox::updateCaretX() {
    UString text = this->getVisibleText().substr(0, this->iCaretPosition);
    this->iCaretX = this->font->getStringWidth(text);
}

void CBaseUITextbox::handleDeleteSelectedText() {
    if(!this->hasSelectedText()) return;

    const int selectedTextLength = (this->iSelectStart < this->iSelectEnd ? this->iSelectEnd - this->iSelectStart
                                                                          : this->iSelectStart - this->iSelectEnd);
    this->sText.erase(this->iSelectStart < this->iSelectEnd ? this->iSelectStart : this->iSelectEnd,
                      selectedTextLength);

    if(this->iSelectEnd > this->iSelectStart) this->iCaretPosition -= selectedTextLength;

    this->setText(this->sText);

    // scroll back if empty white space
    this->updateTextPos();

    this->deselectText();

    this->setText(this->sText);
}

void CBaseUITextbox::insertTextFromClipboard() {
    const UString clipstring = env->getClipBoardText();

    /*
    debugLog("got clip string: %s\n", clipstring.toUtf8());
    for (int i=0; i<clipstring.length(); i++)
    {
            debugLog("char #%i = %i\n", i, clipstring[i]);
    }
    */

    if(clipstring.length() > 0) {
        this->handleDeleteSelectedText();
        {
            this->sText.insert(this->iCaretPosition, clipstring);
            this->iCaretPosition = this->iCaretPosition + clipstring.length();
        }
        this->setText(this->sText);
    }
}

void CBaseUITextbox::updateTextPos() {
    if(this->iTextJustification == 0) {
        if((this->iTextAddX + this->fTextScrollAddX) > cv::ui_textbox_text_offset_x.getInt()) {
            if(this->hasSelectedText() && this->iCaretPosition == 0) {
                this->fTextScrollAddX = cv::ui_textbox_text_offset_x.getInt() - this->iTextAddX;
            } else
                this->fTextScrollAddX = cv::ui_textbox_text_offset_x.getInt() - this->iTextAddX;
        }
    }
}

void CBaseUITextbox::deselectText() {
    this->iSelectStart = 0;
    this->iSelectEnd = 0;
}

UString CBaseUITextbox::getSelectedText() {
    const int selectedTextLength = (this->iSelectStart < this->iSelectEnd ? this->iSelectEnd - this->iSelectStart
                                                                          : this->iSelectStart - this->iSelectEnd);

    if(selectedTextLength > 0)
        return this->sText.substr(this->iSelectStart < this->iSelectEnd ? this->iSelectStart : this->iSelectEnd,
                                  selectedTextLength);
    else
        return "";
}

void CBaseUITextbox::onResized() {
    CBaseUIElement::onResized();

    // HACKHACK: brute force fix layout
    this->setText(this->sText);
}

void CBaseUITextbox::onMouseDownInside(bool  /*left*/, bool  /*right*/) {
    // force busy, can't drag scroll release (textbox requires full focus due to text selection)
    this->bBusy = true;
}

void CBaseUITextbox::onMouseDownOutside(bool left, bool right) {
    CBaseUIElement::onMouseDownOutside(left, right);

    this->bBusy = false;
    this->bActive = false;

    this->deselectText();
}

void CBaseUITextbox::onMouseUpInside(bool left, bool right) {
    CBaseUIElement::onMouseUpInside(left, right);

    this->bBusy = false;
}

void CBaseUITextbox::onMouseUpOutside(bool left, bool right) {
    CBaseUIElement::onMouseUpOutside(left, right);

    this->bBusy = false;
}
