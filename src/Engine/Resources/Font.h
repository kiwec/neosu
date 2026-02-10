//========== Copyright (c) 2015, PG & 2025, WH, All rights reserved. ============//
//
// Purpose:		freetype font wrapper with unicode support
//
// $NoKeywords: $fnt
//===============================================================================//

#pragma once
#ifndef FONT_H
#define FONT_H

#include "Resource.h"
#include "Vectors.h"
#include "StaticPImpl.h"
#include "Color.h"

#include <vector>
#include <optional>
#include <span>

class Graphics;
class OpenGLInterface;
class OpenGLES32Interface;
class DirectX11Interface;
class NullGraphics;
class SDLGPUInterface;
struct TextShadow;
class UString;

struct McFontImpl;
class McFont final : public Resource {
    NOCOPY_NOMOVE(McFont)
   private:
    friend Graphics;
    friend OpenGLInterface;
    friend OpenGLES32Interface;
    friend DirectX11Interface;
    friend NullGraphics;
    friend SDLGPUInterface;
    void drawString(const UString &text, std::optional<TextShadow> shadow);

   public:
    McFont(std::string filepath, int fontSize = 16, bool antialiasing = true, int fontDPI = 96);
    McFont(std::string filepath, const std::span<const char16_t> &characters, int fontSize = 16,
           bool antialiasing = true, int fontDPI = 96);
    ~McFont() override;

    // called once on engine startup
    static bool initSharedResources();

    // called on engine shutdown to clean up freetype/shared fallback fonts
    static void cleanupSharedResources();

    void setSize(int fontSize);
    void setDPI(int dpi);
    void setHeight(float height);

    [[nodiscard]] int getSize() const;
    [[nodiscard]] int getDPI() const;
    [[nodiscard]] float getHeight() const;  // precomputed average height (fast)

    [[nodiscard]] float getGlyphWidth(char16_t character) const;
    [[nodiscard]] float getGlyphHeight(char16_t character) const;
    [[nodiscard]] float getStringWidth(const UString &text) const;
    [[nodiscard]] float getStringHeight(const UString &text) const;
    [[nodiscard]] std::vector<UString> wrap(const UString &text, f64 max_width) const;

   public:
    McFont *asFont() override { return this; }
    [[nodiscard]] const McFont *asFont() const override { return this; }

    // debug convar
    void drawDebug() const;
    bool m_bDebugDrawAtlas{false};

    // debug manual
    void drawTextureAtlas() const;

   protected:
    void init() override;
    void initAsync() override;
    void destroy() override;

   private:
    friend struct McFontImpl;
    StaticPImpl<McFontImpl, 768> pImpl;
};

#endif
