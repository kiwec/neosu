// Copyright (c) 2017, PG, All rights reserved.
#include "SkinImage.h"

#include "ConVar.h"
#include "Engine.h"
#include "Environment.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "Skin.h"
#include "Logging.h"

SkinImage::SkinImage(Skin* skin, const std::string& skinElementName, vec2 baseSizeForScaling2x, float osuSize,
                     const std::string& animationSeparator, bool ignoreDefaultSkin) {
    this->skin = skin;
    this->vBaseSizeForScaling2x = baseSizeForScaling2x;
    this->fOsuSize = osuSize;

    this->bReady = false;

    this->iCurMusicPos = 0;
    this->iFrameCounter = 0;
    this->iFrameCounterUnclamped = 0;
    this->iBeatmapAnimationTimeStartOffset = 0;

    this->bIsMissingTexture = false;
    this->bIsFromDefaultSkin = false;

    this->fDrawClipWidthPercent = 1.0f;

    // logic: first load user skin (true), and if no image could be found then load the default skin (false)
    // this is necessary so that all elements can be correctly overridden with a user skin (e.g. if the user skin only
    // has sliderb.png, but the default skin has sliderb0.png!)
    if(!this->load(skinElementName, animationSeparator, true)) {
        if(!ignoreDefaultSkin) this->load(skinElementName, animationSeparator, false);
    }

    // if we couldn't load ANYTHING at all, gracefully fallback to missing texture
    if(this->images.size() < 1) {
        this->bIsMissingTexture = true;

        IMAGE missingTexture;

        missingTexture.img = this->skin->getMissingTexture();
        missingTexture.scale = 2;

        this->images.push_back(missingTexture);
    }

    // if AnimationFramerate is defined in skin, use that. otherwise derive framerate from number of frames
    if(this->skin->getAnimationFramerate() > 0.0f)
        this->fFrameDuration = 1.0f / this->skin->getAnimationFramerate();
    else if(this->images.size() > 0)
        this->fFrameDuration = 1.0f / (float)this->images.size();
}

bool SkinImage::load(const std::string& skinElementName, const std::string& animationSeparator,
                     bool ignoreDefaultSkin) {
    std::string animatedSkinElementStartName = skinElementName;
    animatedSkinElementStartName.append(animationSeparator);
    animatedSkinElementStartName.append("0");
    if(this->loadImage(animatedSkinElementStartName,
                       ignoreDefaultSkin))  // try loading the first animated element (if this exists then we continue
                                            // loading until the first missing frame)
    {
        int frame = 1;
        while(true) {
            std::string currentAnimatedSkinElementFrameName = skinElementName;
            currentAnimatedSkinElementFrameName.append(animationSeparator);
            currentAnimatedSkinElementFrameName.append(std::to_string(frame));

            if(!this->loadImage(currentAnimatedSkinElementFrameName, ignoreDefaultSkin))
                break;  // stop loading on the first missing frame

            frame++;

            // sanity check
            if(frame > 511) {
                debugLog("SkinImage WARNING: Force stopped loading after 512 frames!");
                break;
            }
        }
    } else  // load non-animated skin element
        this->loadImage(skinElementName, ignoreDefaultSkin);

    return this->images.size() > 0;  // if any image was found
}

