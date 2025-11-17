#pragma once
// Copyright (c) 2011, PG, All rights reserved.

#include "CBaseUIElement.h"
#include "SyncMutex.h"

#include <atomic>

class CBaseUITextbox;
class CBaseUIButton;
class CBaseUIScrollView;

class ConsoleBoxTextbox;

class ConsoleBox : public CBaseUIElement {
    NOCOPY_NOMOVE(ConsoleBox)
   public:
    ConsoleBox();
    ~ConsoleBox() override;

    void draw() override;
    void drawLogOverlay();
    void mouse_update(bool *propagate_clicks) override;

    void onKeyDown(KeyboardEvent &e) override;
    void onChar(KeyboardEvent &e) override;

    void onResolutionChange(vec2 newResolution);

    void processCommand(const std::string &command);

    // set
    void setRequireShiftToActivate(bool requireShiftToActivate) {
        this->bRequireShiftToActivate = requireShiftToActivate;
    }

    // get
    bool isBusy() override;
    bool isActive() override;

   private:
    friend class Logger;
    void log(const UString &text, Color textColor = 0xffffffff);

    struct LOG_ENTRY {
        UString text;
        Color textColor;
    };

   private:
    // callback
    inline void clear() { this->bClearPending = true; }

    void onSuggestionClicked(CBaseUIButton *suggestion);

    void addSuggestion(const UString &text, const UString &helpText, const UString &command);
    void clearSuggestions();

    void show();
    void toggle(KeyboardEvent &e);

    float getAnimTargetY();

    float getDPIScale();

    void processPendingLogAnimations();

    int iSuggestionCount{0};
    int iSelectedSuggestion{-1};  // for up/down buttons

    std::unique_ptr<ConsoleBoxTextbox> textbox{nullptr};
    std::unique_ptr<CBaseUIScrollView> suggestion{nullptr};
    std::vector<CBaseUIButton *> vSuggestionButtons;
    float fSuggestionY{0.f};

    bool bRequireShiftToActivate{false};
    bool bConsoleAnimateOnce{false};  // set to true for on-launch anim in
    float fConsoleDelay;
    float fConsoleAnimation{0.f};
    bool bConsoleAnimateIn{false};
    bool bConsoleAnimateOut{false};

    bool bSuggestionAnimateIn{false};
    bool bSuggestionAnimateOut{false};
    float fSuggestionAnimation{0.f};

    float fLogTime{0.f};
    float fLogYPos{0.f};
    std::vector<LOG_ENTRY> log_entries;
    McFont *logFont;

    std::vector<std::string> commandHistory;
    int iSelectedHistory{-1};
    bool bClearPending{false};

    Sync::shared_mutex logMutex;

    // thread-safe log animation state
    std::atomic<bool> bLogAnimationResetPending{false};
    std::atomic<float> fPendingLogTime{0.f};
    std::atomic<bool> bForceLogVisible{
        false};  // needed as an "ohshit" when a ton of lines are added in a single frame after
                 // the log has been hidden already
};
