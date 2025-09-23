// Copyright (c) 2012, PG, All rights reserved.
#include "Resource.h"

#include "File.h"
#include "Logging.h"

#include <utility>

Resource::Resource(std::string filepath) {
    this->sFilePath = std::move(filepath);

    bool file_found = true;
    if(File::existsCaseInsensitive(this->sFilePath) != File::FILETYPE::FILE)  // modifies the input string if found
    {
        debugLog("Resource Warning: File {:s} does not exist!", this->sFilePath);
        file_found = false;
    }

    // give it a dummy name for unnamed resources, mainly for debugging purposes
    this->sName =
        fmt::format("{:p}:postinit=n:found={}:{:s}", static_cast<const void*>(this), file_found, this->sFilePath);
}

void Resource::load() { init(); }

void Resource::loadAsync() { initAsync(); }

void Resource::reload() {
    release();
    loadAsync();
    load();
}

void Resource::release() {
    destroy();

    // NOTE: these are set afterwards on purpose
    this->bReady = false;
    this->bAsyncReady = false;
}

void Resource::interruptLoad() { this->bInterrupted = true; }
