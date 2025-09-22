// Copyright (c) 2025, WH, All rights reserved.
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_process.h>
#include <SDL3/SDL_properties.h>

#define SDL_h_

#include "main_impl.h"

#include "Osu.h" // TODO: remove needing this

#include "MakeDelegateWrapper.h"
#include "Engine.h"
#include "ConVar.h"
#include "FPSLimiter.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "Profiler.h"
#include "Logging.h"

SDLMain::SDLMain(int argc, char *argv[]) : Environment(argc, argv) {
    // setup callbacks
    cv::fps_max.setCallback(SA::MakeDelegate<&SDLMain::fps_max_callback>(this));
    cv::fps_max_background.setCallback(SA::MakeDelegate<&SDLMain::fps_max_background_callback>(this));
}

void SDLMain::setFgFPS() {
    if constexpr(Env::cfg(FEAT::MAINCB))
        SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, fmt::format("{}", m_iFpsMax).c_str());
    else
        FPSLimiter::reset();
}

void SDLMain::setBgFPS() {
    if constexpr(Env::cfg(FEAT::MAINCB))
        SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, fmt::format("{}", m_iFpsMaxBG).c_str());
}

// convar change callbacks, to set app iteration rate
void SDLMain::fps_max_callback(float newVal) {
    int newFps = static_cast<int>(newVal);
    if((newFps == 0 || newFps >= 30)) m_iFpsMax = newFps;
    if(m_bHasFocus) setFgFPS();
}

void SDLMain::fps_max_background_callback(float newVal) {
    int newFps = static_cast<int>(newVal);
    if(newFps >= 0) m_iFpsMaxBG = newFps;
    if(!m_bHasFocus) setBgFPS();
}

