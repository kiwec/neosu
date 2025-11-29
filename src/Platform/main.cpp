// Copyright (c) 2025, WH, All rights reserved.
#include "BaseEnvironment.h"
#include "EngineConfig.h"
#include "Logging.h"

#if defined(MCENGINE_PLATFORM_WASM) || defined(MCENGINE_FEATURE_MAINCALLBACKS)
#define MAIN_FUNC SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
#define SDL_MAIN_USE_CALLBACKS  // this enables the use of SDL_AppInit/AppEvent/AppIterate instead of a traditional
                                // mainloop, needed for wasm (works on desktop too, but it's not necessary)
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_process.h>
namespace {
void setcwdexe(const std::string & /*unused*/) {}
}  // namespace
#else
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_process.h>
#define MAIN_FUNC int main(int argc, char *argv[])
#include <filesystem>

#include "File.h"

namespace {
void setcwdexe(const std::string &exePathStr) noexcept {
    // Fix path in case user is running it from the wrong folder.
    // We only do this if MCENGINE_DATA_DIR is set to its default value, since if it's changed,
    // the packager clearly wants the executable in a different location.
    if constexpr(!(MCENGINE_DATA_DIR[0] == '.' && MCENGINE_DATA_DIR[1] == '/')) {
        return;
    }
    namespace fs = std::filesystem;

    bool failed = true;
    std::error_code ec;

    fs::path exe_path{File::getFsPath(exePathStr)};

    if(!exe_path.empty() && exe_path.has_parent_path()) {
        fs::current_path(exe_path.parent_path(), ec);
        failed = !!ec;
    }

    if(failed) {
        debugLog("WARNING: failed to set working directory to parent of {}", exePathStr.c_str());
    }
}
}  // namespace
#endif

#if defined(_WIN32)
#include "WinDebloatDefs.h"
#include <consoleapi2.h>  // for SetConsoleOutputCP
#include <processenv.h>   // for GetCommandLine
#endif

#include "CrashHandler.h"
#include "Profiler.h"

#if defined(__SSE__) || (defined(_M_IX86_FP) && (_M_IX86_FP > 0))
#ifndef _MSC_VER
#include <xmmintrin.h>
#endif
#define SET_FPU_DAZ_FTZ _mm_setcsr(_mm_getcsr() | 0x8040);
#else
#define SET_FPU_DAZ_FTZ
#endif

#ifdef WITH_LIVEPP
#include "LPP_API_x64_CPP.h"
#endif

#include "main_impl.h"
#include "DiffCalcTool.h"

//*********************************//
//	SDL CALLBACKS/MAINLOOP BEGINS  //
//*********************************//

// called when the SDL_APP_SUCCESS (normal exit) or SDL_APP_FAILURE (something bad happened) event is returned from
// Init/Iterate/Event
void SDL_AppQuit(void *appstate, SDL_AppResult result) {
    if(!appstate || result == SDL_APP_FAILURE) {
        debugLog("Force exiting now, a fatal error occurred. (SDL error: {})", SDL_GetError());
        std::exit(-1);
    }

    auto *fmain = static_cast<SDLMain *>(appstate);

    fmain->setCursorClip(false, {});  // release input devices
    fmain->setWindowsKeyDisabled(false);

    const bool restart = fmain->isRestartScheduled();
    std::vector<std::string> restartArgs{};
    if(restart) {
        restartArgs = fmain->getCommandLine();
    }

    fmain->shutdown(result);  // FIXME: redundant?
    delete fmain;
    fmain = nullptr;

    if constexpr(!Env::cfg(OS::WASM)) {
        if(restart) {
            SDLMain::restart(restartArgs);
        }
        if constexpr(!Env::cfg(FEAT::MAINCB)) {
            SDL_Quit();
            printf("Shutdown success.\n");
            std::exit(0);
        }
    } else {
        printf("Shutdown success.\n");
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
#ifdef WITH_LIVEPP
    debugLog("Starting Live++");
    lpp::LppSynchronizedAgent lppAgent = lpp::LppCreateSynchronizedAgent(nullptr, L"../../../LivePP");
    if(!lpp::LppIsValidSynchronizedAgent(&lppAgent)) {
        return 1;
    }

    lppAgent.EnableModule(lpp::LppGetCurrentModulePath(), lpp::LPP_MODULES_OPTION_NONE, nullptr, nullptr);
#endif

    const bool diffcalcOnly = argc >= 2 && strncmp(argv[1], "-diffcalc", sizeof("-diffcalc") - 1) == 0;
    if(diffcalcOnly) {
        return run_diffcalc(argc, argv);
    }

    // if a neosu instance is already running, send it a message then quit
    // only works on windows for now
    Environment::Interop::handle_existing_window(argc, argv);

    CrashHandler::init();  // initialize minidump handling

    // set up spdlog logging (after handle_existing_window, so we don't clobber a running instance's log messages)
    Logger::init();
    atexit(Logger::shutdown);

#if defined(_WIN32)
    {
        constexpr int CS_VREDRAW_ = 0x1;
        constexpr int CS_HREDRAW_ = 0x2;
        constexpr int CS_BYTEALIGNCLIENT_ = 0x1000;
        constexpr int CS_BYTEALIGNWINDOW_ = 0x2000;

        // required for handle_existing_window to find this running instance
        SDL_RegisterApp(PACKAGE_NAME, CS_BYTEALIGNCLIENT_ | CS_BYTEALIGNWINDOW_ | CS_VREDRAW_ | CS_HREDRAW_, nullptr);
    }
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

    // set up some common app metadata (SDL says these should be called as early as possible)
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_NAME_STRING, PACKAGE_NAME);
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_VERSION_STRING, NEOSU_VERSION);
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_IDENTIFIER_STRING, "net.kiwec.neosu");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_CREATOR_STRING, "kiwec/spectator/McKay");
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_COPYRIGHT_STRING, "MIT/GPL3");  // neosu is gpl3, mcengine is mit
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_URL_STRING, PACKAGE_URL);
    SDL_SetAppMetadataProperty(SDL_PROP_APP_METADATA_TYPE_STRING, "game");

    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DOUBLE_BUFFER, "1", SDL_HINT_NORMAL);

    // parse args here

    // simple vector representation of the whole cmdline including the program name (as the first element)
    auto arg_cmdline = std::vector<std::string>(argv, argv + argc);

    // more easily queryable representation of the args as a map
    auto arg_map = [&]() -> std::unordered_map<std::string, std::optional<std::string>> {
        // example usages:
        // args.contains("-file")
        // auto filename = args["-file"].value_or("default.txt");
        // if (args["-output"].has_value())
        // 	auto outfile = args["-output"].value();
        std::unordered_map<std::string, std::optional<std::string>> args;
        for(int i = 1; i < argc; ++i) {
            std::string_view arg{argv[i]};
            if(arg.starts_with('-'))
                if(i + 1 < argc && !(argv[i + 1][0] == '-')) {
                    args[std::string(arg)] = argv[i + 1];
                    ++i;
                } else
                    args[std::string(arg)] = std::nullopt;
            else
                args[std::string(arg)] = std::nullopt;
        }
        return args;
    }();

