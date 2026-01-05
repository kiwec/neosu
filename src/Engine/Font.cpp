//========== Copyright (c) 2015, PG & 2025, WH, All rights reserved. ============//
//
// Purpose:		freetype font wrapper with unicode support
//
// $NoKeywords: $fnt
//===============================================================================//

#include "Font.h"

#include "ConVar.h"
#include "Engine.h"
#include "File.h"
#include "FontTypeMap.h"
#include "ResourceManager.h"
#include "VertexArrayObject.h"
#include "TextureAtlas.h"
#include "Logging.h"
#include "Environment.h"
#include "SyncMutex.h"
#include "Image.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <cassert>
#include <unordered_set>
#include <memory>
#include <unordered_map>

#include <freetype/freetype.h>
#include <freetype/ftbitmap.h>
#include <freetype/ftglyph.h>
#include <freetype/ftoutln.h>
#include <freetype/fttrigon.h>
#include <ft2build.h>

// TODO: use fontconfig on linux?
#ifdef _WIN32
#include "WinDebloatDefs.h"
#include <shlobj.h>
#include <windows.h>
#endif

namespace {  // static namespace

// constants for atlas generation and rendering
constexpr const float ATLAS_OCCUPANCY_TARGET{0.75f};  // target atlas occupancy before resize

// how much larger to make atlas for dynamic region
// we initially pack the ASCII characters + initial characters into a region,
// then dynamically loaded glyphs are placed in the remaining space in fixed-size slots (not packed)
// this maximizes the amount of fallback glyphs we can have loaded at once for a fixed amount of memory usage
constexpr float ATLAS_SIZE_MULTIPLIER{4.0f};

// size of each dynamic slot
constexpr int DYNAMIC_SLOT_SIZE{64};

constexpr const size_t MIN_ATLAS_SIZE{256};
constexpr const size_t MAX_ATLAS_SIZE{4096};

constexpr const char16_t UNKNOWN_CHAR{u'?'};  // ASCII '?'

constexpr const size_t VERTS_PER_VAO{Env::cfg(REND::GLES32 | REND::DX11) ? 6 : 4};

// other shared-across-instances things
struct FallbackFont {
    UString fontPath;
    FT_Face face;
    bool isSystemFont;
};

// global shared freetype resources
FT_Library s_sharedFtLibrary{nullptr};
std::vector<FallbackFont> s_sharedFallbackFonts;
std::unordered_set<char16_t> s_sharedFallbackFaceBlacklist;

bool s_sharedFtLibraryInitialized{false};
bool s_sharedFallbacksInitialized{false};

Sync::shared_mutex s_sharedResourcesMutex;

}  // namespace

// implementation details for each McFont object
struct McFontImpl final {
    NOCOPY_NOMOVE(McFontImpl);
    McFont *m_parent;

   public:
    // Internal data structures and state
    struct GLYPH_METRICS {
        char16_t character;
        unsigned int uvPixelsX, uvPixelsY;
        unsigned int sizePixelsX, sizePixelsY;
        int left, top, width, rows;
        float advance_x;
        int fontIndex;  // which font this glyph came from (0 = primary, >0 = fallback)
    };

    struct BatchEntry {
        UString text;
        vec3 pos{0.f};
        Color color;
    };

    struct TextBatch {
        size_t totalVerts{0};
        size_t usedEntries{0};
        std::vector<BatchEntry> entryList;
    };

    // texture atlas dynamic slot management
    struct DynamicSlot {
        int x, y;            // position in atlas
        char16_t character;  // character in this slot (0 if empty)
        uint64_t lastUsed;   // for LRU eviction
        bool occupied;
    };

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
    bool m_bHeightManuallySet{false};

    // should we look for fallbacks for this specific font face
    bool m_bTryFindFallbacks;

   public:
    McFontImpl() = delete;

    McFontImpl(McFont *parent, int fontSize, bool antialiasing, int fontDPI)
        : m_parent(parent),
          m_vao(g->createVertexArrayObject((Env::cfg(REND::GLES32 | REND::DX11) ? DrawPrimitive::PRIMITIVE_TRIANGLES
                                                                                : DrawPrimitive::PRIMITIVE_QUADS),
                                           DrawUsageType::USAGE_DYNAMIC, false)) {
        std::vector<char16_t> characters;
        characters.reserve(96);  // reserve space for basic ASCII, load the rest as needed
        for(int i = 32; i < 128; i++) {
            characters.push_back(static_cast<char16_t>(i));
        }
        m_bTryFindFallbacks = true;
        constructor(characters.data(), characters.size(), fontSize, antialiasing, fontDPI);
    }

    McFontImpl(McFont *parent, const char16_t *characters, size_t numCharacters, int fontSize, bool antialiasing,
               int fontDPI)
        : m_parent(parent),
          m_vao(g->createVertexArrayObject((Env::cfg(REND::GLES32 | REND::DX11) ? DrawPrimitive::PRIMITIVE_TRIANGLES
                                                                                : DrawPrimitive::PRIMITIVE_QUADS),
                                           DrawUsageType::USAGE_DYNAMIC, false)) {
        // don't try to find fallbacks if we had an explicitly-passed character set on construction
        m_bTryFindFallbacks = false;
        constructor(characters, numCharacters, fontSize, antialiasing, fontDPI);
    }