SDLMain::~SDLMain() {
    m_engine.reset();

    // clean up GL context
    if(m_context && (Env::cfg((REND::GL | REND::GLES32), !REND::DX11))) {
        SDL_GL_DestroyContext(m_context);
        m_context = nullptr;
    }
    // close/delete the window
    if(m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
}

SDL_AppResult SDLMain::initialize() {
    doEarlyCmdlineOverrides();
    setupLogging();

    // create window with props
    if(!createWindow()) {
        return SDL_APP_FAILURE;
    }

    this->getEnvInterop().setup_system_integrations();  // only implemented for windows atm

    // disable (filter) some SDL events we don't care about
    configureEvents();

    // initialize engine, now that all the setup is done
    m_engine = std::make_unique<Engine>();

    // if we got to this point, all relevant subsystems (input handling, graphics interface, etc.) have been initialized

    // load app
    m_engine->loadApp();

    // make window visible now, after we loaded the config and set the wanted window size & fullscreen state
    SDL_ShowWindow(m_window);
    SDL_RaiseWindow(m_window);

    syncWindow();

    setWindowResizable(true);

    // initialize mouse position
    {
        float x{0.f}, y{0.f};
        SDL_GetGlobalMouseState(&x, &y);
        vec2 posInWindow = vec2{x, y} - getWindowPos();

        setOSMousePos(posInWindow);
        mouse->onPosChange(posInWindow);
    }

    // SDL3 stops listening to text input globally when window is created
    SDL_StartTextInput(m_window);
    SDL_SetWindowKeyboardGrab(m_window, false);  // this allows windows key and such to work

    // return init success
    return SDL_APP_CONTINUE;
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif

static_assert(SDL_EVENT_WINDOW_FIRST == SDL_EVENT_WINDOW_SHOWN);
static_assert(SDL_EVENT_WINDOW_LAST == SDL_EVENT_WINDOW_HDR_STATE_CHANGED);

SDL_AppResult SDLMain::handleEvent(SDL_Event *event) {
    switch(event->type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if(m_bRunning) {
                m_bRunning = false;
                m_engine->shutdown();
                if constexpr(Env::cfg(FEAT::MAINCB))
                    return SDL_APP_SUCCESS;
                else
                    SDL_AppQuit(this, SDL_APP_SUCCESS);
            }
            break;

            // drag-drop events
            // clang-format off
        case SDL_EVENT_DROP_FILE: case SDL_EVENT_DROP_TEXT: case SDL_EVENT_DROP_BEGIN:
        case SDL_EVENT_DROP_COMPLETE: case SDL_EVENT_DROP_POSITION:
            // clang-format on
            switch(event->drop.type) {
                case SDL_EVENT_DROP_BEGIN: {
                    m_vDroppedData.clear();
                } break;
                case SDL_EVENT_DROP_COMPLETE: {
                    this->getEnvInterop().handle_cmdline_args(m_vDroppedData);
                    m_vDroppedData.clear();
                } break;
                case SDL_EVENT_DROP_TEXT:
                case SDL_EVENT_DROP_FILE: {
                    std::string dropped_data{event->drop.data};
                    if(dropped_data.length() < 2) {
                        break;
                    }
                    m_vDroppedData.push_back(dropped_data);
                    if(m_bEnvDebug) {
                        Logger::logRaw("DEBUG: got SDL drag-drop event {}, current dropped_data queue is ",
                                       static_cast<int>(event->drop.type));
                        for(const auto &d : m_vDroppedData) {
                            Logger::logRaw("{}", d);
                        }
                        Logger::logRaw(".\n");
                    }
                } break;
                case SDL_EVENT_DROP_POSITION:  // we don't really care
                default:
                    if(m_bEnvDebug)
                        debugLog("DEBUG: unhandled SDL drag-drop event {}\n", static_cast<int>(event->drop.type));
                    break;
            }
            break;

            // window events (i hate you msvc ffs)
            // clang-format off
        case SDL_EVENT_WINDOW_SHOWN:				 case SDL_EVENT_WINDOW_HIDDEN:			  case SDL_EVENT_WINDOW_EXPOSED:
        case SDL_EVENT_WINDOW_MOVED:				 case SDL_EVENT_WINDOW_RESIZED:			  case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_METAL_VIEW_RESIZED:	 case SDL_EVENT_WINDOW_MINIMIZED:		  case SDL_EVENT_WINDOW_MAXIMIZED:
        case SDL_EVENT_WINDOW_RESTORED:				 case SDL_EVENT_WINDOW_MOUSE_ENTER:		  case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        case SDL_EVENT_WINDOW_FOCUS_GAINED:			 case SDL_EVENT_WINDOW_FOCUS_LOST:		  /* case SDL_EVENT_WINDOW_CLOSE_REQUESTED: */
        case SDL_EVENT_WINDOW_HIT_TEST:				 case SDL_EVENT_WINDOW_ICCPROF_CHANGED:	  case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED: case SDL_EVENT_WINDOW_SAFE_AREA_CHANGED: case SDL_EVENT_WINDOW_OCCLUDED:
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:		 case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:  case SDL_EVENT_WINDOW_DESTROYED:
        case SDL_EVENT_WINDOW_HDR_STATE_CHANGED:
            // clang-format on
            switch(event->window.type) {
                case SDL_EVENT_WINDOW_FOCUS_GAINED:
                    m_bHasFocus = true;
                    m_engine->onFocusGained();
                    setFgFPS();
                    break;

                case SDL_EVENT_WINDOW_FOCUS_LOST:
                    m_bHasFocus = false;
                    m_engine->onFocusLost();
                    setBgFPS();
                    break;

                case SDL_EVENT_WINDOW_MAXIMIZED:
                    m_bMinimized = false;
                    m_bHasFocus = true;
                    m_engine->onMaximized();
                    setFgFPS();
                    break;

                case SDL_EVENT_WINDOW_MOUSE_ENTER:
                    m_bIsCursorInsideWindow = true;
                    break;

                case SDL_EVENT_WINDOW_MOUSE_LEAVE:
                    m_bIsCursorInsideWindow = false;
                    setCursorVisible(true);
                    break;

                case SDL_EVENT_WINDOW_MINIMIZED:
                    m_bMinimized = true;
                    m_bHasFocus = false;
                    m_engine->onMinimized();
                    setBgFPS();
                    break;

                case SDL_EVENT_WINDOW_RESTORED:
                    m_bMinimized = false;
                    m_bHasFocus = true;
                    m_engine->onRestored();
                    setFgFPS();
                    break;

                case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
                    cv::fullscreen.setValue(true, false);
                    m_bFullscreen = true;
                    break;

                case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
                    cv::fullscreen.setValue(false, false);
                    m_bFullscreen = false;
                    break;

                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                case SDL_EVENT_WINDOW_RESIZED:
                    m_bHasFocus = true;
                    m_fDisplayHzSecs = 1.0f / (m_fDisplayHz = queryDisplayHz());
                    m_engine->requestResolutionChange(
                        vec2(static_cast<float>(event->window.data1), static_cast<float>(event->window.data2)));
                    setFgFPS();
                    break;

                case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
                    cv::monitor.setValue(event->window.data1, false);
                case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:  // TODO?
                    m_engine->requestResolutionChange(getWindowSize());
                    m_fDisplayHzSecs = 1.0f / (m_fDisplayHz = queryDisplayHz());
                    break;

                default:
                    if(m_bEnvDebug)
                        debugLog("DEBUG: unhandled SDL window event {}\n", static_cast<int>(event->window.type));
                    break;
            }
            break;

        // keyboard events
        case SDL_EVENT_KEY_DOWN:
            keyboard->onKeyDown(event->key.scancode);
            break;

        case SDL_EVENT_KEY_UP:
            keyboard->onKeyUp(event->key.scancode);
            break;

        case SDL_EVENT_TEXT_INPUT:
            for(const auto &key : UString(event->text.text)) keyboard->onChar(key);
            break;

        // mouse events
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            mouse->onButtonChange(static_cast<ButtonIndex>(event->button.button),
                                  true);  // C++ needs me to cast an unsigned char to an unsigned char
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            mouse->onButtonChange(static_cast<ButtonIndex>(event->button.button), false);
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            if(event->wheel.x != 0)
                mouse->onWheelHorizontal(event->wheel.x > 0 ? 120 * std::abs(static_cast<int>(event->wheel.x))
                                                            : -120 * std::abs(static_cast<int>(event->wheel.x)));
            if(event->wheel.y != 0)
                mouse->onWheelVertical(event->wheel.y > 0 ? 120 * std::abs(static_cast<int>(event->wheel.y))
                                                          : -120 * std::abs(static_cast<int>(event->wheel.y)));
            break;

        case SDL_EVENT_PEN_MOTION:  // ignored, see comment in configureEvents
            break;

        default:
            if(m_bEnvDebug) debugLog("DEBUG: unhandled SDL event {}\n", static_cast<int>(event->type));
            break;
    }

    return SDL_APP_CONTINUE;
}

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

SDL_AppResult SDLMain::iterate() {
    if(!m_bRunning) return SDL_APP_SUCCESS;

    // update
    {
        m_engine->onUpdate();
    }

    // draw
    {
        m_bDrawing = true;
        m_engine->onPaint();
        m_bDrawing = false;
    }

    if constexpr(!Env::cfg(FEAT::MAINCB))  // main callbacks use SDL iteration rate to limit fps
    {
        VPROF_BUDGET("FPSLimiter", VPROF_BUDGETGROUP_SLEEP);

        // if minimized or unfocused, use BG fps, otherwise use fps_max (if 0 it's unlimited)
        const int targetFPS = (m_bMinimized || !m_bHasFocus)
                                  ? m_iFpsMaxBG
                                  : ((osu && osu->isInPlayMode()) ? m_iFpsMax : cv::fps_max_menu.getInt());
        FPSLimiter::limit_frames(targetFPS);
    }

    return SDL_APP_CONTINUE;
}

// window configuration
static constexpr auto WINDOW_TITLE = PACKAGE_NAME;
static constexpr auto WINDOW_WIDTH = 1280L;
static constexpr auto WINDOW_HEIGHT = 720L;
static constexpr auto WINDOW_WIDTH_MIN = 320;
static constexpr auto WINDOW_HEIGHT_MIN = 240;

bool SDLMain::createWindow() {
    // pre window-creation settings
    if constexpr(Env::cfg((REND::GL | REND::GLES32), !REND::DX11)) {
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                            Env::cfg(REND::GL) ? SDL_GL_CONTEXT_PROFILE_COMPATIBILITY : SDL_GL_CONTEXT_PROFILE_ES);
        if constexpr(!Env::cfg(REND::GL)) {
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        }
        // setup antialiasing from -aa command line argument
        if(m_mArgMap["-aa"].has_value()) {
            auto aaSamples = std::strtoull(m_mArgMap["-aa"].value().c_str(), nullptr, 10);
            if(aaSamples > 1) {
                aaSamples = std::clamp(std::bit_floor(aaSamples), 2ULL, 16ULL);
                SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
                SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, static_cast<int>(aaSamples));
            }
        }
        // create gl debug context
        if(m_mArgMap.contains("-debugctx")) {
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
            SDL_SetLogPriority(SDL_LOG_CATEGORY_VIDEO, SDL_LOG_PRIORITY_TRACE);
        }
    }

    // set vulkan for linux dxvk-native, opengl otherwise (or none for windows dx11)
    constexpr auto windowFlags =
        SDL_WINDOW_HIDDEN | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS |
        (Env::cfg((REND::GL | REND::GLES32)) ? SDL_WINDOW_OPENGL
                                             : (Env::cfg(OS::LINUX, REND::DX11) ? SDL_WINDOW_VULKAN : 0UL));

    // limit default window size so it fits the screen
    long windowCreateWidth = WINDOW_WIDTH;
    long windowCreateHeight = WINDOW_HEIGHT;
    {
        SDL_DisplayID di = SDL_GetPrimaryDisplay();
        const SDL_DisplayMode *dm = nullptr;
        if(di && (dm = SDL_GetDesktopDisplayMode(di))) {
            if(dm->w < windowCreateWidth) windowCreateWidth = dm->w;
            if(dm->h < windowCreateHeight) windowCreateHeight = dm->h;
        }
    }

    // set this size as the initial fallback window size (for Environment::getWindowSize())
    m_vLastKnownWindowSize = vec2{static_cast<float>(windowCreateWidth), static_cast<float>(windowCreateHeight)};

    SDL_PropertiesID props = SDL_CreateProperties();
    // if constexpr (Env::cfg(REND::DX11))
    // 	SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_EXTERNAL_GRAPHICS_CONTEXT_BOOLEAN, true);
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, WINDOW_TITLE);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, windowCreateWidth);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, windowCreateHeight);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, false);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_MAXIMIZED_BOOLEAN, false);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, false);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, false);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER, windowFlags);

    if constexpr(!Env::cfg(OS::WINDOWS)) {
        SDL_SetHintWithPriority(SDL_HINT_MOUSE_AUTO_CAPTURE, "0", SDL_HINT_OVERRIDE);
    }

    SDL_SetHintWithPriority(SDL_HINT_MOUSE_RELATIVE_MODE_CENTER, "0", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_TOUCH_MOUSE_EVENTS, "0", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_MOUSE_EMULATE_WARP_WITH_RELATIVE, "0", SDL_HINT_OVERRIDE);

    // create window
    m_window = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);

    if(m_window == nullptr) {
        debugLog("Couldn't SDL_CreateWindow(): {:s}\n", SDL_GetError());
        return false;
    }

    cv::monitor.setValue(SDL_GetDisplayForWindow(m_window), false);

    // create gl context
    if constexpr(Env::cfg((REND::GL | REND::GLES32), !REND::DX11)) {
        m_context = SDL_GL_CreateContext(m_window);
        if(!m_context) {
            debugLog("Couldn't create OpenGL context: {:s}\n", SDL_GetError());
            return false;
        }
        if(!SDL_GL_MakeCurrent(m_window, m_context)) {
            debugLog("Couldn't make OpenGL context current: {:s}\n", SDL_GetError());
            return false;
        }
    }

    SDL_SetWindowMinimumSize(m_window, WINDOW_WIDTH_MIN, WINDOW_HEIGHT_MIN);

    // initialize with the display refresh rate of the current monitor
    m_fDisplayHzSecs = 1.0f / (m_fDisplayHz = queryDisplayHz());
    {
        const auto hz = std::round(m_fDisplayHz);
        const auto fourxhz = std::round(std::clamp<float>(hz * 4.0f, hz, 1000.0f));

        // also set fps_max to 4x the refresh rate
        cv::fps_max.setDefaultDouble(fourxhz);
        cv::fps_max.setValue(fourxhz);
        cv::fps_max_menu.setDefaultDouble(hz);
        cv::fps_max_menu.setValue(hz);
    }

    return true;
}