bool SkinImage::loadImage(const std::string& skinElementName, bool ignoreDefaultSkin) {
    std::string filepath1 = this->skin->getFilePath();
    filepath1.append(skinElementName);
    filepath1.append("@2x.png");

    std::string filepath2 = this->skin->getFilePath();
    filepath2.append(skinElementName);
    filepath2.append(".png");

    std::string defaultFilePath1 = MCENGINE_IMAGES_PATH "/default/";
    defaultFilePath1.append(skinElementName);
    defaultFilePath1.append("@2x.png");

    std::string defaultFilePath2 = MCENGINE_IMAGES_PATH "/default/";
    defaultFilePath2.append(skinElementName);
    defaultFilePath2.append(".png");

    const bool existsFilepath1 = env->fileExists(filepath1);
    const bool existsFilepath2 = env->fileExists(filepath2);
    const bool existsDefaultFilePath1 = env->fileExists(defaultFilePath1);
    const bool existsDefaultFilePath2 = env->fileExists(defaultFilePath2);

    // load user skin

    // check if an @2x version of this image exists
    if(cv::skin_hd.getBool()) {
        // load user skin

        if(existsFilepath1) {
            IMAGE image;

            if(cv::skin_async.getBool()) resourceManager->requestNextLoadAsync();

            image.img = resourceManager->loadImageAbsUnnamed(filepath1, cv::skin_mipmaps.getBool());
            image.scale = 2.0f;

            this->images.push_back(image);

            // export
            {
                this->filepathsForExport.push_back(filepath1);

                if(existsFilepath2) this->filepathsForExport.push_back(filepath2);
            }

            this->is_2x = true;
            return true;  // nothing more to do here
        }
    }
    // else load the normal version

    // load user skin

    if(existsFilepath2) {
        IMAGE image;

        if(cv::skin_async.getBool()) resourceManager->requestNextLoadAsync();

        image.img = resourceManager->loadImageAbsUnnamed(filepath2, cv::skin_mipmaps.getBool());
        image.scale = 1.0f;

        this->images.push_back(image);

        // export
        {
            this->filepathsForExport.push_back(filepath2);

            if(existsFilepath1) this->filepathsForExport.push_back(filepath1);
        }

        this->is_2x = false;
        return true;  // nothing more to do here
    }

    if(ignoreDefaultSkin) return false;

    // load default skin

    this->bIsFromDefaultSkin = true;

    // check if an @2x version of this image exists
    if(cv::skin_hd.getBool()) {
        if(existsDefaultFilePath1) {
            IMAGE image;

            if(cv::skin_async.getBool()) resourceManager->requestNextLoadAsync();

            image.img = resourceManager->loadImageAbsUnnamed(defaultFilePath1, cv::skin_mipmaps.getBool());
            image.scale = 2.0f;

            this->images.push_back(image);

            // export
            {
                this->filepathsForExport.push_back(defaultFilePath1);

                if(existsDefaultFilePath2) this->filepathsForExport.push_back(defaultFilePath2);
            }

            this->is_2x = true;
            return true;  // nothing more to do here
        }
    }
    // else load the normal version

    if(existsDefaultFilePath2) {
        IMAGE image;

        if(cv::skin_async.getBool()) resourceManager->requestNextLoadAsync();

        image.img = resourceManager->loadImageAbsUnnamed(defaultFilePath2, cv::skin_mipmaps.getBool());
        image.scale = 1.0f;

        this->images.push_back(image);

        // export
        {
            this->filepathsForExport.push_back(defaultFilePath2);

            if(existsDefaultFilePath1) this->filepathsForExport.push_back(defaultFilePath1);
        }

        this->is_2x = false;
        return true;  // nothing more to do here
    }

    return false;
}

SkinImage::~SkinImage() {
    for(auto& image : this->images) {
        if(image.img != this->skin->getMissingTexture()) resourceManager->destroyResource(image.img);
    }
    this->images.clear();

    this->filepathsForExport.clear();
}

void SkinImage::draw(vec2 pos, float scale) {
    if(this->images.size() < 1) return;

    scale *= this->getScale();  // auto scale to current resolution

    g->pushTransform();
    {
        g->scale(scale, scale);
        g->translate(pos.x, pos.y);

        Image* img = this->getImageForCurrentFrame().img;

        if(this->fDrawClipWidthPercent == 1.0f)
            g->drawImage(img);
        else if(img->isReady()) {
            const float realWidth = img->getWidth();
            const float realHeight = img->getHeight();

            const float width = realWidth * this->fDrawClipWidthPercent;
            const float height = realHeight;

            const float x = -realWidth / 2;
            const float y = -realHeight / 2;

            VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_QUADS);

            vao.addVertex(x, y);
            vao.addTexcoord(0, 0);

            vao.addVertex(x, (y + height));
            vao.addTexcoord(0, 1);

            vao.addVertex((x + width), (y + height));
            vao.addTexcoord(this->fDrawClipWidthPercent, 1);

            vao.addVertex((x + width), y);
            vao.addTexcoord(this->fDrawClipWidthPercent, 0);

            img->bind();
            {
                g->drawVAO(&vao);
            }
            img->unbind();
        }
    }
    g->popTransform();
}

void SkinImage::drawRaw(vec2 pos, float scale, AnchorPoint anchor) {
    if(this->images.size() < 1) return;

    g->pushTransform();
    {
        g->scale(scale, scale);
        g->translate(pos.x, pos.y);

        Image* img = this->getImageForCurrentFrame().img;

        if(this->fDrawClipWidthPercent == 1.0f) {
            g->drawImage(img, anchor);
        } else if(img->isReady()) {
            // NOTE: Anchor point not handled here, but fDrawClipWidthPercent only used for health bar right now
            const float realWidth = img->getWidth();
            const float realHeight = img->getHeight();

            const float width = realWidth * this->fDrawClipWidthPercent;
            const float height = realHeight;

            const float x = -realWidth / 2;
            const float y = -realHeight / 2;

            VertexArrayObject vao(Graphics::PRIMITIVE::PRIMITIVE_QUADS);

            vao.addVertex(x, y);
            vao.addTexcoord(0, 0);

            vao.addVertex(x, (y + height));
            vao.addTexcoord(0, 1);

            vao.addVertex((x + width), (y + height));
            vao.addTexcoord(this->fDrawClipWidthPercent, 1);

            vao.addVertex((x + width), y);
            vao.addTexcoord(this->fDrawClipWidthPercent, 0);

            img->bind();
            {
                g->drawVAO(&vao);
            }
            img->unbind();
        }
    }
    g->popTransform();
}