    void constructor(const char16_t *characters, size_t numCharacters, int fontSize, bool antialiasing, int fontDPI) {
        m_iFontSize = fontSize;
        m_bAntialiasing = antialiasing;
        m_iFontDPI = fontDPI;
        m_fHeight = 1.0f;
        m_batchActive = false;
        m_batchQueue.totalVerts = 0;
        m_batchQueue.usedEntries = 0;

        // per-instance freetype initialization state
        m_ftFace = nullptr;
        m_bFreeTypeInitialized = false;

        // initialize dynamic atlas management
        m_staticRegionHeight = 0;
        m_dynamicRegionY = 0;
        m_slotsPerRow = 0;
        m_currentTime = 0;
        m_atlasNeedsReload = false;

        // setup error glyph
        m_errorGlyph = {.character = UNKNOWN_CHAR,
                        .uvPixelsX = 10,
                        .uvPixelsY = 1,
                        .sizePixelsX = 1,
                        .sizePixelsY = 0,
                        .left = 0,
                        .top = 10,
                        .width = 10,
                        .rows = 1,
                        .advance_x = 0,
                        .fontIndex = 0};

        // pre-allocate space for initial glyphs
        m_vGlyphs.reserve(numCharacters);
        for(int i = 0; i < numCharacters; ++i) {
            addGlyph(characters[i]);
        }
    }

    ~McFontImpl() { destroy(); }

    void init() {
        if(!m_parent->isAsyncReady()) return;  // failed

        // finalize atlas texture
        resourceManager->loadResource(m_textureAtlas.get());

        m_parent->setReady(true);
    }

    void initAsync() {
        debugLog("Loading font: {:s}", m_parent->sFilePath);
        assert(s_sharedFtLibraryInitialized);
        assert(s_sharedFallbacksInitialized);

        if(!initializeFreeType()) return;

        // set font size for this instance's primary face
        setFaceSize(m_ftFace);

        // load metrics for all initial glyphs
        for(char16_t ch : m_vGlyphs) {
            loadGlyphMetrics(ch);
        }

        // create atlas and render all glyphs
        if(!createAndPackAtlas(m_vGlyphs)) return;

        // precalculate average/max ASCII glyph height (unless it was already set manually)
        if(!m_bHeightManuallySet && m_fHeight > 0.f) {
            m_fHeight = 0.0f;
            for(int i = 32; i < 128; i++) {
                const int curHeight = getGlyphMetrics(static_cast<char16_t>(i)).top;
                m_fHeight = std::max(m_fHeight, static_cast<float>(curHeight));
            }
        }

        m_parent->setAsyncReady(true);
    }

    void destroy() {
        // only clean up per-instance resources (primary font face and atlas)
        // shared resources are cleaned up separately via cleanupSharedResources()

        if(m_bFreeTypeInitialized) {
            if(m_ftFace) {
                FT_Done_Face(m_ftFace);
                m_ftFace = nullptr;
            }
            m_bFreeTypeInitialized = false;
        }

        m_vGlyphMetrics.clear();
        m_dynamicSlots.clear();
        m_dynamicSlotMap.clear();

        if(!m_bHeightManuallySet) {
            m_fHeight = 1.0f;
        }

        m_atlasNeedsReload = false;
    }

    void drawString(const UString &text) {
        if(!m_parent->isReady()) return;

        if(text.length() == 0 || text.length() > cv::r_drawstring_max_string_length.getInt()) return;

        m_vao->clear();

        const size_t totalVerts = text.length() * VERTS_PER_VAO;
        m_vao->reserve(totalVerts);
        m_vertices.resize(totalVerts);
        m_texcoords.resize(totalVerts);

        size_t vertexCount = 0;
        buildStringGeometry(text, vertexCount);

        m_vao->setVertices(m_vertices);
        m_vao->setTexcoords(m_texcoords);

        m_textureAtlas->getAtlasImage()->bind();
        g->drawVAO(m_vao.get());

        if(cv::r_debug_drawstring_unbind.getBool()) m_textureAtlas->getAtlasImage()->unbind();
    }

    void beginBatch() {
        m_batchActive = true;
        m_batchQueue.totalVerts = 0;
        m_batchQueue.usedEntries = 0;  // don't clear/reallocate, reuse the entries instead
    }

    void addToBatch(const UString &text, const vec3 &pos, Color color = 0xffffffff) {
        size_t verts{};
        if(!m_batchActive || (verts = text.length() * VERTS_PER_VAO) == 0) return;
        m_batchQueue.totalVerts += verts;

        if(m_batchQueue.usedEntries < m_batchQueue.entryList.size()) {
            // reuse existing entry
            BatchEntry &entry = m_batchQueue.entryList[m_batchQueue.usedEntries];
            entry.text = text;
            entry.pos = pos;
            entry.color = color;
        } else {
            // need to add new entry
            m_batchQueue.entryList.push_back({text, pos, color});
        }
        m_batchQueue.usedEntries++;
    }

    void flushBatch() {
        if(!m_batchActive || !m_batchQueue.totalVerts) {
            m_batchActive = false;
            return;
        }

        static std::vector<Color> colors;
        colors.clear();
        m_vertices.resize(m_batchQueue.totalVerts);
        m_texcoords.resize(m_batchQueue.totalVerts);
        m_vao->clear();
        m_vao->reserve(m_batchQueue.totalVerts);

        size_t currentVertex = 0;
        for(size_t i = 0; i < m_batchQueue.usedEntries; i++) {
            const auto &entry = m_batchQueue.entryList[i];
            const size_t stringStart = currentVertex;
            buildStringGeometry(entry.text, currentVertex);

            for(size_t j = stringStart; j < currentVertex; j++) {
                m_vertices[j] += entry.pos;
                colors.push_back(entry.color);
            }
        }

        m_vao->setVertices(m_vertices);
        m_vao->setTexcoords(m_texcoords);
        m_vao->setColors(colors);

        m_textureAtlas->getAtlasImage()->bind();

        g->drawVAO(m_vao.get());

        if(cv::r_debug_drawstring_unbind.getBool()) m_textureAtlas->getAtlasImage()->unbind();

        m_batchActive = false;
    }

    void setSize(int fontSize) { m_iFontSize = fontSize; }
    void setDPI(int dpi) { m_iFontDPI = dpi; }
    void setHeight(float height) {
        m_fHeight = height;
        m_bHeightManuallySet = true;
    }