void SDLMain::configureEvents() {
    // disable unused events
    SDL_SetEventEnabled(SDL_EVENT_MOUSE_MOTION, false);

    // joystick
    SDL_SetEventEnabled(SDL_EVENT_JOYSTICK_AXIS_MOTION, false);
    SDL_SetEventEnabled(SDL_EVENT_JOYSTICK_BALL_MOTION, false);
    SDL_SetEventEnabled(SDL_EVENT_JOYSTICK_HAT_MOTION, false);
    SDL_SetEventEnabled(SDL_EVENT_JOYSTICK_BUTTON_DOWN, false);
    SDL_SetEventEnabled(SDL_EVENT_JOYSTICK_BUTTON_UP, false);
    SDL_SetEventEnabled(SDL_EVENT_JOYSTICK_ADDED, false);
    SDL_SetEventEnabled(SDL_EVENT_JOYSTICK_REMOVED, false);
    SDL_SetEventEnabled(SDL_EVENT_JOYSTICK_BATTERY_UPDATED, false);
    SDL_SetEventEnabled(SDL_EVENT_JOYSTICK_UPDATE_COMPLETE, false);

    // pen
    SDL_SetEventEnabled(SDL_EVENT_PEN_PROXIMITY_IN, false);
    SDL_SetEventEnabled(SDL_EVENT_PEN_PROXIMITY_OUT, false);
    SDL_SetEventEnabled(SDL_EVENT_PEN_DOWN, false);
    SDL_SetEventEnabled(SDL_EVENT_PEN_UP, false);
    SDL_SetEventEnabled(SDL_EVENT_PEN_BUTTON_DOWN, false);
    SDL_SetEventEnabled(SDL_EVENT_PEN_BUTTON_UP, false);
    SDL_SetEventEnabled(SDL_EVENT_PEN_AXIS, false);
    // we don't actually have to handle it in the event handler ourselves, but SDL_GetMouseState does not include pen motion events if it's disabled
    // this is weird because SDL_EVENT_MOUSE_MOTION does not work this way... whatever
    SDL_SetEventEnabled(SDL_EVENT_PEN_MOTION, true);

    // allow callback to enable/disable too
    cv::pen_input.setCallback(
        [](float on) -> void { SDL_SetEventEnabled(SDL_EVENT_PEN_MOTION, !!static_cast<int>(on)); });

    // touch
    SDL_SetEventEnabled(SDL_EVENT_FINGER_DOWN, false);
    SDL_SetEventEnabled(SDL_EVENT_FINGER_UP, false);
    SDL_SetEventEnabled(SDL_EVENT_FINGER_MOTION, false);
    SDL_SetEventEnabled(SDL_EVENT_FINGER_CANCELED, false);
}

