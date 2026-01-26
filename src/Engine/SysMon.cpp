/*
 * Original author: David Robert Nadeau
 * Site:            http://NadeauSoftware.com/
 * License:         Creative Commons Attribution 3.0 Unported License
 *                  http://creativecommons.org/licenses/by/3.0/deed.en_US
 *
 * Sourced from: https://github.com/lemire/Code-used-on-Daniel-Lemire-s-blog/blob/master/2022/11/10/nadeau.h
 *
 * Extended with additional memory/CPU metrics.
 */

#include "config.h"

#include "SysMon.h"

#if defined(MCENGINE_PLATFORM_WINDOWS)
#include "WinDebloatDefs.h"
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include "Timing.h"
#include "dynutils.h"

namespace {

BOOL get_process_memory_info(HANDLE proc, PPROCESS_MEMORY_COUNTERS counters, DWORD cb) {
    using GetProcessMemoryInfo_t = BOOL WINAPI(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
    static GetProcessMemoryInfo_t* pGetProcessMemoryInfo{nullptr};
    static bool triedToLoad = false;
    if(!triedToLoad) {
        triedToLoad = true;
        dynutils::lib_obj* kernel32Handle = reinterpret_cast<dynutils::lib_obj*>(GetModuleHandle(TEXT("kernel32.dll")));
        if(kernel32Handle) {
            pGetProcessMemoryInfo = dynutils::load_func<GetProcessMemoryInfo_t>(kernel32Handle, "GetProcessMemoryInfo");
        }
        if(!pGetProcessMemoryInfo) {
            dynutils::lib_obj* psapiHandle = dynutils::load_lib_system("psapi.dll");
            if(psapiHandle) {
                pGetProcessMemoryInfo =
                    dynutils::load_func<GetProcessMemoryInfo_t>(psapiHandle, "GetProcessMemoryInfo");
                if(!pGetProcessMemoryInfo) {
                    dynutils::unload_lib(psapiHandle);
                }
            }
        }
    }

    if(pGetProcessMemoryInfo) {
        return pGetProcessMemoryInfo(proc, counters, cb);
    }

    return false;
}

// Enumerates virtual memory regions to calculate total committed size and shared/private breakdown.
// VirtualQuery is the only reliable way to get actual virtual memory stats on Windows.
struct VirtualMemoryStats {
    size_t totalCommitted;
    size_t privateBytes;
    size_t sharedBytes;  // MEM_MAPPED + MEM_IMAGE
};

inline VirtualMemoryStats enumerateVirtualMemory() {
    // This is quite expensive, so only update it every once in a while (5 seconds).
    static uint64_t lastQueryMS = 0;
    static VirtualMemoryStats stats = {0, 0, 0};
    if(const uint64_t now = Timing::getTicksMS(); lastQueryMS == 0 || (lastQueryMS + 5000 < now)) {
        stats = {};

        lastQueryMS = now;

        MEMORY_BASIC_INFORMATION mbi;
        const char* addr = nullptr;

        while(VirtualQuery(addr, &mbi, sizeof(mbi)) != 0) {
            if(mbi.State == MEM_COMMIT) {
                stats.totalCommitted += mbi.RegionSize;
                if(mbi.Type == MEM_PRIVATE) {
                    stats.privateBytes += mbi.RegionSize;
                } else {
                    // MEM_MAPPED (memory-mapped files) and MEM_IMAGE (DLLs/EXEs) are shareable
                    stats.sharedBytes += mbi.RegionSize;
                }
            }
            addr += mbi.RegionSize;

            // Prevent infinite loop on address space wrap-around
            if(reinterpret_cast<uintptr_t>(addr) < reinterpret_cast<uintptr_t>(mbi.BaseAddress)) break;
        }
    }

    return stats;
}
}  // namespace

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
#include <sys/resource.h>
#include <unistd.h>
#include <ctime>

#if defined(__APPLE__) && defined(__MACH__)
#include <mach/mach.h>
#include <sys/sysctl.h>

#elif (defined(_AIX) || defined(__TOS__AIX__)) || \
    (defined(__sun__) || defined(__sun) || defined(sun) && (defined(__SVR4) || defined(__svr4__)))
#include <fcntl.h>
#include <procfs.h>

#elif defined(MCENGINE_PLATFORM_LINUX)
#include <fcntl.h>
#include <cstdio>
#include <cstring>

#endif

#else
#define SYSMON_UNSUPPORTED
#endif

#ifndef SYSMON_UNSUPPORTED
#include "Parsing.h"
#endif

namespace SysMon {
#ifdef SYSMON_UNSUPPORTED
// This stuff isn't mission-critical, so just define these as doing nothing if it's not supported (I'd imagine WASM isn't).
size_t getPeakRSS() { return 0; }
size_t getCurrentRSS() { return 0; }
size_t getVirtualSize() { return 0; }
size_t getTotalPhysicalMemory() { return 0; }
size_t getAvailablePhysicalMemory() { return 0; }
size_t getTotalVirtualAddressSpace() { return 0; }
uint64_t getPageFaultCount() { return 0; }
size_t getSharedMemory() { return 0; }
void getMemoryInfo(MemoryInfo& info) { (void)info; }
double getUserCPUTime() { return 0.0; }
double getKernelCPUTime() { return 0.0; }
double getCPUUsagePercent() { return 0.0; }
uint32_t getThreadCount() { return 0; }
void getCpuInfo(CpuInfo& info) { (void)info; }
#else

#ifdef MCENGINE_PLATFORM_LINUX
namespace {

// Just so we can leave the file open persistently and do pread().
// Doing this to avoid doing 3 syscalls per query.
class ProcReader {
   public:
    NOCOPY_NOMOVE(ProcReader)
   protected:
    ProcReader(const char* procPath) : m_fd(open(procPath, O_RDONLY)) {}