    [[nodiscard]] inline int getSize() const { return m_iFontSize; }
    [[nodiscard]] inline int getDPI() const { return m_iFontDPI; }
    [[nodiscard]] inline float getHeight() const { return m_fHeight; }

    [[nodiscard]] float getGlyphWidth(char16_t character) const {
        if(!m_parent->isReady()) return 1.0f;

        return static_cast<float>(getGlyphMetrics(character).advance_x);
    }
    [[nodiscard]] float getGlyphHeight(char16_t character) const {
        if(!m_parent->isReady()) return 1.0f;

        return static_cast<float>(getGlyphMetrics(character).top);
    }
    [[nodiscard]] float getStringWidth(const UString &text) const {
        if(!m_parent->isReady()) return 1.0f;

        float width = 0.0f;
        for(int i = 0; i < text.length(); i++) {
            width += getGlyphMetrics(text[i]).advance_x;
        }
        return width;
    }
    [[nodiscard]] float getStringHeight(const UString &text) const {
        if(!m_parent->isReady()) return 1.0f;

        float height = 0.0f;
        for(int i = 0; i < text.length(); i++) {
            height = std::max(height, static_cast<float>(getGlyphMetrics(text[i]).top));
        }
        return height;
    }
    [[nodiscard]] std::vector<UString> wrap(const UString &text, f64 max_width) const {
        std::vector<UString> lines;
        lines.emplace_back();

        UString word{};
        u32 line = 0;
        f64 line_width = 0.0;
        f64 word_width = 0.0;
        for(int i = 0; i < text.length(); i++) {
            if(text[i] == u'\n') {
                lines[line].append(word);
                lines.emplace_back();
                line++;
                line_width = 0.0;
                word.clear();
                word_width = 0.0;
                continue;
            }

            f32 char_width = getGlyphWidth(text[i]);

            if(text[i] == u' ') {
                lines[line].append(word);
                line_width += word_width;
                word.clear();
                word_width = 0.0;

                if(line_width + char_width > max_width) {
                    // Ignore spaces at the end of a line
                    lines.emplace_back();
                    line++;
                    line_width = 0.0;
                } else if(line_width > 0.0) {
                    lines[line].append(u' ');
                    line_width += char_width;
                }
            } else {
                if(word_width + char_width > max_width) {
                    // Split word onto new line
                    lines[line].append(word);
                    lines.emplace_back();
                    line++;
                    line_width = 0.0;
                    word.clear();
                    word_width = 0.0;
                } else if(line_width + word_width + char_width > max_width) {
                    // Wrap word on new line
                    lines.emplace_back();
                    line++;
                    line_width = 0.0;
                    word.append(text[i]);
                    word_width += char_width;
                } else {
                    // Add character to word
                    word.append(text[i]);
                    word_width += char_width;
                }
            }
        }

        // Don't forget! ;)
        lines[line].append(word);

        return lines;
    }

    // Internal helper methods

    const GLYPH_METRICS &getGlyphMetrics(char16_t ch) const {
        auto it = m_vGlyphMetrics.find(ch);
        if(it != m_vGlyphMetrics.end()) return it->second;

        // attempt dynamic loading for unicode characters
        if(m_bTryFindFallbacks && const_cast<McFontImpl *>(this)->loadGlyphDynamic(ch)) {
            it = m_vGlyphMetrics.find(ch);
            if(it != m_vGlyphMetrics.end()) return it->second;
        }

        // fallback to unknown character glyph
        it = m_vGlyphMetrics.find(UNKNOWN_CHAR);
        if(it != m_vGlyphMetrics.end()) return it->second;

        debugLog("Font Error: Missing default backup glyph (UNKNOWN_CHAR)?");
        return m_errorGlyph;
    }

    forceinline bool hasGlyph(char16_t ch) const { return m_vGlyphMetrics.find(ch) != m_vGlyphMetrics.end(); };

    bool addGlyph(char16_t ch) {
        if(m_vGlyphExistence.find(ch) != m_vGlyphExistence.end() || ch < 32) return false;

        m_vGlyphs.push_back(ch);
        m_vGlyphExistence[ch] = true;
        return true;
    }

    bool loadGlyphDynamic(char16_t ch) {
        if(!m_bFreeTypeInitialized || hasGlyph(ch)) return hasGlyph(ch);

        int fontIndex = 0;
        FT_Face targetFace = getFontFaceForGlyph(ch, fontIndex);

        if(!targetFace) {
            if(cv::r_debug_font_unicode.getBool()) {
                // character not supported by any available font
                const char *charRange = FontTypeMap::getCharacterRangeName(ch);
                if(charRange)
                    debugLog("Font Warning: Character U+{:04X} ({:s}) not supported by any font", (unsigned int)ch,
                             charRange);
                else
                    debugLog("Font Warning: Character U+{:04X} not supported by any font", (unsigned int)ch);
            }
            return false;
        }

        logIf(cv::r_debug_font_unicode.getBool() && fontIndex > 0,
              "Font Info (for font resource {}): Using fallback font #{:d} for character U+{:04X}", m_parent->getName(),
              fontIndex, (unsigned int)ch);

        // load glyph from the selected font face
        if(!loadGlyphFromFace(ch, targetFace, fontIndex)) return false;

        const auto &metrics = m_vGlyphMetrics[ch];

        // check if we need atlas space for non-empty glyphs
        if(metrics.sizePixelsX > 0 && metrics.sizePixelsY > 0) {
            // allocate dynamic slot (always fits, will clip if necessary)
            int slotIndex = allocateDynamicSlot(ch);
            const DynamicSlot &slot = m_dynamicSlots[slotIndex];

            // warn about clipping if glyph is oversized
            const int maxSlotContent = DYNAMIC_SLOT_SIZE - 2 * TextureAtlas::ATLAS_PADDING;
            if(metrics.sizePixelsX > maxSlotContent || metrics.sizePixelsY > maxSlotContent) {
                if(cv::r_debug_font_unicode.getBool()) {
                    debugLog("Font Info: Clipping oversized glyph U+{:04X} ({}x{}) to fit dynamic slot ({}x{})",
                             (u32)ch, metrics.sizePixelsX, metrics.sizePixelsY, maxSlotContent, maxSlotContent);
                }
            }

            // render glyph to slot (with padding, will clip if necessary)
            renderGlyphToAtlas(ch, slot.x + TextureAtlas::ATLAS_PADDING, slot.y + TextureAtlas::ATLAS_PADDING,
                               targetFace, true /*dynamic*/);

            // update metrics with slot position
            GLYPH_METRICS &glyphMetrics = m_vGlyphMetrics[ch];
            glyphMetrics.uvPixelsX = static_cast<u32>(slot.x + TextureAtlas::ATLAS_PADDING);
            glyphMetrics.uvPixelsY = static_cast<u32>(slot.y + TextureAtlas::ATLAS_PADDING);

            // flag that atlas needs reload
            m_atlasNeedsReload = true;

            if(cv::r_debug_font_unicode.getBool()) {
                debugLog("Font Info: Placed glyph U+{:04X} in dynamic slot {} at ({}, {})", (u32)ch, slotIndex, slot.x,
                         slot.y);
            }
        }

        addGlyph(ch);
        return true;
    }