float SDLMain::queryDisplayHz() {
    // get the screen refresh rate, and set fps_max to that as default
    if constexpr(!Env::cfg(OS::WASM))  // not in WASM
    {
        const SDL_DisplayID display = SDL_GetDisplayForWindow(m_window);
        const SDL_DisplayMode *currentDisplayMode = display ? SDL_GetCurrentDisplayMode(display) : nullptr;

        if(currentDisplayMode && currentDisplayMode->refresh_rate > 0) {
            if((m_fDisplayHz > currentDisplayMode->refresh_rate + 0.01) ||
               (m_fDisplayHz < currentDisplayMode->refresh_rate - 0.01)) {
                debugLog("Got refresh rate {:.3f} Hz for display {:d}.\n", currentDisplayMode->refresh_rate, display);
            }
            const auto refreshRateSanityClamped = std::clamp<float>(currentDisplayMode->refresh_rate, 60.0f, 540.0f);
            return refreshRateSanityClamped;
        } else {
            static int once;
            if(!once++)
                debugLog("Couldn't SDL_GetCurrentDisplayMode(SDL display: {:d}): {:s}\n", display, SDL_GetError());
        }
    }
    // in wasm or if we couldn't get the refresh rate just return a sane value to use for "vsync"-related calculations
    return std::clamp<float>(cv::fps_max.getFloat(), 60.0f, 360.0f);
}

