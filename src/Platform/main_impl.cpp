// Copyright (c) 2025, WH, All rights reserved.
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_process.h>
#include <SDL3/SDL_properties.h>

#define SDL_h_

#include "main_impl.h"

#include "Osu.h"  // TODO: remove needing this

#include "MakeDelegateWrapper.h"
#include "Engine.h"
#include "ConVar.h"
#include "FPSLimiter.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "Profiler.h"
#include "Logging.h"

SDLMain::SDLMain(const std::unordered_map<std::string, std::optional<std::string>> &argMap,
                 const std::vector<std::string> &argVec)
    : Environment(argMap, argVec) {
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
    if(winFocused()) setFgFPS();
}

void SDLMain::fps_max_background_callback(float newVal) {
    int newFps = static_cast<int>(newVal);
    if(newFps >= 0) m_iFpsMaxBG = newFps;
    if(!winFocused()) setBgFPS();
}

SDLMain::~SDLMain() {
    m_engine.reset();

    // clean up GL context
    if(m_context && (!m_bUsingDX11)) {
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

    this->getEnvInterop().setup_system_integrations();  // only does anything for windows atm

    // disable (filter) some SDL events we don't care about
    configureEvents();

    // initialize engine, now that all the setup is done
    m_engine = std::make_unique<Engine>();

    if(!m_engine || m_engine->isShuttingDown()) {
        return SDL_APP_FAILURE;
    }
    // if we got to this point, all relevant subsystems (input handling, graphics interface, etc.) have been initialized

    // load app
    m_engine->loadApp();

    // make window visible now, after we loaded the config and set the wanted window size & fullscreen state
    SDL_ShowWindow(m_window);
    SDL_RaiseWindow(m_window);

    syncWindow();

    updateWindowFlags();

    // clear spurious window minimize/unfocus events accumulated during startup
    SDL_PumpEvents();
    SDL_FlushEvent(SDL_EVENT_WINDOW_MINIMIZED);
    SDL_FlushEvent(SDL_EVENT_WINDOW_FOCUS_LOST);

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
    if(m_bEnvDebug) {
        static std::array<char, 512> logBuf{};
        size_t logsz =
            std::min(logBuf.size(), static_cast<size_t>(SDL_GetEventDescription(event, logBuf.data(), logBuf.size())));
        if(logsz > 0) {
            Logger::logRaw(fmt::format("[handleEvent] frame: {}; event: {}"_cf, m_engine->getFrameCount(),
                                       std::string_view{logBuf.data(), logsz}));
        }
    }

    switch(event->type) {
        case SDL_EVENT_QUIT: {
            SDL_Window *source = SDL_GetWindowFromEvent(event);
            if(source && source != m_window) break;
            if(m_bRunning) {
                m_bRunning = false;
                if(m_engine && !m_engine->isShuttingDown()) {
                    m_engine->shutdown();
                }
                if constexpr(Env::cfg(FEAT::MAINCB))
                    return SDL_APP_SUCCESS;
                else
                    SDL_AppQuit(this, SDL_APP_SUCCESS);
            }
        } break;

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
                        std::string logString =
                            fmt::format("DEBUG: got SDL drag-drop event {}, current dropped_data queue is ",
                                        static_cast<int>(event->drop.type));
                        for(const auto &d : m_vDroppedData) {
                            logString += fmt::format("{}", d);
                        }
                        logString += ".";
                        debugLog(logString);
                    }
                } break;
                case SDL_EVENT_DROP_POSITION:  // we don't really care
                default:
                    if(m_bEnvDebug)
                        debugLog("DEBUG: unhandled SDL drag-drop event {}", static_cast<int>(event->drop.type));
                    break;
            }
            break;

            // window events (i hate you msvc ffs)
            // clang-format off
        case SDL_EVENT_WINDOW_SHOWN:				 case SDL_EVENT_WINDOW_HIDDEN:			  case SDL_EVENT_WINDOW_EXPOSED:
        case SDL_EVENT_WINDOW_MOVED:				 case SDL_EVENT_WINDOW_RESIZED:			  case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_METAL_VIEW_RESIZED:	 case SDL_EVENT_WINDOW_MINIMIZED:		  case SDL_EVENT_WINDOW_MAXIMIZED:
        case SDL_EVENT_WINDOW_RESTORED:				 case SDL_EVENT_WINDOW_MOUSE_ENTER:		  case SDL_EVENT_WINDOW_MOUSE_LEAVE:
        case SDL_EVENT_WINDOW_FOCUS_GAINED:			 case SDL_EVENT_WINDOW_FOCUS_LOST:		  case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        case SDL_EVENT_WINDOW_HIT_TEST:				 case SDL_EVENT_WINDOW_ICCPROF_CHANGED:	  case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED: case SDL_EVENT_WINDOW_SAFE_AREA_CHANGED: case SDL_EVENT_WINDOW_OCCLUDED:
        case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:		 case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:  case SDL_EVENT_WINDOW_DESTROYED:
        case SDL_EVENT_WINDOW_HDR_STATE_CHANGED:
            // clang-format on
            updateWindowFlags();  // update our window flags enum from current SDL window flags
            switch(event->window.type) {
                case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
                    SDL_Window *source = SDL_GetWindowFromEvent(event);
                    if(source && source != m_window) break;
                    if(m_bRunning) {
                        m_engine->shutdown();
                    }
                } break;

                case SDL_EVENT_WINDOW_FOCUS_GAINED:
                    // add these window flags now to make env->winFocused() return true after this
                    m_winflags |= (WinFlags::F_MOUSE_FOCUS | WinFlags::F_INPUT_FOCUS);
                    if(!winMinimized() && m_bRestoreFullscreen) {
                        // we can get into this state if the current window manager doesn't support minimizing
                        // (i.e. re-gaining focus without first being restored, after we unfullscreened and tried to minimize the window)
                        // re-fullscreen once, then set a flag to ignore future minimize requests
                        m_bRestoreFullscreen = false;
                        m_bMinimizeSupported = false;
                        SDL_SetWindowFullscreen(m_window, true);
                    }
                    m_engine->onFocusGained();
                    setFgFPS();
                    break;

                case SDL_EVENT_WINDOW_FOCUS_LOST:
                    // remove these window flags now to avoid env->winFocused() returning true immediately
                    m_winflags &= ~(WinFlags::F_MOUSE_FOCUS | WinFlags::F_INPUT_FOCUS);
                    m_engine->onFocusLost();
                    setBgFPS();
                    break;

                case SDL_EVENT_WINDOW_MAXIMIZED:
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
                    m_engine->onMinimized();
                    setBgFPS();
                    break;

                case SDL_EVENT_WINDOW_RESTORED:
                    if(m_bRestoreFullscreen) {
                        m_bRestoreFullscreen = false;
                        SDL_SetWindowFullscreen(m_window, true);
                    }
                    m_engine->onRestored();
                    setFgFPS();
                    break;

                case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
                    cv::fullscreen.setValue(true, false);
                    m_bRestoreFullscreen = false;
                    break;

                case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
                    cv::fullscreen.setValue(false, false);
                    if(!m_bRestoreFullscreen) {  // make sure we re-add window borders, unless we're in the minimize-on-focus-lost-hack-state
                        SDL_SetWindowBordered(m_window, true);
                    }
                    break;

                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                case SDL_EVENT_WINDOW_RESIZED:
                    // don't trust the event coordinates if we're in fullscreen, use the fullscreen size directly
                    if(!winMinimized() && !m_bRestoreFullscreen) {
                        vec2 resize = winFullscreened() ? getNativeScreenSize()
                                                        : vec2{(float)event->window.data1, (float)event->window.data2};
                        m_engine->requestResolutionChange(resize);
                        setFgFPS();
                    }
                    break;

                case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
                    cv::monitor.setValue(event->window.data1, false);
                    m_engine->requestResolutionChange(getWindowSize());
                    m_fDisplayHzSecs = 1.0f / (m_fDisplayHz = queryDisplayHz());
                    break;

                case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
                    m_engine->onDPIChange();
                    break;

                default:
                    if(m_bEnvDebug)
                        debugLog("DEBUG: unhandled SDL window event {}", static_cast<int>(event->window.type));
                    break;
            }
            break;

            // display events
            // clang-format off
        case SDL_EVENT_DISPLAY_ORIENTATION:			  case SDL_EVENT_DISPLAY_ADDED:				   case SDL_EVENT_DISPLAY_REMOVED:
        case SDL_EVENT_DISPLAY_MOVED:				  case SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED: case SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED:
        case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED:
            // clang-format on
            updateWindowFlags();
            switch(event->display.type) {
                case SDL_EVENT_DISPLAY_CONTENT_SCALE_CHANGED:
                    m_engine->onDPIChange();
                    // fallthrough
                default:
                    // reinit monitors, and update hz in any case
                    initMonitors(true);
                    m_fDisplayHzSecs = 1.0f / (m_fDisplayHz = queryDisplayHz());
                    break;
            }
            break;

        // keyboard events
        case SDL_EVENT_KEY_DOWN:
            keyboard->onKeyDown({static_cast<KEYCODE>(event->key.scancode), event->key.timestamp, event->key.repeat});
            break;

        case SDL_EVENT_KEY_UP:
            keyboard->onKeyUp({static_cast<KEYCODE>(event->key.scancode), event->key.timestamp, event->key.repeat});
            break;

        case SDL_EVENT_TEXT_INPUT:
            for(const auto &chr : UString(event->text.text)) keyboard->onChar({chr, event->text.timestamp, false});
            break;

        // mouse events
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            mouse->onButtonChange({event->button.timestamp, (MouseButtonFlags)(1 << (event->button.button - 1)), true});
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            mouse->onButtonChange(
                {event->button.timestamp, (MouseButtonFlags)(1 << (event->button.button - 1)), false});
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
            if(m_bEnvDebug) debugLog("DEBUG: unhandled SDL event {}", event->type);
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
    if(!winMinimized() && !m_bRestoreFullscreen) {
        m_engine->onPaint();
    }

    if constexpr(!Env::cfg(FEAT::MAINCB))  // main callbacks use SDL iteration rate to limit fps
    {
        VPROF_BUDGET("FPSLimiter", VPROF_BUDGETGROUP_SLEEP);

        // if minimized or unfocused, use BG fps, otherwise use fps_max (if 0 it's unlimited)
        const int targetFPS = (winMinimized() || !winFocused())
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
    if(!m_bUsingDX11) {  // these are only for opengl
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                            Env::cfg(REND::GL) ? SDL_GL_CONTEXT_PROFILE_COMPATIBILITY : SDL_GL_CONTEXT_PROFILE_ES);
        if constexpr(!Env::cfg(REND::GL)) {  // OpenGL ES
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
    const i64 windowFlags =
        SDL_WINDOW_HIDDEN | SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_MOUSE_FOCUS |
        (!m_bUsingDX11 ? SDL_WINDOW_OPENGL : (Env::cfg(OS::LINUX, REND::DX11) ? SDL_WINDOW_VULKAN : 0LL));

    // limit default window size so it fits the screen
    i32 windowCreateWidth = WINDOW_WIDTH;
    i32 windowCreateHeight = WINDOW_HEIGHT;
    SDL_DisplayID initDisplayID = SDL_GetPrimaryDisplay();

    // start on the highest refresh rate monitor for kmsdrm
    if(m_bIsKMSDRM) {
        int dispCount = 0;
        float maxHz = 0;
        SDL_DisplayID *ids = SDL_GetDisplays(&dispCount);
        for(int i = 0; i < dispCount; i++) {
            const SDL_DisplayMode *currentDisplayMode = SDL_GetCurrentDisplayMode(ids[i]);
            if(currentDisplayMode && currentDisplayMode->refresh_rate >= maxHz) {
                maxHz = currentDisplayMode->refresh_rate;
                initDisplayID = currentDisplayMode->displayID;
                windowCreateWidth = currentDisplayMode->w;
                windowCreateHeight = currentDisplayMode->h;
            }
        }
        SDL_free(ids);
    } else {
        if(!initDisplayID) {
            debugLog("NOTICE: Couldn't get primary display: {}", SDL_GetError());
        } else {
            const SDL_DisplayMode *dm = SDL_GetDesktopDisplayMode(initDisplayID);
            if(dm) {
                if(dm->w < windowCreateWidth) windowCreateWidth = dm->w;
                if(dm->h < windowCreateHeight) windowCreateHeight = dm->h;
            }
        }
    }

    // set this size as the initial fallback window size (for Environment::getWindowSize())
    m_vLastKnownWindowSize = vec2{static_cast<float>(windowCreateWidth), static_cast<float>(windowCreateHeight)};

    SDL_PropertiesID props = SDL_CreateProperties();
    if(m_bUsingDX11) SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_EXTERNAL_GRAPHICS_CONTEXT_BOOLEAN, true);
    SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, WINDOW_TITLE);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY(initDisplayID));
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED_DISPLAY(initDisplayID));
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, windowCreateWidth);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, windowCreateHeight);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_MAXIMIZED_BOOLEAN, false);
    SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, m_bIsKMSDRM ? true : false);
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
        debugLog("Couldn't SDL_CreateWindow(): {:s}", SDL_GetError());
        return false;
    }

    m_windowID = SDL_GetWindowID(m_window);

    if(m_bIsKMSDRM) {
        cv::monitor.setValue(initDisplayID, false);
    } else {
        cv::monitor.setValue(SDL_GetDisplayForWindow(m_window), false);
    }

    // create gl context
    if(!m_bUsingDX11) {
        m_context = SDL_GL_CreateContext(m_window);
        if(!m_context) {
            debugLog("Couldn't create OpenGL context: {:s}", SDL_GetError());
            return false;
        }
        if(!SDL_GL_MakeCurrent(m_window, m_context)) {
            debugLog("Couldn't make OpenGL context current: {:s}", SDL_GetError());
            return false;
        }
    }

    if(m_bIsKMSDRM) {
        SDL_SetWindowMinimumSize(m_window, windowCreateWidth, windowCreateHeight);
    } else {
        SDL_SetWindowMinimumSize(m_window, WINDOW_WIDTH_MIN, WINDOW_HEIGHT_MIN);
    }

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

    // initialize window flags
    updateWindowFlags();

    return true;
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
                debugLog("Got refresh rate {:.3f} Hz for display {:d}.", currentDisplayMode->refresh_rate, display);
            }
            const auto refreshRateSanityClamped = std::clamp<float>(currentDisplayMode->refresh_rate, 60.0f, 540.0f);
            return refreshRateSanityClamped;
        } else {
            static int once;
            if(!once++)
                debugLog("Couldn't SDL_GetCurrentDisplayMode(SDL display: {:d}): {:s}", display, SDL_GetError());
        }
    }
    // in wasm or if we couldn't get the refresh rate just return a sane value to use for "vsync"-related calculations
    return std::clamp<float>(cv::fps_max.getFloat(), 60.0f, 360.0f);
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
            Logger::logRaw("SDL[{}]: {}", catStr, message);
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
        auto *imm32_handle = load_lib_system("imm32.dll");
        if(imm32_handle) {
            auto disable_ime_func = load_func<BOOL WINAPI(DWORD)>(imm32_handle, "ImmDisableIME");
            if(disable_ime_func) disable_ime_func(-1);
            unload_lib(imm32_handle);
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
