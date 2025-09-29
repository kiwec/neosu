// Copyright (c) 2015, PG, All rights reserved.
#include "MainMenu.h"

#include <cmath>
#include <utility>

#include "AnimationHandler.h"
#include "AsyncIOHandler.h"
#include "BackgroundImageHandler.h"
#include "Bancho.h"
#include "BanchoNetworking.h"
#include "BeatmapInterface.h"
#include "CBaseUIButton.h"
#include "CBaseUIContainer.h"
#include "ConVar.h"
#include "Database.h"
#include "DatabaseBeatmap.h"
#include "Downloader.h"
#include "Engine.h"
#include "File.h"
#include "HUD.h"
#include "Keyboard.h"
#include "Lobby.h"
#include "Mouse.h"
#include "OptionsMenu.h"
#include "Osu.h"
#include "ResourceManager.h"
#include "RichPresence.h"
#include "Skin.h"
#include "SkinImage.h"
#include "SongBrowser/SongBrowser.h"
#include "SoundEngine.h"
#include "UIButton.h"
#include "UpdateHandler.h"
#include "VertexArrayObject.h"
#include "Logging.h"

UString MainMenu::NEOSU_MAIN_BUTTON_TEXT = UString("neosu");
UString MainMenu::NEOSU_MAIN_BUTTON_SUBTEXT = UString("Multiplayer Client");

class MainMenu::CubeButton final : public CBaseUIButton {
   public:
    CubeButton(MainMenu *parent, float xPos, float yPos, float xSize, float ySize, UString name, UString text)
        : CBaseUIButton(xPos, yPos, xSize, ySize, std::move(name), std::move(text)), mm_ptr(parent) {}

    void draw() override {
        // draw nothing
    }

    void onMouseInside() override {
        anim->moveQuadInOut(&this->mm_ptr->fSizeAddAnim, 0.12f, 0.15f, 0.0f, true);

        CBaseUIButton::onMouseInside();

        if(this->mm_ptr->button_sound_cooldown + 0.05f < engine->getTime()) {
            soundEngine->play(osu->getSkin()->getHoverMainMenuCubeSound());
            this->mm_ptr->button_sound_cooldown = engine->getTime();
        }
    }

    void onMouseOutside() override {
        anim->moveQuadInOut(&this->mm_ptr->fSizeAddAnim, 0.0f, 0.15f, 0.0f, true);

        CBaseUIButton::onMouseOutside();
    }

   private:
    MainMenu *mm_ptr;
};

class MainMenu::MainButton final : public CBaseUIButton {
   public:
    MainButton(MainMenu *parent, float xPos, float yPos, float xSize, float ySize, UString name, UString text)
        : CBaseUIButton(xPos, yPos, xSize, ySize, std::move(name), std::move(text)), mm_ptr(parent) {}

    void onMouseDownInside(bool left = true, bool right = false) override {
        if(this->mm_ptr->cube->isMouseInside()) return;
        CBaseUIButton::onMouseDownInside(left, right);
    }
    void onMouseInside() override {
        if(this->mm_ptr->cube->isMouseInside()) return;
        CBaseUIButton::onMouseInside();

        if(this->mm_ptr->button_sound_cooldown + 0.05f < engine->getTime()) {
            if(this->getText() == UString("Singleplayer")) {
                soundEngine->play(osu->getSkin()->getHoverSingleplayerSound());
            } else if(this->getText() == UString("Multiplayer")) {
                soundEngine->play(osu->getSkin()->getHoverMultiplayerSound());
            } else if(this->getText() == UString("Options (CTRL + O)")) {
                soundEngine->play(osu->getSkin()->getHoverOptionsSound());
            } else if(this->getText() == UString("Exit")) {
                soundEngine->play(osu->getSkin()->getHoverExitSound());
            }

            this->mm_ptr->button_sound_cooldown = engine->getTime();
        }
    }

   private:
    MainMenu *mm_ptr;
};

MainMenu::MainMenu() : OsuScreen() {
    // engine settings
    mouse->addListener(this);

    this->fSizeAddAnim = 0.0f;
    this->fCenterOffsetAnim = 0.0f;
    this->bMenuElementsVisible = false;

    this->fMainMenuAnimTime = 0.0f;
    this->fMainMenuAnimDuration = 0.0f;
    this->fMainMenuAnim = 0.0f;
    this->fMainMenuAnim1 = 0.0f;
    this->fMainMenuAnim2 = 0.0f;
    this->fMainMenuAnim3 = 0.0f;
    this->fMainMenuAnim1Target = 0.0f;
    this->fMainMenuAnim2Target = 0.0f;
    this->fMainMenuAnim3Target = 0.0f;
    this->bInMainMenuRandomAnim = false;
    this->iMainMenuRandomAnimType = 0;
    this->iMainMenuAnimBeatCounter = 0;

    this->bMainMenuAnimFriend = false;
    this->bMainMenuAnimFadeToFriendForNextAnim = false;
    this->bMainMenuAnimFriendScheduled = false;
    this->fMainMenuAnimFriendPercent = 0.0f;
    this->fMainMenuAnimFriendEyeFollowX = 0.0f;
    this->fMainMenuAnimFriendEyeFollowY = 0.0f;

    this->fShutdownScheduledTime = 0.0f;
    this->bWasCleanShutdown = false;

    this->fUpdateStatusTime = 0.0f;
    this->fUpdateButtonTextTime = 0.0f;
    this->fUpdateButtonAnim = 0.0f;
    this->fUpdateButtonAnimTime = 0.0f;
    this->bHasClickedUpdate = false;

    this->logo_img = resourceManager->loadImage("neosu.png", "NEOSU_LOGO", true /* mipmapped */);
    // background_shader = resourceManager->loadShader("main_menu_bg.vsh", "main_menu_bg.fsh");

    // check if the user has never clicked the changelog for this update
    this->bDidUserUpdateFromOlderVersion = false;
    this->bDrawVersionNotificationArrow = false;
    {
        if(env->fileExists("version.txt")) {
            File versionFile("version.txt");
            std::string linebuf{};
            float version = -1.f;
            u64 buildstamp = 0;
            // get version number
            if(versionFile.canRead() && ((linebuf = versionFile.readLine()) != "") &&
               ((version = std::strtof(linebuf.c_str(), nullptr)) > 0.f)) {
                // get build timestamp
                if(versionFile.canRead() && ((linebuf = versionFile.readLine()) != "") &&
                   ((buildstamp = std::strtoull(linebuf.c_str(), nullptr, 10)) > 0)) {
                    // ignore bogus build timestamps (before 2025 or after 2030)
                    if(buildstamp > 30000000 || buildstamp < 25000000) {
                        buildstamp = cv::build_timestamp.getVal<u64>();
                    }
                }
                // debugLog("versionFile version: {} our version: {}{}", version, cv::version.getFloat(),
                //           buildstamp > 0.0f ? fmt::format(" build timestamp: {}", buildstamp) : "");
                if(version < cv::version.getFloat() || buildstamp < cv::build_timestamp.getVal<u64>()) {
                    this->bDrawVersionNotificationArrow = true;
                }
                if(version < 35.06) {
                    // SoundEngine choking issues have been fixed, option has been removed from settings menu
                    // We leave the cvar available as it could still be useful for some players
                    cv::restart_sound_engine_before_playing.setValue(false);

                    // 0.5 is shit default value
                    if(cv::songbrowser_search_delay.getFloat() == 0.5f) {
                        cv::songbrowser_search_delay.setValue(0.2f);
                    }

                    // Match osu!stable value
                    if(cv::relax_offset.getFloat() == 0.f) {
                        cv::relax_offset.setValue(-12.f);
                    }

                    osu->getOptionsMenu()->save();
                }
                if(version < 39.00) {
                    if(!cv::mp_password.getString().empty()) {
                        const char *plaintext_pw{cv::mp_password.getString().c_str()};
                        const auto hash{BanchoState::md5((u8 *)plaintext_pw, strlen(plaintext_pw))};
                        cv::mp_password_md5.setValue(hash.string());
                        cv::mp_password.setValue("");
                        osu->getOptionsMenu()->save();
                    }
                }
                if(version < 39.01) {
                    if(cv::fps_unlimited.getBool()) {
                        cv::fps_max.setValue(0);
                        osu->getOptionsMenu()->save();
                    }
                }
                if(version < 40.00) {
                    for(auto key : KeyBindings::ALL) {
                        if(key->getFloat() == key->getDefaultFloat()) continue;
                        key->setValue(KeyBindings::old_keycode_to_sdl_keycode(key->getInt()));
                    }
                    osu->getOptionsMenu()->save();
                }
            } else {
                this->bDrawVersionNotificationArrow = true;
            }
        }
    }
    this->bDidUserUpdateFromOlderVersion = this->bDrawVersionNotificationArrow;  // (same logic atm)

    this->setPos(-1, 0);
    this->setSize(osu->getVirtScreenWidth(), osu->getVirtScreenHeight());

    this->cube = new CubeButton(this, 0, 0, 1, 1, "", "");
    this->cube->setClickCallback(SA::MakeDelegate<&MainMenu::onCubePressed>(this));
    this->addBaseUIElement(this->cube);

    this->addMainMenuButton("Singleplayer")->setClickCallback(SA::MakeDelegate<&MainMenu::onPlayButtonPressed>(this));
    this->addMainMenuButton("Multiplayer")
        ->setClickCallback(SA::MakeDelegate<&MainMenu::onMultiplayerButtonPressed>(this));
    this->addMainMenuButton("Options (CTRL + O)")
        ->setClickCallback(SA::MakeDelegate<&MainMenu::onOptionsButtonPressed>(this));
    this->addMainMenuButton("Exit")->setClickCallback(SA::MakeDelegate<&MainMenu::onExitButtonPressed>(this));

    this->pauseButton = new PauseButton(0, 0, 0, 0, "", "");
    this->pauseButton->setClickCallback(SA::MakeDelegate<&MainMenu::onPausePressed>(this));
    this->addBaseUIElement(this->pauseButton);

    this->updateAvailableButton = new UIButton(0, 0, 0, 0, "", "Checking for updates ...");
    this->updateAvailableButton->setUseDefaultSkin();
    this->updateAvailableButton->setClickCallback(SA::MakeDelegate<&MainMenu::onUpdatePressed>(this));
    this->updateAvailableButton->setColor(0x2200d900);
    this->updateAvailableButton->setTextColor(0x22ffffff);

    this->versionButton = new CBaseUIButton(0, 0, 0, 0, "", "");
    this->versionButton->setDrawBackground(false);
    this->versionButton->setDrawFrame(false);
    this->versionButton->setClickCallback(SA::MakeDelegate<&MainMenu::onVersionPressed>(this));
    this->addBaseUIElement(this->versionButton);
}