    // atlas management methods
    int allocateDynamicSlot(char16_t ch) {
        m_currentTime++;

        // look for free slot
        for(size_t i = 0; i < m_dynamicSlots.size(); i++) {
            auto &dynamicSlot = m_dynamicSlots[i];
            if(!dynamicSlot.occupied) {
                dynamicSlot.character = ch;
                dynamicSlot.lastUsed = m_currentTime;
                dynamicSlot.occupied = true;
                m_dynamicSlotMap[ch] = static_cast<int>(i);
                return static_cast<int>(i);
            }
        }

        // no free slots, find LRU slot
        int lruIndex = 0;
        uint64_t oldestTime = m_dynamicSlots[0].lastUsed;
        for(size_t i = 1; i < m_dynamicSlots.size(); i++) {
            auto &dynamicSlot = m_dynamicSlots[i];
            if(dynamicSlot.lastUsed < oldestTime) {
                oldestTime = dynamicSlot.lastUsed;
                lruIndex = static_cast<int>(i);
            }
        }

        // evict the LRU slot
        auto &dynamicLRUSlot = m_dynamicSlots[lruIndex];
        if(dynamicLRUSlot.character != 0) {
            // remove evicted character from slot map
            m_dynamicSlotMap.erase(dynamicLRUSlot.character);

            // HACK: clear the slot content area to remove leftover pixels from previous glyph
            // this should not be necessary (perf), but otherwise, a single-pixel border can appear on the right and bottom sides of the glyph rect
            const int maxSlotContent = DYNAMIC_SLOT_SIZE - 2 * TextureAtlas::ATLAS_PADDING;
            m_textureAtlas->clearRegion(dynamicLRUSlot.x + TextureAtlas::ATLAS_PADDING,
                                        dynamicLRUSlot.y + TextureAtlas::ATLAS_PADDING, maxSlotContent, maxSlotContent,
                                        false, true);

            // remove evicted character from metrics and existence map
            m_vGlyphMetrics.erase(dynamicLRUSlot.character);
            m_vGlyphExistence.erase(dynamicLRUSlot.character);
        }

        dynamicLRUSlot.character = ch;
        dynamicLRUSlot.lastUsed = m_currentTime;
        dynamicLRUSlot.occupied = true;
        m_dynamicSlotMap[ch] = lruIndex;

        return lruIndex;
    }

    void markSlotUsed(char16_t ch) {
        auto it = m_dynamicSlotMap.find(ch);
        if(it != m_dynamicSlotMap.end()) {
            m_currentTime++;
            m_dynamicSlots[it->second].lastUsed = m_currentTime;
        }
    }

    void initializeDynamicRegion(int atlasSize) {
        // calculate dynamic region layout
        m_dynamicRegionY = m_staticRegionHeight + TextureAtlas::ATLAS_PADDING;
        m_slotsPerRow = atlasSize / DYNAMIC_SLOT_SIZE;

        // initialize dynamic slots
        const int dynamicHeight = atlasSize - m_dynamicRegionY;
        const int slotsPerColumn = dynamicHeight / DYNAMIC_SLOT_SIZE;
        const int totalSlots = m_slotsPerRow * slotsPerColumn;

        m_dynamicSlots.clear();
        m_dynamicSlots.reserve(totalSlots);
        m_dynamicSlotMap.clear();

        for(int row = 0; row < slotsPerColumn; row++) {
            for(int col = 0; col < m_slotsPerRow; col++) {
                DynamicSlot slot{.x = col * DYNAMIC_SLOT_SIZE,
                                 .y = m_dynamicRegionY + row * DYNAMIC_SLOT_SIZE,
                                 .character = 0,
                                 .lastUsed = 0,
                                 .occupied = false};
                m_dynamicSlots.push_back(slot);
            }
        }

        if(cv::r_debug_font_unicode.getBool()) {
            debugLog("Font Info: Initialized dynamic region with {} slots ({}x{} each) starting at y={}", totalSlots,
                     DYNAMIC_SLOT_SIZE, DYNAMIC_SLOT_SIZE, m_dynamicRegionY);
        }
    }

