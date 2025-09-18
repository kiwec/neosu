// Copyright (c) 2025, WH, All rights reserved.
#include "Engine.h"
#include "ConVar.h"

#if defined(MCENGINE_PLATFORM_WASM) || defined(MCENGINE_FEATURE_MAINCALLBACKS)
#define MAIN_FUNC SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
#define nocbinline
#define SDL_MAIN_USE_CALLBACKS  // this enables the use of SDL_AppInit/AppEvent/AppIterate instead of a traditional
                                // mainloop, needed for wasm (works on desktop too, but it's not necessary)
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_process.h>
namespace {
void setcwdexe(const std::string & /*unused*/) {}
}  // namespace
#else
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_process.h>
#define MAIN_FUNC int main(int argc, char *argv[])
#define nocbinline forceinline
#include <filesystem>
namespace {
void setcwdexe(const std::string &exePathStr) noexcept {
    // Fix path in case user is running it from the wrong folder.
    // We only do this if MCENGINE_DATA_DIR is set to its default value, since if it's changed,
    // the packager clearly wants the executable in a different location.
    std::string dataDir{MCENGINE_DATA_DIR};
    if(dataDir != "./" && dataDir != ".\\") {
        return;
    }
    namespace fs = std::filesystem;

    bool failed = true;
    std::error_code ec;

    UString uPath{exePathStr.c_str()};
    fs::path exe_path{uPath.plat_str()};

    if(!exe_path.empty() && exe_path.has_parent_path()) {
        fs::current_path(exe_path.parent_path(), ec);
        failed = !!ec;
    }

    if(failed) {
        printf("WARNING: failed to set working directory to parent of %s\n", exePathStr.c_str());
    }
}
}  // namespace
#endif

#if defined(_WIN32)
#include "WinDebloatDefs.h"
#include <consoleapi2.h>  // for SetConsoleOutputCP
#include <processenv.h>   // for GetCommandLine
#endif

#include "SString.h"
#include "Profiler.h"

#if defined(__SSE__) || (defined(_M_IX86_FP) && (_M_IX86_FP > 0))
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <xmmintrin.h>
#endif
#define SET_FPU_DAZ_FTZ _mm_setcsr(_mm_getcsr() | 0x8040);
#else
#define SET_FPU_DAZ_FTZ
#endif

#include "main_impl.h"

//*********************************//
//	SDL CALLBACKS/MAINLOOP BEGINS  //
//*********************************//

// called when the SDL_APP_SUCCESS (normal exit) or SDL_APP_FAILURE (something bad happened) event is returned from
// Init/Iterate/Event
void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    if(result == SDL_APP_FAILURE) {
        fprintf(stderr, "[main]: Force exiting now, a fatal error occurred. (SDL error: %s)\n", SDL_GetError());
        std::abort();
    }

    auto *fmain = static_cast<SDLMain *>(appstate);

    fmain->setCursorClip(false, {});  // release input devices
    fmain->grabKeyboard(false);

    const bool restart = fmain->isRestartScheduled();
    std::vector<std::string> restartArgs{};
    if(restart) {
        restartArgs = fmain->getCommandLine();
    }

    fmain->shutdown(result);
    delete fmain;
    fmain = nullptr;

    printf("[main]: Shutdown success.\n");

    if constexpr(!Env::cfg(OS::WASM)) {
        if(restart) {
            SDLMain::restart(restartArgs);
        }
        if constexpr(!Env::cfg(FEAT::MAINCB)) {
            SDL_Quit();
            std::exit(0);
        }
    }
}

// we can just call handleEvent and iterate directly if we're not using main callbacks
#if defined(MCENGINE_PLATFORM_WASM) || defined(MCENGINE_FEATURE_MAINCALLBACKS)
// (event queue processing) serialized with SDL_AppIterate
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
    return static_cast<SDLMain *>(appstate)->handleEvent(event);
}

// (update tick) serialized with SDL_AppEvent
SDL_AppResult SDL_AppIterate(void *appstate) { return static_cast<SDLMain *>(appstate)->iterate(); }
#endif

