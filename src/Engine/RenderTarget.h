#pragma once
// Copyright (c) 2013, PG, All rights reserved.
#include "Resource.h"
#include "Graphics.h"

class ConVar;

class RenderTarget : public Resource {
   public:
    RenderTarget(int x, int y, int width, int height,
                 Graphics::MULTISAMPLE_TYPE multiSampleType = Graphics::MULTISAMPLE_TYPE::MULTISAMPLE_0X);
    ~RenderTarget() override { ; }

    virtual void draw(int x, int y);
    virtual void draw(int x, int y, int width, int height);
    virtual void drawRect(int x, int y, int width, int height);

    virtual void enable() = 0;
    virtual void disable() = 0;

    virtual void bind(unsigned int textureUnit = 0) = 0;
    virtual void unbind() = 0;

    void rebuild(int x, int y, int width, int height, Graphics::MULTISAMPLE_TYPE multiSampleType);
    void rebuild(int x, int y, int width, int height);
    void rebuild(int width, int height);
    void rebuild(int width, int height, Graphics::MULTISAMPLE_TYPE multiSampleType);

    // set
    void setPos(int x, int y) {
        this->vPos.x = x;
        this->vPos.y = y;
    }
    void setPos(Vector2 pos) { this->vPos = pos; }
    void setColor(Color color) { this->color = color; }
    void setClearColor(Color clearColor) { this->clearColor = clearColor; }
    void setClearColorOnDraw(bool clearColorOnDraw) { this->bClearColorOnDraw = clearColorOnDraw; }
    void setClearDepthOnDraw(bool clearDepthOnDraw) { this->bClearDepthOnDraw = clearDepthOnDraw; }

    // get
    [[nodiscard]] float getWidth() const { return this->vSize.x; }
    [[nodiscard]] float getHeight() const { return this->vSize.y; }
    [[nodiscard]] inline Vector2 getSize() const { return this->vSize; }
    [[nodiscard]] inline Vector2 getPos() const { return this->vPos; }
    [[nodiscard]] inline Graphics::MULTISAMPLE_TYPE getMultiSampleType() const { return this->multiSampleType; }

    [[nodiscard]] inline bool isMultiSampled() const {
        return this->multiSampleType != Graphics::MULTISAMPLE_TYPE::MULTISAMPLE_0X;
    }

    // type inspection
    [[nodiscard]] Type getResType() const final { return RENDERTARGET; }

    RenderTarget *asRenderTarget() final { return this; }
    [[nodiscard]] const RenderTarget *asRenderTarget() const final { return this; }

   protected:
    void init() override = 0;
    void initAsync() override = 0;
    void destroy() override = 0;

    bool bClearColorOnDraw;
    bool bClearDepthOnDraw;

    Vector2 vPos;
    Vector2 vSize;

    Graphics::MULTISAMPLE_TYPE multiSampleType;

    Color color;
    Color clearColor;
};