   public:
    virtual ~ProcReader() {
        if(m_fd >= 0) close(m_fd);
    }

    [[nodiscard]] inline bool isOpen() const { return m_fd >= 0; }

   protected:
    int m_fd;
};

class ProcStatmReader : public ProcReader {
   public:
    NOCOPY_NOMOVE(ProcStatmReader)
   public:
    ProcStatmReader() : ProcReader("/proc/self/statm") {}
    ~ProcStatmReader() override = default;

    // Returns size, resident, shared, text, lib, data, dt (all in pages)
    [[nodiscard]] inline bool read(i64& size, i64& resident, i64& shared, i64& text, i64& data) const {
        if(m_fd < 0) return false;

        char buf[128];
        ssize_t n = pread(m_fd, buf, sizeof(buf) - 1, 0);
        if(n <= 0) return false;

        buf[n] = '\0';
        i64 lib, dt;
        using Parsing::SPC;
        if(!Parsing::parse((const char*)buf, &size, SPC, &resident, SPC, &shared, SPC, &text, SPC, &lib, SPC, &data,
                           SPC, &dt)) {
            return false;
        }
        return true;
    }
};

// Reader for /proc/self/stat (CPU times, thread count, etc.)
class ProcStatReader : public ProcReader {
   public:
    NOCOPY_NOMOVE(ProcStatReader)
   public:
    ProcStatReader() : ProcReader("/proc/self/stat") {
        m_clkTck = sysconf(_SC_CLK_TCK);
        if(m_clkTck <= 0) m_clkTck = 100;  // fallback
    }
    ~ProcStatReader() override = default;

    struct StatData {
        double userTime;    // seconds
        double kernelTime;  // seconds
        uint32_t threadCount;
    };