void SDLMain::setupLogging() {
    SDL_SetLogOutputFunction(
        [](void *, int category, SDL_LogPriority, const char *message) {
            const char *catStr = "???";
            switch(category) {
                case SDL_LOG_CATEGORY_APPLICATION:
                    catStr = "APP";
                    break;
                case SDL_LOG_CATEGORY_ERROR:
                    catStr = "ERR";
                    break;
                case SDL_LOG_CATEGORY_SYSTEM:
                    catStr = "SYS";
                    break;
                case SDL_LOG_CATEGORY_AUDIO:
                    catStr = "AUD";
                    break;
                case SDL_LOG_CATEGORY_VIDEO:
                    catStr = "VID";
                    break;
                case SDL_LOG_CATEGORY_RENDER:
                    catStr = "REN";
                    break;
                case SDL_LOG_CATEGORY_INPUT:
                    catStr = "INP";
                    break;
                case SDL_LOG_CATEGORY_CUSTOM:
                    catStr = "USR";
                    break;
                default:
                    break;
            }
            Logger::logRaw("SDL[{}]: {}\n", catStr, message);
        },
        nullptr);
}

#ifdef MCENGINE_PLATFORM_WINDOWS
#include "WinDebloatDefs.h"
#include <objbase.h>
#include "dynutils.h"
#endif

