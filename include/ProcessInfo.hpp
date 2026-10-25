#ifndef PROCESS_INFO_HPP
#define PROCESS_INFO_HPP

#include <cstdint>
#include <optional>
#include <string>

/// Represents information about a single Linux process, gathered from /proc/<PID>/status.
///
/// Fields that may be absent in /proc are wrapped in std::optional so callers
/// can distinguish "the kernel did not report this value" from "the value is 0".
/// Memory sizes are stored in KiB, matching the unit used by /proc/<PID>/status.
struct ProcessInfo {
    int pid = 0;
    std::string name;                         // "Name" field from /proc/<PID>/status
    std::optional<char> state;                // 'R', 'S', 'D', 'Z', 'T', etc.
    std::optional<int> ppid;                  // Parent PID
    std::optional<uint64_t> vm_size_kib;      // VmSize  – total virtual memory (KiB)
    std::optional<uint64_t> vm_rss_kib;       // VmRSS   – resident set size (KiB)
    std::optional<uint64_t> vm_peak_kib;      // VmPeak  – peak virtual memory (KiB)
    std::optional<uint64_t> vm_hwm_kib;       // VmHWM   – peak resident set size (KiB)
    std::optional<int> threads;               // Threads count
};

#endif // PROCESS_INFO_HPP
