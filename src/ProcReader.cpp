#include "ProcReader.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Internal helpers (anonymous namespace keeps them file-local)
// ---------------------------------------------------------------------------
namespace {

/// Try to parse a string as an unsigned 64-bit integer.
/// Returns std::nullopt on overflow, trailing garbage, or empty input.
std::optional<uint64_t> safeParseUint64(const std::string& s) {
    if (s.empty()) return std::nullopt;

    // std::stoull throws on invalid input, but we also need to reject
    // negative numbers (stoull silently wraps them).
    for (char c : s) {
        if (c != ' ' && c != '\t' && (c < '0' || c > '9')) {
            return std::nullopt;
        }
    }

    try {
        size_t pos = 0;
        unsigned long long val = std::stoull(s, &pos);
        // Reject if there are trailing non-whitespace characters
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) {
            ++pos;
        }
        if (pos != s.size()) return std::nullopt;
        return static_cast<uint64_t>(val);
    } catch (...) {
        // std::out_of_range or std::invalid_argument
        return std::nullopt;
    }
}

/// Try to parse a string as a signed integer (for PID, PPID, thread count).
/// Returns std::nullopt on overflow or invalid input.
std::optional<int> safeParseInt(const std::string& s) {
    if (s.empty()) return std::nullopt;

    try {
        size_t pos = 0;
        long val = std::stol(s, &pos);
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t')) {
            ++pos;
        }
        if (pos != s.size()) return std::nullopt;
        // Check that value fits in an int
        if (val < static_cast<long>(std::numeric_limits<int>::min()) ||
            val > static_cast<long>(std::numeric_limits<int>::max())) {
            return std::nullopt;
        }
        return static_cast<int>(val);
    } catch (...) {
        return std::nullopt;
    }
}

/// Read the entire contents of a file into a string.
/// Returns std::nullopt if the file cannot be opened (e.g. process exited).
std::optional<std::string> readFileContents(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return std::nullopt;

    std::ostringstream buf;
    buf << file.rdbuf();
    if (file.bad()) return std::nullopt;  // read error (rare for /proc)

    return buf.str();
}

/// Extract the first whitespace-delimited token from a value string.
/// Example: "12345 kB" -> "12345"
std::string firstToken(const std::string& value) {
    std::istringstream iss(value);
    std::string token;
    iss >> token;
    return token;
}

/// Check whether a directory entry name is numeric (i.e. a PID directory).
bool isNumericName(const std::string& name) {
    if (name.empty()) return false;
    for (char c : name) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API implementation
// ---------------------------------------------------------------------------

ProcessInfo ProcReader::parseStatusContent(int pid, const std::string& content) {
    ProcessInfo info;
    info.pid = pid;

    // /proc/<PID>/status has lines of the form "Key:\tValue\n".
    // We parse line-by-line and extract the fields we care about.
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        // Find the colon that separates key from value
        auto colon_pos = line.find(':');
        if (colon_pos == std::string::npos) continue;

        std::string key = line.substr(0, colon_pos);
        // Value starts after the colon; skip leading whitespace/tabs
        std::string value_raw = line.substr(colon_pos + 1);
        // Trim leading whitespace
        size_t val_start = value_raw.find_first_not_of(" \t");
        std::string value = (val_start != std::string::npos)
                                ? value_raw.substr(val_start)
                                : "";

        if (key == "Name") {
            info.name = value;
        } else if (key == "State") {
            // State line looks like "S (sleeping)" — we want just the first character.
            if (!value.empty()) {
                info.state = value[0];
            }
        } else if (key == "PPid") {
            info.ppid = safeParseInt(firstToken(value));
        } else if (key == "VmSize") {
            // Value is typically "12345 kB" — we extract the number.
            // /proc reports memory in kB which actually means KiB (1024 bytes).
            info.vm_size_kib = safeParseUint64(firstToken(value));
        } else if (key == "VmRSS") {
            info.vm_rss_kib = safeParseUint64(firstToken(value));
        } else if (key == "VmPeak") {
            info.vm_peak_kib = safeParseUint64(firstToken(value));
        } else if (key == "VmHWM") {
            info.vm_hwm_kib = safeParseUint64(firstToken(value));
        } else if (key == "Threads") {
            info.threads = safeParseInt(firstToken(value));
        }
    }

    return info;
}

std::optional<ProcessInfo> ProcReader::readProcess(int pid) {
    std::string path = "/proc/" + std::to_string(pid) + "/status";
    auto content = readFileContents(path);
    if (!content) return std::nullopt;  // Process may have exited

    return parseStatusContent(pid, *content);
}

std::vector<ProcessInfo> ProcReader::listAllProcesses() {
    std::vector<ProcessInfo> result;

    // Open /proc and iterate over its entries.
    // We use opendir/readdir (POSIX) rather than std::filesystem to avoid
    // exceptions that std::filesystem::directory_iterator can throw when
    // entries disappear mid-iteration, which is common with /proc.
    DIR* proc_dir = opendir("/proc");
    if (proc_dir == nullptr) {
        std::cerr << "Error: cannot open /proc directory\n";
        return result;
    }

    // Collect PIDs first, then read — this minimizes the window
    // between listing and reading, but TOCTOU is inherent with /proc.
    std::vector<int> pids;
    struct dirent* entry = nullptr;
    while ((entry = readdir(proc_dir)) != nullptr) {
        std::string name = entry->d_name;
        if (isNumericName(name)) {
            auto parsed = safeParseInt(name);
            if (parsed) {
                pids.push_back(*parsed);
            }
        }
    }
    closedir(proc_dir);

    result.reserve(pids.size());
    for (int pid : pids) {
        auto info = readProcess(pid);
        if (info) {
            result.push_back(std::move(*info));
        }
        // If readProcess returns nullopt the process likely exited — skip it.
    }

    return result;
}

std::vector<ProcessInfo> ProcReader::topByMemory(int n) {
    auto all = listAllProcesses();

    // Remove processes that don't have VmRSS data —
    // kernel threads typically lack memory mappings.
    all.erase(
        std::remove_if(all.begin(), all.end(),
                       [](const ProcessInfo& p) { return !p.vm_rss_kib.has_value(); }),
        all.end());

    // Sort descending by VmRSS
    std::sort(all.begin(), all.end(),
              [](const ProcessInfo& a, const ProcessInfo& b) {
                  return a.vm_rss_kib.value() > b.vm_rss_kib.value();
              });

    // Truncate to the requested count
    if (n >= 0 && static_cast<size_t>(n) < all.size()) {
        all.resize(static_cast<size_t>(n));
    }

    return all;
}
