//========== Copyright (c) 2015, PG & 2025, WH, All rights reserved. ============//
//
// Purpose:		freetype font wrapper with unicode support
//
// $NoKeywords: $fnt
//===============================================================================//

#include "Font.h"

#include <freetype/freetype.h>
#include <freetype/ftbitmap.h>
#include <freetype/ftglyph.h>
#include <freetype/ftoutln.h>
#include <freetype/fttrigon.h>
#include <ft2build.h>

#include <algorithm>
#include <utility>

#include "ConVar.h"
#include "Engine.h"
#include "File.h"
#include "FontTypeMap.h"
#include "ResourceManager.h"
#include "TextureAtlas.h"
#include "Logging.h"
#include "Environment.h"

// TODO: use fontconfig on linux?
#ifdef _WIN32
#include "WinDebloatDefs.h"
#include <shlobj.h>
#include <windows.h>
#endif

// constants for atlas generation and rendering
static constexpr const float ATLAS_OCCUPANCY_TARGET{0.75f};  // target atlas occupancy before resize
static constexpr const size_t MIN_ATLAS_SIZE{256};
static constexpr const size_t MAX_ATLAS_SIZE{4096};
static constexpr const wchar_t UNKNOWN_CHAR{L'?'};  // ASCII '?'

static constexpr const size_t VERTS_PER_VAO{Env::cfg(REND::GLES32 | REND::DX11) ? 6 : 4};

// static member definitions
FT_Library McFont::s_sharedFtLibrary = nullptr;
bool McFont::s_sharedFtLibraryInitialized = false;
std::vector<McFont::FallbackFont> McFont::s_sharedFallbackFonts;
bool McFont::s_sharedFallbacksInitialized = false;
std::unordered_set<wchar_t> McFont::s_sharedFallbackFaceBlacklist;

McFont::McFont(std::string filepath, int fontSize, bool antialiasing, int fontDPI)
    : Resource(std::move(filepath)),
      m_vao((Env::cfg(REND::GLES32 | REND::DX11) ? Graphics::PRIMITIVE::PRIMITIVE_TRIANGLES
                                                 : Graphics::PRIMITIVE::PRIMITIVE_QUADS),
            Graphics::USAGE_TYPE::USAGE_DYNAMIC) {
    std::vector<wchar_t> characters;
    characters.reserve(96);  // reserve space for basic ASCII, load the rest as needed
    for(int i = 32; i < 128; i++) {
        characters.push_back(static_cast<wchar_t>(i));
    }
    m_bTryFindFallbacks = true;
    constructor(characters, fontSize, antialiasing, fontDPI);
}

McFont::McFont(std::string filepath, const std::vector<wchar_t> &characters, int fontSize, bool antialiasing,
               int fontDPI)
    : Resource(std::move(filepath)),
      m_vao((Env::cfg(REND::GLES32 | REND::DX11) ? Graphics::PRIMITIVE::PRIMITIVE_TRIANGLES
                                                 : Graphics::PRIMITIVE::PRIMITIVE_QUADS),
            Graphics::USAGE_TYPE::USAGE_DYNAMIC) {
    m_bTryFindFallbacks = false;
    constructor(characters, fontSize, antialiasing, fontDPI);
}

void McFont::constructor(const std::vector<wchar_t> &characters, int fontSize, bool antialiasing, int fontDPI) {
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
    m_vGlyphs.reserve(characters.size());
    for(wchar_t ch : characters) {
        addGlyph(ch);
    }
}

McFont::~McFont() { destroy(); }

