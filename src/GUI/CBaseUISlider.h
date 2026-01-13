#pragma once
// Copyright (c) 2012, PG, All rights reserved.

// TODO: fix vertical sliders
// TODO: this entire class is a mess

#include "CBaseUIElement.h"
#include "Color.h"
#include "Delegate.h"

class CBaseUISlider : public CBaseUIElement {
    NOCOPY_NOMOVE(CBaseUISlider)
   public:
    CBaseUISlider(float xPos = 0, float yPos = 0, float xSize = 0, float ySize = 0, UString name = "");
    ~CBaseUISlider() override = default;

    void draw() override;
    void update(CBaseUIEventCtx &c) override;

    void onKeyDown(KeyboardEvent &e) override;

    void fireChangeCallback();

    void setOrientation(bool horizontal) {
        this->bHorizontal = horizontal;
        this->onResized();
    }

    void setDrawFrame(bool drawFrame) { this->bDrawFrame = drawFrame; }
    void setDrawBackground(bool drawBackground) { this->bDrawBackground = drawBackground; }

    void setFrameColor(Color frameColor) { this->frameColor = frameColor; }
    void setBackgroundColor(Color backgroundColor) { this->backgroundColor = backgroundColor; }

    void setBlockSize(float xSize, float ySize);

    // callbacks, either void or with ourself as the argument
    using SliderChangeCallback = SA::delegate<void(CBaseUISlider *)>;
    CBaseUISlider *setChangeCallback(const SliderChangeCallback &changeCallback) {
        this->sliderChangeCallback = changeCallback;
        return this;
    }

    CBaseUISlider *setAllowMouseWheel(bool allowMouseWheel) {
        this->bAllowMouseWheel = allowMouseWheel;
        return this;
    }
    CBaseUISlider *setAnimated(bool animated) {
        this->bAnimated = animated;
        return this;
    }
    CBaseUISlider *setLiveUpdate(bool liveUpdate) {
        this->bLiveUpdate = liveUpdate;
        return this;
    }
    CBaseUISlider *setBounds(float minValue, float maxValue);
    CBaseUISlider *setKeyDelta(float keyDelta) {
        this->fKeyDelta = keyDelta;
        return this;
    }
    CBaseUISlider *setValue(float value, bool animate = true, bool call_callback = true);
    CBaseUISlider *setInitialValue(float value);

    inline float getFloat() { return this->fCurValue; }
    inline int getInt() { return (int)this->fCurValue; }
    inline bool getBool() { return (bool)this->fCurValue; }
    inline float getMax() { return this->fMaxValue; }
    inline float getMin() { return this->fMinValue; }
    float getPercent();

    // TODO: DEPRECATED, don't use this function anymore, use setChangeCallback() instead
    bool hasChanged();

    void onFocusStolen() override;
    void onMouseUpInside(bool left = true, bool right = false) override;
    void onMouseUpOutside(bool left = true, bool right = false) override;
    void onMouseDownInside(bool left = true, bool right = false) override;
    void onResized() override;

   protected:
    virtual void drawBlock();

    void updateBlockPos();

    SliderChangeCallback sliderChangeCallback;

    vec2 vBlockSize, vBlockPos{0.f};
    vec2 vGrabBackup{0.f};

    // to avoid "fighting" between externally set values and mouse-based slider values, if the mouse position hasn't moved
    vec2 vLastMousePos{0.f};

    float fMinValue, fMaxValue, fCurValue, fCurPercent;
    float fPrevValue;
    float fKeyDelta;
    float fLastSoundPlayTime = 0.f;

    Color frameColor, backgroundColor;

    bool bDrawFrame;
    bool bDrawBackground;
    bool bHorizontal;
    bool bHasChanged;
    bool bAnimated;
    bool bLiveUpdate;
    bool bAllowMouseWheel;
};
