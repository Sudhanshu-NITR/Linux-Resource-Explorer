#ifndef SYSTEM_MEMORY_HPP
#define SYSTEM_MEMORY_HPP

#include <cstdint>
#include <optional>
#include <string>

/// Holds system-wide memory statistics read from /proc/meminfo.
/// All values are stored in KiB, matching /proc/meminfo's native unit.
struct SystemMemoryInfo {
    std::optional<uint64_t> mem_total_kib;
    std::optional<uint64_t> mem_free_kib;
    std::optional<uint64_t> mem_available_kib;
    std::optional<uint64_t> buffers_kib;
    std::optional<uint64_t> cached_kib;
    std::optional<uint64_t> swap_total_kib;
    std::optional<uint64_t> swap_free_kib;
};

namespace SystemMemory {

    /// Parse the raw text content of /proc/meminfo into a SystemMemoryInfo struct.
    /// Separated from I/O so it can be unit-tested with synthetic input.
    SystemMemoryInfo parseMeminfo(const std::string& content);

    /// Read /proc/meminfo from disk and return the parsed result.
    /// Returns std::nullopt if the file cannot be opened.
    std::optional<SystemMemoryInfo> readMemoryInfo();

} // namespace SystemMemory

#endif // SYSTEM_MEMORY_HPP