    [[nodiscard]] inline bool read(StatData& data) const {
        if(m_fd < 0) return false;

        char buf[512];
        ssize_t n = pread(m_fd, buf, sizeof(buf) - 1, 0);
        if(n <= 0) return false;
        buf[n] = '\0';

        // Format: pid (comm) state ppid pgrp session tty_nr tpgid flags
        //         minflt cminflt majflt cmajflt utime stime cutime cstime
        //         priority nice num_threads ...
        // Fields 14 and 15 are utime and stime (in clock ticks)
        // Field 20 is num_threads

        // Skip past the comm field (may contain spaces/parens)
        const char* p = strchr(buf, ')');
        if(!p) return false;
        p += 2;  // Skip ") "

        // state(1) ppid(2) pgrp(3) session(4) tty_nr(5) tpgid(6) flags(7)
        // minflt(8) cminflt(9) majflt(10) cmajflt(11) utime(12) stime(13)
        // cutime(14) cstime(15) priority(16) nice(17) num_threads(18)
        i64 utime, stime, num_threads;
        using Parsing::skip;
        using Parsing::SPC;
        const bool read = Parsing::parse(p,  //
                                         // state through flags //
                                         skip<char>, SPC, skip<i32>, SPC, skip<i32>, SPC, skip<i32>, SPC, skip<i32>,
                                         SPC, skip<i32>, SPC, skip<u32>, SPC,  //
                                         // minflt through cmajflt //
                                         skip<u64>, SPC, skip<u64>, SPC, skip<u64>, SPC, skip<u64>, SPC,  //
                                         &utime, SPC, &stime, SPC,                                        //
                                         // cutime, cstime, priority, nice //
                                         skip<i64>, SPC, skip<i64>, SPC, skip<i64>, SPC, skip<i64>, SPC,  //
                                         &num_threads);                                                   //
        if(!read) {
            return false;
        }

        data.userTime = (double)utime / m_clkTck;
        data.kernelTime = (double)stime / m_clkTck;
        data.threadCount = (uint32_t)num_threads;
        return true;
    }

   private:
    i64 m_clkTck;
};

// Reader for /proc/self/status (context switches)
class ProcStatusReader : public ProcReader {
   public:
    NOCOPY_NOMOVE(ProcStatusReader)
   public:
    ProcStatusReader() : ProcReader("/proc/self/status") {}
    ~ProcStatusReader() override = default;