MainMenu::~MainMenu() {
    this->clearPreloadedMaps();
    SAFE_DELETE(this->updateAvailableButton);

    anim->deleteExistingAnimation(&this->fUpdateButtonAnim);

    anim->deleteExistingAnimation(&this->fMainMenuAnimFriendEyeFollowX);
    anim->deleteExistingAnimation(&this->fMainMenuAnimFriendEyeFollowY);

    anim->deleteExistingAnimation(&this->fMainMenuAnim);
    anim->deleteExistingAnimation(&this->fMainMenuAnim1);
    anim->deleteExistingAnimation(&this->fMainMenuAnim2);
    anim->deleteExistingAnimation(&this->fMainMenuAnim3);

    anim->deleteExistingAnimation(&this->fCenterOffsetAnim);
    anim->deleteExistingAnimation(&this->fStartupAnim);
    anim->deleteExistingAnimation(&this->fStartupAnim2);

    // if the user didn't click on the update notification during this session, quietly remove it so it's not annoying
    if(this->bWasCleanShutdown) this->writeVersionFile();
}

void MainMenu::drawFriend(const McRect &mainButtonRect, float pulse, bool haveTimingpoints) {
    // ears
    {
        const float width = mainButtonRect.getWidth() * 0.11f * 2.0f * (1.0f - pulse * 0.05f);

        const float margin = width * 0.4f;

        const float offset = mainButtonRect.getWidth() * 0.02f;

        VertexArrayObject vao;
        {
            const vec2 pos = vec2(mainButtonRect.getX(), mainButtonRect.getY() - offset);

            vec2 left = pos + vec2(0, 0);
            vec2 top = pos + vec2(width / 2, -width * std::sqrt(3.0f) / 2.0f);
            vec2 right = pos + vec2(width, 0);

            vec2 topRightDir = (top - right);
            {
                const float temp = topRightDir.x;
                topRightDir.x = -topRightDir.y;
                topRightDir.y = temp;
            }

            vec2 innerLeft = left + vec::normalize(topRightDir) * margin;

            vao.addVertex(left.x, left.y);
            vao.addVertex(top.x, top.y);
            vao.addVertex(innerLeft.x, innerLeft.y);

            vec2 leftRightDir = (right - left);
            {
                const float temp = leftRightDir.x;
                leftRightDir.x = -leftRightDir.y;
                leftRightDir.y = temp;
            }

            vec2 innerTop = top + vec::normalize(leftRightDir) * margin;

            vao.addVertex(top.x, top.y);
            vao.addVertex(innerTop.x, innerTop.y);
            vao.addVertex(innerLeft.x, innerLeft.y);

            vec2 leftTopDir = (left - top);
            {
                const float temp = leftTopDir.x;
                leftTopDir.x = -leftTopDir.y;
                leftTopDir.y = temp;
            }

            vec2 innerRight = right + vec::normalize(leftTopDir) * margin;

            vao.addVertex(top.x, top.y);
            vao.addVertex(innerRight.x, innerRight.y);
            vao.addVertex(innerTop.x, innerTop.y);

            vao.addVertex(top.x, top.y);
            vao.addVertex(right.x, right.y);
            vao.addVertex(innerRight.x, innerRight.y);

            vao.addVertex(left.x, left.y);
            vao.addVertex(innerLeft.x, innerLeft.y);
            vao.addVertex(innerRight.x, innerRight.y);

            vao.addVertex(left.x, left.y);
            vao.addVertex(innerRight.x, innerRight.y);
            vao.addVertex(right.x, right.y);
        }

        // left
        g->setColor(Color(0xffc8faf1).setA(this->fMainMenuAnimFriendPercent * cv::main_menu_alpha.getFloat()));

        g->drawVAO(&vao);

        // right
        g->pushTransform();
        {
            g->translate(mainButtonRect.getWidth() - width, 0);
            g->drawVAO(&vao);
        }
        g->popTransform();
    }

    float headBob = 0.0f;
    {
        float customPulse = 0.0f;
        if(pulse > 0.5f)
            customPulse = (pulse - 0.5f) / 0.5f;
        else
            customPulse = (0.5f - pulse) / 0.5f;

        customPulse = 1.0f - customPulse;

        if(!haveTimingpoints) customPulse = 1.0f;

        headBob = (customPulse) * (customPulse);
        headBob *= this->fMainMenuAnimFriendPercent;
    }

    const float mouthEyeOffsetY = mainButtonRect.getWidth() * 0.18f + headBob * mainButtonRect.getWidth() * 0.075f;

    // mouth
    {
        const float width = mainButtonRect.getWidth() * 0.10f;
        const float height = mainButtonRect.getHeight() * 0.03f * 1.75f;

        const float length = width * std::sqrt(2.0f) * 2;

        const float offsetY = mainButtonRect.getHeight() / 2.0f + mouthEyeOffsetY;

        g->pushTransform();
        {
            g->rotate(135);
            g->translate(mainButtonRect.getX() + length / 2 + mainButtonRect.getWidth() / 2 -
                             this->fMainMenuAnimFriendEyeFollowX * mainButtonRect.getWidth() * 0.5f,
                         mainButtonRect.getY() + offsetY -
                             this->fMainMenuAnimFriendEyeFollowY * mainButtonRect.getWidth() * 0.5f);

            g->setColor(Color(0xff000000).setA(cv::main_menu_alpha.getFloat()));

            g->fillRectf(0, 0, width, height);
            g->fillRectf(width - height / 2.0f, 0, height, width);
            g->fillRectf(width - height / 2.0f, width - height / 2.0f, width, height);
            g->fillRectf(width * 2 - height, width - height / 2.0f, height, width + height / 2);
        }
        g->popTransform();
    }

    // eyes
    {
        const float width = mainButtonRect.getWidth() * 0.22f;
        const float height = mainButtonRect.getHeight() * 0.03f * 2;

        const float offsetX = mainButtonRect.getWidth() * 0.18f;
        const float offsetY = mainButtonRect.getHeight() * 0.21f + mouthEyeOffsetY;

        const float rotation = 25.0f;

        // left
        g->pushTransform();
        {
            g->translate(-width, 0);
            g->rotate(-rotation);
            g->translate(width, 0);
            g->translate(
                mainButtonRect.getX() + offsetX - this->fMainMenuAnimFriendEyeFollowX * mainButtonRect.getWidth(),
                mainButtonRect.getY() + offsetY - this->fMainMenuAnimFriendEyeFollowY * mainButtonRect.getWidth());

            g->setColor(Color(0xff000000).setA(cv::main_menu_alpha.getFloat()));

            g->fillRectf(0, 0, width, height);
        }
        g->popTransform();

        // right
        g->pushTransform();
        {
            g->rotate(rotation);
            g->translate(
                mainButtonRect.getX() + mainButtonRect.getWidth() - offsetX - width -
                    this->fMainMenuAnimFriendEyeFollowX * mainButtonRect.getWidth(),
                mainButtonRect.getY() + offsetY - this->fMainMenuAnimFriendEyeFollowY * mainButtonRect.getWidth());

            g->setColor(Color(0xff000000).setA(cv::main_menu_alpha.getFloat()));

            g->fillRectf(0, 0, width, height);
        }
        g->popTransform();

        // tear
        g->setColor(Color(0xff000000).setA(cv::main_menu_alpha.getFloat()));

        g->fillRectf(mainButtonRect.getX() + offsetX + width * 0.375f -
                         this->fMainMenuAnimFriendEyeFollowX * mainButtonRect.getWidth(),
                     mainButtonRect.getY() + offsetY + width / 2.0f -
                         this->fMainMenuAnimFriendEyeFollowY * mainButtonRect.getWidth(),
                     height * 0.75f, width * 0.375f);
    }

    // hands
    {
        const float size = mainButtonRect.getWidth() * 0.2f;

        const float offset = -size * 0.75f;

        float customPulse = 0.0f;
        if(pulse > 0.5f)
            customPulse = (pulse - 0.5f) / 0.5f;
        else
            customPulse = (0.5f - pulse) / 0.5f;

        customPulse = 1.0f - customPulse;

        if(!haveTimingpoints) customPulse = 1.0f;

        const float animLeftMultiplier = (this->iMainMenuAnimBeatCounter % 2 == 0 ? 1.0f : 0.1f);
        const float animRightMultiplier = (this->iMainMenuAnimBeatCounter % 2 == 1 ? 1.0f : 0.1f);

        const float animMoveUp = std::lerp((1.0f - customPulse) * (1.0f - customPulse), (1.0f - customPulse), 0.35f) *
                                 this->fMainMenuAnimFriendPercent;

        const float animLeftMoveUp = animMoveUp * animLeftMultiplier;
        const float animRightMoveUp = animMoveUp * animRightMultiplier;

        const float animLeftMoveLeft = animRightMoveUp * (this->iMainMenuAnimBeatCounter % 2 == 1 ? 1.0f : 0.0f);
        const float animRightMoveRight = animLeftMoveUp * (this->iMainMenuAnimBeatCounter % 2 == 0 ? 1.0f : 0.0f);

        // left
        g->setColor(Color(0xffd5f6fd).setA(this->fMainMenuAnimFriendPercent * cv::main_menu_alpha.getFloat()));

        g->pushTransform();
        {
            g->rotate(40 - (1.0f - customPulse) * 10 + animLeftMoveLeft * animLeftMoveLeft * 20);
            g->translate(mainButtonRect.getX() - size - offset -
                             animLeftMoveLeft * mainButtonRect.getWidth() * -0.025f -
                             animLeftMoveUp * mainButtonRect.getWidth() * 0.25f,
                         mainButtonRect.getY() + mainButtonRect.getHeight() - size -
                             animLeftMoveUp * mainButtonRect.getHeight() * 0.85f,
                         -0.5f);
            g->fillRectf(0, 0, size, size);
        }
        g->popTransform();

        // right
        g->pushTransform();
        {
            g->rotate(50 + (1.0f - customPulse) * 10 - animRightMoveRight * animRightMoveRight * 20);
            g->translate(mainButtonRect.getX() + mainButtonRect.getWidth() + size + offset +
                             animRightMoveRight * mainButtonRect.getWidth() * -0.025f +
                             animRightMoveUp * mainButtonRect.getWidth() * 0.25f,
                         mainButtonRect.getY() + mainButtonRect.getHeight() - size -
                             animRightMoveUp * mainButtonRect.getHeight() * 0.85f,
                         -0.5f);
            g->fillRectf(0, 0, size, size);
        }
        g->popTransform();
    }
}