    // consolidated glyph processing methods
    bool initializeFreeType() {
        assert(s_sharedFtLibraryInitialized);

        // load this font's primary face
        {
            Sync::unique_lock<Sync::shared_mutex> lock(s_sharedResourcesMutex);
            if(FT_New_Face(s_sharedFtLibrary, m_parent->sFilePath.c_str(), 0, &m_ftFace)) {
                engine->showMessageError("Font Error", "Couldn't load font file!");
                return false;
            }
        }

        if(FT_Select_Charmap(m_ftFace, ft_encoding_unicode)) {
            engine->showMessageError("Font Error", "FT_Select_Charmap() failed!");
            FT_Done_Face(m_ftFace);
            return false;
        }

        m_bFreeTypeInitialized = true;
        return true;
    }

    bool loadGlyphMetrics(char16_t ch) {
        if(!m_bFreeTypeInitialized) return false;

        int fontIndex = 0;
        FT_Face face = getFontFaceForGlyph(ch, fontIndex);

        if(!face) return false;

        return loadGlyphFromFace(ch, face, fontIndex);
    }

    std::unique_ptr<Color[]> createExpandedBitmapData(const FT_Bitmap &bitmap) {
        auto expandedData = std::make_unique_for_overwrite<Color[]>(static_cast<size_t>(bitmap.width) * bitmap.rows);

        std::unique_ptr<Channel[]> monoBitmapUnpacked{nullptr};
        if(!m_bAntialiasing) monoBitmapUnpacked = unpackMonoBitmap(bitmap);

        for(u32 j = 0; j < bitmap.rows; j++) {
            for(u32 k = 0; k < bitmap.width; k++) {
                const sSz expandedIdx = (k + (bitmap.rows - j - 1) * bitmap.width);

                Channel alpha = m_bAntialiasing                                ? bitmap.buffer[k + bitmap.width * j]
                                : monoBitmapUnpacked[k + bitmap.width * j] > 0 ? 255
                                                                               : 0;
                expandedData[expandedIdx] = argb(alpha, 255, 255, 255);
            }
        }

        return expandedData;
    }

    void renderGlyphToAtlas(char16_t ch, int x, int y, FT_Face face = nullptr, bool isDynamicSlot = false) {
        if(!face) {
            // fall back to getting the face again if not provided
            int fontIndex = 0;
            face = getFontFaceForGlyph(ch, fontIndex);
            if(!face) return;
        } else if(face != m_ftFace) {
            // make sure fallback face has the correct size for this font instance
            setFaceSize(face);
        }

        if(FT_Load_Glyph(face, FT_Get_Char_Index(face, ch),
                         m_bAntialiasing ? FT_LOAD_TARGET_NORMAL : FT_LOAD_TARGET_MONO))
            return;

        FT_Glyph glyph{};
        if(FT_Get_Glyph(face->glyph, &glyph)) return;

        FT_Glyph_To_Bitmap(&glyph, m_bAntialiasing ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO, nullptr, 1);

        auto bitmapGlyph = reinterpret_cast<FT_BitmapGlyph>(glyph);
        auto &bitmap = bitmapGlyph->bitmap;

        if(bitmap.width > 0 && bitmap.rows > 0) {
            const int atlasWidth = m_textureAtlas->getWidth();
            const int atlasHeight = m_textureAtlas->getHeight();

            int availableWidth = std::min(static_cast<int>(bitmap.width), atlasWidth - x);
            int availableHeight = std::min(static_cast<int>(bitmap.rows), atlasHeight - y);

            // only apply slot size clipping for dynamic glyphs
            // TODO: consolidate the logic for this, it's checked in too many places
            // doing redundant work to create expanded then clipped instead of just all in 1 go
            if(isDynamicSlot) {
                const int maxSlotContent = DYNAMIC_SLOT_SIZE - 2 * TextureAtlas::ATLAS_PADDING;
                availableWidth = std::min(availableWidth, maxSlotContent);
                availableHeight = std::min(availableHeight, maxSlotContent);
            }

            auto expandedData = createExpandedBitmapData(bitmap);

            // if clipping is needed, create clipped data
            if(std::cmp_less(availableWidth, bitmap.width) || std::cmp_less(availableHeight, bitmap.rows)) {
                auto clippedData =
                    std::make_unique_for_overwrite<Color[]>(static_cast<size_t>(availableWidth) * availableHeight);
                for(i32 row = 0; row < availableHeight; row++) {
                    for(i32 col = 0; col < availableWidth; col++) {
                        const u32 srcIdx = row * bitmap.width + col;
                        const u32 dstIdx = row * availableWidth + col;
                        clippedData[dstIdx] = expandedData[srcIdx];
                    }
                }
                m_textureAtlas->putAt(x, y, availableWidth, availableHeight, false, true, clippedData.get());
            } else {
                m_textureAtlas->putAt(x, y, bitmap.width, bitmap.rows, false, true, expandedData.get());
            }

            // update metrics with atlas coordinates
            GLYPH_METRICS &metrics = m_vGlyphMetrics[ch];
            metrics.uvPixelsX = static_cast<u32>(x);
            metrics.uvPixelsY = static_cast<u32>(y);
        }

        FT_Done_Glyph(glyph);
    }

