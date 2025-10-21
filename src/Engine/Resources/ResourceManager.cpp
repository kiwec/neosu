//========== Copyright (c) 2015, PG & 2025, WH, All rights reserved. ============//
//
// Purpose:		resource manager
//
// $NoKeywords: $rm
//===============================================================================//

#include "ResourceManager.h"

#include "App.h"
#include "AsyncResourceLoader.h"
#include "ConVar.h"
#include "Resource.h"
#include "Engine.h"
#include "Logging.h"

#include <algorithm>
#include <utility>

ResourceManager::ResourceManager() {
    // create directories we will assume already exist later on
    Environment::createDirectory(MCENGINE_FONTS_PATH);
    Environment::createDirectory(MCENGINE_IMAGES_PATH);
    Environment::createDirectory(MCENGINE_SHADERS_PATH);
    Environment::createDirectory(MCENGINE_SOUNDS_PATH);

    this->bNextLoadAsync.store(false, std::memory_order_release);

    // reserve space for typed vectors
    this->vImages.reserve(256);
    this->vFonts.reserve(16);
    this->vSounds.reserve(64);
    this->vShaders.reserve(32);
    this->vRenderTargets.reserve(8);
    this->vTextureAtlases.reserve(8);
    this->vVertexArrayObjects.reserve(32);

    // create async loader
    this->asyncLoader = std::make_unique<AsyncResourceLoader>();
}

ResourceManager::~ResourceManager() {
    // release all not-currently-being-loaded resources
    destroyResources();

    // async loader shutdown handles thread cleanup
    this->asyncLoader->shutdown();
}

void ResourceManager::update() {
    // delegate to async loader
    bool lowLatency = app && app->isInPlayModeAndNotPaused();
    this->asyncLoader->update(lowLatency);
}

void ResourceManager::destroyResources() {
    while(this->vResources.size() > 0) {
        destroyResource(this->vResources[0], DestroyMode::FORCE_BLOCKING);
    }
    this->vResources.clear();
    this->vImages.clear();
    this->vFonts.clear();
    this->vSounds.clear();
    this->vShaders.clear();
    this->vRenderTargets.clear();
    this->vTextureAtlases.clear();
    this->vVertexArrayObjects.clear();
    this->mNameToResourceMap.clear();
}

void ResourceManager::destroyResource(Resource *rs, DestroyMode destroyMode) {
    const bool debug = cv::debug_rm.getBool();
    if(rs == nullptr) {
        logIf(debug, "ResourceManager Warning: destroyResource(NULL)!");
        return;
    }

    logIf(debug, "ResourceManager: destroying {:8p} : {:s}", static_cast<const void *>(rs), rs->getName());

    bool isManagedResource = false;
    int managedResourceIndex = -1;
    for(size_t i = 0; i < this->vResources.size(); i++) {
        if(this->vResources[i] == rs) {
            isManagedResource = true;
            managedResourceIndex = i;
            break;
        }
    }

    // check if it's being loaded and schedule async destroy if so
    // (destroyMode == DestroyMode::FORCE_ASYNC) ||
    if(this->asyncLoader->isLoadingResource(rs)) {
        logIf(debug, "Resource Manager: Scheduled async destroy of {:8p} : {:s}", static_cast<const void *>(rs),
              rs->getName());

        // interrupt async load
        rs->interruptLoad();

        this->asyncLoader->scheduleAsyncDestroy(rs);

        if(isManagedResource) removeManagedResource(rs, managedResourceIndex);

        if(destroyMode == DestroyMode::FORCE_BLOCKING) {
            do {
                this->update();
            } while(this->asyncLoader->isLoadingResource(rs));
        }

        return;
    }

    // standard destroy
    if(isManagedResource) removeManagedResource(rs, managedResourceIndex);

    SAFE_DELETE(rs);
}

