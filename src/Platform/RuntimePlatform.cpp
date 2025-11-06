// Copyright (c) 2025, WH, All rights reserved.

#include "RuntimePlatform.h"
#include "BaseEnvironment.h"

#ifdef MCENGINE_PLATFORM_WINDOWS
#include "WinDebloatDefs.h"
#include <windows.h>

#include <array>
#endif

#include <utility>  // std::unreachable

namespace RuntimePlatform {

namespace {
static unsigned short cur_ver{Env::cfg(OS::WINDOWS) ? 0
                              : Env::cfg(OS::LINUX) ? LINUX
                              : Env::cfg(OS::WASM)  ? WASM
                              : Env::cfg(OS::MAC)   ? MACOS
                                                    : 0};
}

#ifndef MCENGINE_PLATFORM_WINDOWS
VERSION current() { return (VERSION)cur_ver; }
#else

namespace {

#define WIN11_VER 0x0A01  // there is no such thing... need to use the build number

static BOOL winver_atleast(DWORD version) {
    const WORD major = HIBYTE(version);
    const WORD minor = (version == WIN11_VER ? 0 : LOBYTE(version));
    const DWORD build = (version == WIN11_VER ? 21996 : 0);

    ULONGLONG conditions = 0;
    conditions = VerSetConditionMask(0, VER_MAJORVERSION, VER_GREATER_EQUAL);
    conditions = VerSetConditionMask(conditions, VER_MINORVERSION, VER_GREATER_EQUAL);
    conditions = VerSetConditionMask(conditions, VER_BUILDNUMBER, VER_GREATER_EQUAL);

    OSVERSIONINFOEX osvi{};

    osvi.dwOSVersionInfoSize = sizeof(osvi);
    osvi.dwMajorVersion = major;
    osvi.dwMinorVersion = minor;
    osvi.dwBuildNumber = build;

    return VerifyVersionInfo(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER, conditions) != FALSE;
}

}  // namespace

VERSION current() {
    if(cur_ver > 0) return (VERSION)cur_ver;

    cur_ver |= WIN;  // always initialize to windows
    if(!!GetProcAddress(GetModuleHandle(TEXT("ntdll.dll")), "wine_get_version")) {
        cur_ver |= WIN_WINE;
    }

    // break out after we find a version, don't add all lower versions
    bool found = false;
    for(auto check_ver : std::array<std::pair<DWORD, VERSION>, 6>{{{WIN11_VER, WIN_11},
                                                                   {_WIN32_WINNT_WIN10, WIN_10},
                                                                   {_WIN32_WINNT_WIN8, WIN_8},
                                                                   {_WIN32_WINNT_WIN7, WIN_7},
                                                                   {_WIN32_WINNT_VISTA, WIN_VISTA},
                                                                   {_WIN32_WINNT_WINXP, WIN_XP}}}) {
        if(winver_atleast(check_ver.first)) {
            found = true;
            cur_ver |= check_ver.second;
            break;
        }
    }

    if(!found) {
        cur_ver |= WIN_UNKNOWN;
    }

    return (VERSION)cur_ver;
}
#endif

#ifdef MCENGINE_PLATFORM_WINDOWS
#ifdef MC_ARCH64
#define ARCHSTR "(x64)"
#elif defined(MC_ARCH32)
#define ARCHSTR "(x86)"
#else
#define ARCHSTR "(?)"
#endif
#else
#ifdef MC_ARCH64
#define ARCHSTR "(x86-64)"
#elif defined(MC_ARCH32)
#define ARCHSTR "(i686)"
#else
#define ARCHSTR "(?)"
#endif
#endif

const char* current_string() {
    static const char* ver_str{nullptr};
    if(ver_str) return ver_str;

    const auto cv = current();
    if(cv & LINUX) {
        ver_str = "Linux " ARCHSTR;
        return ver_str;
    }
    if(cv & WASM) {
        ver_str = "WASM " ARCHSTR;
        return ver_str;
    }
    if(cv & MACOS) {
        ver_str = "macOS " ARCHSTR;
        return ver_str;
    }

    // windows section
    if(cv & WIN_UNKNOWN) {
        ver_str = (cv & WIN_WINE) ? "(WINE) Windows (version unknown) " ARCHSTR : "Windows (version unknown) " ARCHSTR;
        return ver_str;
    }
    if(cv & WIN_XP) {
        ver_str = (cv & WIN_WINE) ? "(WINE) Windows XP " ARCHSTR : "Windows XP " ARCHSTR;
        return ver_str;
    }
    if(cv & WIN_VISTA) {
        ver_str = (cv & WIN_WINE) ? "(WINE) Windows Vista " ARCHSTR : "Windows Vista " ARCHSTR;
        return ver_str;
    }
    if(cv & WIN_7) {
        ver_str = (cv & WIN_WINE) ? "(WINE) Windows 7 " ARCHSTR : "Windows 7 " ARCHSTR;
        return ver_str;
    }
    if(cv & WIN_8) {
        ver_str = (cv & WIN_WINE) ? "(WINE) Windows 8 " ARCHSTR : "Windows 8 " ARCHSTR;
        return ver_str;
    }
    if(cv & WIN_10) {
        ver_str = (cv & WIN_WINE) ? "(WINE) Windows 10 " ARCHSTR : "Windows 10 " ARCHSTR;
        return ver_str;
    }
    if(cv & WIN_11) {
        ver_str = (cv & WIN_WINE) ? "(WINE) Windows 11 " ARCHSTR : "Windows 11 " ARCHSTR;
        return ver_str;
    }
    std::unreachable();
}
}  // namespace RuntimePlatform