    bool createAndPackAtlas(const std::vector<char16_t> &glyphs) {
        if(glyphs.empty()) return true;

        // prepare packing rectangles
        std::vector<TextureAtlas::PackRect> packRects;
        std::vector<char16_t> rectsToChars;
        packRects.reserve(glyphs.size());
        rectsToChars.reserve(glyphs.size());

        size_t rectIndex = 0;
        for(char16_t ch : glyphs) {
            const auto &metrics = m_vGlyphMetrics[ch];
            if(metrics.sizePixelsX > 0 && metrics.sizePixelsY > 0) {
                // add packrect
                TextureAtlas::PackRect pr{.x = 0,
                                          .y = 0,
                                          .width = static_cast<int>(metrics.sizePixelsX),
                                          .height = static_cast<int>(metrics.sizePixelsY),
                                          .id = static_cast<int>(rectIndex)};
                packRects.push_back(pr);
                rectsToChars.push_back(ch);
                rectIndex++;
            }
        }

        if(packRects.empty()) return true;

        // calculate optimal size for static glyphs and create larger atlas for dynamic region
        const size_t staticAtlasSize =
            TextureAtlas::calculateOptimalSize(packRects, ATLAS_OCCUPANCY_TARGET, MIN_ATLAS_SIZE, MAX_ATLAS_SIZE);

        const size_t totalAtlasSize = static_cast<size_t>(staticAtlasSize * ATLAS_SIZE_MULTIPLIER);
        const size_t finalAtlasSize = std::min(totalAtlasSize, static_cast<size_t>(MAX_ATLAS_SIZE));

        resourceManager->requestNextLoadUnmanaged();
        m_textureAtlas.reset(resourceManager->createTextureAtlas(static_cast<int>(finalAtlasSize),
                                                                 static_cast<int>(finalAtlasSize), m_bAntialiasing));

        // pack glyphs into static region only
        if(!m_textureAtlas->packRects(packRects)) {
            engine->showMessageError("Font Error", "Failed to pack glyphs into atlas!");
            return false;
        }

        // find the height used by static glyphs
        m_staticRegionHeight = 0;
        for(const auto &rect : packRects) {
            m_staticRegionHeight = std::max(m_staticRegionHeight, rect.y + rect.height + TextureAtlas::ATLAS_PADDING);
        }

        // render all packed glyphs to static region
        for(const auto &rect : packRects) {
            const char16_t ch = rectsToChars[rect.id];

            // get the correct font face for this glyph
            int fontIndex = 0;
            FT_Face face = getFontFaceForGlyph(ch, fontIndex);

            renderGlyphToAtlas(ch, rect.x, rect.y, face, false /*not dynamic*/);
        }

        // initialize dynamic region after static glyphs are placed
        initializeDynamicRegion(static_cast<int>(finalAtlasSize));

        return true;
    }

    // fallback font management
    FT_Face getFontFaceForGlyph(char16_t ch, int &fontIndex) {
        fontIndex = 0;

        // quick blacklist check
        if(m_bTryFindFallbacks && m_parent->isAsyncReady()) {  // skip blacklisting during initial load
            Sync::shared_lock<Sync::shared_mutex> lock(s_sharedResourcesMutex);
            if(s_sharedFallbackFaceBlacklist.contains(ch)) {
                return nullptr;
            }
        }

        // then check primary font
        FT_UInt glyphIndex = FT_Get_Char_Index(m_ftFace, ch);
        if(glyphIndex != 0) return m_ftFace;
        if(!m_bTryFindFallbacks) return nullptr;

        // search through shared fallback fonts
        FT_Face foundFace = nullptr;
        {
            Sync::shared_lock<Sync::shared_mutex> lock(s_sharedResourcesMutex);
            for(size_t i = 0; i < s_sharedFallbackFonts.size(); ++i) {
                glyphIndex = FT_Get_Char_Index(s_sharedFallbackFonts[i].face, ch);
                if(glyphIndex != 0) {
                    fontIndex = static_cast<int>(i + 1);
                    foundFace = s_sharedFallbackFonts[i].face;
                    break;
                }
            }
        }

        if(foundFace) {
            // note: setFaceSize() and subsequent FT operations on shared faces
            // may need additional synchronization depending on usage patterns
            setFaceSize(foundFace);
            return foundFace;
        }

        // character not found in any font, add to blacklist
        // NOTE: skip blacklisting during initial load
        // this is to allow us to more lazily synchronize with other fonts that may be loading simultaneously
        // i.e. only add to blacklist when we actually try to draw a string with this glyph
        if(m_parent->isAsyncReady()) {
            Sync::unique_lock<Sync::shared_mutex> lock(s_sharedResourcesMutex);
            s_sharedFallbackFaceBlacklist.insert(ch);
        }

        return nullptr;
    }
    bool loadGlyphFromFace(char16_t ch, FT_Face face, int fontIndex) {
        if(FT_Load_Glyph(face, FT_Get_Char_Index(face, ch),
                         m_bAntialiasing ? FT_LOAD_TARGET_NORMAL : FT_LOAD_TARGET_MONO)) {
            debugLog("Font Error: Failed to load glyph for character {:d} from font index {:d}", (wint_t)ch, fontIndex);
            return false;
        }

        FT_Glyph glyph{};
        if(FT_Get_Glyph(face->glyph, &glyph)) {
            debugLog("Font Error: Failed to get glyph for character {:d} from font index {:d}", (wint_t)ch, fontIndex);
            return false;
        }

        FT_Glyph_To_Bitmap(&glyph, m_bAntialiasing ? FT_RENDER_MODE_NORMAL : FT_RENDER_MODE_MONO, nullptr, 1);

        auto bitmapGlyph = reinterpret_cast<FT_BitmapGlyph>(glyph);

        // store metrics for the character
        GLYPH_METRICS &metrics = m_vGlyphMetrics[ch];
        metrics.character = ch;
        metrics.left = bitmapGlyph->left;
        metrics.top = bitmapGlyph->top;
        metrics.width = bitmapGlyph->bitmap.width;
        metrics.rows = bitmapGlyph->bitmap.rows;
        metrics.advance_x = static_cast<float>(face->glyph->advance.x >> 6);
        metrics.fontIndex = fontIndex;

        // initialize texture coordinates (will be updated when rendered to atlas)
        metrics.uvPixelsX = 0;
        metrics.uvPixelsY = 0;
        metrics.sizePixelsX = bitmapGlyph->bitmap.width;
        metrics.sizePixelsY = bitmapGlyph->bitmap.rows;

        FT_Done_Glyph(glyph);
        return true;
    }