void MainMenu::drawLogoImage(const McRect &mainButtonRect) {
    auto logo = this->logo_img;
    if(BanchoState::server_icon != nullptr && BanchoState::server_icon->isReady() &&
       cv::main_menu_use_server_logo.getBool()) {
        logo = BanchoState::server_icon;
    }

    float alpha = (1.0f - this->fMainMenuAnimFriendPercent) * (1.0f - this->fMainMenuAnimFriendPercent) *
                  (1.0f - this->fMainMenuAnimFriendPercent);

    float xscale = mainButtonRect.getWidth() / static_cast<float>(logo->getWidth());
    float yscale = mainButtonRect.getHeight() / static_cast<float>(logo->getHeight());
    float scale = std::min(xscale, yscale) * 0.8f;

    g->pushTransform();
    g->setColor(argb(alpha, 1.0f, 1.0f, 1.0f));
    g->scale(scale, scale);

    g->translate(this->vCenter.x - this->fCenterOffsetAnim, this->vCenter.y);

    g->drawImage(logo);
    g->popTransform();
}

std::pair<bool, float> MainMenu::getTimingpointPulseAmount() {
    constexpr const float div = 1.25f;

    float pulse = (div - fmod(engine->getTime(), div)) / div;

    const auto &selectedMap = osu->getMapInterface();
    if(!selectedMap) {
        return {false, pulse};
    }

    const auto &music = selectedMap->getMusic();
    if(!music || !music->isPlaying()) {
        return {false, pulse};
    }

    const auto &map = selectedMap->beatmap;
    if(!map) {
        return {false, pulse};
    }

    // playing music, get dynamic pulse amount
    const long curMusicPos = (long)music->getPositionMS() +
                             (long)(cv::universal_offset.getFloat() * selectedMap->getSpeedMultiplier()) +
                             music->getBASSStreamLatencyCompensation() - map->getLocalOffset() -
                             map->getOnlineOffset() - (map->getVersion() < 5 ? cv::old_beatmap_offset.getInt() : 0);

    DatabaseBeatmap::TIMING_INFO t = map->getTimingInfoForTime(curMusicPos);

    if(t.beatLengthBase == 0.0f)  // bah
        t.beatLengthBase = 1.0f;

    this->iMainMenuAnimBeatCounter =
        (curMusicPos - t.offset - (long)(std::max((long)t.beatLengthBase, (long)1) * 0.5f)) /
        std::max((long)t.beatLengthBase, (long)1);

    pulse = (float)((curMusicPos - t.offset) % std::max((long)t.beatLengthBase, (long)1)) /
            t.beatLengthBase;  // modulo must be >= 1
    pulse = std::clamp<float>(pulse, -1.0f, 1.0f);
    if(pulse < 0.0f) pulse = 1.0f - std::abs(pulse);

    return {true, pulse};
}

