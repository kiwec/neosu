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

void pngWarning(png_structp /*unused*/, [[maybe_unused]] png_const_charp warning_msg) {
#ifdef _DEBUG
    debugLog("PNG Warning: {:s}", warning_msg);
#endif
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

bool Image::decodePNGFromMemory(const u8 *data, u64 size, std::vector<u8> &outData, i32 &outWidth, i32 &outHeight) {
    garbage_zlib();
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if(!png_ptr) {
        debugLog("Image Error: png_create_read_struct failed");
        return false;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if(!info_ptr) {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        debugLog("Image Error: png_create_info_struct failed");
        return false;
    }

    pngErrorManager err;
    png_set_error_fn(png_ptr, &err, pngErrorExit, pngWarning);

    if(setjmp(&err.setjmp_buffer[0])) {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        return false;
    }

    // Set up memory reading
    pngMemoryReader reader{.pdata = data, .size = size, .offset = 0};

    png_set_read_fn(png_ptr, &reader, pngReadFromMemory);

    png_read_info(png_ptr, info_ptr);

    u32 tempOutWidth = png_get_image_width(png_ptr, info_ptr);
    u32 tempOutHeight = png_get_image_height(png_ptr, info_ptr);

    if(tempOutWidth > 8192 || tempOutHeight > 8192) {
        png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
        debugLog("Image Error: PNG image size is too big ({} x {})", tempOutWidth, tempOutHeight);
        return false;
    }

    outWidth = static_cast<i32>(tempOutWidth);
    outHeight = static_cast<i32>(tempOutHeight);

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

    // allocate memory for the image
    outData.resize(static_cast<u64>(outWidth) * outHeight * Image::NUM_CHANNELS);

    for(sSz y = 0; y < outHeight; y++) {
        png_read_row(png_ptr, &outData[y * outWidth * Image::NUM_CHANNELS], nullptr);
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
    return true;
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
    this->filterMode = Graphics::FILTER_MODE::FILTER_MODE_LINEAR;
    this->wrapMode = Graphics::WRAP_MODE::WRAP_MODE_CLAMP;
    this->iWidth = 1;
    this->iHeight = 1;

    this->bCreatedImage = false;
}

Image::Image(i32 width, i32 height, bool mipmapped, bool keepInSystemMemory) : Resource() {
    this->bMipmapped = mipmapped;
    this->bKeepInSystemMemory = keepInSystemMemory;

    this->type = Image::TYPE::TYPE_RGBA;
    this->filterMode = Graphics::FILTER_MODE::FILTER_MODE_LINEAR;
    this->wrapMode = Graphics::WRAP_MODE::WRAP_MODE_CLAMP;
    this->iWidth = width;
    this->iHeight = height;

    this->bCreatedImage = true;

    // reserve
    this->rawImage.resize(static_cast<u64>(this->iWidth) * this->iHeight * Image::NUM_CHANNELS);
    if(cv::debug_image.getBool()) {
        // fill with pink pixels
        for(u64 i = 0; i < static_cast<u64>(this->iWidth) * this->iHeight; i++) {
            this->rawImage.push_back(255);
            this->rawImage.push_back(0);
            this->rawImage.push_back(255);
            this->rawImage.push_back(255);
        }
    }

    // special case: filled rawimage is always already async ready
    this->bAsyncReady = true;
}

bool Image::loadRawImage() {
    bool alreadyLoaded = this->rawImage.size() > 0;

    // if it isn't a created image (created within the engine), load it from the corresponding file
    if(!this->bCreatedImage) {
        if(alreadyLoaded)  // has already been loaded (or loading it again after setPixel(s))
            return true;

        if(!env->fileExists(this->sFilePath)) {
            debugLog("Image Error: Couldn't find file {:s}", this->sFilePath);
            return false;
        }

        if(this->bInterrupted)  // cancellation point
            return false;

        // load entire file
        std::vector<u8> fileBuffer;
        size_t fileSize{0};
        {
            File file(this->sFilePath);
            if(!file.canRead()) {
                debugLog("Image Error: Couldn't canRead() file {:s}", this->sFilePath);
                return false;
            }
            if((fileSize = file.getFileSize()) < 4) {
                debugLog("Image Error: FileSize is < 4 in file {:s}", this->sFilePath);
                return false;
            }

            if(this->bInterrupted)  // cancellation point
                return false;

            fileBuffer = file.takeFileBuffer();
            if(fileBuffer.empty()) {
                debugLog("Image Error: Couldn't readFile() file {:s}", this->sFilePath);
                return false;
            }
            // don't keep the file open
        }

        if(this->bInterrupted)  // cancellation point
            return false;

        // determine file type by magic number (png/jpg)
        bool isJPEG = false;
        bool isPNG = false;
        {
            if(fileBuffer.at(0) == 0xff && fileBuffer.at(1) == 0xD8 && fileBuffer.at(2) == 0xff)  // 0xFFD8FF
                isJPEG = true;
            else if(fileBuffer.at(0) == 0x89 && fileBuffer.at(1) == 0x50 && fileBuffer.at(2) == 0x4E &&
                    fileBuffer.at(3) == 0x47)  // 0x89504E47 (%PNG)
                isPNG = true;
        }

        // depending on the type, load either jpeg or png
        if(isJPEG) {
            this->type = Image::TYPE::TYPE_JPG;

            // decode jpeg
            tjhandle tjInstance = tj3Init(TJINIT_DECOMPRESS);
            if(!tjInstance) {
                debugLog("Image Error: tj3Init failed in file {:s}", this->sFilePath);
                return false;
            }

            if(tj3DecompressHeader(tjInstance, fileBuffer.data(), fileSize) < 0) {
                debugLog("Image Error: tj3DecompressHeader failed: {:s} in file {:s}", tj3GetErrorStr(tjInstance),
                         this->sFilePath);
                tj3Destroy(tjInstance);
                return false;
            }

            if(this->bInterrupted)  // cancellation point
            {
                tj3Destroy(tjInstance);
                return false;
            }

            this->iWidth = tj3Get(tjInstance, TJPARAM_JPEGWIDTH);
            this->iHeight = tj3Get(tjInstance, TJPARAM_JPEGHEIGHT);

            if(this->iWidth > 8192 || this->iHeight > 8192) {
                debugLog("Image Error: JPEG image size is too big ({} x {}) in file {:s}", this->iWidth, this->iHeight,
                         this->sFilePath);
                tj3Destroy(tjInstance);
                return false;
            }

            if(this->bInterrupted)  // cancellation point
            {
                tj3Destroy(tjInstance);
                return false;
            }

            // preallocate
            this->rawImage.resize(static_cast<u64>(this->iWidth) * this->iHeight * Image::NUM_CHANNELS);

            // always convert to RGBA for consistency with PNG
            // decompress directly to RGBA
            if(tj3Decompress8(tjInstance, fileBuffer.data(), fileSize, &this->rawImage[0], 0, TJPF_RGBA) < 0) {
                debugLog("Image Error: tj3Decompress8 failed: {:s} in file {:s}", tj3GetErrorStr(tjInstance),
                         this->sFilePath);
                tj3Destroy(tjInstance);
                return false;
            }

            tj3Destroy(tjInstance);
        } else if(isPNG) {
            this->type = Image::TYPE::TYPE_PNG;

            // decode png using libpng
            if(!decodePNGFromMemory(fileBuffer.data(), fileSize, this->rawImage, this->iWidth, this->iHeight)) {
                debugLog("Image Error: PNG decoding failed in file {:s}", this->sFilePath);
                return false;
            }
        } else {
            debugLog("Image Error: Neither PNG nor JPEG in file {:s}", this->sFilePath);
            return false;
        }
    }

    if(this->bInterrupted)  // cancellation point
        return false;

    // error checking

    // size sanity check
    if(this->rawImage.size() < static_cast<u64>(this->iWidth) * this->iHeight * Image::NUM_CHANNELS) {
        debugLog("Image Error: Loaded image has only {}/{} bytes in file {:s}", this->rawImage.size(),
                 this->iWidth * this->iHeight * Image::NUM_CHANNELS, this->sFilePath);
        // engine->showMessageError("Image Error", UString::format("Loaded image has only %i/%i bytes in file %s",
        // rawImage.size(), iWidth*iHeight*iNumChannels, this->sFilePath));
        return false;
    }

    // optimization: ignore completely transparent images (don't render) (only PNGs can have them, obviously)
    if(!alreadyLoaded && (type == Image::TYPE::TYPE_PNG) &&
       canHaveTransparency(this->rawImage.data(), this->rawImage.size()) && isCompletelyTransparent()) {
        if(!this->bInterrupted) debugLog("Image: Ignoring empty transparent image {:s}", this->sFilePath);
        return false;
    }

    return true;
}

Color Image::getPixel(i32 x, i32 y) const {
    if(unlikely(x < 0 || y < 0 || this->rawImage.size() < 1)) return 0xffffff00;

    const u64 indexEnd = Image::NUM_CHANNELS * y * this->iWidth + Image::NUM_CHANNELS * x + Image::NUM_CHANNELS;
    if(unlikely(indexEnd > this->rawImage.size())) return 0xffffff00;
    const u64 indexBegin = Image::NUM_CHANNELS * y * this->iWidth + Image::NUM_CHANNELS * x;

    const Channel &r{this->rawImage[indexBegin + 0]};
    const Channel &g{this->rawImage[indexBegin + 1]};
    const Channel &b{this->rawImage[indexBegin + 2]};
    const Channel &a{this->rawImage[indexBegin + 3]};

    return argb(a, r, g, b);
}

void Image::setPixel(i32 x, i32 y, Color color) {
    if(unlikely(x < 0 || y < 0 || this->rawImage.size() < 1)) return;

    const u64 indexEnd = Image::NUM_CHANNELS * y * this->iWidth + Image::NUM_CHANNELS * x + Image::NUM_CHANNELS;
    if(unlikely(indexEnd > this->rawImage.size())) return;
    const u64 indexBegin = Image::NUM_CHANNELS * y * this->iWidth + Image::NUM_CHANNELS * x;

    this->rawImage[indexBegin + 0] = color.R();
    this->rawImage[indexBegin + 1] = color.G();
    this->rawImage[indexBegin + 2] = color.B();
    this->rawImage[indexBegin + 3] = color.A();
}

void Image::setPixels(const u8 *data, u64 size, TYPE type) {
    if(data == nullptr) return;

    // TODO: implement remaining types
    switch(type) {
        case TYPE::TYPE_PNG: {
            if(!decodePNGFromMemory(data, size, this->rawImage, this->iWidth, this->iHeight)) {
                debugLog("Image Error: PNG decoding failed in setPixels");
            }
        } break;

        default:
            debugLog("Image Error: Format not yet implemented");
            break;
    }
}

void Image::setPixels(const std::vector<u8> &pixels) {
    if(pixels.size() < static_cast<u64>(this->iWidth) * this->iHeight * Image::NUM_CHANNELS) {
        debugLog("Image Error: setPixels() supplied array is too small!");
        return;
    }

    this->rawImage = pixels;
}

// internal
bool Image::canHaveTransparency(const u8 *data, u64 size) {
    if(size < 33)  // not enough data for IHDR, so just assume true
        return true;

    // PNG IHDR chunk starts at offset 16 (8 bytes signature + 8 bytes chunk header)
    // color type is at offset 25 (16 + 4 width + 4 height + 1 bit depth)
    if(size > 25) {
        u8 colorType = data[25];
        return colorType != 2;  // RGB without alpha
    }

    return true;  // unknown format? just assume true
}

bool Image::isCompletelyTransparent() const {
    if(this->rawImage.empty()) return false;

    const i64 alphaOffset = 3;
    const i64 totalPixels = static_cast<i64>(this->iWidth) * this->iHeight;

    for(i64 i = 0; i < totalPixels; ++i) {
        if(this->bInterrupted)  // cancellation point
            return false;

        // check alpha channel directly
        if(this->rawImage[i * Image::NUM_CHANNELS + alphaOffset] > 0) return false;  // non-transparent pixel
    }

    return true;  // all pixels are transparent
}