void ResourceManager::loadResource(Resource *res, bool load) {
    if(res == nullptr) {
        logIfCV(debug_rm, "ResourceManager Warning: loadResource(NULL)!");
        resetFlags();
        return;
    }

    // handle flags
    // NOLINTNEXTLINE(cppcoreguidelines-init-variables)
    bool isManaged;
    {
        Sync::shared_lock lock(this->managedLoadMutex);
        isManaged = (this->nextLoadUnmanagedStack.size() < 1 || !this->nextLoadUnmanagedStack.top());
    }

    if(isManaged) addManagedResource(res);

    const bool isNextLoadAsync = this->bNextLoadAsync.load(std::memory_order_acquire);

    // flags must be reset on every load, to not carry over
    resetFlags();

    if(!load) return;

    if(!isNextLoadAsync) {
        // load normally
        res->loadAsync();
        res->load();
    } else {
        // delegate to async loader
        this->asyncLoader->requestAsyncLoad(res);
    }
}

bool ResourceManager::isLoading() const { return this->asyncLoader->isLoading(); }

bool ResourceManager::isLoadingResource(Resource *rs) const { return this->asyncLoader->isLoadingResource(rs); }

size_t ResourceManager::getNumLoadingWork() const { return this->asyncLoader->getNumLoadingWork(); }

size_t ResourceManager::getNumActiveThreads() const { return this->asyncLoader->getNumActiveThreads(); }

size_t ResourceManager::getNumLoadingWorkAsyncDestroy() const {
    return this->asyncLoader->getNumLoadingWorkAsyncDestroy();
}

void ResourceManager::resetFlags() {
    {
        Sync::unique_lock lock(this->managedLoadMutex);
        if(this->nextLoadUnmanagedStack.size() > 0) this->nextLoadUnmanagedStack.pop();
    }

    this->bNextLoadAsync.store(false, std::memory_order_release);
}

void ResourceManager::requestNextLoadAsync() { this->bNextLoadAsync.store(true, std::memory_order_release); }

void ResourceManager::requestNextLoadUnmanaged() {
    Sync::unique_lock lock(this->managedLoadMutex);
    this->nextLoadUnmanagedStack.push(true);
}

size_t ResourceManager::getSyncLoadMaxBatchSize() const { return this->asyncLoader->getMaxPerUpdate(); }

void ResourceManager::resetSyncLoadMaxBatchSize() { this->asyncLoader->resetMaxPerUpdate(); }

void ResourceManager::setSyncLoadMaxBatchSize(size_t resourcesToLoad) {
    this->asyncLoader->setMaxPerUpdate(resourcesToLoad);
}

void ResourceManager::reloadResource(Resource *rs, bool async) {
    if(rs == nullptr) {
        logIfCV(debug_rm, "ResourceManager Warning: reloadResource(NULL)!");
        return;
    }

    const std::vector<Resource *> resourceToReload{rs};
    reloadResources(resourceToReload, async);
}

void ResourceManager::reloadResources(const std::vector<Resource *> &resources, bool async) {
    if(resources.empty()) {
        logIfCV(debug_rm, "ResourceManager Warning: reloadResources with an empty resources vector!");
        return;
    }

    if(!async)  // synchronous
    {
        // the resource could be in the middle of async loading
        // so if we explicitly want to load synchronously we have to wait until that's finished
        std::vector<Resource *> asyncLoadingList;
        for(auto &res : resources) {
            if(unlikely(this->asyncLoader->isLoadingResource(res))) {
                res->interruptLoad();
                asyncLoadingList.push_back(res);
            } else {
                res->reload();
            }
        }

        while(!asyncLoadingList.empty()) {
            this->update();
            for(auto resit = asyncLoadingList.begin(); resit != asyncLoadingList.end();) {
                if(this->asyncLoader->isLoadingResource(*resit)) {
                    ++resit;
                } else {
                    (*resit)->reload();
                    resit = asyncLoadingList.erase(resit);
                    continue;
                }
            }
        }

        return;
    }

    // delegate to async loader
    this->asyncLoader->reloadResources(resources);
}