    [[nodiscard]] inline bool readContextSwitches(uint64_t& voluntary, uint64_t& involuntary) const {
        if(m_fd < 0) return false;

        char buf[2048];
        ssize_t n = pread(m_fd, buf, sizeof(buf) - 1, 0);
        if(n <= 0) return false;
        buf[n] = '\0';

        voluntary = 0;
        involuntary = 0;

        // Look for voluntary_ctxt_switches and nonvoluntary_ctxt_switches
        const char* vol = strstr(buf, "voluntary_ctxt_switches:");
        if(vol) {
            vol += 24;  // Skip label
            while(*vol == ' ' || *vol == '\t') vol++;
            voluntary = strtoull(vol, nullptr, 10);
        }

        const char* invol = strstr(buf, "nonvoluntary_ctxt_switches:");
        if(invol) {
            invol += 27;
            while(*invol == ' ' || *invol == '\t') invol++;
            involuntary = strtoull(invol, nullptr, 10);
        }

        return vol != nullptr || invol != nullptr;
    }
};

inline ProcStatmReader& getStatmReader() {
    static ProcStatmReader reader;
    return reader;
}

inline ProcStatReader& getStatReader() {
    static ProcStatReader reader;
    return reader;
}

inline ProcStatusReader& getStatusReader() {
    static ProcStatusReader reader;
    return reader;
}
}  // namespace

#endif  // __linux__

/**
 * Returns the peak (maximum so far) resident set size (physical memory use) in bytes.
 */
size_t getPeakRSS() {
#if defined(MCENGINE_PLATFORM_WINDOWS)
    PROCESS_MEMORY_COUNTERS info;
    get_process_memory_info(GetCurrentProcess(), &info, sizeof(info));
    return (size_t)info.PeakWorkingSetSize;

#elif (defined(_AIX) || defined(__TOS__AIX__)) || \
    (defined(__sun__) || defined(__sun) || defined(sun) && (defined(__SVR4) || defined(__svr4__)))
    struct psinfo psinfo;
    int fd = -1;
    if((fd = open("/proc/self/psinfo", O_RDONLY)) == -1) return (size_t)0L;
    if(read(fd, &psinfo, sizeof(psinfo)) != sizeof(psinfo)) {
        close(fd);
        return (size_t)0L;
    }
    close(fd);
    return (size_t)(psinfo.pr_rssize * 1024L);

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
    struct rusage rusage;
    getrusage(RUSAGE_SELF, &rusage);
#if defined(__APPLE__) && defined(__MACH__)
    return (size_t)rusage.ru_maxrss;
#else
    return (size_t)(rusage.ru_maxrss * 1024L);
#endif

#else
    return (size_t)0L;
#endif
}

/**
 * Returns the current resident set size (physical memory use) in bytes.
 */
size_t getCurrentRSS() {
#if defined(MCENGINE_PLATFORM_WINDOWS)
    PROCESS_MEMORY_COUNTERS info;
    get_process_memory_info(GetCurrentProcess(), &info, sizeof(info));
    return (size_t)info.WorkingSetSize;

#elif defined(__APPLE__) && defined(__MACH__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if(task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &infoCount) != KERN_SUCCESS)
        return (size_t)0L;
    return (size_t)info.resident_size;

#elif defined(MCENGINE_PLATFORM_LINUX)
    // Single syscall via persistent fd + pread
    i64 size, resident, shared, text, data;
    if(!getStatmReader().read(size, resident, shared, text, data)) {
        return (size_t)0L;
    }
    return (size_t)resident * (size_t)sysconf(_SC_PAGESIZE);

#else
    return (size_t)0L;
#endif
}

/**
 * Returns the current virtual memory size in bytes.
 */
size_t getVirtualSize() {
#if defined(MCENGINE_PLATFORM_WINDOWS)
    return enumerateVirtualMemory().totalCommitted;

#elif defined(__APPLE__) && defined(__MACH__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if(task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &infoCount) != KERN_SUCCESS)
        return (size_t)0L;
    return (size_t)info.virtual_size;

#elif defined(MCENGINE_PLATFORM_LINUX)
    i64 size, resident, shared, text, data;
    if(!getStatmReader().read(size, resident, shared, text, data)) {
        return (size_t)0L;
    }
    return (size_t)size * (size_t)sysconf(_SC_PAGESIZE);

#else
    return (size_t)0L;
#endif
}

/**
 * Returns the total physical memory in the system in bytes.
 */
size_t getTotalPhysicalMemory() {
#if defined(MCENGINE_PLATFORM_WINDOWS)
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if(GlobalMemoryStatusEx(&memInfo)) return (size_t)memInfo.ullTotalPhys;
    return (size_t)0L;

#elif defined(__APPLE__) && defined(__MACH__)
    int mib[2] = {CTL_HW, HW_MEMSIZE};
    int64_t memsize = 0;
    size_t len = sizeof(memsize);
    if(sysctl(mib, 2, &memsize, &len, nullptr, 0) == 0) return (size_t)memsize;
    return (size_t)0L;

#elif defined(MCENGINE_PLATFORM_LINUX)
    i64 pages = sysconf(_SC_PHYS_PAGES);
    i64 page_size = sysconf(_SC_PAGESIZE);
    if(pages > 0 && page_size > 0) return (size_t)pages * (size_t)page_size;
    return (size_t)0L;

#else
    return (size_t)0L;
#endif
}

/**
 * Returns available (free + reclaimable) physical memory in bytes.
 */
size_t getAvailablePhysicalMemory() {
#if defined(MCENGINE_PLATFORM_WINDOWS)
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if(GlobalMemoryStatusEx(&memInfo)) return (size_t)memInfo.ullAvailPhys;
    return (size_t)0L;

#elif defined(__APPLE__) && defined(__MACH__)
    vm_statistics64_data_t vmStats;
    mach_msg_type_number_t infoCount = HOST_VM_INFO64_COUNT;
    if(host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vmStats, &infoCount) != KERN_SUCCESS)
        return (size_t)0L;
    // Free + inactive pages are generally considered "available"
    return (size_t)(vmStats.free_count + vmStats.inactive_count) * (size_t)sysconf(_SC_PAGESIZE);

#elif defined(MCENGINE_PLATFORM_LINUX)
    // _SC_AVPHYS_PAGES gives free pages, but doesn't include reclaimable cache.
    // For a more accurate "available" value, /proc/meminfo has MemAvailable (kernel 3.14+).
    // Fall back to free pages if that's not available.
    i64 pages = sysconf(_SC_AVPHYS_PAGES);
    i64 page_size = sysconf(_SC_PAGESIZE);
    if(pages > 0 && page_size > 0) return (size_t)pages * (size_t)page_size;
    return (size_t)0L;

#else
    return (size_t)0L;
#endif
}

/**
 * Returns the total virtual address space available to this process in bytes.
 * On 32-bit systems, this reflects the actual usable address space (which may be
 * 2GB, 3GB, or 4GB depending on configuration and WoW64).
 */
size_t getTotalVirtualAddressSpace() {
#if defined(MCENGINE_PLATFORM_WINDOWS)
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if(GlobalMemoryStatusEx(&memInfo)) return (size_t)memInfo.ullTotalVirtual;
    return (size_t)0L;

#elif defined(__APPLE__) && defined(__MACH__) || defined(__linux__) || defined(__linux) || defined(linux) || \
    defined(__gnu_linux__)
    // On Unix-like systems, the virtual address space limit is typically:
    // - 32-bit: ~3GB user space (1GB kernel)
    // - 64-bit: 128TB+ (varies by kernel/architecture)
    // RLIMIT_AS gives the soft limit if set, otherwise it's RLIM_INFINITY
    struct rlimit rl;
    if(getrlimit(RLIMIT_AS, &rl) == 0 && rl.rlim_cur != RLIM_INFINITY) return (size_t)rl.rlim_cur;

    // Return a reasonable default based on pointer size
    if(sizeof(void*) == 4)
        return (size_t)3UL * 1024 * 1024 * 1024;  // 3GB typical for 32-bit
    else
        return (size_t)128UL * 1024 * 1024 * 1024 * 1024;  // 128TB typical for 64-bit

#else
    return (size_t)0L;
#endif
}

/**
 * Returns cumulative page fault count.
 * On Windows, this is total page faults.
 * On Unix, this is major faults (requiring disk I/O) from getrusage.
 */
uint64_t getPageFaultCount() {
#if defined(MCENGINE_PLATFORM_WINDOWS)
    PROCESS_MEMORY_COUNTERS info;
    if(get_process_memory_info(GetCurrentProcess(), &info, sizeof(info))) return (uint64_t)info.PageFaultCount;
    return 0;

#elif defined(__unix__) || defined(__unix) || defined(unix) || (defined(__APPLE__) && defined(__MACH__))
    struct rusage rusage;
    if(getrusage(RUSAGE_SELF, &rusage) == 0) return (uint64_t)(rusage.ru_majflt + rusage.ru_minflt);
    return 0;

#else
    return 0;
#endif
}

/**
 * Returns shared memory size in bytes (memory shared with other processes,
 * typically memory-mapped files and shared libraries).
 */
size_t getSharedMemory() {
#if defined(MCENGINE_PLATFORM_WINDOWS)
    return enumerateVirtualMemory().sharedBytes;

#elif defined(__APPLE__) && defined(__MACH__)
    // Use task_vm_info for more detailed breakdown
    task_vm_info_data_t vmInfo;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if(task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&vmInfo, &count) == KERN_SUCCESS) {
        // external pages are file-backed/shared, internal pages are anonymous/private
        return (size_t)vmInfo.external * (size_t)sysconf(_SC_PAGESIZE);
    }
    return (size_t)0L;

#elif defined(MCENGINE_PLATFORM_LINUX)
    i64 size, resident, shared, text, data;
    if(!getStatmReader().read(size, resident, shared, text, data)) {
        return (size_t)0L;
    }
    return (size_t)shared * (size_t)sysconf(_SC_PAGESIZE);

#else
    return (size_t)0L;
#endif
}