void McFont::init() {
    debugLog("Loading font: {:s}", this->sFilePath);

    if(!initializeFreeType()) return;

    // setup the static fallbacks once
    initializeSharedFallbackFonts();

    // load metrics for all initial glyphs
    for(wchar_t ch : m_vGlyphs) {
        loadGlyphMetrics(ch);
    }

    // create atlas and render all glyphs
    if(!createAndPackAtlas(m_vGlyphs)) return;

    // precalculate average/max ASCII glyph height
    m_fHeight = 0.0f;
    for(int i = 32; i < 128; i++) {
        const int curHeight = getGlyphMetrics(static_cast<wchar_t>(i)).top;
        m_fHeight = std::max(m_fHeight, static_cast<float>(curHeight));
    }

    this->bReady = true;
}

void McFont::initAsync() { this->bAsyncReady = true; }

void McFont::destroy() {
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
    m_fHeight = 1.0f;
    m_atlasNeedsReload = false;
}

bool McFont::addGlyph(wchar_t ch) {
    if(m_vGlyphExistence.find(ch) != m_vGlyphExistence.end() || ch < 32) return false;

    m_vGlyphs.push_back(ch);
    m_vGlyphExistence[ch] = true;
    return true;
}

int McFont::allocateDynamicSlot(wchar_t ch) {
    m_currentTime++;

    // look for free slot
    for(size_t i = 0; i < m_dynamicSlots.size(); i++) {
        if(!m_dynamicSlots[i].occupied) {
            m_dynamicSlots[i].character = ch;
            m_dynamicSlots[i].lastUsed = m_currentTime;
            m_dynamicSlots[i].occupied = true;
            m_dynamicSlotMap[ch] = static_cast<int>(i);
            return static_cast<int>(i);
        }
    }

    // no free slots, find LRU slot
    int lruIndex = 0;
    uint64_t oldestTime = m_dynamicSlots[0].lastUsed;
    for(size_t i = 1; i < m_dynamicSlots.size(); i++) {
        if(m_dynamicSlots[i].lastUsed < oldestTime) {
            oldestTime = m_dynamicSlots[i].lastUsed;
            lruIndex = static_cast<int>(i);
        }
    }

    // evict the LRU slot
    if(m_dynamicSlots[lruIndex].character != 0) {
        // remove evicted character from slot map
        m_dynamicSlotMap.erase(m_dynamicSlots[lruIndex].character);

        // HACK: clear the slot content area to remove leftover pixels from previous glyph
        // this should not be necessary (perf), but otherwise, a single-pixel border can appear on the right and bottom sides of the glyph rect
        const int maxSlotContent = DYNAMIC_SLOT_SIZE - 2 * TextureAtlas::ATLAS_PADDING;
        auto clearData = std::make_unique<Color[]>(static_cast<size_t>(maxSlotContent) * maxSlotContent);
        std::fill_n(clearData.get(), static_cast<size_t>(maxSlotContent) * maxSlotContent, argb(0, 0, 0, 0));
        m_textureAtlas->putAt(m_dynamicSlots[lruIndex].x + TextureAtlas::ATLAS_PADDING,
                              m_dynamicSlots[lruIndex].y + TextureAtlas::ATLAS_PADDING, maxSlotContent, maxSlotContent,
                              false, true, clearData.get());

        // remove evicted character from metrics and existence map
        m_vGlyphMetrics.erase(m_dynamicSlots[lruIndex].character);
        m_vGlyphExistence.erase(m_dynamicSlots[lruIndex].character);
    }

    m_dynamicSlots[lruIndex].character = ch;
    m_dynamicSlots[lruIndex].lastUsed = m_currentTime;
    m_dynamicSlots[lruIndex].occupied = true;
    m_dynamicSlotMap[ch] = lruIndex;

    return lruIndex;
}

void McFont::markSlotUsed(wchar_t ch) {
    auto it = m_dynamicSlotMap.find(ch);
    if(it != m_dynamicSlotMap.end()) {
        m_currentTime++;
        m_dynamicSlots[it->second].lastUsed = m_currentTime;
    }
}

