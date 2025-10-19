//========== Copyright (c) 2012, PG & 2025, WH, All rights reserved. ============//
//
// Purpose:		image wrapper
//
// $NoKeywords: $img
//===============================================================================//

#include "Image.h"

#include <png.h>
#include <turbojpeg.h>
#include <zlib.h>

#include <csetjmp>
#include <cstddef>
#include <cstring>
#include <utility>

/* ====== stb_image config ====== */
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_PNM
#define STBI_NO_PIC
#define STBI_MINGW_ENABLE_SSE2                     // bring on the pain
#define STB_IMAGE_STATIC                           // we only use stb_image in this translation unit
#define STBI_MAX_DIMENSIONS (16384ULL * 16384ULL)  // there's no way we need anything more than this

#ifndef _DEBUG
#define STBI_ASSERT
#endif

#define STB_IMAGE_IMPLEMENTATION

#include <stb_image.h>
/* ==== end stb_image config ==== */

#include "Engine.h"
#include "Environment.h"
#include "File.h"
#include "Logging.h"
#include "ConVar.h"

#if defined(ZLIBNG_VERNUM) && ZLIBNG_VERNUM < 0x020205F0L
#include "SyncMutex.h"

namespace {
// this is complete bullshit and a bug in zlib-ng (probably, less likely libpng)
// need to prevent zlib from lazy-initializing the crc tables, otherwise data race galore
// literally causes insane lags/issues in completely unrelated places for async loading
Sync::mutex zlib_init_mutex;
std::atomic<bool> zlib_initialized{false};

void garbage_zlib() {
    // otherwise we need to do this song and dance
    if(zlib_initialized.load(std::memory_order_acquire)) return;
    Sync::scoped_lock lock(zlib_init_mutex);
    if(zlib_initialized.load(std::memory_order_relaxed)) return;
    uLong dummy_crc = crc32(0L, Z_NULL, 0);
    std::array<const u8, 5> test_data{"shit"};
    dummy_crc = crc32(dummy_crc, reinterpret_cast<const Bytef *>(test_data.data()), 4);
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    if(inflateInit(&strm) == Z_OK) inflateEnd(&strm);
    (void)dummy_crc;
    zlib_initialized.store(true, std::memory_order_release);
}
}  // namespace
#else
#define garbage_zlib()
#endif

namespace {  // static
struct pngErrorManager {
    jmp_buf setjmp_buffer{};
};

void pngErrorExit(png_structp png_ptr, png_const_charp error_msg) {
    debugLog("PNG Error: {:s}", error_msg);
    auto *err = static_cast<pngErrorManager *>(png_get_error_ptr(png_ptr));
    longjmp(&err->setjmp_buffer[0], 1);
}

void pngWarning(png_structp /*unused*/, png_const_charp warning_msg) {
    if constexpr(Env::cfg(BUILD::DEBUG)) {
        debugLog("PNG Warning: {:s}", warning_msg);
    }
}

struct pngMemoryReader {
    const u8 *pdata{nullptr};
    u64 size{0};
    u64 offset{0};
};

void pngReadFromMemory(png_structp png_ptr, png_bytep outBytes, png_size_t byteCountToRead) {
    auto *reader = static_cast<pngMemoryReader *>(png_get_io_ptr(png_ptr));

    if(reader->offset + byteCountToRead > reader->size) {
        png_error(png_ptr, "Read past end of data");
        return;
    }

    memcpy(outBytes, reader->pdata + reader->offset, byteCountToRead);
    reader->offset += byteCountToRead;
}
}  // namespace

