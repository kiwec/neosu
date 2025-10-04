#pragma once
// Copyright (c) 2025, WH, All rights reserved.

#include "Environment.h"

namespace Timing {
class Timer;
}

#if !defined(SDL_h_) && !defined(SDL_main_h_)
typedef struct SDL_GLContextState *SDL_GLContext;
typedef union SDL_Event SDL_Event;
enum SDL_AppResult : uint8_t;
#endif

#if !(defined(SDL_main_h_) && defined(MCENGINE_FEATURE_MAINCALLBACKS))
extern void SDL_AppQuit(void *appstate, SDL_AppResult result);
#endif

class SDLMain final : public Environment {
    NOCOPY_NOMOVE(SDLMain)
   public:
    SDLMain(const std::unordered_map<std::string, std::optional<std::string>> &argMap,
            const std::vector<std::string> &argVec);
    ~SDLMain() override;

    SDL_AppResult initialize();
    SDL_AppResult iterate();
    SDL_AppResult handleEvent(SDL_Event *event);
    void shutdown(SDL_AppResult result);

    static void restart(const std::vector<std::string> &restartArgs);

   private:
    // init methods
    bool createWindow();
    void setupLogging();
    void configureEvents();
    float queryDisplayHz();
    void doEarlyCmdlineOverrides();

    // callback handlers
    void fps_max_callback(float newVal);
    void fps_max_background_callback(float newVal);

    // set iteration rate for callbacks
    void setFgFPS();
    void setBgFPS();

    // GL context (must be created early, during window creation)
    SDL_GLContext m_context{nullptr};

    int m_iFpsMax{360};
    int m_iFpsMaxBG{30};

    std::vector<std::string> m_vDroppedData;  // queued data dropped onto window
};
