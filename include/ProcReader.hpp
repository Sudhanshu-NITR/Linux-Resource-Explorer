#ifndef PROC_READER_HPP
#define PROC_READER_HPP

#include "ProcessInfo.hpp"

#include <optional>
#include <string>
#include <vector>

/// Reads process information from the Linux /proc pseudo-filesystem.
///
/// All functions are designed to fail gracefully: if a file cannot be read
/// (e.g. the process exits mid-inspection), the function returns an empty
/// optional or skips the entry rather than crashing.
namespace ProcReader {

    /// Parse a /proc/<PID>/status file from the given text content.
    /// This is the core parsing function, intentionally separated from I/O
    /// so it can be unit-tested with controlled input.
    ///
    /// @param pid        The process ID (set on the returned struct).
    /// @param content    The raw text content of /proc/<PID>/status.
    /// @return           A populated ProcessInfo struct.
    ProcessInfo parseStatusContent(int pid, const std::string& content);

    /// Read a single process's information from /proc/<PID>/status on disk.
    ///
    /// @param pid  The process ID to inspect.
    /// @return     ProcessInfo if the file could be read, std::nullopt otherwise.
    std::optional<ProcessInfo> readProcess(int pid);

    /// Enumerate all processes visible under /proc and read their status.
    /// Processes that disappear or become unreadable between directory listing
    /// and file reading are silently skipped.
    ///
    /// @return  A vector of ProcessInfo for every successfully read process.
    std::vector<ProcessInfo> listAllProcesses();

    /// Return the N processes with the highest VmRSS.
    /// Processes without VmRSS data are excluded from the ranking.
    ///
    /// @param n  Maximum number of processes to return.
    /// @return   A vector sorted by VmRSS in descending order.
    std::vector<ProcessInfo> topByMemory(int n);

} // namespace ProcReader

#endif // PROC_READER_HPP