// the cube
void MainMenu::drawMainButton() {
    const auto [haveTimingpoints, pulse] = this->getTimingpointPulseAmount();

    vec2 size = this->vSize;
    const float pulseSub = 0.05f * pulse;
    size -= size * pulseSub;
    size += size * this->fSizeAddAnim;
    size *= this->fStartupAnim;

    const McRect mainButtonRect{this->vCenter.x - size.x / 2.0f - this->fCenterOffsetAnim,
                                this->vCenter.y - size.y / 2.0f, size.x, size.y};

    // draw main button cube
    bool drawing_full_cube = (this->fMainMenuAnim > 0.0f && this->fMainMenuAnim != 1.0f) ||
                             (haveTimingpoints && this->fMainMenuAnimFriendPercent > 0.0f);

    float inset = 0.0f;
    if(drawing_full_cube) {
        inset = (1.0f - 0.5f * this->fMainMenuAnimFriendPercent);
        osu->getAAFrameBuffer()->enable();

        g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_PREMUL_ALPHA);

        // avoid ugly aliasing with rotation
        g->setAntialiasing(true);
        g->setDepthBuffer(true);
        g->clearDepthBuffer();
        g->setCulling(true);

        g->push3DScene(mainButtonRect);
        g->offset3DScene(0, 0, mainButtonRect.getWidth() / 2.f);

        float friendRotation = 0.0f;
        float friendTranslationX = 0.0f;
        float friendTranslationY = 0.0f;
        if(haveTimingpoints && this->fMainMenuAnimFriendPercent > 0.0f) {
            float customPulse = 0.0f;
            if(pulse > 0.5f)
                customPulse = (pulse - 0.5f) / 0.5f;
            else
                customPulse = (0.5f - pulse) / 0.5f;

            customPulse = 1.0f - customPulse;

            const float anim1 = std::lerp((1.0f - customPulse) * (1.0f - customPulse), (1.0f - customPulse), 0.25f);
            const float anim2 = anim1 * (this->iMainMenuAnimBeatCounter % 2 == 1 ? 1.0f : -1.0f);
            const float anim3 = anim1;

            friendRotation = anim2 * 13;
            friendTranslationX = -anim2 * mainButtonRect.getWidth() * 0.175f;
            friendTranslationY = anim3 * mainButtonRect.getWidth() * 0.10f;

            friendRotation *= this->fMainMenuAnimFriendPercent;
            friendTranslationX *= this->fMainMenuAnimFriendPercent;
            friendTranslationY *= this->fMainMenuAnimFriendPercent;
        }

        g->translate3DScene(friendTranslationX, friendTranslationY, 0);
        g->rotate3DScene(this->fMainMenuAnim1 * 360.0f, this->fMainMenuAnim2 * 360.0f,
                         this->fMainMenuAnim3 * 360.0f + friendRotation);
    }

    const Color cubeColor =
        argb(cv::main_menu_alpha.getFloat(), std::lerp(0.0f, 0.5f, this->fMainMenuAnimFriendPercent),
             std::lerp(0.0f, 0.768f, this->fMainMenuAnimFriendPercent),
             std::lerp(0.0f, 0.965f, this->fMainMenuAnimFriendPercent));
    const Color cubeBorderColor = argb(1.0f, std::lerp(1.0f, 0.5f, this->fMainMenuAnimFriendPercent),
                                       std::lerp(1.0f, 0.768f, this->fMainMenuAnimFriendPercent),
                                       std::lerp(1.0f, 0.965f, this->fMainMenuAnimFriendPercent));

    // front side
    g->setLineWidth(2.0f);
    g->pushTransform();
    g->translate(0, 0, inset);
    g->setColor(cubeColor);

    g->fillRectf(mainButtonRect.getX() + inset, mainButtonRect.getY() + inset, mainButtonRect.getWidth() - 2 * inset,
                 mainButtonRect.getHeight() - 2 * inset);
    g->translate(0, 0, -0.2f);  // move the border slightly towards the camera to prevent Z fighting
    g->setColor(cubeBorderColor);
    g->drawRectf(mainButtonRect.getX() + inset, mainButtonRect.getY() + inset, mainButtonRect.getWidth() - 2 * inset,
                 mainButtonRect.getHeight() - 2 * inset);
    g->popTransform();
    g->setLineWidth(1.0f);

    // friend
    if(this->fMainMenuAnimFriendPercent > 0.0f) {
        if(drawing_full_cube) {
            g->setCulling(false);  // ears get culled when rotating otherwise
        }
        this->drawFriend(mainButtonRect, pulse, haveTimingpoints);
        if(drawing_full_cube) {
            g->setCulling(true);
        }
    }

    // neosu/server logo
    this->drawLogoImage(mainButtonRect);

    if(drawing_full_cube) {
        g->setLineWidth(2.0f);
        // back side
        g->rotate3DScene(0, -180, 0);
        g->pushTransform();
        g->translate(0, 0, inset);
        g->setColor(Color(cubeColor).setA(cv::main_menu_alpha.getFloat()));

        g->fillRectf(mainButtonRect.getX() + inset, mainButtonRect.getY() + inset,
                     mainButtonRect.getWidth() - 2 * inset, mainButtonRect.getHeight() - 2 * inset);
        g->translate(0, 0, -0.2f);
        g->setColor(cubeBorderColor);
        g->drawRectf(mainButtonRect.getX() + inset, mainButtonRect.getY() + inset,
                     mainButtonRect.getWidth() - 2 * inset, mainButtonRect.getHeight() - 2 * inset);
        g->popTransform();

        // right side
        g->offset3DScene(0, 0, mainButtonRect.getWidth() / 2);
        g->rotate3DScene(0, 90, 0);
        g->pushTransform();
        g->translate(0, 0, inset);
        g->setColor(Color(cubeColor).setA(cv::main_menu_alpha.getFloat()));

        g->fillRectf(mainButtonRect.getX() + inset, mainButtonRect.getY() + inset,
                     mainButtonRect.getWidth() - 2 * inset, mainButtonRect.getHeight() - 2 * inset);
        g->translate(0, 0, -0.2f);
        g->setColor(cubeBorderColor);
        g->drawRectf(mainButtonRect.getX() + inset, mainButtonRect.getY() + inset,
                     mainButtonRect.getWidth() - 2 * inset, mainButtonRect.getHeight() - 2 * inset);
        g->popTransform();
        g->rotate3DScene(0, -90, 0);
        g->offset3DScene(0, 0, 0);

        // left side
        g->offset3DScene(0, 0, mainButtonRect.getWidth() / 2);
        g->rotate3DScene(0, -90, 0);
        g->pushTransform();
        g->translate(0, 0, inset);
        g->setColor(Color(cubeColor).setA(cv::main_menu_alpha.getFloat()));

        g->fillRectf(mainButtonRect.getX() + inset, mainButtonRect.getY() + inset,
                     mainButtonRect.getWidth() - 2 * inset, mainButtonRect.getHeight() - 2 * inset);
        g->translate(0, 0, -0.2f);
        g->setColor(cubeBorderColor);
        g->drawRectf(mainButtonRect.getX() + inset, mainButtonRect.getY() + inset,
                     mainButtonRect.getWidth() - 2 * inset, mainButtonRect.getHeight() - 2 * inset);
        g->popTransform();
        g->rotate3DScene(0, 90, 0);
        g->offset3DScene(0, 0, 0);

        // top side
        g->offset3DScene(0, 0, mainButtonRect.getHeight() / 2);
        g->rotate3DScene(90, 0, 0);
        g->pushTransform();
        g->translate(0, 0, inset);
        g->setColor(Color(cubeColor).setA(cv::main_menu_alpha.getFloat()));

        g->fillRectf(mainButtonRect.getX() + inset, mainButtonRect.getY() + inset,
                     mainButtonRect.getWidth() - 2 * inset, mainButtonRect.getHeight() - 2 * inset);
        g->translate(0, 0, -0.2f);
        g->setColor(cubeBorderColor);
        g->drawRectf(mainButtonRect.getX() + inset, mainButtonRect.getY() + inset,
                     mainButtonRect.getWidth() - 2 * inset, mainButtonRect.getHeight() - 2 * inset);
        g->popTransform();
        g->rotate3DScene(-90, 0, 0);
        g->offset3DScene(0, 0, 0);

        // bottom side
        g->offset3DScene(0, 0, mainButtonRect.getHeight() / 2);
        g->rotate3DScene(-90, 0, 0);
        g->pushTransform();
        g->translate(0, 0, inset);
        g->setColor(Color(cubeColor).setA(cv::main_menu_alpha.getFloat()));

        g->fillRectf(mainButtonRect.getX() + inset, mainButtonRect.getY() + inset,
                     mainButtonRect.getWidth() - 2 * inset, mainButtonRect.getHeight() - 2 * inset);
        g->translate(0, 0, -0.2f);
        g->setColor(cubeBorderColor);
        g->drawRectf(mainButtonRect.getX() + inset, mainButtonRect.getY() + inset,
                     mainButtonRect.getWidth() - 2 * inset, mainButtonRect.getHeight() - 2 * inset);
        g->popTransform();
        g->rotate3DScene(90, 0, 0);
        g->offset3DScene(0, 0, 0);

        g->pop3DScene();

        g->setCulling(false);
        g->setDepthBuffer(false);
        g->setAntialiasing(false);

        g->setLineWidth(1.0f);

        g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_ALPHA);

        osu->getAAFrameBuffer()->disable();
        osu->getAAFrameBuffer()->draw(0, 0);
    }
}

void MainMenu::clearPreloadedMaps() {
    for(auto preloaded_set : this->preloadedMaps) {
        for(auto &diff : preloaded_set->getDifficulties()) {
            if(diff == this->lastMap) this->lastMap = nullptr;
            if(diff == this->currentMap) this->currentMap = nullptr;
        }

        SAFE_DELETE(preloaded_set);
    }

    this->preloadedMaps.clear();
}

void MainMenu::drawMapBackground(DatabaseBeatmap *beatmap, f32 alpha) {
    const Image *bg = nullptr;

    if(beatmap == nullptr) {
        bool just_launched = engine->getTime() < 5.0;
        if(cv::draw_menu_background.getBool() && !just_launched) {
            bg = osu->getSkin()->getMenuBackground();
        }
    } else {
        bg = osu->getBackgroundImageHandler()->getLoadBackgroundImage(beatmap, true);
    }

    if(bg == nullptr || !bg->isReady() || bg == MISSING_TEXTURE) return;

    g->pushTransform();
    {
        const f32 scale = Osu::getImageScaleToFillResolution(bg, osu->getVirtScreenSize());
        g->scale(scale, scale);
        g->translate(osu->getVirtScreenWidth() / 2, osu->getVirtScreenHeight() / 2);
        g->setColor(Color(0xffffffff).setA(alpha));
        g->drawImage(bg);
    }
    g->popTransform();
}

