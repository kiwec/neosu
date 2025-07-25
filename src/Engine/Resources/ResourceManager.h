//========== Copyright (c) 2015, PG & 2025, WH, All rights reserved. ============//
//
// Purpose:		resource manager
//
// $NoKeywords: $rm
//===============================================================================//

#pragma once
#ifndef RESOURCEMANAGER_H
#define RESOURCEMANAGER_H

#include "ConVar.h"
#include "Engine.h"
#include "Font.h"
#include "Image.h"
#include "RenderTarget.h"
#include "Shader.h"
#include "Sound.h"
#include "TextureAtlas.h"
#include "VertexArrayObject.h"

#include <stack>
#include <unordered_map>

class Sound;
class TextureAtlas;
class AsyncResourceLoader;
class ResourceManager final {
   public:
    static constexpr auto PATH_DEFAULT_IMAGES = "materials/";
    static constexpr auto PATH_DEFAULT_FONTS = "fonts/";
    static constexpr auto PATH_DEFAULT_SOUNDS = "sounds/";
    static constexpr auto PATH_DEFAULT_SHADERS = "shaders/";

   public:
    ResourceManager();
    ~ResourceManager();

    void update();

    void loadResource(Resource *rs) {
        requestNextLoadUnmanaged();
        loadResource(rs, true);
    }
    void destroyResource(Resource *rs);
    void destroyResources();

    // async reload
    void reloadResource(Resource *rs, bool async = false);
    void reloadResources(const std::vector<Resource *> &resources, bool async = false);

    void requestNextLoadAsync();
    void requestNextLoadUnmanaged();

    // can't allow directly setting resource names, otherwise the map will go out of sync
    void setResourceName(Resource *res, std::string name);

    // images
    Image *loadImage(std::string filepath, std::string resourceName, bool mipmapped = false,
                     bool keepInSystemMemory = false);
    Image *loadImageUnnamed(std::string filepath, bool mipmapped = false, bool keepInSystemMemory = false);
    Image *loadImageAbs(std::string absoluteFilepath, std::string resourceName, bool mipmapped = false,
                        bool keepInSystemMemory = false);
    Image *loadImageAbsUnnamed(std::string absoluteFilepath, bool mipmapped = false, bool keepInSystemMemory = false);
    Image *createImage(unsigned int width, unsigned int height, bool mipmapped = false,
                       bool keepInSystemMemory = false);

    // fonts
    McFont *loadFont(std::string filepath, std::string resourceName, int fontSize = 16, bool antialiasing = true,
                     int fontDPI = 96);
    McFont *loadFont(std::string filepath, std::string resourceName, const std::vector<wchar_t> &characters,
                     int fontSize = 16, bool antialiasing = true, int fontDPI = 96);

    // sounds
    Sound *loadSound(std::string filepath, std::string resourceName, bool stream = false, bool overlayable = false,
                     bool loop = false);
    Sound *loadSoundAbs(std::string filepath, std::string resourceName, bool stream = false, bool overlayable = false,
                        bool loop = false);

    // shaders
    Shader *loadShader(std::string vertexShaderFilePath, std::string fragmentShaderFilePath, std::string resourceName);
    Shader *loadShader(std::string vertexShaderFilePath, std::string fragmentShaderFilePath);
    Shader *createShader(std::string vertexShader, std::string fragmentShader, std::string resourceName);
    Shader *createShader(std::string vertexShader, std::string fragmentShader);

    // rendertargets
    RenderTarget *createRenderTarget(
        int x, int y, int width, int height,
        Graphics::MULTISAMPLE_TYPE multiSampleType = Graphics::MULTISAMPLE_TYPE::MULTISAMPLE_0X);
    RenderTarget *createRenderTarget(
        int width, int height, Graphics::MULTISAMPLE_TYPE multiSampleType = Graphics::MULTISAMPLE_TYPE::MULTISAMPLE_0X);

    // texture atlas
    TextureAtlas *createTextureAtlas(int width, int height);

