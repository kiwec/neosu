#pragma once
// Copyright (c) 2011, PG, All rights reserved.

#include "CBaseUIElement.h"

#include <atomic>
#include <mutex>

class CBaseUITextbox;
class CBaseUIButton;
class CBaseUIScrollView;

class ConsoleBoxTextbox;

class ConsoleBox : public CBaseUIElement {
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
    void execConfigFile(std::string filename);

    // set
    void setRequireShiftToActivate(bool requireShiftToActivate) {
        this->bRequireShiftToActivate = requireShiftToActivate;
    }

    // get
    bool isBusy() override;
    bool isActive() override;

    // ILLEGAL:
    [[nodiscard]] inline ConsoleBoxTextbox *getTextbox() const { return this->textbox; }

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

    int iSuggestionCount;
    int iSelectedSuggestion;  // for up/down buttons

    ConsoleBoxTextbox *textbox;
    CBaseUIScrollView *suggestion;
    std::vector<CBaseUIButton *> vSuggestionButtons;
    float fSuggestionY;

    bool bRequireShiftToActivate;
    bool bConsoleAnimateOnce;
    float fConsoleDelay;
    float fConsoleAnimation;
    bool bConsoleAnimateIn;
    bool bConsoleAnimateOut;

    bool bSuggestionAnimateIn;
    bool bSuggestionAnimateOut;
    float fSuggestionAnimation;

    float fLogTime;
    float fLogYPos;
    std::vector<LOG_ENTRY> log_entries;
    McFont *logFont;

    std::vector<std::string> commandHistory;
    int iSelectedHistory;
    bool bClearPending{false};

    std::recursive_mutex logMutex;

    // thread-safe log animation state
    std::atomic<bool> bLogAnimationResetPending;
    std::atomic<float> fPendingLogTime;
    std::atomic<bool> bForceLogVisible;  // needed as an "ohshit" when a ton of lines are added in a single frame after
                                         // the log has been hidden already
};
