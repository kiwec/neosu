//================ Copyright (c) 2014, PG, All rights reserved. =================//
//
// Purpose:		3D flip bar used for music scrolling/searching/play history
//
// $NoKeywords: $
//===============================================================================//

#ifndef VSTITLEBAR_H
#define VSTITLEBAR_H

#include "CBaseUIElement.h"

class McFont;

class CBaseUIContainer;
class CBaseUIButton;

class VSTitleBar : public CBaseUIElement {
   public:
    using SeekCallback = SA::delegate<void()>;

   public:
    VSTitleBar(int x, int y, int xSize, McFont *font);
    ~VSTitleBar() override;

    void draw() override;
    void mouse_update(bool *propagate_clicks) override;

    void setSeekCallback(const SeekCallback& callback) { this->seekCallback = callback; }
    void setTitle(UString title, bool reverse = false);

    [[nodiscard]] inline bool isSeeking() const { return this->bIsSeeking; }

   protected:
    void onResized() override;
    void onMoved() override;
    void onFocusStolen() override;

   private:
    void drawTitle1();
    void drawTitle2();

    SeekCallback seekCallback;

    McFont *font;

    CBaseUIContainer *container;

    CBaseUIButton *title;
    CBaseUIButton *title2;

    float fRot;

    int iFlip;

    bool bIsSeeking;
};

#endif