void ResourceManager::setResourceName(Resource *res, std::string name) {
    if(!res) {
        logIfCV(debug_rm, "ResourceManager: attempted to set name {:s} on NULL resource!", name);
        return;
    }

    std::string currentName = res->getName();
    if(!currentName.empty() && currentName == name) return;  // it's already the same name, nothing to do

    if(name.empty())  // add a default name (mostly for debugging, see Resource constructor)
        name = fmt::format("{:p}:postinit=y:{:s}", static_cast<const void *>(res), res->getFilePath());

    res->setName(name);
    // add the new name to the resource map (if it's a managed resource)
    if(this->nextLoadUnmanagedStack.size() < 1 || !this->nextLoadUnmanagedStack.top())
        this->mNameToResourceMap.try_emplace(name, res);
    return;
}

Image *ResourceManager::loadImage(std::string filepath, const std::string &resourceName, bool mipmapped,
                                  bool keepInSystemMemory) {
    auto res = checkIfExistsAndHandle<Image>(resourceName);
    if(res != nullptr) return res;

    // create instance and load it
    filepath.insert(0, MCENGINE_IMAGES_PATH "/");
    Image *img = g->createImage(filepath, mipmapped, keepInSystemMemory);
    setResourceName(img, resourceName);

    loadResource(img, true);

    return img;
}

Image *ResourceManager::loadImageUnnamed(std::string filepath, bool mipmapped, bool keepInSystemMemory) {
    filepath.insert(0, MCENGINE_IMAGES_PATH "/");
    Image *img = g->createImage(filepath, mipmapped, keepInSystemMemory);

    loadResource(img, true);

    return img;
}

Image *ResourceManager::loadImageAbs(std::string absoluteFilepath, const std::string &resourceName, bool mipmapped,
                                     bool keepInSystemMemory) {
    auto res = checkIfExistsAndHandle<Image>(resourceName);
    if(res != nullptr) return res;

    // create instance and load it
    Image *img = g->createImage(std::move(absoluteFilepath), mipmapped, keepInSystemMemory);
    setResourceName(img, resourceName);

    loadResource(img, true);

    return img;
}

Image *ResourceManager::loadImageAbsUnnamed(std::string absoluteFilepath, bool mipmapped, bool keepInSystemMemory) {
    Image *img = g->createImage(std::move(absoluteFilepath), mipmapped, keepInSystemMemory);

    loadResource(img, true);

    return img;
}

Image *ResourceManager::createImage(i32 width, i32 height, bool mipmapped, bool keepInSystemMemory) {
    if(width > 8192 || height > 8192) {
        engine->showMessageError(
            "Resource Manager Error",
            UString::format("Invalid parameters in createImage(%i, %i, %i)!", width, height, (int)mipmapped));
        return nullptr;
    }

    Image *img = g->createImage(width, height, mipmapped, keepInSystemMemory);

    loadResource(img, false);

    return img;
}

McFont *ResourceManager::loadFont(std::string filepath, const std::string &resourceName, int fontSize,
                                  bool antialiasing, int fontDPI) {
    auto res = checkIfExistsAndHandle<McFont>(resourceName);
    if(res != nullptr) return res;

    // create instance and load it
    filepath.insert(0, MCENGINE_FONTS_PATH "/");
    auto *fnt = new McFont(std::move(filepath), fontSize, antialiasing, fontDPI);
    setResourceName(fnt, resourceName);

    loadResource(fnt, true);

    return fnt;
}

McFont *ResourceManager::loadFont(std::string filepath, const std::string &resourceName,
                                  const std::vector<char16_t> &characters, int fontSize, bool antialiasing,
                                  int fontDPI) {
    auto res = checkIfExistsAndHandle<McFont>(resourceName);
    if(res != nullptr) return res;

    // create instance and load it
    filepath.insert(0, MCENGINE_FONTS_PATH "/");
    auto *fnt = new McFont(std::move(filepath), characters, fontSize, antialiasing, fontDPI);
    setResourceName(fnt, resourceName);

    loadResource(fnt, true);

    return fnt;
}

