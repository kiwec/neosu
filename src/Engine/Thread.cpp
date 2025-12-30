// Copyright (c) 2025, WH, All rights reserved.

#include "Thread.h"
#include "Logging.h"
#include "UString.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_cpuinfo.h>

#if defined(_WIN32)
#include "WinDebloatDefs.h"
#include <winbase.h>
#include <processthreadsapi.h>
#ifndef SUCCEEDED
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#endif
#include <libloaderapi.h>
#include "dynutils.h"

namespace {
using SetThreadDescription_t = HRESULT WINAPI(HANDLE hThread, PCWSTR lpThreadDescription);
using GetThreadDescription_t = HRESULT WINAPI(HANDLE hThread, PWSTR *ppszThreadDescription);
SetThreadDescription_t *pset_thread_desc{nullptr};
GetThreadDescription_t *pget_thread_desc{nullptr};

thread_local char thread_name_buffer[256];

void try_load_funcs() {
    static dynutils::lib_obj *kernel32_handle{nullptr};
    static bool load_attempted{false};
    if(!load_attempted) {
        load_attempted = true;
        kernel32_handle = reinterpret_cast<dynutils::lib_obj *>(GetModuleHandle(TEXT("kernel32.dll")));
        if(kernel32_handle) {
            pset_thread_desc = load_func<SetThreadDescription_t>(kernel32_handle, "SetThreadDescription");
            pget_thread_desc = load_func<GetThreadDescription_t>(kernel32_handle, "GetThreadDescription");
        }
    }
}
}  // namespace

#else
#include <pthread.h>
namespace {
#if defined(__linux__)
thread_local char thread_name_buffer[16];
#elif defined(__FreeBSD__)
thread_local char thread_name_buffer[256];
#else
thread_local char thread_name_buffer[256];
#endif
}  // namespace
#endif

namespace McThread {
// WARNING: must be called from within the thread itself! otherwise, the main process name will be changed
bool set_current_thread_name(const UString &name) {
#if defined(_WIN32)
    try_load_funcs();
    if(pset_thread_desc) {
        HANDLE handle = GetCurrentThread();
        HRESULT hr = pset_thread_desc(handle, name.wchar_str());
        return SUCCEEDED(hr);
    }
#elif defined(__linux__)
    auto truncated_name = name.substr<std::string>(0, 15);
    return pthread_setname_np(pthread_self(), truncated_name.c_str()) == 0;
#elif defined(__APPLE__)
    return pthread_setname_np(name.toUtf8()) == 0;
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    pthread_set_name_np(pthread_self(), name.toUtf8());
    return true;
#endif
    return false;
}

const char *get_current_thread_name() {
#if defined(_WIN32)
    try_load_funcs();
    if(pget_thread_desc) {
        HANDLE handle = GetCurrentThread();
        PWSTR thread_desc;
        HRESULT hr = pget_thread_desc(handle, &thread_desc);
        if(SUCCEEDED(hr) && thread_desc) {
            UString name{thread_desc};
            LocalFree(thread_desc);
            auto utf8_name = name.toUtf8();
            strncpy_s(thread_name_buffer, sizeof(thread_name_buffer), utf8_name, _TRUNCATE);
            return thread_name_buffer;
        }
    }
#elif defined(__linux__) || defined(__APPLE__)
    if(pthread_getname_np(pthread_self(), thread_name_buffer, sizeof(thread_name_buffer)) == 0) {
        return thread_name_buffer[0] ? thread_name_buffer : PACKAGE_NAME;
    }
#elif defined(__FreeBSD__)
    pthread_get_name_np(pthread_self(), thread_name_buffer, sizeof(thread_name_buffer));
    return thread_name_buffer[0] ? thread_name_buffer : PACKAGE_NAME;
#endif
    return PACKAGE_NAME;
}

bool is_main_thread() { return SDL_IsMainThread(); }

int get_logical_cpu_count() { return SDL_GetNumLogicalCPUCores(); }

void set_current_thread_prio(Priority prio) {
    SDL_ThreadPriority sdlprio;
    const char *priostring;
    if(prio < NORMAL || prio > REALTIME) prio = NORMAL;  // sanity
    switch(prio) {
        case NORMAL:
            sdlprio = SDL_THREAD_PRIORITY_NORMAL;
            priostring = "normal";
            break;
        case HIGH:
            sdlprio = SDL_THREAD_PRIORITY_HIGH;
            priostring = "high";
            break;
        case LOW:
            sdlprio = SDL_THREAD_PRIORITY_LOW;
            priostring = "low";
            break;
        case REALTIME:
            sdlprio = SDL_THREAD_PRIORITY_TIME_CRITICAL;
            priostring = "realtime";
            break;
    }
    if(!SDL_SetCurrentThreadPriority(sdlprio)) {
        debugLog("couldn't set thread priority to {}: {}", priostring, SDL_GetError());
    }
#ifdef MCENGINE_PLATFORM_WINDOWS
    static int logcpus = get_logical_cpu_count();
    // tested in a windows vm, this causes things to just behave WAY worse than if you leave it alone with low core counts
    if(logcpus > 4 && is_main_thread()) {
        // only allow setting normal/high for process priority class
        if(!SetPriorityClass(GetCurrentProcess(),
                             (prio == REALTIME || prio == HIGH) ? HIGH_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS)) {
            debugLog("couldn't set process priority class to {}: {}", priostring, GetLastError());
        }
    }
#endif
}

};  // namespace McThread
