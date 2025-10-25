// Copyright (c) 2025, WH, All rights reserved.

#include "Thread.h"
#include "Logging.h"
#include "UString.h"
#include "Environment.h"

#include <SDL3/SDL_init.h>

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
decltype(SetThreadDescription) *pset_thread_desc{nullptr};
decltype(GetThreadDescription) *pget_thread_desc{nullptr};

thread_local char thread_name_buffer[256];

void try_load_funcs() {
    static dynutils::lib_obj *kernel32_handle{nullptr};
    static bool load_attempted{false};
    if(!load_attempted) {
        load_attempted = true;
        kernel32_handle = reinterpret_cast<dynutils::lib_obj *>(
            LoadLibraryEx(TEXT("kernel32.dll"), nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
        if(kernel32_handle) {
            pset_thread_desc = load_func<decltype(SetThreadDescription)>(kernel32_handle, "SetThreadDescription");
            pget_thread_desc = load_func<decltype(GetThreadDescription)>(kernel32_handle, "GetThreadDescription");
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
bool set_current_thread_name(const char *cname) {
    [[maybe_unused]] UString name{cname};
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
#elif defined(__linux__)
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

void set_current_thread_prio(bool high) {
    if (!SDL_SetCurrentThreadPriority(high ? SDL_THREAD_PRIORITY_HIGH : SDL_THREAD_PRIORITY_NORMAL)) {
        debugLog("couldn't set thread priority to {}", high ? "high" : "normal");
    }
#ifdef MCENGINE_PLATFORM_WINDOWS
    if (is_main_thread()) {
        if (!SetPriorityClass(GetCurrentProcess(), high ? HIGH_PRIORITY_CLASS : NORMAL_PRIORITY_CLASS)) {
            debugLog("couldn't set process priority class to {}: {:#x}", high ? "high" : "normal", GetLastError());
        }
    }
#endif
}

};  // namespace McThread