void MainMenu::draw() {
    if(!this->bVisible) return;

    // draw background
    {
        // background_shader->enable();
        // background_shader->setUniform1f("time", engine->getTime());
        // background_shader->setUniform2f("resolution", osu->getVirtScreenWidth(), osu->getVirtScreenHeight());

        // Check if we need to update the background
        if(this->mapFadeAnim == 1.f && this->currentMap != osu->getMapInterface()->beatmap) {
            this->lastMap = this->currentMap;
            this->currentMap = osu->getMapInterface()->beatmap;
            this->mapFadeAnim = 0.f;
            anim->moveLinear(&this->mapFadeAnim, 1.f, cv::main_menu_background_fade_duration.getFloat(), true);
        }

        this->drawMapBackground(this->lastMap, 1.f - this->mapFadeAnim);
        this->drawMapBackground(this->currentMap, this->mapFadeAnim);

        // background_shader->disable();
    }

    // draw notification arrow for changelog (version button)
    if(this->bDrawVersionNotificationArrow) {
        float animation = std::fmod((float)(engine->getTime()) * 3.2f, 2.0f);
        if(animation > 1.0f) animation = 2.0f - animation;
        animation = -animation * (animation - 2);  // quad out
        float offset = osu->getUIScale(45.0f * animation);

        const float scale =
            this->versionButton->getSize().x / osu->getSkin()->getPlayWarningArrow2()->getSizeBaseRaw().x;

        const vec2 arrowPos = vec2(this->versionButton->getSize().x / 1.75f,
                                   osu->getVirtScreenHeight() - this->versionButton->getSize().y * 2 -
                                       this->versionButton->getSize().y * scale);

        UString notificationText = "Changelog";
        g->setColor(0xffffffff);
        g->pushTransform();
        {
            McFont *smallFont = osu->getSubTitleFont();
            g->translate(arrowPos.x - smallFont->getStringWidth(notificationText) / 2.0f,
                         (-offset * 2) * scale + arrowPos.y -
                             (osu->getSkin()->getPlayWarningArrow2()->getSizeBaseRaw().y * scale) / 1.5f,
                         0);
            g->drawString(smallFont, notificationText);
        }
        g->popTransform();

        g->setColor(0xffffffff);
        g->pushTransform();
        {
            g->rotate(90.0f);
            g->translate(0, -offset * 2, 0);
            osu->getSkin()->getPlayWarningArrow2()->drawRaw(arrowPos, scale);
        }
        g->popTransform();
    }

    // draw container
    OsuScreen::draw();

    // draw update check button
    if(this->updateAvailableButton != nullptr) {
        using enum UpdateHandler::STATUS;
        const auto status = osu->getUpdateHandler()->getStatus();
        const bool drawAnim = (status == STATUS_DOWNLOAD_COMPLETE);
        if(drawAnim) {
            g->push3DScene(McRect(this->updateAvailableButton->getPos().x, this->updateAvailableButton->getPos().y,
                                  this->updateAvailableButton->getSize().x, this->updateAvailableButton->getSize().y));
            g->rotate3DScene(this->fUpdateButtonAnim * 360.0f, 0, 0);
        }
        this->updateAvailableButton->draw();
        if(drawAnim) {
            g->pop3DScene();
        }
    }

    // draw button/cube
    this->drawMainButton();
}

void MainMenu::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible) return;

    UString versionString;
    if(cv::is_bleedingedge.getBool()) {
        versionString = UString::fmt("Version {:.2f} ({:s})", cv::version.getFloat(), cv::build_timestamp.getString());
        this->versionButton->setTextColor(rgb(255, 220, 220));
    } else {
        versionString = UString::fmt("Version {:.2f}", cv::version.getFloat());
        this->versionButton->setTextColor(rgb(255, 255, 255));
    }
    this->versionButton->setText(versionString);

    this->updateLayout();

    // update and focus handling
    OsuScreen::mouse_update(propagate_clicks);

    if(this->updateAvailableButton != nullptr) {
        this->updateAvailableButton->mouse_update(propagate_clicks);
    }

    // handle automatic menu closing
    if(this->fMainMenuButtonCloseTime != 0.0f && engine->getTime() > this->fMainMenuButtonCloseTime) {
        this->fMainMenuButtonCloseTime = 0.0f;
        this->setMenuElementsVisible(false);
    }

    // hide the buttons if the closing animation finished
    if(!anim->isAnimating(&this->fCenterOffsetAnim) && this->fCenterOffsetAnim == 0.0f) {
        for(auto &menuElement : this->menuElements) {
            menuElement->setVisible(false);
        }
    }

    // handle delayed shutdown
    bool shutting_down = false;
    if(this->fShutdownScheduledTime != 0.0f &&
       (engine->getTime() > this->fShutdownScheduledTime || !anim->isAnimating(&this->fCenterOffsetAnim))) {
        engine->shutdown();
        this->fShutdownScheduledTime = 0.0f;
        shutting_down = true;
    }

    // main button autohide + anim
    if(this->bMenuElementsVisible) {
        this->fMainMenuAnimDuration = 15.0f;
        this->fMainMenuAnimTime = engine->getTime() + this->fMainMenuAnimDuration;
    }
    if(engine->getTime() > this->fMainMenuAnimTime) {
        if(this->bMainMenuAnimFriendScheduled) this->bMainMenuAnimFriend = true;
        if(this->bMainMenuAnimFadeToFriendForNextAnim) this->bMainMenuAnimFriendScheduled = true;

        this->fMainMenuAnimDuration = 10.0f + (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 5.0f;
        this->fMainMenuAnimTime = engine->getTime() + this->fMainMenuAnimDuration;
        this->animMainButton();
    }

    if(this->bInMainMenuRandomAnim && this->iMainMenuRandomAnimType == 1 && anim->isAnimating(&this->fMainMenuAnim)) {
        vec2 mouseDelta = (this->cube->getPos() + this->cube->getSize() / 2.f) - mouse->getPos();
        mouseDelta.x = std::clamp<float>(mouseDelta.x, -engine->getScreenSize().x / 2, engine->getScreenSize().x / 2);
        mouseDelta.y = std::clamp<float>(mouseDelta.y, -engine->getScreenSize().y / 2, engine->getScreenSize().y / 2);
        mouseDelta.x /= engine->getScreenSize().x;
        mouseDelta.y /= engine->getScreenSize().y;

        const float decay = std::clamp<float>((1.0f - this->fMainMenuAnim - 0.075f) / 0.025f, 0.0f, 1.0f);

        const vec2 pushAngle = vec2(mouseDelta.y, -mouseDelta.x) * vec2(0.15f, 0.15f) * decay;

        anim->moveQuadOut(&this->fMainMenuAnim1, pushAngle.x, 0.15f, true);

        anim->moveQuadOut(&this->fMainMenuAnim2, pushAngle.y, 0.15f, true);

        anim->moveQuadOut(&this->fMainMenuAnim3, 0.0f, 0.15f, true);
    }

    {
        this->fMainMenuAnimFriendPercent =
            1.0f - std::clamp<float>((this->fMainMenuAnimDuration > 0.0f
                                          ? (this->fMainMenuAnimTime - engine->getTime()) / this->fMainMenuAnimDuration
                                          : 0.0f),
                                     0.0f, 1.0f);
        this->fMainMenuAnimFriendPercent =
            std::clamp<float>((this->fMainMenuAnimFriendPercent - 0.5f) / 0.5f, 0.0f, 1.0f);
        if(this->bMainMenuAnimFriend) this->fMainMenuAnimFriendPercent = 1.0f;
        if(!this->bMainMenuAnimFriendScheduled) this->fMainMenuAnimFriendPercent = 0.0f;

        vec2 mouseDelta = (this->cube->getPos() + this->cube->getSize() / 2.f) - mouse->getPos();
        mouseDelta.x = std::clamp<float>(mouseDelta.x, -engine->getScreenSize().x / 2, engine->getScreenSize().x / 2);
        mouseDelta.y = std::clamp<float>(mouseDelta.y, -engine->getScreenSize().y / 2, engine->getScreenSize().y / 2);
        mouseDelta.x /= engine->getScreenSize().x;
        mouseDelta.y /= engine->getScreenSize().y;

        const vec2 pushAngle = vec2(mouseDelta.x, mouseDelta.y) * 0.1f;

        anim->moveLinear(&this->fMainMenuAnimFriendEyeFollowX, pushAngle.x, 0.25f, true);
        anim->moveLinear(&this->fMainMenuAnimFriendEyeFollowY, pushAngle.y, 0.25f, true);
    }

    // handle update checker and status text
    if(this->updateAvailableButton != nullptr) {
        using enum UpdateHandler::STATUS;
        const auto status = osu->getUpdateHandler()->getStatus();

        switch(status) {
            case UpdateHandler::STATUS::STATUS_IDLE:
                if(this->updateAvailableButton->isVisible()) {
                    this->updateAvailableButton->setVisible(false);
                }
                break;
            case UpdateHandler::STATUS::STATUS_CHECKING_FOR_UPDATE:
                this->updateAvailableButton->setText("Checking for updates ...");
                this->updateAvailableButton->setColor(0x2200d900);
                this->updateAvailableButton->setVisible(true);
                break;
            case UpdateHandler::STATUS::STATUS_DOWNLOADING_UPDATE:
                this->updateAvailableButton->setText("Downloading ...");
                this->updateAvailableButton->setColor(0x2200d900);
                this->updateAvailableButton->setVisible(true);
                break;
            case UpdateHandler::STATUS::STATUS_DOWNLOAD_COMPLETE:
                if(engine->getTime() > this->fUpdateButtonTextTime && anim->isAnimating(&this->fUpdateButtonAnim) &&
                   this->fUpdateButtonAnim > 0.175f) {
                    this->fUpdateButtonTextTime = this->fUpdateButtonAnimTime;

                    this->updateAvailableButton->setColor(rgb(0, 130, 200));
                    this->updateAvailableButton->setTextColor(0xffffffff);
                    this->updateAvailableButton->setVisible(true);

                    if(this->updateAvailableButton->getText().find("ready") != -1)
                        this->updateAvailableButton->setText("Click here to install the update!");
                    else
                        this->updateAvailableButton->setText("A new version of neosu is ready!");
                }
                if(engine->getTime() > this->fUpdateButtonAnimTime) {
                    this->fUpdateButtonAnimTime = engine->getTime() + 3.0f;
                    this->fUpdateButtonAnim = 0.0f;
                    anim->moveQuadInOut(&this->fUpdateButtonAnim, 1.0f, 0.5f, true);
                }
                break;
            case UpdateHandler::STATUS::STATUS_ERROR:
                this->updateAvailableButton->setText("Update Error! Click to retry ...");
                this->updateAvailableButton->setColor(rgb(220, 0, 0));
                this->updateAvailableButton->setTextColor(0xffffffff);
                this->updateAvailableButton->setVisible(true);
                break;
        }
    }

    // Update pause button and shuffle songs
    this->pauseButton->setPaused(true);

    if(soundEngine->isReady()) {
        auto *music = osu->getMapInterface()->getMusic();

        // try getting existing playing music track, even if osu->getMapInterface()->getMusic() did not have one
        if(!music) {
            music = resourceManager->getSound("BEATMAP_MUSIC");
        }

        if(!music) {
            this->selectRandomBeatmap();
        } else {
            if(!music->isReady() || music->isFinished()) {
                this->selectRandomBeatmap();
            } else if(music->isPlaying()) {
                this->pauseButton->setPaused(false);

                // NOTE: We set this every frame, because music loading isn't instant
                music->setLoop(false);

                // load timing points if needed
                // XXX: file io, don't block main thread
                auto *map = osu->getMapInterface()->beatmap;
                if(map && map->getTimingpoints().empty()) {
                    map->loadMetadata(false);
                }
            }
        }
    }

    // load server icon
    if(!shutting_down && BanchoState::is_online() && BanchoState::server_icon_url.length() > 0 &&
       BanchoState::server_icon == nullptr) {
        const std::string icon_path =
            fmt::format(NEOSU_AVATARS_PATH "/{}/server_icon", BanchoState::endpoint, "/server_icon");

        float progress = -1.f;
        std::vector<u8> data;
        int response_code;
        Downloader::download(BanchoState::server_icon_url.c_str(), &progress, data, &response_code);
        if(progress == -1.f) BanchoState::server_icon_url = "";
        if(!data.empty()) {
            io->write(icon_path, data, [icon_path](bool success) {
                if(success) {
                    resourceManager->requestNextLoadAsync();
                    BanchoState::server_icon = resourceManager->loadImageAbs(icon_path, icon_path);
                }
            });
        }
    }
}