/**
 * Fills a MemoryInfo struct with all available metrics in one call.
 * More efficient to call this than call each function one at a time.
 */
void getMemoryInfo(MemoryInfo& info) {
    memset(&info, 0, sizeof(info));

#if defined(MCENGINE_PLATFORM_WINDOWS)
    PROCESS_MEMORY_COUNTERS_EX procInfo;
    if(get_process_memory_info(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&procInfo, sizeof(procInfo))) {
        info.currentRSS = (size_t)procInfo.WorkingSetSize;
        info.peakRSS = (size_t)procInfo.PeakWorkingSetSize;
        info.pageFaults = (uint64_t)procInfo.PageFaultCount;
    }

    MEMORYSTATUSEX memStatus;
    memStatus.dwLength = sizeof(MEMORYSTATUSEX);
    if(GlobalMemoryStatusEx(&memStatus)) {
        info.totalPhysical = (size_t)memStatus.ullTotalPhys;
        info.availPhysical = (size_t)memStatus.ullAvailPhys;
        info.totalVirtual = (size_t)memStatus.ullTotalVirtual;
    }

    // VirtualQuery enumeration for accurate virtual memory breakdown
    const auto vmStats = enumerateVirtualMemory();
    info.virtualSize = vmStats.totalCommitted;
    info.privateBytes = vmStats.privateBytes;
    info.sharedBytes = vmStats.sharedBytes;

#elif defined(__APPLE__) && defined(__MACH__)
    struct mach_task_basic_info taskInfo;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if(task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&taskInfo, &infoCount) == KERN_SUCCESS) {
        info.currentRSS = (size_t)taskInfo.resident_size;
        info.virtualSize = (size_t)taskInfo.virtual_size;
    }

    // Use task_vm_info for private/shared breakdown
    task_vm_info_data_t vmInfo;
    infoCount = TASK_VM_INFO_COUNT;
    if(task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&vmInfo, &infoCount) == KERN_SUCCESS) {
        size_t pageSize = (size_t)sysconf(_SC_PAGESIZE);
        info.privateBytes = (size_t)vmInfo.internal * pageSize;
        info.sharedBytes = (size_t)vmInfo.external * pageSize;
    }

    struct rusage rusage;
    if(getrusage(RUSAGE_SELF, &rusage) == 0) {
        info.peakRSS = (size_t)rusage.ru_maxrss;
        info.pageFaults = (uint64_t)(rusage.ru_majflt + rusage.ru_minflt);
    }

    int mib[2] = {CTL_HW, HW_MEMSIZE};
    int64_t memsize = 0;
    size_t len = sizeof(memsize);
    if(sysctl(mib, 2, &memsize, &len, nullptr, 0) == 0) info.totalPhysical = (size_t)memsize;

    vm_statistics64_data_t vmStats;
    infoCount = HOST_VM_INFO64_COUNT;
    if(host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vmStats, &infoCount) == KERN_SUCCESS) {
        info.availPhysical = (size_t)(vmStats.free_count + vmStats.inactive_count) * (size_t)sysconf(_SC_PAGESIZE);
    }

    struct rlimit rl;
    if(getrlimit(RLIMIT_AS, &rl) == 0 && rl.rlim_cur != RLIM_INFINITY) info.totalVirtual = (size_t)rl.rlim_cur;

