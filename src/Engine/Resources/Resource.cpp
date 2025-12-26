// Copyright (c) 2012, PG, All rights reserved.
#include "Resource.h"

#include "File.h"
#include "Logging.h"

#include <utility>

Resource::Resource(std::string filepath) {
    this->sFilePath = std::move(filepath);

    const bool exists = this->doPathFixup(this->sFilePath);

    // give it a dummy name for unnamed resources, mainly for debugging purposes
    this->sName = fmt::format("{:p}:postinit=n:found={}:{:s}", static_cast<const void*>(this), exists, this->sFilePath);
}

// separate helper for possible reload with new path
bool Resource::doPathFixup(std::string &input) {
    bool file_found = true;
    if(File::existsCaseInsensitive(input) != File::FILETYPE::FILE)  // modifies the input string if found
    {
        debugLog("Resource Warning: File {:s} does not exist!", input);
        file_found = false;
    }

    return file_found;
}

void Resource::load() {
    this->init();
    if(this->onInit.has_value()) {
        this->onInit->callback(this, this->onInit->userdata);
    }
}

void Resource::loadAsync() {
    this->bInterrupted.store(false, std::memory_order_release);
    this->initAsync();
}

void Resource::reload() {
    this->release();
    this->loadAsync();
    this->load();
}

void Resource::release() {
    this->bInterrupted.store(true, std::memory_order_release);
    this->destroy();

    // NOTE: these are set afterwards on purpose
    this->bReady.store(false, std::memory_order_release);
    this->bAsyncReady.store(false, std::memory_order_release);
    this->bInterrupted.store(false, std::memory_order_release);
}

void Resource::interruptLoad() { this->bInterrupted.store(true, std::memory_order_release); }
