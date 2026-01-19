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

#pragma once

#include <cstddef>
#include <cstdint>

namespace SysMon {

struct MemoryInfo {
    uint64_t pageFaults;   // Total page faults (major + minor where available)
    size_t currentRSS;     // Current resident set size (physical memory in use)
    size_t peakRSS;        // Peak resident set size
    size_t virtualSize;    // Total virtual memory mapped
    size_t privateBytes;   // Private (non-shared) memory
    size_t sharedBytes;    // Shared memory (libraries, mmap'd files)
    size_t totalPhysical;  // Total system physical memory
    size_t availPhysical;  // Available system physical memory
    size_t totalVirtual;   // Total virtual address space (useful for 32-bit limits)
};

struct CpuInfo {
    double userTime;                  // User-mode CPU time in seconds
    double kernelTime;                // Kernel-mode CPU time in seconds
    double cpuUsage;                  // CPU usage percentage (0-100+, can exceed 100% on multicore)
    uint64_t voluntaryCtxSwitches;    // Voluntary context switches (yielded CPU)
    uint64_t involuntaryCtxSwitches;  // Involuntary context switches (preempted)
    uint32_t threadCount;             // Number of threads in the process
};

/**
 * Returns the peak (maximum so far) resident set size (physical memory use) in bytes.
 */
size_t getPeakRSS();

/**
 * Returns the current resident set size (physical memory use) in bytes.
 */
size_t getCurrentRSS();

/**
 * Returns the current virtual memory size in bytes.
 */
size_t getVirtualSize();

/**
 * Returns the total physical memory in the system in bytes.
 */
size_t getTotalPhysicalMemory();

/**
 * Returns available (free + reclaimable) physical memory in bytes.
 */
size_t getAvailablePhysicalMemory();

/**
 * Returns the total virtual address space available to this process in bytes.
 * On 32-bit systems, this reflects the actual usable address space (which may be
 * 2GB, 3GB, or 4GB depending on configuration and WoW64).
 */
size_t getTotalVirtualAddressSpace();

/**
 * Returns cumulative page fault count.
 * On Windows, this is total page faults.
 * On Unix, this is major faults (requiring disk I/O) from getrusage.
 */
uint64_t getPageFaultCount();

/**
 * Returns shared memory size in bytes (memory shared with other processes,
 * typically memory-mapped files and shared libraries).
 */
size_t getSharedMemory();

/**
 * Fills a MemoryInfo struct with all available metrics in one call.
 * More efficient to call this than call each function one at a time.
 */
void getMemoryInfo(MemoryInfo& info);

/**
 * Returns total user-mode CPU time consumed by this process in seconds.
 */
double getUserCPUTime();

/**
 * Returns total kernel-mode CPU time consumed by this process in seconds.
 */
double getKernelCPUTime();

/**
 * Returns current CPU usage percentage for this process.
 * Can exceed 100% on multicore systems (e.g., 200% = 2 cores fully utilized).
 * Uses internal sampling; first call returns 0.
 */
double getCPUUsagePercent();

/**
 * Returns the number of threads in this process.
 */
uint32_t getThreadCount();

/**
 * Fills a CpuInfo struct with all available CPU metrics in one call.
 * More efficient to call this than call each function one at a time.
 */
void getCpuInfo(CpuInfo& info);

}  // namespace SysMon
