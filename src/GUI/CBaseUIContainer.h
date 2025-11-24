#pragma once
// Copyright (c) 2011, PG, All rights reserved.
#include "CBaseUIElement.h"

class CBaseUIContainer : public CBaseUIElement {
    NOCOPY_NOMOVE(CBaseUIContainer)
   public:
    CBaseUIContainer(float xPos = 0, float yPos = 0, float xSize = 0, float ySize = 0, UString name = "");
    ~CBaseUIContainer() override;

    void freeElements();
    void invalidate();

    void draw_debug();
    void draw() override;
    void mouse_update(bool *propagate_clicks) override;

    void onKeyUp(KeyboardEvent &e) override;
    void onKeyDown(KeyboardEvent &e) override;
    void onChar(KeyboardEvent &e) override;

    CBaseUIContainer *addBaseUIElement(CBaseUIElement *element, float xPos, float yPos);
    CBaseUIContainer *addBaseUIElement(CBaseUIElement *element);
    CBaseUIContainer *addBaseUIElementBack(CBaseUIElement *element, float xPos, float yPos);
    CBaseUIContainer *addBaseUIElementBack(CBaseUIElement *element);

    CBaseUIContainer *insertBaseUIElement(CBaseUIElement *element, CBaseUIElement *index);
    CBaseUIContainer *insertBaseUIElementBack(CBaseUIElement *element, CBaseUIElement *index);

    CBaseUIContainer *removeBaseUIElement(CBaseUIElement *element);
    CBaseUIContainer *deleteBaseUIElement(CBaseUIElement *element);

    CBaseUIElement *getBaseUIElement(const UString &name);

    void onMoved() override { this->update_pos(); }
    void onResized() override { this->update_pos(); }

    bool isBusy() override;
    bool isActive() override;

    void onMouseDownOutside(bool left = true, bool right = false) override;

    void onFocusStolen() override;
    void onEnabled() override;
    void onDisabled() override;

    void update_pos();

    std::vector<CBaseUIElement *> vElements;
};