Sound *ResourceManager::loadSound(std::string filepath, const std::string &resourceName, bool stream, bool overlayable,
                                  bool loop) {
    auto res = checkIfExistsAndHandle<Sound>(resourceName);
    if(res != nullptr) return res;

    // create instance and load it
    filepath.insert(0, MCENGINE_SOUNDS_PATH "/");
    auto *snd{Sound::createSound(filepath, stream, overlayable, loop)};
    setResourceName(snd, resourceName);

    loadResource(snd, true);

    return snd;
}

Sound *ResourceManager::loadSoundAbs(std::string filepath, const std::string &resourceName, bool stream,
                                     bool overlayable, bool loop) {
    auto res = checkIfExistsAndHandle<Sound>(resourceName);
    if(res != nullptr) return res;

    // create instance and load it
    auto *snd{Sound::createSound(std::move(filepath), stream, overlayable, loop)};
    setResourceName(snd, resourceName);

    loadResource(snd, true);

    return snd;
}

Shader *ResourceManager::loadShader(std::string vertexShaderFilePath, std::string fragmentShaderFilePath,
                                    const std::string &resourceName) {
    auto res = checkIfExistsAndHandle<Shader>(resourceName);
    if(res != nullptr) return res;

    // create instance and load it
    vertexShaderFilePath.insert(0, MCENGINE_SHADERS_PATH "/");
    fragmentShaderFilePath.insert(0, MCENGINE_SHADERS_PATH "/");
    Shader *shader = g->createShaderFromFile(vertexShaderFilePath, fragmentShaderFilePath);
    setResourceName(shader, resourceName);

    loadResource(shader, true);

    return shader;
}

Shader *ResourceManager::loadShader(std::string vertexShaderFilePath, std::string fragmentShaderFilePath) {
    vertexShaderFilePath.insert(0, MCENGINE_SHADERS_PATH "/");
    fragmentShaderFilePath.insert(0, MCENGINE_SHADERS_PATH "/");
    Shader *shader = g->createShaderFromFile(vertexShaderFilePath, fragmentShaderFilePath);

    loadResource(shader, true);

    return shader;
}

Shader *ResourceManager::createShader(std::string vertexShader, std::string fragmentShader,
                                      const std::string &resourceName) {
    auto res = checkIfExistsAndHandle<Shader>(resourceName);
    if(res != nullptr) return res;

    // create instance and load it
    Shader *shader = g->createShaderFromSource(std::move(vertexShader), std::move(fragmentShader));
    setResourceName(shader, resourceName);

    loadResource(shader, true);

    return shader;
}

Shader *ResourceManager::createShader(std::string vertexShader, std::string fragmentShader) {
    Shader *shader = g->createShaderFromSource(std::move(vertexShader), std::move(fragmentShader));

    loadResource(shader, true);

    return shader;
}

RenderTarget *ResourceManager::createRenderTarget(int x, int y, int width, int height,
                                                  Graphics::MULTISAMPLE_TYPE multiSampleType) {
    RenderTarget *rt = g->createRenderTarget(x, y, width, height, multiSampleType);
    setResourceName(rt, UString::format("_RT_%ix%i", width, height).toUtf8());

    loadResource(rt, true);

    return rt;
}

RenderTarget *ResourceManager::createRenderTarget(int width, int height, Graphics::MULTISAMPLE_TYPE multiSampleType) {
    return createRenderTarget(0, 0, width, height, multiSampleType);
}

TextureAtlas *ResourceManager::createTextureAtlas(int width, int height, bool filtering) {
    auto *ta = new TextureAtlas(width, height, filtering);
    setResourceName(ta, UString::format("_TA_%ix%i", width, height).toUtf8());

    loadResource(ta, false);

    return ta;
}

VertexArrayObject *ResourceManager::createVertexArrayObject(Graphics::PRIMITIVE primitive, Graphics::USAGE_TYPE usage,
                                                            bool keepInSystemMemory) {
    VertexArrayObject *vao = g->createVertexArrayObject(primitive, usage, keepInSystemMemory);

    loadResource(vao, false);

    return vao;
}

