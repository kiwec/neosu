#pragma once
#include "CBaseUISlider.h"

class McFont;

class UIVolumeSlider : public CBaseUISlider {
   public:
    enum class TYPE : uint8_t { MASTER, MUSIC, EFFECTS };

   public:
    UIVolumeSlider(float xPos, float yPos, float xSize, float ySize, UString name);

    void setType(TYPE type) { this->type = type; }
    void setSelected(bool selected);

    bool checkWentMouseInside();

    float getMinimumExtraTextWidth();

    [[nodiscard]] inline bool isSelected() const { return this->bSelected; }

   private:
    void drawBlock() override;

    void onMouseInside() override;

    TYPE type;
    bool bSelected;

    bool bWentMouseInside;
    float fSelectionAnim;

    McFont *font;
};