    void buildGlyphGeometry(const GLYPH_METRICS &gm, const vec3 &basePos, float advanceX, size_t &vertexCount) {
        const float atlasWidth{static_cast<float>(m_textureAtlas->getAtlasImage()->getWidth())};
        const float atlasHeight{static_cast<float>(m_textureAtlas->getAtlasImage()->getHeight())};

        const float x{basePos.x + static_cast<float>(gm.left) + advanceX};
        const float y{basePos.y - static_cast<float>(gm.top - gm.rows)};
        const float z{basePos.z};
        const float sx{static_cast<float>(gm.width)};
        const float sy{static_cast<float>(-gm.rows)};

        const float texX{static_cast<float>(gm.uvPixelsX) / atlasWidth};
        const float texY{static_cast<float>(gm.uvPixelsY) / atlasHeight};
        const float texSizeX{static_cast<float>(gm.sizePixelsX) / atlasWidth};
        const float texSizeY{static_cast<float>(gm.sizePixelsY) / atlasHeight};

        // corners of the "quad"
        vec3 bottomLeft{x, y + sy, z};
        vec3 topLeft{x, y, z};
        vec3 topRight{x + sx, y, z};
        vec3 bottomRight{x + sx, y + sy, z};

        // texcoords
        vec2 texBottomLeft{texX, texY};
        vec2 texTopLeft{texX, texY + texSizeY};
        vec2 texTopRight{texX + texSizeX, texY + texSizeY};
        vec2 texBottomRight{texX + texSizeX, texY};

        const size_t idx{vertexCount};

        if constexpr(VERTS_PER_VAO > 4) {
            // triangles (quads are slower for GL ES because they need to be converted to triangles at submit time)
            // first triangle (bottom-left, top-left, top-right)
            m_vertices[idx] = bottomLeft;
            m_vertices[idx + 1] = topLeft;
            m_vertices[idx + 2] = topRight;

            m_texcoords[idx] = texBottomLeft;
            m_texcoords[idx + 1] = texTopLeft;
            m_texcoords[idx + 2] = texTopRight;

            // second triangle (bottom-left, top-right, bottom-right)
            m_vertices[idx + 3] = bottomLeft;
            m_vertices[idx + 4] = topRight;
            m_vertices[idx + 5] = bottomRight;

            m_texcoords[idx + 3] = texBottomLeft;
            m_texcoords[idx + 4] = texTopRight;
            m_texcoords[idx + 5] = texBottomRight;
        } else {
            // quads
            m_vertices[idx] = bottomLeft;       // bottom-left
            m_vertices[idx + 1] = topLeft;      // top-left
            m_vertices[idx + 2] = topRight;     // top-right
            m_vertices[idx + 3] = bottomRight;  // bottom-right

            m_texcoords[idx] = texBottomLeft;
            m_texcoords[idx + 1] = texTopLeft;
            m_texcoords[idx + 2] = texTopRight;
            m_texcoords[idx + 3] = texBottomRight;
        }
        vertexCount += VERTS_PER_VAO;
    }

    void buildStringGeometry(const UString &text, size_t &vertexCount) {
        float advanceX = 0.0f;
        const int maxGlyphs =
            std::min(text.length(), (int)((double)(m_vertices.size() - vertexCount) / (double)VERTS_PER_VAO));

        for(int i = 0; i < maxGlyphs; i++) {
            const GLYPH_METRICS &gm = getGlyphMetrics(text[i]);
            buildGlyphGeometry(gm, vec3(), advanceX, vertexCount);
            advanceX += gm.advance_x;

            // mark dynamic slot as recently used (if this character is in a dynamic slot)
            markSlotUsed(text[i]);
        }

        // reload atlas if new glyphs were added to dynamic slots
        if(m_atlasNeedsReload) {
            m_textureAtlas->getAtlasImage()->reload();
            m_atlasNeedsReload = false;
        }
    }

    static std::unique_ptr<Channel[]> unpackMonoBitmap(const FT_Bitmap &bitmap) {
        auto result = std::make_unique_for_overwrite<Channel[]>(static_cast<size_t>(bitmap.rows) * bitmap.width);

        for(u32 y = 0; y < bitmap.rows; y++) {
            for(i32 byteIdx = 0; byteIdx < bitmap.pitch; byteIdx++) {
                const u8 byteValue = bitmap.buffer[y * bitmap.pitch + byteIdx];
                const u32 numBitsDone = byteIdx * 8;
                const u32 rowstart = y * bitmap.width + byteIdx * 8;

                // why do these have to be 32bit ints exactly... ill look into it later
                const int bits = std::min(8, static_cast<int>(bitmap.width - numBitsDone));
                for(int bitIdx = 0; bitIdx < bits; bitIdx++) {
                    result[rowstart + bitIdx] = (byteValue & (1 << (7 - bitIdx))) ? 1 : 0;
                }
            }
        }

        return result;
    }

    // helper to set font size on any face for this font instance
    void setFaceSize(FT_Face face) const {
        FT_Set_Char_Size(face, (FT_F26Dot6)(m_iFontSize * 64L), (FT_F26Dot6)(m_iFontSize * 64L), m_iFontDPI,
                         m_iFontDPI);
    }
};

////////////////////////////////////////////////////////////////////////////////////
// Public passthroughs start
////////////////////////////////////////////////////////////////////////////////////

McFont::McFont(std::string filepath, int fontSize, bool antialiasing, int fontDPI)
    : Resource(FONT, std::move(filepath)), pImpl(this, fontSize, antialiasing, fontDPI) {}

McFont::McFont(std::string filepath, const char16_t *characters, size_t numCharacters, int fontSize, bool antialiasing,
               int fontDPI)
    : Resource(FONT, std::move(filepath)), pImpl(this, characters, numCharacters, fontSize, antialiasing, fontDPI) {}

McFont::~McFont() { destroy(); }