void SkinImage::update(float speedMultiplier, bool useEngineTimeForAnimations, i32 curMusicPos) {
    if(this->images.size() < 1 || speedMultiplier == 0.f) return;

    this->iCurMusicPos = curMusicPos;

    const f64 frameDurationInSeconds =
        (cv::skin_animation_fps_override.getFloat() > 0.0f ? (1.0f / cv::skin_animation_fps_override.getFloat())
                                                           : this->fFrameDuration) /
        speedMultiplier;
    if(frameDurationInSeconds == 0.f) {
        this->iFrameCounter = 0;
        this->iFrameCounterUnclamped = 0;
        return;
    }

    if(useEngineTimeForAnimations) {
        this->iFrameCounter = (i32)(engine->getTime() / frameDurationInSeconds) % this->images.size();
    } else {
        // when playing a beatmap, objects start the animation at frame 0 exactly when they first become visible (this
        // wouldn't work with the engine time method) therefore we need an offset parameter in the same time-space as
        // the beatmap (this->iBeatmapTimeAnimationStartOffset), and we need the beatmap time (curMusicPos) as a
        // relative base m_iBeatmapAnimationTimeStartOffset must be set by all hitobjects live while drawing (e.g. to
        // their click_time-m_iObjectTime), since we don't have any animation state saved in the hitobjects!

        i32 frame_duration_ms = frameDurationInSeconds * 1000.0f;

        // freeze animation on frame 0 on negative offsets
        this->iFrameCounter =
            std::max((i32)((curMusicPos - this->iBeatmapAnimationTimeStartOffset) / frame_duration_ms), 0);
        this->iFrameCounterUnclamped = this->iFrameCounter;
        this->iFrameCounter = this->iFrameCounter % this->images.size();
    }
}

void SkinImage::setAnimationTimeOffset(float speedMultiplier, i32 offset) {
    this->iBeatmapAnimationTimeStartOffset = offset;
    this->update(speedMultiplier, false, this->iCurMusicPos);  // force update
}

void SkinImage::setAnimationFrameForce(int frame) {
    if(this->images.size() < 1) return;
    this->iFrameCounter = frame % this->images.size();
    this->iFrameCounterUnclamped = this->iFrameCounter;
}

void SkinImage::setAnimationFrameClampUp() {
    if(this->images.size() > 0 && this->iFrameCounterUnclamped > this->images.size() - 1)
        this->iFrameCounter = this->images.size() - 1;
}

vec2 SkinImage::getSize() {
    if(this->images.size() > 0)
        return this->getImageForCurrentFrame().img->getSize() * this->getScale();
    else
        return this->getSizeBase();
}

vec2 SkinImage::getSizeBase() { return this->vBaseSizeForScaling2x * this->getResolutionScale(); }

vec2 SkinImage::getSizeBaseRaw() { return this->vBaseSizeForScaling2x * this->getImageForCurrentFrame().scale; }

vec2 SkinImage::getImageSizeForCurrentFrame() { return this->getImageForCurrentFrame().img->getSize(); }

float SkinImage::getScale() { return this->getImageScale() * this->getResolutionScale(); }

float SkinImage::getImageScale() {
    if(this->images.size() > 0)
        return this->vBaseSizeForScaling2x.x / this->getSizeBaseRaw().x;  // allow overscale and underscale
    else
        return 1.0f;
}

float SkinImage::getResolutionScale() { return Osu::getImageScale(this->vBaseSizeForScaling2x, this->fOsuSize); }

bool SkinImage::isReady() {
    if(this->bReady) return true;

    for(auto& image : this->images) {
        if(resourceManager->isLoadingResource(image.img)) return false;
    }

    this->bReady = true;
    return this->bReady;
}

SkinImage::IMAGE SkinImage::getImageForCurrentFrame() {
    if(this->images.size() > 0)
        return this->images[this->iFrameCounter % this->images.size()];
    else {
        IMAGE image;

        image.img = this->skin->getMissingTexture();
        image.scale = 1.0f;

        return image;
    }
}
