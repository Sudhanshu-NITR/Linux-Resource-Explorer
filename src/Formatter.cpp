#include "Formatter.hpp"

#include <iomanip>
#include <sstream>

std::string Formatter::formatMemoryKiB(uint64_t kib) {
    std::ostringstream oss;

    if (kib >= 1048576) {
        // 1 GiB = 1048576 KiB
        double gib = static_cast<double>(kib) / 1048576.0;
        oss << std::fixed << std::setprecision(1) << gib << " GiB";
    } else if (kib >= 1024) {
        double mib = static_cast<double>(kib) / 1024.0;
        oss << std::fixed << std::setprecision(1) << mib << " MiB";
    } else {
        oss << kib << " KiB";
    }

    return oss.str();
}

std::string Formatter::formatOptionalMemory(const std::optional<uint64_t>& kib) {
    if (!kib.has_value()) return "N/A";
    return formatMemoryKiB(*kib);
}

std::string Formatter::formatOptionalInt(const std::optional<int>& value) {
    if (!value.has_value()) return "N/A";
    return std::to_string(*value);
}

std::string Formatter::formatOptionalState(const std::optional<char>& state) {
    if (!state.has_value()) return "?";
    return std::string(1, *state);
}

std::string Formatter::padRight(const std::string& s, size_t width) {
    if (s.size() >= width) return s;
    return s + std::string(width - s.size(), ' ');
}

std::string Formatter::padLeft(const std::string& s, size_t width) {
    if (s.size() >= width) return s;
    return std::string(width - s.size(), ' ') + s;
}