void McFont::init() { pImpl->init(); }
void McFont::initAsync() { pImpl->initAsync(); }
void McFont::destroy() { pImpl->destroy(); }

void McFont::setSize(int fontSize) { pImpl->setSize(fontSize); }
void McFont::setDPI(int dpi) { pImpl->setDPI(dpi); }
void McFont::setHeight(float height) { pImpl->setHeight(height); }

int McFont::getSize() const { return pImpl->getSize(); }
int McFont::getDPI() const { return pImpl->getDPI(); }
float McFont::getHeight() const { return pImpl->getHeight(); }

void McFont::drawString(const UString &text) { return pImpl->drawString(text); }
void McFont::beginBatch() { return pImpl->beginBatch(); }
void McFont::addToBatch(const UString &text, const vec3 &pos, Color color) {
    return pImpl->addToBatch(text, pos, color);
}
void McFont::flushBatch() { return pImpl->flushBatch(); }

float McFont::getGlyphWidth(char16_t character) const { return pImpl->getGlyphWidth(character); }
float McFont::getGlyphHeight(char16_t character) const { return pImpl->getGlyphHeight(character); }
float McFont::getStringWidth(const UString &text) const { return pImpl->getStringWidth(text); }
float McFont::getStringHeight(const UString &text) const { return pImpl->getStringHeight(text); }
std::vector<UString> McFont::wrap(const UString &text, f64 max_width) const { return pImpl->wrap(text, max_width); }

////////////////////////////////////////////////////////////////////////////////////
// Public passthroughs end
////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////
// shared/static FreeType management stuff below
////////////////////////////////////////////////////////////////////////////////////

namespace {  // static namespace

bool loadFallbackFont(const UString &fontPath, bool isSystemFont) {
    FT_Face face{};
    if(FT_New_Face(s_sharedFtLibrary, fontPath.toUtf8(), 0, &face)) {
        logIfCV(r_debug_font_unicode, "Font Warning: Failed to load fallback font: {:s}", fontPath);
        return false;
    }

    if(FT_Select_Charmap(face, ft_encoding_unicode)) {
        logIfCV(r_debug_font_unicode, "Font Warning: Failed to select unicode charmap for fallback font: {:s}",
                fontPath);
        FT_Done_Face(face);
        return false;
    }

    // don't set font size here, will be set when the face is used by individual font instances
    s_sharedFallbackFonts.push_back(FallbackFont{fontPath, face, isSystemFont});
    return true;
}

void discoverSystemFallbacks() {
#ifdef MCENGINE_PLATFORM_WINDOWS
    std::string windir;
    windir.resize(MAX_PATH + 1);
    int ret = GetWindowsDirectoryA(windir.data(), MAX_PATH);
    if(ret <= 0) return;
    windir.resize(ret);

    std::vector<std::string> systemFonts = {
        windir + "\\Fonts\\arial.ttf",
        windir + "\\Fonts\\msyh.ttc",      // Microsoft YaHei (Chinese)
        windir + "\\Fonts\\malgun.ttf",    // Malgun Gothic (Korean)
        windir + "\\Fonts\\meiryo.ttc",    // Meiryo (Japanese)
        windir + "\\Fonts\\seguiemj.ttf",  // Segoe UI Emoji
        windir + "\\Fonts\\seguisym.ttf"   // Segoe UI Symbol
    };
#elif defined(MCENGINE_PLATFORM_LINUX)
    // linux system fonts (common locations)
    std::vector<std::string> systemFonts = {"/usr/share/fonts/TTF/dejavu/DejaVuSans.ttf",
                                            "/usr/share/fonts/TTF/liberation/LiberationSans-Regular.ttf",
                                            "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
                                            "/usr/share/fonts/noto/NotoSans-Regular.ttf",
                                            "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc",
                                            "/usr/share/fonts/TTF/noto/NotoColorEmoji.ttf"};
#else  // TODO: loading WOFF fonts in wasm? idk
    std::vector<std::string> systemFonts;
    return;
#endif

    for(auto &fontPath : systemFonts) {
        if(File::existsCaseInsensitive(fontPath) == File::FILETYPE::FILE) {
            loadFallbackFont(fontPath.c_str(), true);
        }
    }
}

}  // namespace

bool McFont::initSharedResources() {
    if(!s_sharedFtLibraryInitialized) {
        if(FT_Init_FreeType(&s_sharedFtLibrary)) {
            engine->showMessageError("Font Error", "FT_Init_FreeType() failed!");
            return false;
        }
    }

    s_sharedFtLibraryInitialized = true;

    // check all bundled fonts first
    std::vector<std::string> bundledFallbacks = env->getFilesInFolder(MCENGINE_FONTS_PATH "/");
    for(const auto &fontName : bundledFallbacks) {
        if(loadFallbackFont(UString{fontName}, false)) {
            logIfCV(r_debug_font_unicode, "Font Info: Loaded bundled fallback font: {:s}", fontName);
        }
    }

    // then find likely system fonts
    if(!Env::cfg(OS::WASM) && cv::font_load_system.getBool()) discoverSystemFallbacks();

    s_sharedFallbacksInitialized = true;

    return s_sharedFtLibraryInitialized && s_sharedFallbacksInitialized;
}

void McFont::cleanupSharedResources() {
    // clean up shared fallback fonts
    for(auto &fallbackFont : s_sharedFallbackFonts) {
        if(fallbackFont.face) FT_Done_Face(fallbackFont.face);
    }
    s_sharedFallbackFonts.clear();
    s_sharedFallbacksInitialized = false;

    // clean up shared freetype library
    if(s_sharedFtLibraryInitialized) {
        if(s_sharedFtLibrary) {
            FT_Done_FreeType(s_sharedFtLibrary);
            s_sharedFtLibrary = nullptr;
        }
        s_sharedFtLibraryInitialized = false;
    }
}