    // models/meshes
    VertexArrayObject *createVertexArrayObject(Graphics::PRIMITIVE primitive = Graphics::PRIMITIVE::PRIMITIVE_TRIANGLES,
                                               Graphics::USAGE_TYPE usage = Graphics::USAGE_TYPE::USAGE_STATIC,
                                               bool keepInSystemMemory = false);

    // resource access by name
    [[nodiscard]] Image *getImage(std::string resourceName) const { return tryGet<Image>(resourceName); }
    [[nodiscard]] McFont *getFont(std::string resourceName) const { return tryGet<McFont>(resourceName); }
    [[nodiscard]] Sound *getSound(std::string resourceName) const { return tryGet<Sound>(resourceName); }
    [[nodiscard]] Shader *getShader(std::string resourceName) const { return tryGet<Shader>(resourceName); }

    // methods for getting all resources of a type
    [[nodiscard]] constexpr const std::vector<Image *> &getImages() const { return this->vImages; }
    [[nodiscard]] constexpr const std::vector<McFont *> &getFonts() const { return this->vFonts; }
    [[nodiscard]] constexpr const std::vector<Sound *> &getSounds() const { return this->vSounds; }
    [[nodiscard]] constexpr const std::vector<Shader *> &getShaders() const { return this->vShaders; }
    [[nodiscard]] constexpr const std::vector<RenderTarget *> &getRenderTargets() const { return this->vRenderTargets; }
    [[nodiscard]] constexpr const std::vector<TextureAtlas *> &getTextureAtlases() const {
        return this->vTextureAtlases;
    }
    [[nodiscard]] constexpr const std::vector<VertexArrayObject *> &getVertexArrayObjects() const {
        return this->vVertexArrayObjects;
    }

    [[nodiscard]] constexpr const std::vector<Resource *> &getResources() const { return this->vResources; }

    [[nodiscard]] bool isLoading() const;
    bool isLoadingResource(Resource *rs) const;
    [[nodiscard]] size_t getNumLoadingWork() const;
    [[nodiscard]] size_t getNumActiveThreads() const;
    [[nodiscard]] size_t getNumLoadingWorkAsyncDestroy() const;

   private:
    template <typename T>
    [[nodiscard]] T *tryGet(std::string &resourceName) const {
        if(resourceName.empty()) return nullptr;
        auto it = this->mNameToResourceMap.find(resourceName);
        if(it != this->mNameToResourceMap.end()) return it->second->as<T>();
        if(cv::debug_rm.getBool())
            debugLogF(R"(ResourceManager WARNING: Resource "{:s}" does not exist!)"
                      "\n",
                      resourceName);
        return nullptr;
    }
    template <typename T>
    [[nodiscard]] T *checkIfExistsAndHandle(std::string &resourceName) {
        if(resourceName.empty()) return nullptr;
        auto it = this->mNameToResourceMap.find(resourceName);
        if(it == this->mNameToResourceMap.end()) return nullptr;
        if(cv::debug_rm.getBool())
            debugLogF(R"(ResourceManager NOTICE: Resource "{:s}" already loaded.)"
                      "\n",
                      resourceName);
        // handle flags (reset them)
        resetFlags();
        return it->second->as<T>();
    }

    void loadResource(Resource *res, bool load);
    void resetFlags();

    void addManagedResource(Resource *res);
    void removeManagedResource(Resource *res, int managedResourceIndex);

    // helper methods for managing typed resource vectors
    void addResourceToTypedVector(Resource *res);
    void removeResourceFromTypedVector(Resource *res);

    // async loading system
    AsyncResourceLoader *asyncLoader;

    // flags
    bool bNextLoadAsync;
    std::stack<bool> nextLoadUnmanagedStack;

    // content
    std::vector<Resource *> vResources;

    // fast name lookup
    std::unordered_map<std::string, Resource *> mNameToResourceMap;

    // typed resource vectors for fast type-specific access
    std::vector<Image *> vImages;
    std::vector<McFont *> vFonts;
    std::vector<Sound *> vSounds;
    std::vector<Shader *> vShaders;
    std::vector<RenderTarget *> vRenderTargets;
    std::vector<TextureAtlas *> vTextureAtlases;
    std::vector<VertexArrayObject *> vVertexArrayObjects;
};

#endif