void SDLMain::doEarlyCmdlineOverrides() {
#if defined(MCENGINE_PLATFORM_WINDOWS) || (defined(_WIN32) && !defined(__linux__))
    using namespace dynutils;
    // disable IME text input if -noime (or if the feature won't be supported)
#ifdef MCENGINE_FEATURE_IMESUPPORT
    if(m_mArgMap.contains("-noime"))
#endif
    {
        auto *imm32_handle =
            reinterpret_cast<lib_obj *>(LoadLibraryEx(TEXT("imm32.dll"), NULL, LOAD_LIBRARY_SEARCH_SYSTEM32));
        if(imm32_handle) {
            auto disable_ime_func = load_func<BOOL WINAPI(DWORD)>(imm32_handle, "ImmDisableIME");
            if(disable_ime_func) disable_ime_func(-1);
            unload_lib(imm32_handle);
        }
    }
    // enable DPI awareness if not -nodpi
    if(!m_mArgMap.contains("-nodpi")) {
        auto *user32_handle = reinterpret_cast<lib_obj *>(GetModuleHandle(TEXT("user32.dll")));
        if(user32_handle) {
            auto spdpi_aware_func = load_func<BOOL WINAPI(VOID)>(user32_handle, "SetProcessDPIAware");
            if(spdpi_aware_func) {
                spdpi_aware_func();
            }
        }
    }
#else
    // nothing yet
    return;
#endif
}

void SDLMain::shutdown(SDL_AppResult result) {
    if(result == SDL_APP_FAILURE)  // force quit now
        return;
    else if(m_window)
        SDL_StopTextInput(m_window);

    Environment::shutdown();
}
