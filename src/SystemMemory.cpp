#include "SystemMemory.hpp"

#include <fstream>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace {

/// Safe uint64 parse — same logic as in ProcReader.cpp but duplicated here
/// to keep the two translation units independent (no shared internal header
/// needed for such a small helper).
std::optional<uint64_t> safeParseUint64(const std::string& s) {
    if (s.empty()) return std::nullopt;
    for (char c : s) {
        if (c != ' ' && c != '\t' && (c < '0' || c > '9')) {
            return std::nullopt;
        }
    }
    try {
        size_t pos = 0;
        unsigned long long val = std::stoull(s, &pos);
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) {
            ++pos;
        }
        if (pos != s.size()) return std::nullopt;
        return static_cast<uint64_t>(val);
    } catch (...) {
        return std::nullopt;
    }
}

/// Extract the first whitespace-delimited token.
std::string firstToken(const std::string& value) {
    std::istringstream iss(value);
    std::string token;
    iss >> token;
    return token;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

SystemMemoryInfo SystemMemory::parseMeminfo(const std::string& content) {
    SystemMemoryInfo info;

    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        auto colon_pos = line.find(':');
        if (colon_pos == std::string::npos) continue;

        std::string key = line.substr(0, colon_pos);
        std::string value_raw = line.substr(colon_pos + 1);
        size_t val_start = value_raw.find_first_not_of(" \t");
        std::string value = (val_start != std::string::npos)
                                ? value_raw.substr(val_start)
                                : "";

        // /proc/meminfo values are in "kB" which actually means KiB.
        auto parsed = safeParseUint64(firstToken(value));

        if      (key == "MemTotal")     info.mem_total_kib     = parsed;
        else if (key == "MemFree")      info.mem_free_kib      = parsed;
        else if (key == "MemAvailable") info.mem_available_kib = parsed;
        else if (key == "Buffers")      info.buffers_kib       = parsed;
        else if (key == "Cached")       info.cached_kib        = parsed;
        else if (key == "SwapTotal")    info.swap_total_kib    = parsed;
        else if (key == "SwapFree")     info.swap_free_kib     = parsed;
    }

    return info;
}

std::optional<SystemMemoryInfo> SystemMemory::readMemoryInfo() {
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) return std::nullopt;

    std::ostringstream buf;
    buf << file.rdbuf();
    if (file.bad()) return std::nullopt;

    return parseMeminfo(buf.str());
}