#elif defined(MCENGINE_PLATFORM_LINUX)
    size_t pageSize = (size_t)sysconf(_SC_PAGESIZE);

    i64 size, resident, shared, text, data;
    if(getStatmReader().read(size, resident, shared, text, data)) {
        info.currentRSS = (size_t)resident * pageSize;
        info.virtualSize = (size_t)size * pageSize;
        info.sharedBytes = (size_t)shared * pageSize;
        info.privateBytes = info.currentRSS - info.sharedBytes;
    }

    struct rusage rusage;
    if(getrusage(RUSAGE_SELF, &rusage) == 0) {
        info.peakRSS = (size_t)(rusage.ru_maxrss * 1024L);
        info.pageFaults = (uint64_t)(rusage.ru_majflt + rusage.ru_minflt);
    }

    i64 pages = sysconf(_SC_PHYS_PAGES);
    if(pages > 0) info.totalPhysical = (size_t)pages * pageSize;

    i64 availPages = sysconf(_SC_AVPHYS_PAGES);
    if(availPages > 0) info.availPhysical = (size_t)availPages * pageSize;

    struct rlimit rl;
    if(getrlimit(RLIMIT_AS, &rl) == 0 && rl.rlim_cur != RLIM_INFINITY)
        info.totalVirtual = (size_t)rl.rlim_cur;
    else
        info.totalVirtual =
            (sizeof(void*) == 4) ? (size_t)3UL * 1024 * 1024 * 1024 : (size_t)128UL * 1024 * 1024 * 1024 * 1024;

#endif
}

/**
 * Returns total user-mode CPU time consumed by this process in seconds.
 */