Image::DECODE_RESULT Image::decodePNGFromMemory(const std::unique_ptr<u8[]> &inData, u64 size) {
    garbage_zlib();
    using enum DECODE_RESULT;

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if(!png_ptr) {
        debugLog("Image Error: png_create_read_struct failed");
        return FAIL;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr) {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        debugLog("Image Error: png_create_info_struct failed");
        return FAIL;
    }

    pngErrorManager err;
    png_set_error_fn(png_ptr, &err, pngErrorExit, pngWarning);

    if(setjmp(&err.setjmp_buffer[0])) {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        return FAIL;
    }

    // Set up memory reading
    pngMemoryReader reader{.pdata = inData.get(), .size = size, .offset = 0};

    png_set_read_fn(png_ptr, &reader, pngReadFromMemory);

    png_read_info(png_ptr, info_ptr);

    u32 tempOutWidth = png_get_image_width(png_ptr, info_ptr);
    u32 tempOutHeight = png_get_image_height(png_ptr, info_ptr);

    if(tempOutWidth > 8192 || tempOutHeight > 8192) {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        debugLog("Image Error: PNG image size is too big ({} x {})", tempOutWidth, tempOutHeight);
        return FAIL;
    }

    if(this->isInterrupted()) {  // cancellation point
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        return INTERRUPTED;
    }

    i32 outWidth = static_cast<i32>(tempOutWidth);
    i32 outHeight = static_cast<i32>(tempOutHeight);

    png_byte color_type = png_get_color_type(png_ptr, info_ptr);
    png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    png_byte interlace_type = png_get_interlace_type(png_ptr, info_ptr);

    // convert to RGBA if needed
    if(bit_depth == 16) png_set_strip_16(png_ptr);

    if(color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);

    if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);

    if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png_ptr);

    // these color types don't have alpha channel, so fill it with 0xff
    if(color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_filler(png_ptr, 0xFF, PNG_FILLER_AFTER);

    if(color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png_ptr);

    // "Interlace handling should be turned on when using png_read_image"
    if(interlace_type != PNG_INTERLACE_NONE) png_set_interlace_handling(png_ptr);

    png_read_update_info(png_ptr, info_ptr);

    if(this->isInterrupted()) {  // cancellation point
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        return INTERRUPTED;
    }

    // allocate memory for the image
    this->rawImage = std::make_unique<SizedRGBABytes>(outWidth, outHeight);

    for(sSz y = 0; y < outHeight; y++) {
        if((outHeight / 4 > 0) && (y % (outHeight / 4)) == 0) {
            if(this->isInterrupted()) {  // cancellation point
                png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
                return INTERRUPTED;
            }
        }
        png_read_row(png_ptr, &this->rawImage->data()[y * outWidth * Image::NUM_CHANNELS], nullptr);
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    return SUCCESS;
}

Image::DECODE_RESULT Image::decodeJPEGFromMemory(const std::unique_ptr<u8[]> &inData, u64 size) {
    using enum DECODE_RESULT;
    // decode jpeg
    tjhandle tjInstance = tj3Init(TJINIT_DECOMPRESS);
    if(!tjInstance) {
        debugLog("Image Error: tj3Init failed");
        return FAIL;
    }

    if(tj3DecompressHeader(tjInstance, inData.get(), size) < 0) {
        debugLog("Image Error: tj3DecompressHeader failed: {:s}", tj3GetErrorStr(tjInstance));
        tj3Destroy(tjInstance);
        return FAIL;
    }

    if(this->isInterrupted())  // cancellation point
    {
        tj3Destroy(tjInstance);
        return INTERRUPTED;
    }

    i32 outWidth = tj3Get(tjInstance, TJPARAM_JPEGWIDTH);
    i32 outHeight = tj3Get(tjInstance, TJPARAM_JPEGHEIGHT);

    if(outWidth > 8192 || outHeight > 8192) {
        debugLog("Image Error: JPEG image size is too big ({} x {})", outWidth, outHeight);
        tj3Destroy(tjInstance);
        return FAIL;
    }

    if(this->isInterrupted())  // cancellation point
    {
        tj3Destroy(tjInstance);
        return INTERRUPTED;
    }

    // preallocate
    this->rawImage = std::make_unique<SizedRGBABytes>(outWidth, outHeight);

    // always convert to RGBA for consistency with PNG
    // decompress directly to RGBA
    if(tj3Decompress8(tjInstance, inData.get(), size, this->rawImage->data(), 0, TJPF_RGBA) < 0) {
        debugLog("Image Error: tj3Decompress8 failed: {:s}", tj3GetErrorStr(tjInstance));
        tj3Destroy(tjInstance);
        return FAIL;
    }

    tj3Destroy(tjInstance);
    return SUCCESS;
}

Image::DECODE_RESULT Image::decodeSTBFromMemory(const std::unique_ptr<u8[]> &inData, u64 size) {
    using enum DECODE_RESULT;

    // use stbi_info to validate dimensions before decoding
    i32 outWidth, outHeight, channels;
    if(!stbi_info_from_memory(inData.get(), static_cast<i32>(size), &outWidth, &outHeight, &channels)) {
        debugLog("Image Error: stb_image info query failed: {:s}", stbi_failure_reason());
        return FAIL;
    }

    if(outWidth > 8192 || outHeight > 8192) {
        debugLog("Image Error: Image size is too big ({} x {})", outWidth, outHeight);
        return FAIL;
    }

    if(this->isInterrupted())  // cancellation point
        return INTERRUPTED;

    u8 *decoded = stbi_load_from_memory(inData.get(), static_cast<i32>(size), &outWidth, &outHeight, &channels,
                                        Image::NUM_CHANNELS);

    if(!decoded) {
        debugLog("Image Error: stb_image failed: {:s}", stbi_failure_reason());
        return FAIL;
    }

    if(this->isInterrupted()) {  // cancellation point
        stbi_image_free(decoded);
        return INTERRUPTED;
    }

    // don't stbi_image_free, we own the data now
    this->rawImage = std::make_unique<SizedRGBABytes>(decoded, outWidth, outHeight);

    return SUCCESS;
}

void Image::saveToImage(const u8 *data, i32 width, i32 height, u8 channels, std::string filepath) {
    if(channels != 3 && channels != 4) {
        debugLog("PNG Error: Can only save 3 or 4 channel image data.");
        return;
    }

    garbage_zlib();
    debugLog("Saving image to {:s} ...", filepath);

    FILE *fp = File::fopen_c(filepath.c_str(), "wb");
    if(!fp) {
        debugLog("PNG error: Could not open file {:s} for writing", filepath);
        engine->showMessageError("PNG Error", "Could not open file for writing");
        return;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if(!png_ptr) {
        fclose(fp);
        debugLog("PNG error: png_create_write_struct failed");
        engine->showMessageError("PNG Error", "png_create_write_struct failed");
        return;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr) {
        png_destroy_write_struct(&png_ptr, nullptr);
        fclose(fp);
        debugLog("PNG error: png_create_info_struct failed");
        engine->showMessageError("PNG Error", "png_create_info_struct failed");
        return;
    }

    if(setjmp(&png_jmpbuf(png_ptr)[0])) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        debugLog("PNG error during write");
        engine->showMessageError("PNG Error", "Error during PNG write");
        return;
    }

    png_init_io(png_ptr, fp);

    // write header (8 bit colour depth, RGB(A))
    const int pngChannelType = (channels == 4 ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB);

    png_set_IHDR(png_ptr, info_ptr, width, height, 8, pngChannelType, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    // write row by row directly from the RGB(A) data
    for(sSz y = 0; y < height; y++) {
        png_write_row(png_ptr, &data[y * width * channels]);
    }

    png_write_end(png_ptr, nullptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
}

Image::Image(std::string filepath, bool mipmapped, bool keepInSystemMemory) : Resource(std::move(filepath)) {
    this->bMipmapped = mipmapped;
    this->bKeepInSystemMemory = keepInSystemMemory;

    this->type = Image::TYPE::TYPE_PNG;
    this->filterMode = mipmapped ? Graphics::FILTER_MODE::FILTER_MODE_MIPMAP : Graphics::FILTER_MODE::FILTER_MODE_LINEAR;
    this->wrapMode = Graphics::WRAP_MODE::WRAP_MODE_CLAMP;
    this->iWidth = 1;
    this->iHeight = 1;

    this->bCreatedImage = false;
}

Image::Image(i32 width, i32 height, bool mipmapped, bool keepInSystemMemory) : Resource() {
    this->bMipmapped = mipmapped;
    this->bKeepInSystemMemory = keepInSystemMemory;

    this->type = Image::TYPE::TYPE_RGBA;
    this->filterMode = mipmapped ? Graphics::FILTER_MODE::FILTER_MODE_MIPMAP : Graphics::FILTER_MODE::FILTER_MODE_LINEAR;
    this->wrapMode = Graphics::WRAP_MODE::WRAP_MODE_CLAMP;
    this->iWidth = std::min(16384, width);  // sanity
    this->iHeight = std::min(16384, height);

    this->bCreatedImage = true;

    // reserve rawImage
    if(cv::debug_image.getBool()) {
        // don't calloc() if we're filling with pink anyways
        this->rawImage = std::make_unique<SizedRGBABytes>(this->iWidth, this->iHeight);
        // fill with pink pixels
        for(u64 i = 0; i < static_cast<u64>(this->iWidth) * this->iHeight; i++) {
            (*this->rawImage)[i * Image::NUM_CHANNELS + 0] = 255;  // R
            (*this->rawImage)[i * Image::NUM_CHANNELS + 1] = 0;    // G
            (*this->rawImage)[i * Image::NUM_CHANNELS + 2] = 255;  // B
            (*this->rawImage)[i * Image::NUM_CHANNELS + 3] = 255;  // A
        }
    } else {
        // otherwise fill with zeroes (transparent black)
        this->rawImage = std::make_unique<SizedRGBABytes>(this->iWidth, this->iHeight, true);
    }

    // special case: filled rawimage is always already async ready
    this->bAsyncReady = true;
}

bool Image::loadRawImage() {
    bool alreadyLoaded = !!this->rawImage && this->totalBytes() >= 4;

    auto exit = [this]() -> bool {
        // if we were interrupted, it's not a load error
        this->bLoadError = !this->isInterrupted();
        this->rawImage.reset();
        this->iWidth = 1;
        this->iHeight = 1;
        this->bLoadedImageEntirelyTransparent = false;
        return false;
    };

    // if it isn't a created image (created within the engine), load it from the corresponding file
    if(!this->bCreatedImage) {
        if(alreadyLoaded) {  // has already been loaded (or loading it again after setPixel(s))
            // don't render if we're still transparent
            return !this->bLoadedImageEntirelyTransparent;
        }

        if(!env->fileExists(this->sFilePath)) {
            debugLog("Image Error: Couldn't find file {:s}", this->sFilePath);
            return exit();
        }

        if(this->isInterrupted())  // cancellation point
            return exit();

        // load entire file
        std::unique_ptr<u8[]> fileBuffer;
        size_t fileSize{0};
        {
            File file(this->sFilePath);
            if(!file.canRead()) {
                debugLog("Image Error: Couldn't canRead() file {:s}", this->sFilePath);
                return exit();
            }
            if(((fileSize = file.getFileSize()) < 32) || fileSize > INT_MAX) {
                debugLog("Image Error: FileSize is {} in file {:s}", this->sFilePath,
                         fileSize < 32 ? "< 32" : "> INT_MAX");
                return exit();
            }

            if(this->isInterrupted())  // cancellation point
                return exit();

            fileBuffer = file.takeFileBuffer();
            if(!fileBuffer) {
                debugLog("Image Error: Couldn't readFile() file {:s}", this->sFilePath);
                return exit();
            }
            // don't keep the file open
        }

        if(this->isInterrupted())  // cancellation point
            return exit();

        // determine file type by magic number
        this->type = Image::TYPE::TYPE_RGBA;  // default for unknown formats
        bool isJPEG = false;
        bool isPNG = false;
        {
            if(fileBuffer[0] == 0xff && fileBuffer[1] == 0xD8 && fileBuffer[2] == 0xff) {  // 0xFFD8FF
                isJPEG = true;
                this->type = Image::TYPE::TYPE_JPG;
            } else if(fileBuffer[0] == 0x89 && fileBuffer[1] == 0x50 && fileBuffer[2] == 0x4E &&
                      fileBuffer[3] == 0x47) {  // 0x89504E47 (%PNG)
                isPNG = true;
                this->type = Image::TYPE::TYPE_PNG;
            }
        }

        DECODE_RESULT res = DECODE_RESULT::FAIL;

        // try format-specific decoder first if format is recognized
        if(isPNG) {
            res = decodePNGFromMemory(fileBuffer, fileSize);
        } else if(isJPEG) {
            res = decodeJPEGFromMemory(fileBuffer, fileSize);
        }

        // early exit on interruption
        if(res == DECODE_RESULT::INTERRUPTED) {
            return exit();
        }

        // fallback to stb_image if primary decoder failed or format was unrecognized
        if(res == DECODE_RESULT::FAIL) {
            if(isPNG || isJPEG) {
                debugLog("Image Warning: Primary decoder failed for {:s}, trying fallback...", this->sFilePath);
            }
            res = decodeSTBFromMemory(fileBuffer, fileSize);
        }

        // final result check
        if(res != DECODE_RESULT::SUCCESS) {
            if(res == DECODE_RESULT::FAIL) {
                debugLog("Image Error: Could not decode image file {:s}", this->sFilePath);
            }
            return exit();
        }

        if((this->type == Image::TYPE::TYPE_PNG) && canHaveTransparency(fileBuffer, fileSize) &&
           isRawImageCompletelyTransparent()) {
            if(!this->isInterrupted()) {
                debugLog("Image: Ignoring empty transparent image {:s}", this->sFilePath);
            }
            // optimization: ignore completely transparent images (don't render)
            this->bLoadedImageEntirelyTransparent = true;
        }
    } else {
        // don't avoid rendering createdImages with the completelyTransparent check
    }

    // sanity check and one more cancellation point
    if(this->isInterrupted() || !this->rawImage || this->rawImage->getNumBytes() < 4) {
        return exit();
    }

    // update standard width/height to raw image's size (just in case)
    this->iWidth = this->rawImage->getX();
    this->iHeight = this->rawImage->getY();

    return !this->bLoadedImageEntirelyTransparent;
}

Color Image::getPixel(i32 x, i32 y) const {
    if(unlikely(x < 0 || y < 0 || this->totalBytes() < 1)) return 0xffffff00;

    const u64 indexEnd = static_cast<u64>(Image::NUM_CHANNELS) * y * this->rawImage->getX() +
                         static_cast<u64>(Image::NUM_CHANNELS) * x + Image::NUM_CHANNELS;
    if(unlikely(indexEnd > this->totalBytes())) return 0xffffff00;
    const u64 indexBegin =
        static_cast<u64>(Image::NUM_CHANNELS) * y * this->rawImage->getX() + static_cast<u64>(Image::NUM_CHANNELS) * x;

    const Channel &r{(*this->rawImage)[indexBegin + 0]};
    const Channel &g{(*this->rawImage)[indexBegin + 1]};
    const Channel &b{(*this->rawImage)[indexBegin + 2]};
    const Channel &a{(*this->rawImage)[indexBegin + 3]};

    return argb(a, r, g, b);
}

void Image::setPixel(i32 x, i32 y, Color color) {
    if(unlikely(x < 0 || y < 0 || this->totalBytes() < 1)) return;

    const u64 indexEnd = static_cast<u64>(Image::NUM_CHANNELS) * y * this->rawImage->getX() +
                         static_cast<u64>(Image::NUM_CHANNELS) * x + Image::NUM_CHANNELS;
    if(unlikely(indexEnd > this->totalBytes())) return;
    const u64 indexBegin =
        static_cast<u64>(Image::NUM_CHANNELS) * y * this->rawImage->getX() + static_cast<u64>(Image::NUM_CHANNELS) * x;

    (*this->rawImage)[indexBegin + 0] = color.R();
    (*this->rawImage)[indexBegin + 1] = color.G();
    (*this->rawImage)[indexBegin + 2] = color.B();
    (*this->rawImage)[indexBegin + 3] = color.A();
    if(!this->bCreatedImage && color.A() != 0) {
        // play it safe, don't recompute the entire alpha channel visibility here
        this->bLoadedImageEntirelyTransparent = false;
    }
}

void Image::setPixels(const std::vector<u8> &pixels) {
    if(pixels.size() < this->totalBytes()) {
        debugLog("Image Error: setPixels() supplied array is too small!");
        return;
    }

    assert(this->totalBytes() == static_cast<u64>(this->iWidth) * this->iHeight * NUM_CHANNELS &&
           "width and height are somehow out of sync with raw image");

    std::memcpy(this->rawImage->data(), pixels.data(), this->totalBytes());
    if(!this->bCreatedImage) {
        // recompute alpha channel visibility here (TODO: remove if slow)
        this->bLoadedImageEntirelyTransparent = isRawImageCompletelyTransparent();
    }
}

// internal
bool Image::canHaveTransparency(const std::unique_ptr<u8[]> &data, u64 size) {
    if(size < 33)  // not enough data for IHDR, so just assume true
        return true;

    // PNG IHDR chunk starts at offset 16 (8 bytes signature + 8 bytes chunk header)
    // color type is at offset 25 (16 + 4 width + 4 height + 1 bit depth)
    if(size > 25) {
        u8 colorType = data.get()[25];
        return colorType != 2;  // RGB without alpha
    }

    return true;  // unknown format? just assume true
}

bool Image::isRawImageCompletelyTransparent() const {
    if(!this->rawImage || this->totalBytes() == 0) return false;

    const i64 alphaOffset = 3;
    const i64 totalPixels = static_cast<i64>(this->rawImage->getArea());

    for(i64 i = 0; i < totalPixels; ++i) {
        if(this->isInterrupted())  // cancellation point
            return false;

        // check alpha channel directly
        if((*this->rawImage)[i * Image::NUM_CHANNELS + alphaOffset] > 0) return false;  // non-transparent pixel
    }

    return true;  // all pixels are transparent
}