void McFont::initializeDynamicRegion(int atlasSize) {
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

bool McFont::loadGlyphDynamic(wchar_t ch) {
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

    if(cv::r_debug_font_unicode.getBool() && fontIndex > 0) {
        debugLog("Font Info (for font resource {}): Using fallback font #{:d} for character U+{:04X}", getName(),
                 fontIndex, (unsigned int)ch);
    }

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
                debugLog("Font Info: Clipping oversized glyph U+{:04X} ({}x{}) to fit dynamic slot ({}x{})", (u32)ch,
                         metrics.sizePixelsX, metrics.sizePixelsY, maxSlotContent, maxSlotContent);
            }
        }

        // render glyph to slot (with padding, will clip if necessary)
        renderGlyphToAtlas(ch, slot.x + TextureAtlas::ATLAS_PADDING, slot.y + TextureAtlas::ATLAS_PADDING, targetFace,
                           true /*dynamic*/);

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

FT_Face McFont::getFontFaceForGlyph(wchar_t ch, int &fontIndex) {
    fontIndex = 0;

    // first check the quick lookup blacklist set
    if(m_bTryFindFallbacks && s_sharedFallbackFaceBlacklist.contains(ch)) {
        return nullptr;
    }

    // then check primary font
    FT_UInt glyphIndex = FT_Get_Char_Index(m_ftFace, ch);
    if(glyphIndex != 0) return m_ftFace;
    if(!m_bTryFindFallbacks) return nullptr;

    // search through shared fallback fonts if initialized
    if(s_sharedFallbacksInitialized) {
        for(size_t i = 0; i < s_sharedFallbackFonts.size(); ++i) {
            glyphIndex = FT_Get_Char_Index(s_sharedFallbackFonts[i].face, ch);
            if(glyphIndex != 0) {
                fontIndex = static_cast<int>(i + 1);  // offset by 1 since 0 is primary font
                // set the appropriate size for this font instance
                setFaceSize(s_sharedFallbackFonts[i].face);
                return s_sharedFallbackFonts[i].face;
            }
        }
    }

    // add it to the blacklist
    s_sharedFallbackFaceBlacklist.insert(ch);
    return nullptr;  // character not found in any font
}