#if defined(_WIN32)
    // this hint needs to be set before SDL_Init
    if(arg_map.contains("-nodpi")) {
        // it's not even defined anywhere...
        SDL_SetHintWithPriority("SDL_WINDOWS_DPI_AWARENESS", "unaware", SDL_HINT_NORMAL);
    }
#endif

    if(!arg_map.contains("-ime")) {
        // FIXME(spec):
        // we want SDL_EVENT_TEXT_INPUT for utf char events all the time, but we don't want a text editing composition window to show up.
        // set this hint so that SDL thinks we can handle IME stuff internally, so it doesn't show any OS-level IME window.
        // !!!this is not good because it doesn't let the user use their native IME to input text in text fields!!!
        // a better/less hacky fix would be to always notify when we actually need text input, such as when a textbox is active,
        // and disable it otherwise, but until then, this is the simplest workaround.
        // followup in main_impl.cpp::SDLMain::configureEvents()
        SDL_SetHintWithPriority(SDL_HINT_IME_IMPLEMENTED_UI, "candidates,composition", SDL_HINT_NORMAL);
    }

    if(!SDL_Init(SDL_INIT_VIDEO))  // other subsystems can be init later
    {
        debugLog("Couldn't SDL_Init(): {}", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    auto *fmain = new SDLMain(arg_map, arg_cmdline);

#if !(defined(MCENGINE_PLATFORM_WASM) || defined(MCENGINE_FEATURE_MAINCALLBACKS))
    if(!fmain || fmain->initialize() == SDL_APP_FAILURE) {
        SDL_AppQuit(fmain, SDL_APP_FAILURE);
    }

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
#ifdef WITH_LIVEPP
            if(lppAgent.WantsReload(lpp::LPP_RELOAD_OPTION_SYNCHRONIZE_WITH_RELOAD)) {
                // XXX: Should pause/restart threads here instead of just yoloing
                lppAgent.Reload(lpp::LPP_RELOAD_BEHAVIOUR_WAIT_UNTIL_CHANGES_ARE_APPLIED);
            }

            if(lppAgent.WantsRestart()) {
                // XXX: Not sure if this works, but I don't think I'll be using live++ restart
                SDLMain::restart(arg_cmdline);
                lppAgent.Restart(lpp::LPP_RESTART_BEHAVIOUR_INSTANT_TERMINATION, 0u, nullptr);
            }
#endif

            // engine update + draw + fps limiter
            fmain->iterate();
        }
    }

    // i don't think this is reachable, but whatever
    // (we should hit SDL_AppQuit before this)
    if(fmain->isRestartScheduled()) {
        SDLMain::restart(arg_cmdline);
    }

#ifdef WITH_LIVEPP
    lpp::LppDestroySynchronizedAgent(&lppAgent);
#endif

    return 0;
#else
    *appstate = fmain;
    return fmain->initialize();
#endif  // SDL_MAIN_USE_CALLBACKS
}
