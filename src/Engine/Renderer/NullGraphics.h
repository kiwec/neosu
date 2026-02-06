// Copyright (c) 2026, WH, All rights reserved.
// null graphics backend for headless mode (no rendering)
#pragma once

#include "Graphics.h"
#include "Image.h"
#include "RenderTarget.h"
#include "Shader.h"
#include "VertexArrayObject.h"
#include "UString.h"

// image that does CPU-side pixel loading but never uploads to GPU
class NullImage final : public Image {
   public:
    NullImage(std::string filepath, bool mipmapped = false, bool keepInSystemMemory = false)
        : Image(std::move(filepath), mipmapped, keepInSystemMemory) {}
    NullImage(i32 width, i32 height, bool mipmapped = false, bool keepInSystemMemory = false)
        : Image(width, height, mipmapped, keepInSystemMemory) {}

    void bind(unsigned int /*textureUnit*/) const override {}
    void unbind() const override {}

   private:
    void init() override {
        if(!this->isAsyncReady()) return;
        if(!this->bKeepInSystemMemory) this->rawImage.clear();
        this->setReady(true);
    }
    void initAsync() override {
        if(!this->bCreatedImage)
            this->setAsyncReady(loadRawImage());
        else
            this->setAsyncReady(true);
    }
    void destroy() override {
        if(!this->bKeepInSystemMemory) this->rawImage.clear();
    }
};

class NullShader final : public Shader {
   public:
    NullShader() : Shader() {}

    void enable() override {}
    void disable() override {}
    void setUniform1f(std::string_view /*name*/, float /*value*/) override {}
    void setUniform1fv(std::string_view /*name*/, int /*count*/, const float *const /*values*/) override {}
    void setUniform1i(std::string_view /*name*/, int /*value*/) override {}
    void setUniform2f(std::string_view /*name*/, float /*x*/, float /*y*/) override {}
    void setUniform2fv(std::string_view /*name*/, int /*count*/, const float *const /*vectors*/) override {}
    void setUniform3f(std::string_view /*name*/, float /*x*/, float /*y*/, float /*z*/) override {}
    void setUniform3fv(std::string_view /*name*/, int /*count*/, const float *const /*vectors*/) override {}
    void setUniform4f(std::string_view /*name*/, float /*x*/, float /*y*/, float /*z*/, float /*w*/) override {}
    void setUniformMatrix4fv(std::string_view /*name*/, const Matrix4 & /*matrix*/) override {}
    void setUniformMatrix4fv(std::string_view /*name*/, const float *const /*v*/) override {}

   private:
    void init() override { this->setReady(true); }
    void initAsync() override { this->setAsyncReady(true); }
    void destroy() override {}
};

class NullRenderTarget final : public RenderTarget {
   public:
    NullRenderTarget(int x, int y, int width, int height, MultisampleType multiSampleType = MultisampleType::X0)
        : RenderTarget(x, y, width, height, multiSampleType) {}

    void enable() override {}
    void disable() override {}
    void bind(unsigned int /*textureUnit*/) override {}
    void unbind() override {}

   private:
    void init() override { this->setReady(true); }
    void initAsync() override { this->setAsyncReady(true); }
    void destroy() override {}
};

class NullVertexArrayObject final : public VertexArrayObject {
   public:
    NullVertexArrayObject(DrawPrimitive primitive = DrawPrimitive::TRIANGLES,
                          DrawUsageType usage = DrawUsageType::STATIC, bool keepInSystemMemory = false)
        : VertexArrayObject(primitive, usage, keepInSystemMemory) {}

    void draw() override {}
};

class NullGraphics final : public Graphics {
   public:
    // scene
    void beginScene() override {}
    void endScene() override {}

    // depth buffer
    void clearDepthBuffer() override {}

    // color
    void setColor(Color /*color*/) override {}
    void setAlpha(float /*alpha*/) override {}