// add a managed resource to the main resources vector + the name map and typed vectors
void ResourceManager::addManagedResource(Resource *res) {
    if(!res) return;
    logIfCV(debug_rm, "ResourceManager: Adding managed {}", res->getName());

    this->vResources.push_back(res);

    if(res->getName().length() > 0) this->mNameToResourceMap.try_emplace(res->getName(), res);
    addResourceToTypedVector(res);
}

// remove a managed resource from the main resources vector + the name map and typed vectors
void ResourceManager::removeManagedResource(Resource *res, int managedResourceIndex) {
    if(!res) return;

    this->vResources.erase(this->vResources.begin() + managedResourceIndex);

    if(res->getName().length() > 0) this->mNameToResourceMap.erase(res->getName());
    removeResourceFromTypedVector(res);
}

void ResourceManager::addResourceToTypedVector(Resource *res) {
    if(!res) return;

    switch(res->getResType()) {
        case Resource::Type::IMAGE:
            this->vImages.push_back(res->asImage());
            break;
        case Resource::Type::FONT:
            this->vFonts.push_back(res->asFont());
            break;
        case Resource::Type::SOUND:
            this->vSounds.push_back(res->asSound());
            break;
        case Resource::Type::SHADER:
            this->vShaders.push_back(res->asShader());
            break;
        case Resource::Type::RENDERTARGET:
            this->vRenderTargets.push_back(res->asRenderTarget());
            break;
        case Resource::Type::TEXTUREATLAS:
            this->vTextureAtlases.push_back(res->asTextureAtlas());
            break;
        case Resource::Type::VAO:
            this->vVertexArrayObjects.push_back(res->asVAO());
            break;
        case Resource::Type::APPDEFINED:
            // app-defined types aren't added to specific vectors
            break;
    }
}

void ResourceManager::removeResourceFromTypedVector(Resource *res) {
    if(!res) return;

    switch(res->getResType()) {
        case Resource::Type::IMAGE: {
            auto it = std::ranges::find(this->vImages, res);
            if(it != this->vImages.end()) this->vImages.erase(it);
        } break;
        case Resource::Type::FONT: {
            auto it = std::ranges::find(this->vFonts, res);
            if(it != this->vFonts.end()) this->vFonts.erase(it);
        } break;
        case Resource::Type::SOUND: {
            auto it = std::ranges::find(this->vSounds, res);
            if(it != this->vSounds.end()) this->vSounds.erase(it);
        } break;
        case Resource::Type::SHADER: {
            auto it = std::ranges::find(this->vShaders, res);
            if(it != this->vShaders.end()) this->vShaders.erase(it);
        } break;
        case Resource::Type::RENDERTARGET: {
            auto it = std::ranges::find(this->vRenderTargets, res);
            if(it != this->vRenderTargets.end()) this->vRenderTargets.erase(it);
        } break;
        case Resource::Type::TEXTUREATLAS: {
            auto it = std::ranges::find(this->vTextureAtlases, res);
            if(it != this->vTextureAtlases.end()) this->vTextureAtlases.erase(it);
        } break;
        case Resource::Type::VAO: {
            auto it = std::ranges::find(this->vVertexArrayObjects, res);
            if(it != this->vVertexArrayObjects.end()) this->vVertexArrayObjects.erase(it);
        } break;
        case Resource::Type::APPDEFINED:
            // app-defined types aren't added to specific vectors
            break;
    }
}

void ResourceManager::notExistLog(std::string_view resourceName) {
    logIfCV(debug_rm, R"(ResourceManager WARNING: Resource "{:s}" does not exist!)", resourceName);
}

void ResourceManager::alreadyLoadedLog(std::string_view resourceName) {
    logIfCV(debug_rm, R"(ResourceManager NOTICE: Resource "{:s}" already loaded.)", resourceName);
}