void MainMenu::selectRandomBeatmap() {
    const auto &sb = osu->getSongBrowser();
    if(db->isFinished() && !sb->beatmapsets.empty()) {
        sb->selectRandomBeatmap();
        RichPresence::onMainMenu();
    } else {
        // Database is not loaded yet, load a random map and select it
        if(!this->songs_enumerator.isAsyncReady()) return;
        auto mapset_folders = this->songs_enumerator.getEntries();
        if(mapset_folders.empty()) {
            // check if it was loaded with a different path
            if(this->songs_enumerator.getFolderPath() != Database::getOsuSongsFolder()) {
                // rebuild will reinit with the current song folder
                this->songs_enumerator.rebuild();
            }
            return;
        }

        osu->getMapInterface()->deselectBeatmap();

        constexpr int RETRY_SETS{10};
        for(int i = 0; i < RETRY_SETS; i++) {
            const auto &mapset_folder = mapset_folders[rand() % mapset_folders.size()];
            BeatmapSet *set = db->loadRawBeatmap(mapset_folder);
            if(set == nullptr) {
                debugLog("Failed to load beatmap set '{:s}'", mapset_folder.c_str());
                continue;
            }

            auto beatmap_diffs = set->getDifficulties();
            if(beatmap_diffs.empty()) {
                debugLog("Mapset '{:s}' has no difficulties!", set->getFolder());
                delete set;
                continue;
            }

            // We're picking a random diff and not the first one, because diffs of the same set
            // can have their own separate sound file.
            const auto &candidate_diff{beatmap_diffs[rand() % beatmap_diffs.size()]};
            assert(candidate_diff);

            const bool skip =
                (i < RETRY_SETS - 1) && !env->fileExists(candidate_diff->getFullBackgroundImageFilePath());
            if(skip) {
                debugLog("Beatmap '{:s}' has no background image, skipping.", candidate_diff->getFilePath());
                delete set;
                continue;
            }

            this->preloadedMaps.push_back(set);
            candidate_diff->do_not_store = true;  // don't store in songbrowser f2 history

            osu->getSongBrowser()->onDifficultySelected(candidate_diff, false);
            RichPresence::onMainMenu();

            return;
        }

        debugLog("Failed to pick random beatmap...");
    }
}

void MainMenu::onKeyDown(KeyboardEvent &e) {
    OsuScreen::onKeyDown(e);  // only used for options menu
    if(!this->bVisible || e.isConsumed()) return;

    if(!osu->getOptionsMenu()->isMouseInside()) {
        if(e == KEY_RIGHT || e == KEY_F2) this->selectRandomBeatmap();
    }

    if(e == KEY_C || e == KEY_F4) this->onPausePressed();

    if(!this->bMenuElementsVisible) {
        if(e == KEY_P || e == KEY_ENTER || e == KEY_NUMPAD_ENTER) this->cube->click();
    } else {
        if(e == KEY_P || e == KEY_ENTER || e == KEY_NUMPAD_ENTER) this->onPlayButtonPressed();
        if(e == KEY_O) this->onOptionsButtonPressed();
        if(e == KEY_E || e == KEY_X) this->onExitButtonPressed();

        if(e == KEY_ESCAPE) this->setMenuElementsVisible(false);
    }
}

void MainMenu::onButtonChange(ButtonIndex button, bool down) {
    using enum ButtonIndex;
    if(!this->bVisible || button != BUTTON_MIDDLE ||
       !(down && !anim->isAnimating(&this->fMainMenuAnim) && !this->bMenuElementsVisible))
        return;

    if(keyboard->isShiftDown()) {
        this->bMainMenuAnimFriend = true;
        this->bMainMenuAnimFriendScheduled = true;
        this->bMainMenuAnimFadeToFriendForNextAnim = true;
    }

    animMainButton();
    this->fMainMenuAnimDuration = 15.0f;
    this->fMainMenuAnimTime = engine->getTime() + this->fMainMenuAnimDuration;
}

void MainMenu::onResolutionChange(vec2 /*newResolution*/) {
    this->updateLayout();
    this->setMenuElementsVisible(this->bMenuElementsVisible);
}

CBaseUIContainer *MainMenu::setVisible(bool visible) {
    this->bVisible = visible;

    if(visible) {
        // Clear background change animation, to avoid "fade" when backing out from song browser
        {
            this->currentMap = osu->getMapInterface()->beatmap;
            anim->deleteExistingAnimation(&this->mapFadeAnim);
            this->mapFadeAnim = 1.f;
        }

        RichPresence::onMainMenu();

        if(!BanchoState::spectators.empty()) {
            Packet packet;
            packet.id = OUT_SPECTATE_FRAMES;
            packet.write<i32>(0);
            packet.write<u16>(0);
            packet.write<u8>(LiveReplayBundle::Action::NONE);
            packet.write<ScoreFrame>(ScoreFrame::get());
            packet.write<u16>(osu->getMapInterface()->spectator_sequence++);
            BANCHO::Net::send_packet(packet);
        }

        this->updateLayout();

        this->fMainMenuAnimDuration = 15.0f;
        this->fMainMenuAnimTime = engine->getTime() + this->fMainMenuAnimDuration;

        if(this->bStartupAnim) {
            this->bStartupAnim = false;
            anim->moveQuadOut(&this->fStartupAnim, 1.0f, cv::main_menu_startup_anim_duration.getFloat());
            anim->moveQuartOut(&this->fStartupAnim2, 1.0f, cv::main_menu_startup_anim_duration.getFloat() * 6.0f,
                               cv::main_menu_startup_anim_duration.getFloat() * 0.5f);
        }
    } else {
        this->setMenuElementsVisible(false, false);
    }

    return this;
}