    // 2d primitive drawing
    void drawPixels(int /*x*/, int /*y*/, int /*width*/, int /*height*/, DrawPixelsType /*type*/,
                    const void * /*pixels*/) override {}
    void drawPixel(int /*x*/, int /*y*/) override {}
    void drawLinef(float /*x1*/, float /*y1*/, float /*x2*/, float /*y2*/) override {}
    void drawRectf(const RectOptions & /*opt*/) override {}
    void fillRectf(float /*x*/, float /*y*/, float /*width*/, float /*height*/) override {}
    void fillRoundedRect(int /*x*/, int /*y*/, int /*width*/, int /*height*/, int /*radius*/) override {}
    void fillGradient(int /*x*/, int /*y*/, int /*width*/, int /*height*/, Color /*topLeftColor*/,
                      Color /*topRightColor*/, Color /*bottomLeftColor*/, Color /*bottomRightColor*/) override {}

    void drawQuad(int /*x*/, int /*y*/, int /*width*/, int /*height*/) override {}
    void drawQuad(vec2 /*topLeft*/, vec2 /*topRight*/, vec2 /*bottomRight*/, vec2 /*bottomLeft*/,
                  Color /*topLeftColor*/, Color /*topRightColor*/, Color /*bottomRightColor*/,
                  Color /*bottomLeftColor*/) override {}

    // 2d resource drawing
    void drawImage(const Image * /*image*/, AnchorPoint /*anchor*/, float /*edgeSoftness*/,
                   McRect /*clipRect*/) override {}
    void drawString(McFont * /*font*/, const UString & /*text*/, std::optional<TextShadow> /*shadow*/) override {}

    // 3d type drawing
    void drawVAO(VertexArrayObject * /*vao*/) override {}

    // 2d clipping
    void setClipRect(McRect /*clipRect*/) override {}
    void pushClipRect(McRect /*clipRect*/) override {}
    void popClipRect() override {}

    // viewport
    void pushViewport() override {}
    void setViewport(int /*x*/, int /*y*/, int /*width*/, int /*height*/) override {}
    void popViewport() override {}

    // stencil buffer
    void pushStencil() override {}
    void fillStencil(bool /*inside*/) override {}
    void popStencil() override {}

    // renderer settings
    void setClipping(bool /*enabled*/) override {}
    void setAlphaTesting(bool /*enabled*/) override {}
    void setAlphaTestFunc(DrawCompareFunc /*alphaFunc*/, float /*ref*/) override {}
    void setDepthBuffer(bool /*enabled*/) override {}
    void setColorWriting(bool /*r*/, bool /*g*/, bool /*b*/, bool /*a*/) override {}
    void setColorInversion(bool /*enabled*/) override {}
    void setCulling(bool /*enabled*/) override {}
    void setVSync(bool /*enabled*/) override {}
    void setAntialiasing(bool /*enabled*/) override {}
    void setWireframe(bool /*enabled*/) override {}

    // renderer actions
    void flush() override {}
    std::vector<u8> getScreenshot(bool /*withAlpha*/) override { return {}; }

    // renderer info
    const char *getName() const override { return "NullGraphics"; }
    [[nodiscard]] vec2 getResolution() const override { return {1280.f, 720.f}; }
    UString getVendor() override { return ""; }
    UString getModel() override { return ""; }
    UString getVersion() override { return ""; }
    int getVRAMTotal() override { return 0; }
    int getVRAMRemaining() override { return 0; }

    // callbacks
    void onResolutionChange(vec2 /*newResolution*/) override {}

    // factory
    Image *createImage(std::string filePath, bool mipmapped, bool keepInSystemMemory) override {
        return new NullImage(std::move(filePath), mipmapped, keepInSystemMemory);
    }
    Image *createImage(i32 width, i32 height, bool mipmapped, bool keepInSystemMemory) override {
        return new NullImage(width, height, mipmapped, keepInSystemMemory);
    }
    RenderTarget *createRenderTarget(int x, int y, int width, int height, MultisampleType msType) override {
        return new NullRenderTarget(x, y, width, height, msType);
    }
    Shader *createShaderFromFile(std::string /*vertexShaderFilePath*/,
                                 std::string /*fragmentShaderFilePath*/) override {
        return new NullShader();
    }
    Shader *createShaderFromSource(std::string /*vertexShader*/, std::string /*fragmentShader*/) override {
        return new NullShader();
    }
    VertexArrayObject *createVertexArrayObject(DrawPrimitive primitive, DrawUsageType usage,
                                               bool keepInSystemMemory) override {
        return new NullVertexArrayObject(primitive, usage, keepInSystemMemory);
    }

   protected:
    void onTransformUpdate() override {}
};
