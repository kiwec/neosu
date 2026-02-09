
#include "NullImage.h"

// image that does CPU-side pixel loading but never uploads to GPU
NullImage::NullImage(std::string filepath, bool mipmapped, bool keepInSystemMemory)
    : Image(std::move(filepath), mipmapped, keepInSystemMemory) {}
NullImage::NullImage(i32 width, i32 height, bool mipmapped, bool keepInSystemMemory)
    : Image(width, height, mipmapped, keepInSystemMemory) {}

void NullImage::bind(unsigned int /*textureUnit*/) const {}
void NullImage::unbind() const {}

void NullImage::init() {
    if(!this->isAsyncReady()) return;
    if(!this->bKeepInSystemMemory) this->rawImage.clear();
    this->setReady(true);
}
void NullImage::initAsync() {
    if(!this->bCreatedImage)
        this->setAsyncReady(loadRawImage());
    else
        this->setAsyncReady(true);
}
void NullImage::destroy() {
    if(!this->bKeepInSystemMemory) this->rawImage.clear();
}