void MainMenu::updateLayout() {
    const float dpiScale = Osu::getUIScale();

    this->vCenter = osu->getVirtScreenSize() / 2.0f;
    const float size = Osu::getUIScale(324.0f);
    this->vSize = vec2(size, size);

    this->cube->setRelPos(this->vCenter - this->vSize / 2.0f - vec2(this->fCenterOffsetAnim, 0.0f));
    this->cube->setSize(this->vSize);

    this->pauseButton->setSize(30 * dpiScale, 30 * dpiScale);
    this->pauseButton->setRelPos(osu->getVirtScreenWidth() - this->pauseButton->getSize().x * 2 - 10 * dpiScale,
                                 this->pauseButton->getSize().y + 10 * dpiScale);

    if(this->updateAvailableButton != nullptr) {
        this->updateAvailableButton->setSize(375 * dpiScale, 50 * dpiScale);
        this->updateAvailableButton->setPos(
            osu->getVirtScreenWidth() / 2 - this->updateAvailableButton->getSize().x / 2,
            osu->getVirtScreenHeight() - this->updateAvailableButton->getSize().y - 10 * dpiScale);
    }

    this->versionButton->onResized();  // HACKHACK: framework, setSizeToContent() does not update string metrics
    this->versionButton->setSizeToContent(8 * dpiScale, 8 * dpiScale);
    this->versionButton->setRelPos(-1, osu->getVirtScreenSize().y - this->versionButton->getSize().y);

    int numButtons = this->menuElements.size();
    int menuElementHeight = this->vSize.y / numButtons;
    int menuElementPadding = numButtons > 3 ? this->vSize.y * 0.04f : this->vSize.y * 0.075f;
    menuElementHeight -= (numButtons - 1) * menuElementPadding;
    int menuElementExtraWidth = this->vSize.x * 0.06f;

    float offsetPercent = this->fCenterOffsetAnim / (this->vSize.x / 2.0f);
    float curY = this->cube->getRelPos().y +
                 (this->vSize.y - menuElementHeight * numButtons - (numButtons - 1) * menuElementPadding) / 2.0f;
    for(int i = 0; i < this->menuElements.size(); i++) {
        curY += (i > 0 ? menuElementHeight + menuElementPadding : 0.0f);

        this->menuElements[i]->onResized();  // HACKHACK: framework, setSize() does not update string metrics
        this->menuElements[i]->setRelPos(this->cube->getRelPos().x + this->cube->getSize().x * offsetPercent -
                                             menuElementExtraWidth * offsetPercent +
                                             menuElementExtraWidth * (1.0f - offsetPercent),
                                         curY);
        this->menuElements[i]->setSize(this->cube->getSize().x + menuElementExtraWidth * offsetPercent -
                                           2.0f * menuElementExtraWidth * (1.0f - offsetPercent),
                                       menuElementHeight);
        this->menuElements[i]->setTextColor(
            argb(offsetPercent * offsetPercent * offsetPercent * offsetPercent, 1.0f, 1.0f, 1.0f));
        this->menuElements[i]->setFrameColor(argb(offsetPercent, 1.0f, 1.0f, 1.0f));
        this->menuElements[i]->setBackgroundColor(
            argb(offsetPercent * cv::main_menu_alpha.getFloat(), 0.0f, 0.0f, 0.0f));
    }

    this->setSize(osu->getVirtScreenSize() + vec2(1, 1));
    this->update_pos();
}

void MainMenu::animMainButton() {
    this->bInMainMenuRandomAnim = true;

    this->iMainMenuRandomAnimType = (rand() % 4) == 1 ? 1 : 0;
    if(!this->bMainMenuAnimFadeToFriendForNextAnim && cv::main_menu_friend.getBool() &&
       Env::cfg(OS::WINDOWS))  // NOTE: z buffer bullshit on other platforms >:(
        this->bMainMenuAnimFadeToFriendForNextAnim = (rand() % 24) == 1;

    this->fMainMenuAnim = 0.0f;
    this->fMainMenuAnim1 = 0.0f;
    this->fMainMenuAnim2 = 0.0f;

    if(this->iMainMenuRandomAnimType == 0) {
        this->fMainMenuAnim3 = 1.0f;

        this->fMainMenuAnim1Target = (rand() % 2) == 1 ? 1.0f : -1.0f;
        this->fMainMenuAnim2Target = (rand() % 2) == 1 ? 1.0f : -1.0f;
        this->fMainMenuAnim3Target = (rand() % 2) == 1 ? 1.0f : -1.0f;

        const float randomDuration1 = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 3.5f;
        const float randomDuration2 = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 3.5f;
        const float randomDuration3 = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 3.5f;

        anim->moveQuadOut(&this->fMainMenuAnim, 1.0f,
                          1.5f + std::max(randomDuration1, std::max(randomDuration2, randomDuration3)));
        anim->moveQuadOut(&this->fMainMenuAnim1, this->fMainMenuAnim1Target, 1.5f + randomDuration1);
        anim->moveQuadOut(&this->fMainMenuAnim2, this->fMainMenuAnim2Target, 1.5f + randomDuration2);
        anim->moveQuadOut(&this->fMainMenuAnim3, this->fMainMenuAnim3Target, 1.5f + randomDuration3);
    } else {
        this->fMainMenuAnim3 = 0.0f;

        this->fMainMenuAnim1Target = 0.0f;
        this->fMainMenuAnim2Target = 0.0f;
        this->fMainMenuAnim3Target = 0.0f;

        this->fMainMenuAnim = 0.0f;
        anim->moveQuadOut(&this->fMainMenuAnim, 1.0f, 5.0f);
    }
}

void MainMenu::animMainButtonBack() {
    this->bInMainMenuRandomAnim = false;

    if(anim->isAnimating(&this->fMainMenuAnim)) {
        anim->moveQuadOut(&this->fMainMenuAnim, 1.0f, 0.25f, true);
        anim->moveQuadOut(&this->fMainMenuAnim1, this->fMainMenuAnim1Target, 0.25f, true);
        anim->moveQuadOut(&this->fMainMenuAnim1, 0.0f, 0.0f, 0.25f);
        anim->moveQuadOut(&this->fMainMenuAnim2, this->fMainMenuAnim2Target, 0.25f, true);
        anim->moveQuadOut(&this->fMainMenuAnim2, 0.0f, 0.0f, 0.25f);
        anim->moveQuadOut(&this->fMainMenuAnim3, this->fMainMenuAnim3Target, 0.10f, true);
        anim->moveQuadOut(&this->fMainMenuAnim3, 0.0f, 0.0f, 0.1f);
    }
}

void MainMenu::setMenuElementsVisible(bool visible, bool animate) {
    this->bMenuElementsVisible = visible;

    if(visible) {
        if(this->bMenuElementsVisible &&
           this->vSize.x / 2.0f < this->fCenterOffsetAnim)  // so we don't see the ends of the menu element buttons
                                                            // if the window gets smaller
            this->fCenterOffsetAnim = this->vSize.x / 2.0f;

        if(animate)
            anim->moveQuadInOut(&this->fCenterOffsetAnim, this->vSize.x / 2.0f, 0.35f, 0.0f, true);
        else {
            anim->deleteExistingAnimation(&this->fCenterOffsetAnim);
            this->fCenterOffsetAnim = this->vSize.x / 2.0f;
        }

        this->fMainMenuButtonCloseTime = engine->getTime() + 6.0f;

        for(auto &menuElement : this->menuElements) {
            menuElement->setVisible(true);
            menuElement->setEnabled(true);
        }
    } else {
        if(animate)
            anim->moveQuadOut(&this->fCenterOffsetAnim, 0.0f,
                              0.5f * (this->fCenterOffsetAnim / (this->vSize.x / 2.0f)) *
                                  (this->fShutdownScheduledTime != 0.0f ? 0.4f : 1.0f),
                              0.0f, true);
        else {
            anim->deleteExistingAnimation(&this->fCenterOffsetAnim);
            this->fCenterOffsetAnim = 0.0f;
        }

        this->fMainMenuButtonCloseTime = 0.0f;

        for(auto &menuElement : this->menuElements) {
            menuElement->setEnabled(false);
        }
    }
}

void MainMenu::writeVersionFile() {
    // remember, don't show the notification arrow until the version changes again
    std::ofstream versionFile("version.txt", std::ios::out | std::ios::trunc);
    if(versionFile.good()) {
        versionFile << cv::version.getString() << '\n' << cv::build_timestamp.getString();
    }
}

MainMenu::MainButton *MainMenu::addMainMenuButton(UString text) {
    auto *button = new MainButton(this, this->vSize.x, 0, 1, 1, "", std::move(text));
    button->setFont(osu->getSubTitleFont());
    button->setVisible(false);

    this->menuElements.push_back(button);
    this->addBaseUIElement(button);
    return button;
}

