#include "config.h"
#include "CrashHandler.h"

#ifndef MCENGINE_PLATFORM_WINDOWS  // does nothing on other platforms atm

namespace CrashHandler {
void init() {}
}  // namespace CrashHandler

#else

#include "BaseEnvironment.h"

#ifdef _MSC_VER
#pragma comment(lib, "dbghelp.lib")
#define RET_ADDR() _ReturnAddress()
#else
#define RET_ADDR() __builtin_return_address(0)
#endif

#include <processthreadsapi.h>
#include <winver.h>
#include <dbghelp.h>
#include <sysinfoapi.h>
#include <fileapi.h>
#include <handleapi.h>
#include <errhandlingapi.h>

#ifndef INVALID_HANDLE_VALUE
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR) - 1)
#endif

#include <sstream>
#include <iostream>
#include <csignal>

namespace CrashHandler {

namespace {

static LONG WINAPI unhandled_exception_handler(EXCEPTION_POINTERS* exceptionInfo) {
    // generate filename with timestamp
    SYSTEMTIME time;
    GetLocalTime(&time);

    std::wstringstream filename;
    filename << L"crash_" << time.wYear << L"-" << time.wMonth << L"-" << time.wDay << L"_" << time.wHour << L"-"
             << time.wMinute << L"-" << time.wSecond << L".dmp";

    HANDLE hFile =
        CreateFileW(filename.str().c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    if(hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION exceptionParam;
        exceptionParam.ThreadId = GetCurrentThreadId();
        exceptionParam.ExceptionPointers = exceptionInfo;
        exceptionParam.ClientPointers = FALSE;

        auto dumpType =
            static_cast<MINIDUMP_TYPE>(MiniDumpWithDataSegs | MiniDumpWithHandleData | MiniDumpWithThreadInfo);

        BOOL success = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, dumpType, &exceptionParam,
                                         nullptr, nullptr);
        (void)success;

        CloseHandle(hFile);
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

static EXCEPTION_POINTERS g_exceptionPointers;
static EXCEPTION_RECORD g_exceptionRecord;
static CONTEXT g_context;

static void abort_handler(int /*signal*/) {
    memset(&g_context, 0, sizeof(CONTEXT));
    memset(&g_exceptionRecord, 0, sizeof(EXCEPTION_RECORD));

    RtlCaptureContext(&g_context);

    g_exceptionRecord.ExceptionCode = EXCEPTION_BREAKPOINT;
    g_exceptionRecord.ExceptionAddress = RET_ADDR();

    g_exceptionPointers.ContextRecord = &g_context;
    g_exceptionPointers.ExceptionRecord = &g_exceptionRecord;

    unhandled_exception_handler(&g_exceptionPointers);

    _exit(3);
}

}  // namespace

void init() {
    SetUnhandledExceptionFilter(unhandled_exception_handler);

    // abort doesn't invoke an unhandled exception :)
    // use fubar_abort instead of std::abort
    signal(SIGABRT, abort_handler);
}

}  // namespace CrashHandler

#endif
