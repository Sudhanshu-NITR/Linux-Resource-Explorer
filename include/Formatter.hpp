#ifndef FORMATTER_HPP
#define FORMATTER_HPP

#include <cstdint>
#include <optional>
#include <string>

/// Utility functions for formatting values for human-readable CLI output.
namespace Formatter {

    /// Format a memory value in KiB to a human-readable string.
    /// Values < 1024 KiB are shown as "NNN KiB".
    /// Values >= 1024 KiB are shown as "NNN.N MiB".
    /// Values >= 1048576 KiB (1 GiB) are shown as "NNN.N GiB".
    std::string formatMemoryKiB(uint64_t kib);

    /// Format an optional memory value; returns "N/A" if the value is absent.
    std::string formatOptionalMemory(const std::optional<uint64_t>& kib);

    /// Format an optional integer; returns "N/A" if the value is absent.
    std::string formatOptionalInt(const std::optional<int>& value);

    /// Format an optional char (process state); returns "?" if absent.
    std::string formatOptionalState(const std::optional<char>& state);

    /// Right-pad or left-pad a string to the given width for table alignment.
    std::string padRight(const std::string& s, size_t width);
    std::string padLeft(const std::string& s, size_t width);

} // namespace Formatter

#endif // FORMATTER_HPP