// actual main/init, called once
MAIN_FUNC /* int argc, char *argv[] */
{
    // if a neosu instance is already running, send it a message then quit
    // only works on windows for now
    Environment::Interop::handle_existing_window(argc, argv);

#if defined(_WIN32)
    // required for handle_existing_window to find this running instance
    SDL_RegisterApp(PACKAGE_NAME, 0, nullptr);
#if defined(_DEBUG)  // only debug builds create a console
    SetConsoleOutputCP(65001 /*CP_UTF8*/);
#endif
#endif

    // improve floating point perf in case this isn't already enabled by the compiler
    SET_FPU_DAZ_FTZ

    // this sets and caches the path in getPathToSelf, so this must be called here
    const auto &selfpath = Environment::getPathToSelf(argv[0]);
    if constexpr(!Env::cfg(OS::WASM)) {
        // set the current working directory to the executable directory, so that relative paths
        // work as expected
        setcwdexe(selfpath);
    }

    std::string lowerPackageName = PACKAGE_NAME;
    SString::lower(lowerPackageName);

    // set up some common app metadata (SDL says these should be called as early as possible)
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, PACKAGE_NAME);
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING, NEOSU_VERSION);
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING,
                               fmt::format("com.mcengine.{}", lowerPackageName).c_str());
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_CREATOR_STRING, "kiwec/spectator/McKay");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_COPYRIGHT_STRING, "MIT/GPL3");  // neosu is gpl3, mcengine is mit
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_URL_STRING, PACKAGE_URL);
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_TYPE_STRING, "game");

    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DOUBLE_BUFFER, "1", SDL_HINT_OVERRIDE);
    if(!SDL_Init(SDL_INIT_VIDEO))  // other subsystems can be init later
    {
        fprintf(stderr, "Couldn't SDL_Init(): %s\n", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    auto *fmain = new SDLMain(argc, argv);

#if !(defined(MCENGINE_PLATFORM_WASM) || defined(MCENGINE_FEATURE_MAINCALLBACKS))
    if(!fmain || fmain->initialize() == SDL_APP_FAILURE) {
        SDL_AppQuit(fmain, SDL_APP_FAILURE);
    }

    const auto restartArgs{fmain->getCommandLine()};

    constexpr int SIZE_EVENTS = 64;
    std::array<SDL_Event, SIZE_EVENTS> events{};

    while(fmain->isRunning()) {
        VPROF_MAIN();
        {
            // event collection
            VPROF_BUDGET("SDL", VPROF_BUDGETGROUP_EVENTS);
            int eventCount = 0;
            {
                VPROF_BUDGET("SDL_PumpEvents", VPROF_BUDGETGROUP_EVENTS);
                SDL_PumpEvents();
            }
            do {
                {
                    VPROF_BUDGET("SDL_PeepEvents", VPROF_BUDGETGROUP_EVENTS);
                    eventCount = SDL_PeepEvents(&events[0], SIZE_EVENTS, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
                }
                {
                    VPROF_BUDGET("handleEvent", VPROF_BUDGETGROUP_EVENTS);
                    for(int i = 0; i < eventCount; ++i) fmain->handleEvent(&events[i]);
                }
            } while(eventCount == SIZE_EVENTS);
        }
        {
            // engine update + draw + fps limiter
            fmain->iterate();
        }
    }

    // i don't think this is reachable, but whatever
    // (we should hit SDL_AppQuit before this)
    if(fmain->isRestartScheduled()) {
        SDLMain::restart(restartArgs);
    }

    return 0;
#else
    *appstate = fmain;
    return fmain->initialize();
#endif  // SDL_MAIN_USE_CALLBACKS
}

void SDLMain::restart(const std::vector<std::string> &args) {
    SDL_PropertiesID restartprops = SDL_CreateProperties();

    std::vector<const char *> restartArgsChar(args.size() + 1);

    restartArgsChar.back() = nullptr;

    for(int i = 0; const auto &arg : args) {
        restartArgsChar[i] = arg.c_str();
        i++;
    }

    if(cv::debug_env.getBool()) {
        Engine::logRaw("restart args: ");
        for(int i = -1; const auto entry : restartArgsChar) {
            i++;
            if(!entry) continue;
            Engine::logRaw("({}) {} ", i, entry);
        }
        Engine::logRaw("\n");
    }

    SDL_SetPointerProperty(restartprops, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void *)restartArgsChar.data());
#ifdef MCENGINE_PLATFORM_WINDOWS
    const char *wincmdline = GetCommandLineA();
    if(wincmdline) {
        SDL_SetStringProperty(restartprops, SDL_PROP_PROCESS_CREATE_CMDLINE_STRING, GetCommandLineA());
    }
    // so that handle_existing_window doesn't find the currently running instance by the class name
    SDL_UnregisterApp();
#endif
    SDL_SetBooleanProperty(restartprops, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);

    if(!SDL_CreateProcessWithProperties(restartprops)) {
        fprintf(stderr, "[restart]: WARNING: couldn't restart!\n");
    }

    SDL_DestroyProperties(restartprops);
}
