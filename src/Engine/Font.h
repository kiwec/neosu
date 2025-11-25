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
#include "UString.h"
#include "VertexArrayObject.h"

#include <memory>
#include <unordered_map>

typedef struct FT_Bitmap_ FT_Bitmap;
typedef struct FT_FaceRec_ *FT_Face;
typedef struct FT_LibraryRec_ *FT_Library;

class TextureAtlas;
class VertexArrayObject;

class McFont final : public Resource {
   public:
    McFont(std::string filepath, int fontSize = 16, bool antialiasing = true, int fontDPI = 96);
    McFont(std::string filepath, const std::vector<char16_t> &characters, int fontSize = 16, bool antialiasing = true,
           int fontDPI = 96);
    ~McFont() override;

    McFont &operator=(const McFont &) = delete;
    McFont &operator=(McFont &&) = delete;

    McFont(const McFont &) = delete;
    McFont(McFont &&) = delete;

    // called once on engine startup
    static bool initSharedResources();

    // called on engine shutdown to clean up freetype/shared fallback fonts
    static void cleanupSharedResources();

    void drawString(const UString &text);
    void beginBatch();
    void addToBatch(const UString &text, const vec3 &pos, Color color = 0xffffffff);
    void flushBatch();

    void setSize(int fontSize) { m_iFontSize = fontSize; }
    void setDPI(int dpi) { m_iFontDPI = dpi; }
    void setHeight(float height) { m_fHeight = height; }

    inline int getSize() const { return m_iFontSize; }
    inline int getDPI() const { return m_iFontDPI; }
    inline float getHeight() const { return m_fHeight; }  // precomputed average height (fast)

    float getStringWidth(const UString &text) const;
    float getStringHeight(const UString &text) const;
    std::vector<UString> wrap(const UString &text, f64 max_width) const;

   public:
    struct GLYPH_METRICS {
        char16_t character;
        unsigned int uvPixelsX, uvPixelsY;
        unsigned int sizePixelsX, sizePixelsY;
        int left, top, width, rows;
        float advance_x;
        int fontIndex;  // which font this glyph came from (0 = primary, >0 = fallback)
    };
    // McKay would say: ILLEGAL
    const GLYPH_METRICS &getGlyphMetrics(char16_t ch) const;

    // type inspection
    [[nodiscard]] Type getResType() const override { return FONT; }

    McFont *asFont() override { return this; }
    [[nodiscard]] const McFont *asFont() const override { return this; }

   protected:
    void constructor(const std::vector<char16_t> &characters, int fontSize, bool antialiasing, int fontDPI);
    void init() override;
    void initAsync() override;
    void destroy() override;

   private:
    struct BatchEntry {
        UString text;
        vec3 pos{0.f};
        Color color;
    };

    struct TextBatch {
        size_t totalVerts;
        size_t usedEntries;
        std::vector<BatchEntry> entryList;
    };

    // texture atlas dynamic slot management
    struct DynamicSlot {
        int x, y;            // position in atlas
        char16_t character;  // character in this slot (0 if empty)
        uint64_t lastUsed;   // for LRU eviction
        bool occupied;
    };

    // atlas layout constants

    // how much larger to make atlas for dynamic region
    // we initially pack the ASCII characters + initial characters into a static region,
    // then dynamically loaded glyphs are placed in the remaining space in fixed-size slots (not packed)
    // this maximizes the amount of fallback glyphs we can have loaded at once for a fixed amount of memory usage
    static constexpr float ATLAS_SIZE_MULTIPLIER{4.0f};
    // size of each dynamic slot
    static constexpr int DYNAMIC_SLOT_SIZE{64};

    forceinline bool hasGlyph(char16_t ch) const { return m_vGlyphMetrics.find(ch) != m_vGlyphMetrics.end(); };
    bool addGlyph(char16_t ch);
    bool loadGlyphDynamic(char16_t ch);

    // atlas management methods
    int allocateDynamicSlot(char16_t ch);
    void markSlotUsed(char16_t ch);
    void initializeDynamicRegion(int atlasSize);

    // consolidated glyph processing methods
    bool initializeFreeType();
    bool loadGlyphMetrics(char16_t ch);
    std::unique_ptr<Color[]> createExpandedBitmapData(const FT_Bitmap &bitmap);
    void renderGlyphToAtlas(char16_t ch, int x, int y, FT_Face face = nullptr, bool isDynamicSlot = false);
    bool createAndPackAtlas(const std::vector<char16_t> &glyphs);

    // fallback font management
    FT_Face getFontFaceForGlyph(char16_t ch, int &fontIndex);
    bool loadGlyphFromFace(char16_t ch, FT_Face face, int fontIndex);

    void buildGlyphGeometry(const GLYPH_METRICS &gm, const vec3 &basePos, float advanceX, size_t &vertexCount);
    void buildStringGeometry(const UString &text, size_t &vertexCount);

    static std::unique_ptr<Channel[]> unpackMonoBitmap(const FT_Bitmap &bitmap);

    // shared resource initialization
    static void discoverSystemFallbacks();
    static bool loadFallbackFont(const UString &fontPath, bool isSystemFont = false);

    // helper to set font size on any face for this font instance
    void setFaceSize(FT_Face face) const;

    std::vector<char16_t> m_vGlyphs;
    std::unordered_map<char16_t, bool> m_vGlyphExistence;
    std::unordered_map<char16_t, GLYPH_METRICS> m_vGlyphMetrics;

    std::unique_ptr<VertexArrayObject> m_vao;
    TextBatch m_batchQueue;
    std::vector<vec3> m_vertices;
    std::vector<vec2> m_texcoords;

    std::unique_ptr<TextureAtlas> m_textureAtlas{nullptr};

    // per-instance freetype resources (only primary font face)
    FT_Face m_ftFace;  // primary font face

    int m_iFontSize;
    int m_iFontDPI;
    float m_fHeight;
    GLYPH_METRICS m_errorGlyph;

    // dynamic atlas management
    int m_staticRegionHeight;  // height of statically packed region
    int m_dynamicRegionY;      // Y coordinate where dynamic region starts
    int m_slotsPerRow;         // number of slots per row in dynamic region
    std::vector<DynamicSlot> m_dynamicSlots;
    std::unordered_map<char16_t, int> m_dynamicSlotMap;  // character -> slot index for O(1) lookup
    uint64_t m_currentTime;                              // for LRU tracking
    bool m_atlasNeedsReload;                             // flag to batch atlas reloads

    bool m_batchActive;
    bool m_bFreeTypeInitialized;
    bool m_bAntialiasing;

    // should we look for fallbacks for this specific font face
    bool m_bTryFindFallbacks;
};

#endif