bool McFont::loadGlyphFromFace(wchar_t ch, FT_Face face, int fontIndex) {
    if(FT_Load_Glyph(face, FT_Get_Char_Index(face, ch),
                     m_bAntialiasing ? FT_LOAD_TARGET_NORMAL : FT_LOAD_TARGET_MONO)) {
        debugLog("Font Error: Failed to load glyph for character {:d} from font index {:d}", (int)ch, fontIndex);
        return false;
    }

    FT_Glyph glyph{};
    if(FT_Get_Glyph(face->glyph, &glyph)) {
        debugLog("Font Error: Failed to get glyph for character {:d} from font index {:d}", (int)ch, fontIndex);
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

void McFont::setFaceSize(FT_Face face) const {
    FT_Set_Char_Size(face, m_iFontSize * 64LL, m_iFontSize * 64LL, m_iFontDPI, m_iFontDPI);
}

bool McFont::initializeFreeType() {
    // initialize shared freetype library
    if(!initializeSharedFreeType()) return false;

    // load this font's primary face
    if(FT_New_Face(s_sharedFtLibrary, this->sFilePath.c_str(), 0, &m_ftFace)) {
        engine->showMessageError("Font Error", "Couldn't load font file!");
        return false;
    }

    if(FT_Select_Charmap(m_ftFace, ft_encoding_unicode)) {
        engine->showMessageError("Font Error", "FT_Select_Charmap() failed!");
        FT_Done_Face(m_ftFace);
        return false;
    }

    // set font size for this instance's primary face
    setFaceSize(m_ftFace);

    m_bFreeTypeInitialized = true;
    return true;
}

bool McFont::loadGlyphMetrics(wchar_t ch) {
    if(!m_bFreeTypeInitialized) return false;

    int fontIndex = 0;
    FT_Face face = getFontFaceForGlyph(ch, fontIndex);

    if(!face) return false;

    return loadGlyphFromFace(ch, face, fontIndex);
}

std::unique_ptr<Channel[]> McFont::unpackMonoBitmap(const FT_Bitmap &bitmap) {
    auto result = std::make_unique<Channel[]>(static_cast<size_t>(bitmap.rows) * bitmap.width);

    for(u32 y = 0; y < bitmap.rows; y++) {
        for(u32 byteIdx = 0; byteIdx < bitmap.pitch; byteIdx++) {
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

std::unique_ptr<Color[]> McFont::createExpandedBitmapData(const FT_Bitmap &bitmap) {
    auto expandedData = std::make_unique<Color[]>(static_cast<ssize_t>(bitmap.width) * bitmap.rows);

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

void McFont::renderGlyphToAtlas(wchar_t ch, int x, int y, FT_Face face, bool isDynamicSlot) {
    if(!face) {
        // fall back to getting the face again if not provided
        int fontIndex = 0;
        face = getFontFaceForGlyph(ch, fontIndex);
        if(!face) return;
    } else if(face != m_ftFace) {
        // make sure fallback face has the correct size for this font instance
        setFaceSize(face);
    }

    if(FT_Load_Glyph(face, FT_Get_Char_Index(face, ch), m_bAntialiasing ? FT_LOAD_TARGET_NORMAL : FT_LOAD_TARGET_MONO))
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
        if(availableWidth < bitmap.width || availableHeight < bitmap.rows) {
            auto clippedData = std::make_unique<Color[]>(static_cast<size_t>(availableWidth) * availableHeight);
            for(u32 row = 0; row < availableHeight; row++) {
                for(u32 col = 0; col < availableWidth; col++) {
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

bool McFont::createAndPackAtlas(const std::vector<wchar_t> &glyphs) {
    if(glyphs.empty()) return true;

    // prepare packing rectangles
    std::vector<TextureAtlas::PackRect> packRects;
    std::vector<wchar_t> rectsToChars;
    packRects.reserve(glyphs.size());
    rectsToChars.reserve(glyphs.size());

    size_t rectIndex = 0;
    for(wchar_t ch : glyphs) {
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
    m_textureAtlas.reset(
        resourceManager->createTextureAtlas(static_cast<int>(finalAtlasSize), static_cast<int>(finalAtlasSize)));

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
        const wchar_t ch = rectsToChars[rect.id];

        // get the correct font face for this glyph
        int fontIndex = 0;
        FT_Face face = getFontFaceForGlyph(ch, fontIndex);

        renderGlyphToAtlas(ch, rect.x, rect.y, face, false /*not dynamic*/);
    }

    // initialize dynamic region after static glyphs are placed
    initializeDynamicRegion(static_cast<int>(finalAtlasSize));

    // finalize atlas texture
    resourceManager->loadResource(m_textureAtlas.get());
    m_textureAtlas->getAtlasImage()->setFilterMode(m_bAntialiasing ? Graphics::FILTER_MODE::FILTER_MODE_LINEAR
                                                                   : Graphics::FILTER_MODE::FILTER_MODE_NONE);
    return true;
}

void McFont::buildGlyphGeometry(const GLYPH_METRICS &gm, const vec3 &basePos, float advanceX, size_t &vertexCount) {
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

void McFont::buildStringGeometry(const UString &text, size_t &vertexCount) {
    if(!this->bReady || text.length() == 0 || text.length() > cv::r_drawstring_max_string_length.getInt()) return;

    float advanceX = 0.0f;
    const size_t maxGlyphs =
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

void McFont::drawString(const UString &text) {
    if(!this->bReady) return;

    const int maxNumGlyphs = cv::r_drawstring_max_string_length.getInt();
    if(text.length() == 0 || text.length() > maxNumGlyphs) return;

    m_vao.clear();

    const size_t totalVerts = text.length() * VERTS_PER_VAO;
    m_vao.reserve(totalVerts);
    m_vertices.resize(totalVerts);
    m_texcoords.resize(totalVerts);

    size_t vertexCount = 0;
    buildStringGeometry(text, vertexCount);

    for(size_t i = 0; i < vertexCount; i++) {
        m_vao.addVertex(m_vertices[i]);
        m_vao.addTexcoord(m_texcoords[i]);
    }

    m_textureAtlas->getAtlasImage()->bind();
    g->drawVAO(&m_vao);

    if(cv::r_debug_drawstring_unbind.getBool()) m_textureAtlas->getAtlasImage()->unbind();
}

void McFont::beginBatch() {
    m_batchActive = true;
    m_batchQueue.totalVerts = 0;
    m_batchQueue.usedEntries = 0;  // don't clear/reallocate, reuse the entries instead
}

void McFont::addToBatch(const UString &text, const vec3 &pos, Color color) {
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

void McFont::flushBatch() {
    if(!m_batchActive || !m_batchQueue.totalVerts) {
        m_batchActive = false;
        return;
    }

    m_vertices.resize(m_batchQueue.totalVerts);
    m_texcoords.resize(m_batchQueue.totalVerts);
    m_vao.clear();
    m_vao.reserve(m_batchQueue.totalVerts);

    size_t currentVertex = 0;
    for(size_t i = 0; i < m_batchQueue.usedEntries; i++) {
        const auto &entry = m_batchQueue.entryList[i];
        const size_t stringStart = currentVertex;
        buildStringGeometry(entry.text, currentVertex);

        for(size_t j = stringStart; j < currentVertex; j++) {
            m_vertices[j] += entry.pos;
            m_vao.addVertex(m_vertices[j]);
            m_vao.addTexcoord(m_texcoords[j]);
            m_vao.addColor(entry.color);
        }
    }

    m_textureAtlas->getAtlasImage()->bind();

    g->drawVAO(&m_vao);

    if(cv::r_debug_drawstring_unbind.getBool()) m_textureAtlas->getAtlasImage()->unbind();

    m_batchActive = false;
}

float McFont::getStringWidth(const UString &text) const {
    if(!this->bReady) return 1.0f;

    float width = 0.0f;
    for(int i = 0; i < text.length(); i++) {
        width += getGlyphMetrics(text[i]).advance_x;
    }
    return width;
}

float McFont::getStringHeight(const UString &text) const {
    if(!this->bReady) return 1.0f;

    float height = 0.0f;
    for(int i = 0; i < text.length(); i++) {
        height = std::max(height, static_cast<float>(getGlyphMetrics(text[i]).top));
    }
    return height;
}

std::vector<UString> McFont::wrap(const UString &text, f64 max_width) const {
    std::vector<UString> lines;
    lines.emplace_back();

    UString word = "";
    u32 line = 0;
    f64 line_width = 0.0;
    f64 word_width = 0.0;
    for(int i = 0; i < text.length(); i++) {
        if(text[i] == '\n') {
            lines[line].append(word);
            lines.emplace_back();
            line++;
            line_width = 0.0;
            word = "";
            word_width = 0.0;
            continue;
        }

        f32 char_width = getGlyphMetrics(text[i]).advance_x;

        if(text[i] == ' ') {
            lines[line].append(word);
            line_width += word_width;
            word = "";
            word_width = 0.0;

            if(line_width + char_width > max_width) {
                // Ignore spaces at the end of a line
                lines.emplace_back();
                line++;
                line_width = 0.0;
            } else if(line_width > 0.0) {
                lines[line].append(' ');
                line_width += char_width;
            }
        } else {
            if(word_width + char_width > max_width) {
                // Split word onto new line
                lines[line].append(word);
                lines.emplace_back();
                line++;
                line_width = 0.0;
                word = "";
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

const McFont::GLYPH_METRICS &McFont::getGlyphMetrics(wchar_t ch) const {
    auto it = m_vGlyphMetrics.find(ch);
    if(it != m_vGlyphMetrics.end()) return it->second;

    // attempt dynamic loading for unicode characters
    if(m_bTryFindFallbacks && const_cast<McFont *>(this)->loadGlyphDynamic(ch)) {
        it = m_vGlyphMetrics.find(ch);
        if(it != m_vGlyphMetrics.end()) return it->second;
    }

    // fallback to unknown character glyph
    it = m_vGlyphMetrics.find(UNKNOWN_CHAR);
    if(it != m_vGlyphMetrics.end()) return it->second;

    debugLog("Font Error: Missing default backup glyph (UNKNOWN_CHAR)?");
    return m_errorGlyph;
}

/*
########################################################################
## shared/static FreeType management stuff below
##########################################################################
*/

bool McFont::initializeSharedFreeType() {
    if(s_sharedFtLibraryInitialized) return true;

    if(FT_Init_FreeType(&s_sharedFtLibrary)) {
        engine->showMessageError("Font Error", "FT_Init_FreeType() failed!");
        return false;
    }

    s_sharedFtLibraryInitialized = true;
    return true;
}

bool McFont::initializeSharedFallbackFonts() {
    if(s_sharedFallbacksInitialized) return true;

    // make sure shared freetype library is initialized first
    if(!initializeSharedFreeType()) return false;

    // check all bundled fonts first
    std::vector<std::string> bundledFallbacks = env->getFilesInFolder(MCENGINE_FONTS_PATH);

    for(const auto &fontName : bundledFallbacks) {
        if(loadFallbackFont(UString{fontName}, false)) {
            if(cv::r_debug_font_unicode.getBool()) debugLog("Font Info: Loaded bundled fallback font: {:s}", fontName);
        }
    }

    // then find likely system fonts
    if(!Env::cfg(OS::WASM) && cv::font_load_system.getBool()) discoverSystemFallbacks();

    s_sharedFallbacksInitialized = true;
    return true;
}

void McFont::discoverSystemFallbacks() {
#ifdef MCENGINE_PLATFORM_WINDOWS
    std::array<char, MAX_PATH> windir{};
    if(GetWindowsDirectoryA(windir.data(), MAX_PATH) <= 0) return;

    std::vector<std::string> systemFonts = {
        std::string{windir.data()} + "\\Fonts\\arial.ttf",
        std::string{windir.data()} + "\\Fonts\\msyh.ttc",      // Microsoft YaHei (Chinese)
        std::string{windir.data()} + "\\Fonts\\malgun.ttf",    // Malgun Gothic (Korean)
        std::string{windir.data()} + "\\Fonts\\meiryo.ttc",    // Meiryo (Japanese)
        std::string{windir.data()} + "\\Fonts\\seguiemj.ttf",  // Segoe UI Emoji
        std::string{windir.data()} + "\\Fonts\\seguisym.ttf"   // Segoe UI Symbol
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

bool McFont::loadFallbackFont(const UString &fontPath, bool isSystemFont) {
    FT_Face face{};
    if(FT_New_Face(s_sharedFtLibrary, fontPath.toUtf8(), 0, &face)) {
        if(cv::r_debug_font_unicode.getBool()) debugLog("Font Warning: Failed to load fallback font: {:s}", fontPath);
        return false;
    }

    if(FT_Select_Charmap(face, ft_encoding_unicode)) {
        if(cv::r_debug_font_unicode.getBool())
            debugLog("Font Warning: Failed to select unicode charmap for fallback font: {:s}", fontPath);
        FT_Done_Face(face);
        return false;
    }

    // don't set font size here, will be set when the face is used by individual font instances
    s_sharedFallbackFonts.push_back(FallbackFont{fontPath, face, isSystemFont});
    return true;
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
