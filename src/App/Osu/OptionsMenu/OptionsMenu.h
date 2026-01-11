#pragma once
// Copyright (c) 2016, PG & 2025-2026 WH, All rights reserved.
#include "NotificationOverlay.h"
#include "ScreenBackable.h"

#include "StaticPImpl.h"

class UIContextMenu;
struct OptionsMenuImpl;

class OptionsMenu final : public ScreenBackable, public NotificationOverlayKeyListener {
    NOCOPY_NOMOVE(OptionsMenu)
   public:
    OptionsMenu();
    ~OptionsMenu() override;

    void draw() override;
    void update() override;

    void onKeyDown(KeyboardEvent &e) override;
    void onChar(KeyboardEvent &e) override;

    void onResolutionChange(vec2 newResolution) override;

    void onKey(KeyboardEvent &e) override;

    CBaseUIContainer *setVisible(bool visible) override;

    void save();

    void openAndScrollToSkinSection();

    void setUsername(UString username);

    bool isMouseInside() override;
    bool isBusy() override;

    void scheduleLayoutUpdate();

    // used by Osu for global skin select keybind
    void onSkinSelect();

    // used by Osu for audio restart callback
    void onOutputDeviceChange();

    // used by Osu for osu_folder callback
    void updateOsuFolderTextbox(std::string_view newFolder);

    // used by Chat
    void askForLoginDetails();

    // used by networking stuff
    void update_login_button(bool loggedIn = false);

    // used by WindowsMain for osk handling (this needs to be moved...)
    void updateSkinNameLabel();

    // used by VolumeOverlay
    UIContextMenu *getContextMenu();

   private:
    void updateLayout() override;
    void onBack() override;

    friend struct OptionsMenuImpl;
    StaticPImpl<OptionsMenuImpl, 1000> pImpl;
};