double getUserCPUTime() {
#if defined(MCENGINE_PLATFORM_WINDOWS)
    FILETIME creation, exit, kernel, user;
    if(GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
        ULARGE_INTEGER u;
        u.LowPart = user.dwLowDateTime;
        u.HighPart = user.dwHighDateTime;
        return (double)u.QuadPart / 10000000.0;  // 100-nanosecond intervals to seconds
    }
    return 0.0;

#elif defined(__APPLE__) && defined(__MACH__)
    struct rusage rusage;
    if(getrusage(RUSAGE_SELF, &rusage) == 0) {
        return (double)rusage.ru_utime.tv_sec + (double)rusage.ru_utime.tv_usec / 1000000.0;
    }
    return 0.0;

#elif defined(MCENGINE_PLATFORM_LINUX)
    ProcStatReader::StatData data;
    if(getStatReader().read(data)) {
        return data.userTime;
    }
    return 0.0;

#else
    return 0.0;
#endif
}

/**
 * Returns total kernel-mode CPU time consumed by this process in seconds.
 */
double getKernelCPUTime() {
#if defined(MCENGINE_PLATFORM_WINDOWS)
    FILETIME creation, exit, kernel, user;
    if(GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
        ULARGE_INTEGER k;
        k.LowPart = kernel.dwLowDateTime;
        k.HighPart = kernel.dwHighDateTime;
        return (double)k.QuadPart / 10000000.0;
    }
    return 0.0;

#elif defined(__APPLE__) && defined(__MACH__)
    struct rusage rusage;
    if(getrusage(RUSAGE_SELF, &rusage) == 0) {
        return (double)rusage.ru_stime.tv_sec + (double)rusage.ru_stime.tv_usec / 1000000.0;
    }
    return 0.0;

#elif defined(MCENGINE_PLATFORM_LINUX)
    ProcStatReader::StatData data;
    if(getStatReader().read(data)) {
        return data.kernelTime;
    }
    return 0.0;

#else
    return 0.0;
#endif
}

namespace {
inline uint64_t getMonotonicTimeMS() {
#if defined(MCENGINE_PLATFORM_WINDOWS)
    return Timing::getTicksMS();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
#endif
}

struct CpuUsageSamplerPortable {
    uint64_t lastSampleTimeMS = 0;
    double lastTotalCpuTime = 0.0;
    double lastUsagePercent = 0.0;

    double sample(double currentTotalCpuTime) {
        const uint64_t now = getMonotonicTimeMS();
        if(lastSampleTimeMS == 0) {
            lastSampleTimeMS = now;
            lastTotalCpuTime = currentTotalCpuTime;
            return 0.0;
        }

        const uint64_t elapsedMS = now - lastSampleTimeMS;
        if(elapsedMS < 100) {
            return lastUsagePercent;
        }

        const double cpuDelta = currentTotalCpuTime - lastTotalCpuTime;
        const double wallDelta = elapsedMS / 1000.0;

        lastUsagePercent = (cpuDelta / wallDelta) * 100.0;
        lastSampleTimeMS = now;
        lastTotalCpuTime = currentTotalCpuTime;

        return lastUsagePercent;
    }
};

inline CpuUsageSamplerPortable& getCpuSampler() {
    static CpuUsageSamplerPortable sampler;
    return sampler;
}
}  // namespace

/**
 * Returns current CPU usage percentage for this process.
 * Can exceed 100% on multicore systems (e.g., 200% = 2 cores fully utilized).
 * Uses internal sampling; first call returns 0.
 */
double getCPUUsagePercent() {
    const double totalCpu = getUserCPUTime() + getKernelCPUTime();
    return getCpuSampler().sample(totalCpu);
}

/**
 * Returns the number of threads in this process.
 */