void MainMenu::onCubePressed() {
    soundEngine->play(osu->getSkin()->getClickMainMenuCubeSound());

    anim->moveQuadInOut(&this->fSizeAddAnim, 0.0f, 0.06f, 0.0f, false);
    anim->moveQuadInOut(&this->fSizeAddAnim, 0.12f, 0.06f, 0.07f, false);

    // if the menu is already visible, this counts as pressing the play button
    if(this->bMenuElementsVisible)
        this->onPlayButtonPressed();
    else
        this->setMenuElementsVisible(true);

    if(anim->isAnimating(&this->fMainMenuAnim) && this->bInMainMenuRandomAnim)
        this->animMainButtonBack();
    else {
        this->bInMainMenuRandomAnim = false;

        vec2 mouseDelta = (this->cube->getPos() + this->cube->getSize() / 2.f) - mouse->getPos();
        mouseDelta.x = std::clamp<float>(mouseDelta.x, -this->cube->getSize().x / 2, this->cube->getSize().x / 2);
        mouseDelta.y = std::clamp<float>(mouseDelta.y, -this->cube->getSize().y / 2, this->cube->getSize().y / 2);
        mouseDelta.x /= this->cube->getSize().x;
        mouseDelta.y /= this->cube->getSize().y;

        const vec2 pushAngle = vec2(mouseDelta.y, -mouseDelta.x) * vec2(0.15f, 0.15f);

        this->fMainMenuAnim = 0.001f;
        anim->moveQuadOut(&this->fMainMenuAnim, 1.0f, 0.15f + 0.4f, true);

        if(!anim->isAnimating(&this->fMainMenuAnim1)) this->fMainMenuAnim1 = 0.0f;

        anim->moveQuadOut(&this->fMainMenuAnim1, pushAngle.x, 0.15f, true);
        anim->moveQuadOut(&this->fMainMenuAnim1, 0.0f, 0.4f, 0.15f);

        if(!anim->isAnimating(&this->fMainMenuAnim2)) this->fMainMenuAnim2 = 0.0f;

        anim->moveQuadOut(&this->fMainMenuAnim2, pushAngle.y, 0.15f, true);
        anim->moveQuadOut(&this->fMainMenuAnim2, 0.0f, 0.4f, 0.15f);

        if(!anim->isAnimating(&this->fMainMenuAnim3)) this->fMainMenuAnim3 = 0.0f;

        anim->moveQuadOut(&this->fMainMenuAnim3, 0.0f, 0.15f, true);
    }
}

void MainMenu::onPlayButtonPressed() {
    this->bMainMenuAnimFriend = false;
    this->bMainMenuAnimFadeToFriendForNextAnim = false;
    this->bMainMenuAnimFriendScheduled = false;

    osu->toggleSongBrowser();

    soundEngine->play(osu->getSkin()->getMenuHit());
    soundEngine->play(osu->getSkin()->getClickSingleplayerSound());
}

void MainMenu::onMultiplayerButtonPressed() {
    if(!BanchoState::is_online()) {
        osu->getOptionsMenu()->askForLoginDetails();
        return;
    }

    this->setVisible(false);
    osu->getLobby()->setVisible(true);

    soundEngine->play(osu->getSkin()->getMenuHit());
    soundEngine->play(osu->getSkin()->getClickMultiplayerSound());
}

void MainMenu::onOptionsButtonPressed() {
    if(!osu->getOptionsMenu()->isVisible()) osu->toggleOptionsMenu();

    soundEngine->play(osu->getSkin()->getClickOptionsSound());
}

void MainMenu::onExitButtonPressed() {
    this->fShutdownScheduledTime = engine->getTime() + 0.3f;
    this->bWasCleanShutdown = true;
    this->setMenuElementsVisible(false);

    soundEngine->play(osu->getSkin()->getClickExitSound());
}

void MainMenu::onPausePressed() {
    if(osu->getMapInterface()->isPreviewMusicPlaying()) {
        osu->getMapInterface()->pausePreviewMusic();
    } else {
        auto music = osu->getMapInterface()->getMusic();
        if(music != nullptr) {
            soundEngine->play(music);
        }
    }
}

void MainMenu::onUpdatePressed() {
    using enum UpdateHandler::STATUS;
    const auto &updateHandler = osu->getUpdateHandler();
    const auto status = updateHandler->getStatus();

    if(status == STATUS_DOWNLOAD_COMPLETE)
        updateHandler->installUpdate();
    else if(status == STATUS_ERROR)
        updateHandler->checkForUpdates(true);
}

void MainMenu::onVersionPressed() {
    this->bDrawVersionNotificationArrow = false;
    this->writeVersionFile();
    osu->toggleChangelog();
}

void PauseButton::draw() {
    int third = this->vSize.x / 3;

    g->setColor(0xffffffff);

    if(!this->bIsPaused) {
        g->fillRect(this->vPos.x, this->vPos.y, third, this->vSize.y + 1);
        g->fillRect(this->vPos.x + 2 * third, this->vPos.y, third, this->vSize.y + 1);
    } else {
        g->setColor(0xffffffff);
        VertexArrayObject vao;

        const int smoothPixels = 2;

        // center triangle
        vao.addVertex(this->vPos.x, this->vPos.y + smoothPixels);
        vao.addColor(0xffffffff);
        vao.addVertex(this->vPos.x + this->vSize.x, this->vPos.y + this->vSize.y / 2);
        vao.addColor(0xffffffff);
        vao.addVertex(this->vPos.x, this->vPos.y + this->vSize.y - smoothPixels);
        vao.addColor(0xffffffff);

        // top smooth
        vao.addVertex(this->vPos.x, this->vPos.y + smoothPixels);
        vao.addColor(0xffffffff);
        vao.addVertex(this->vPos.x, this->vPos.y);
        vao.addColor(0x00000000);
        vao.addVertex(this->vPos.x + this->vSize.x, this->vPos.y + this->vSize.y / 2);
        vao.addColor(0xffffffff);

        vao.addVertex(this->vPos.x, this->vPos.y);
        vao.addColor(0x00000000);
        vao.addVertex(this->vPos.x + this->vSize.x, this->vPos.y + this->vSize.y / 2);
        vao.addColor(0xffffffff);
        vao.addVertex(this->vPos.x + this->vSize.x, this->vPos.y + this->vSize.y / 2 - smoothPixels);
        vao.addColor(0x00000000);

        // bottom smooth
        vao.addVertex(this->vPos.x, this->vPos.y + this->vSize.y - smoothPixels);
        vao.addColor(0xffffffff);
        vao.addVertex(this->vPos.x, this->vPos.y + this->vSize.y);
        vao.addColor(0x00000000);
        vao.addVertex(this->vPos.x + this->vSize.x, this->vPos.y + this->vSize.y / 2);
        vao.addColor(0xffffffff);

        vao.addVertex(this->vPos.x, this->vPos.y + this->vSize.y);
        vao.addColor(0x00000000);
        vao.addVertex(this->vPos.x + this->vSize.x, this->vPos.y + this->vSize.y / 2);
        vao.addColor(0xffffffff);
        vao.addVertex(this->vPos.x + this->vSize.x, this->vPos.y + this->vSize.y / 2 + smoothPixels);
        vao.addColor(0x00000000);

        g->drawVAO(&vao);
    }

    // draw hover rects
    g->setColor(this->frameColor);
    if(this->bMouseInside && this->bEnabled) {
        if(!this->bActive && !mouse->isLeftDown())
            this->drawHoverRect(3);
        else if(this->bActive)
            this->drawHoverRect(3);
    }
    if(this->bActive && this->bEnabled) this->drawHoverRect(6);
};

MainMenu::SongsFolderEnumerator::SongsFolderEnumerator() : Resource() {
    resourceManager->requestNextLoadAsync();
    resourceManager->loadResource(this);
}

void MainMenu::SongsFolderEnumerator::rebuild() {
    this->bAsyncReady.store(false);
    this->bReady = false;
    resourceManager->reloadResource(this, true);
}

void MainMenu::SongsFolderEnumerator::initAsync() {
    this->folderPath = Database::getOsuSongsFolder();
    if(env->directoryExists(this->folderPath)) {
        auto peppy_mapsets = env->getFoldersInFolder(this->folderPath);
        for(const auto &mapset : peppy_mapsets) {
            this->entries.push_back(fmt::format("{}/{}/", this->folderPath, mapset));
        }
    }

    auto neosu_mapsets = env->getFoldersInFolder(NEOSU_MAPS_PATH "/");
    for(const auto &mapset : neosu_mapsets) {
        this->entries.push_back(fmt::format(NEOSU_MAPS_PATH "/{}/", mapset));
    }

    this->bAsyncReady = true;
}