uint32_t getThreadCount() {
#if defined(MCENGINE_PLATFORM_WINDOWS)
    DWORD pid = GetCurrentProcessId();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if(snapshot == INVALID_HANDLE_VALUE) return 0;

    uint32_t count = 0;
    THREADENTRY32 te;
    te.dwSize = sizeof(THREADENTRY32);

    if(Thread32First(snapshot, &te)) {
        do {
            if(te.th32OwnerProcessID == pid) count++;
        } while(Thread32Next(snapshot, &te));
    }

    CloseHandle(snapshot);
    return count;

#elif defined(__APPLE__) && defined(__MACH__)
    struct mach_task_basic_info info;
    mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
    if(task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &infoCount) == KERN_SUCCESS) {
        // task_basic_info doesn't have thread count directly, need thread_info
        thread_array_t threads;
        mach_msg_type_number_t threadCount;
        if(task_threads(mach_task_self(), &threads, &threadCount) == KERN_SUCCESS) {
            // Deallocate the thread list
            for(mach_msg_type_number_t i = 0; i < threadCount; i++) {
                mach_port_deallocate(mach_task_self(), threads[i]);
            }
            vm_deallocate(mach_task_self(), (vm_address_t)threads, threadCount * sizeof(thread_t));
            return (uint32_t)threadCount;
        }
    }
    return 0;

#elif defined(MCENGINE_PLATFORM_LINUX)
    ProcStatReader::StatData data;
    if(getStatReader().read(data)) {
        return data.threadCount;
    }
    return 0;

#else
    return 0;
#endif
}

/**
 * Fills a CpuInfo struct with all available CPU metrics in one call.
 * More efficient to call this than call each function one at a time.
 */
void getCpuInfo(CpuInfo& info) {
    memset(&info, 0, sizeof(info));

#if defined(MCENGINE_PLATFORM_WINDOWS)
    FILETIME creation, exit, kernel, user;
    if(GetProcessTimes(GetCurrentProcess(), &creation, &exit, &kernel, &user)) {
        ULARGE_INTEGER u, k;
        u.LowPart = user.dwLowDateTime;
        u.HighPart = user.dwHighDateTime;
        k.LowPart = kernel.dwLowDateTime;
        k.HighPart = kernel.dwHighDateTime;
        info.userTime = (double)u.QuadPart / 10000000.0;
        info.kernelTime = (double)k.QuadPart / 10000000.0;
    }

    info.cpuUsage = getCpuSampler().sample(info.userTime + info.kernelTime);
    info.threadCount = getThreadCount();

    // Windows doesn't expose context switches per-process easily
    // Could use ETW or performance counters, but that's heavyweight

#elif defined(__APPLE__) && defined(__MACH__)
    struct rusage rusage;
    if(getrusage(RUSAGE_SELF, &rusage) == 0) {
        info.userTime = (double)rusage.ru_utime.tv_sec + (double)rusage.ru_utime.tv_usec / 1000000.0;
        info.kernelTime = (double)rusage.ru_stime.tv_sec + (double)rusage.ru_stime.tv_usec / 1000000.0;
        info.voluntaryCtxSwitches = (uint64_t)rusage.ru_nvcsw;
        info.involuntaryCtxSwitches = (uint64_t)rusage.ru_nivcsw;
    }

    info.cpuUsage = getCpuSampler().sample(info.userTime + info.kernelTime);

    thread_array_t threads;
    mach_msg_type_number_t threadCount;
    if(task_threads(mach_task_self(), &threads, &threadCount) == KERN_SUCCESS) {
        info.threadCount = (uint32_t)threadCount;
        for(mach_msg_type_number_t i = 0; i < threadCount; i++) {
            mach_port_deallocate(mach_task_self(), threads[i]);
        }
        vm_deallocate(mach_task_self(), (vm_address_t)threads, threadCount * sizeof(thread_t));
    }

#elif defined(MCENGINE_PLATFORM_LINUX)
    ProcStatReader::StatData statData;
    if(getStatReader().read(statData)) {
        info.userTime = statData.userTime;
        info.kernelTime = statData.kernelTime;
        info.threadCount = statData.threadCount;
    }

    info.cpuUsage = getCpuSampler().sample(info.userTime + info.kernelTime);
    {
        uint64_t voluntary, involuntary;
        if(getStatusReader().readContextSwitches(voluntary, involuntary)) {
            info.voluntaryCtxSwitches = voluntary;
            info.involuntaryCtxSwitches = involuntary;
        }
    }

#endif
}

#endif  // SYSMON_UNSUPPORTED

}  // namespace SysMon
