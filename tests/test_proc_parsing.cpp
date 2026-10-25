#include "../include/Formatter.hpp"
#include "../include/ProcReader.hpp"
#include "../include/SystemMemory.hpp"

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Minimal test framework — no external dependencies needed.
// ---------------------------------------------------------------------------
static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(name)                                                \
    do {                                                          \
        ++g_tests_run;                                            \
        std::cout << "  [TEST] " << #name << " ... ";            \
        try {                                                     \
            test_##name();                                        \
            ++g_tests_passed;                                     \
            std::cout << "PASSED\n";                              \
        } catch (const std::exception& e) {                      \
            ++g_tests_failed;                                     \
            std::cout << "FAILED: " << e.what() << "\n";         \
        }                                                         \
    } while (0)

#define ASSERT_EQ(a, b)                                                         \
    do {                                                                         \
        if ((a) != (b)) {                                                        \
            std::ostringstream _msg;                                             \
            _msg << "Assertion failed: " << #a << " == " << #b                  \
                 << " (got " << (a) << " vs " << (b) << ") at line " << __LINE__;\
            throw std::runtime_error(_msg.str());                                \
        }                                                                        \
    } while (0)

#define ASSERT_TRUE(expr)                                                       \
    do {                                                                          \
        if (!(expr)) {                                                            \
            std::ostringstream _msg;                                              \
            _msg << "Assertion failed: " << #expr << " at line " << __LINE__;    \
            throw std::runtime_error(_msg.str());                                 \
        }                                                                          \
    } while (0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

// ===================================================================
// Section 1: ProcReader::parseStatusContent tests
// ===================================================================

/// Test parsing a normal, well-formed /proc/<PID>/status.
void test_parse_normal_status() {
    std::string content =
        "Name:\tsystemd\n"
        "Umask:\t0000\n"
        "State:\tS (sleeping)\n"
        "Tgid:\t1\n"
        "Ngid:\t0\n"
        "Pid:\t1\n"
        "PPid:\t0\n"
        "TracerPid:\t0\n"
        "Uid:\t0\t0\t0\t0\n"
        "Gid:\t0\t0\t0\t0\n"
        "VmPeak:\t  235768 kB\n"
        "VmSize:\t  170204 kB\n"
        "VmRSS:\t   12340 kB\n"
        "VmHWM:\t   14560 kB\n"
        "Threads:\t1\n";

    auto info = ProcReader::parseStatusContent(42, content);

    ASSERT_EQ(info.pid, 42);
    ASSERT_EQ(info.name, "systemd");
    ASSERT_TRUE(info.state.has_value());
    ASSERT_EQ(*info.state, 'S');
    ASSERT_TRUE(info.ppid.has_value());
    ASSERT_EQ(*info.ppid, 0);
    ASSERT_TRUE(info.vm_peak_kib.has_value());
    ASSERT_EQ(*info.vm_peak_kib, 235768u);
    ASSERT_TRUE(info.vm_size_kib.has_value());
    ASSERT_EQ(*info.vm_size_kib, 170204u);
    ASSERT_TRUE(info.vm_rss_kib.has_value());
    ASSERT_EQ(*info.vm_rss_kib, 12340u);
    ASSERT_TRUE(info.vm_hwm_kib.has_value());
    ASSERT_EQ(*info.vm_hwm_kib, 14560u);
    ASSERT_TRUE(info.threads.has_value());
    ASSERT_EQ(*info.threads, 1);
}

/// Test that missing optional fields result in std::nullopt, not zero.
void test_parse_missing_optional_fields() {
    // Kernel threads often have no Vm* fields at all.
    std::string content =
        "Name:\tkworker/0:1\n"
        "State:\tI (idle)\n"
        "PPid:\t2\n"
        "Threads:\t1\n";

    auto info = ProcReader::parseStatusContent(100, content);

    ASSERT_EQ(info.name, "kworker/0:1");
    ASSERT_TRUE(info.state.has_value());
    ASSERT_EQ(*info.state, 'I');
    ASSERT_FALSE(info.vm_size_kib.has_value());   // must be nullopt, not 0
    ASSERT_FALSE(info.vm_rss_kib.has_value());
    ASSERT_FALSE(info.vm_peak_kib.has_value());
    ASSERT_FALSE(info.vm_hwm_kib.has_value());
}

/// Test parsing with malformed numeric values.
void test_parse_malformed_numeric() {
    std::string content =
        "Name:\tbadproc\n"
        "State:\tR (running)\n"
        "PPid:\tnot_a_number\n"
        "VmSize:\tabc kB\n"
        "VmRSS:\t kB\n"
        "Threads:\t-1\n";  // -1 is technically valid for stol but unusual

    auto info = ProcReader::parseStatusContent(200, content);

    ASSERT_EQ(info.name, "badproc");
    // PPid should fail to parse
    ASSERT_FALSE(info.ppid.has_value());
    // VmSize has non-numeric value
    ASSERT_FALSE(info.vm_size_kib.has_value());
    // VmRSS is effectively empty (just whitespace + "kB")
    ASSERT_FALSE(info.vm_rss_kib.has_value());
    // Threads = -1: our uint64 parser rejects negative, but int parser accepts it
    ASSERT_TRUE(info.threads.has_value());
    ASSERT_EQ(*info.threads, -1);
}

/// Test parsing empty input.
void test_parse_empty_content() {
    auto info = ProcReader::parseStatusContent(999, "");

    ASSERT_EQ(info.pid, 999);
    ASSERT_TRUE(info.name.empty());
    ASSERT_FALSE(info.state.has_value());
    ASSERT_FALSE(info.ppid.has_value());
    ASSERT_FALSE(info.vm_size_kib.has_value());
    ASSERT_FALSE(info.vm_rss_kib.has_value());
}

/// Test that a process name with spaces is handled correctly.
void test_parse_name_with_spaces() {
    std::string content = "Name:\tWeb Content\nState:\tS (sleeping)\n";

    auto info = ProcReader::parseStatusContent(300, content);

    // The Name field value includes everything after the tab until the newline.
    ASSERT_EQ(info.name, "Web Content");
}

/// Test handling of extra whitespace in values.
void test_parse_extra_whitespace() {
    std::string content =
        "Name:\t  myproc  \n"
        "State:\t  R (running)  \n"
        "PPid:\t  42  \n"
        "VmRSS:\t  5678 kB\n";

    auto info = ProcReader::parseStatusContent(400, content);

    // Name value starts after leading whitespace trim
    ASSERT_EQ(info.name, "myproc  ");  // trailing whitespace preserved per spec
    ASSERT_TRUE(info.state.has_value());
    ASSERT_EQ(*info.state, 'R');
    ASSERT_TRUE(info.ppid.has_value());
    ASSERT_EQ(*info.ppid, 42);
    ASSERT_TRUE(info.vm_rss_kib.has_value());
    ASSERT_EQ(*info.vm_rss_kib, 5678u);
}

/// Test truncated input (no trailing newline, mid-line cut).
void test_parse_truncated_input() {
    std::string content = "Name:\ttrunc\nVmRSS:\t100";
    // No trailing newline, no "kB" suffix

    auto info = ProcReader::parseStatusContent(500, content);

    ASSERT_EQ(info.name, "trunc");
    ASSERT_TRUE(info.vm_rss_kib.has_value());
    ASSERT_EQ(*info.vm_rss_kib, 100u);
}

/// Test integer overflow in memory values.
void test_parse_overflow_value() {
    // A value that overflows uint64_t
    std::string content =
        "Name:\toverflow_test\n"
        "VmRSS:\t99999999999999999999999999 kB\n";

    auto info = ProcReader::parseStatusContent(600, content);

    ASSERT_EQ(info.name, "overflow_test");
    // The parser should reject the overflow rather than returning garbage
    ASSERT_FALSE(info.vm_rss_kib.has_value());
}

/// Test zero memory value — must be distinguishable from "absent".
void test_parse_zero_vs_absent() {
    std::string content_with_zero =
        "Name:\tzero_rss\n"
        "VmRSS:\t0 kB\n";

    std::string content_without =
        "Name:\tno_rss\n";

    auto info_zero = ProcReader::parseStatusContent(701, content_with_zero);
    auto info_none = ProcReader::parseStatusContent(702, content_without);

    // Zero should be present as 0
    ASSERT_TRUE(info_zero.vm_rss_kib.has_value());
    ASSERT_EQ(*info_zero.vm_rss_kib, 0u);

    // Absent should be nullopt
    ASSERT_FALSE(info_none.vm_rss_kib.has_value());
}

/// Test line with no colon (malformed line).
void test_parse_no_colon_lines() {
    std::string content =
        "This line has no colon\n"
        "Name:\tvalid\n"
        "Another bad line\n"
        "VmRSS:\t1024 kB\n";

    auto info = ProcReader::parseStatusContent(800, content);

    ASSERT_EQ(info.name, "valid");
    ASSERT_TRUE(info.vm_rss_kib.has_value());
    ASSERT_EQ(*info.vm_rss_kib, 1024u);
}

// ===================================================================
// Section 2: SystemMemory::parseMeminfo tests
// ===================================================================

/// Test parsing a normal /proc/meminfo.
void test_parse_meminfo_normal() {
    std::string content =
        "MemTotal:       16384000 kB\n"
        "MemFree:         2048000 kB\n"
        "MemAvailable:    8192000 kB\n"
        "Buffers:          512000 kB\n"
        "Cached:          4096000 kB\n"
        "SwapCached:       128000 kB\n"
        "Active:          6000000 kB\n"
        "Inactive:        3000000 kB\n"
        "SwapTotal:       2048000 kB\n"
        "SwapFree:        1024000 kB\n";

    auto info = SystemMemory::parseMeminfo(content);

    ASSERT_TRUE(info.mem_total_kib.has_value());
    ASSERT_EQ(*info.mem_total_kib, 16384000u);
    ASSERT_TRUE(info.mem_free_kib.has_value());
    ASSERT_EQ(*info.mem_free_kib, 2048000u);
    ASSERT_TRUE(info.mem_available_kib.has_value());
    ASSERT_EQ(*info.mem_available_kib, 8192000u);
    ASSERT_TRUE(info.buffers_kib.has_value());
    ASSERT_EQ(*info.buffers_kib, 512000u);
    ASSERT_TRUE(info.cached_kib.has_value());
    ASSERT_EQ(*info.cached_kib, 4096000u);
    ASSERT_TRUE(info.swap_total_kib.has_value());
    ASSERT_EQ(*info.swap_total_kib, 2048000u);
    ASSERT_TRUE(info.swap_free_kib.has_value());
    ASSERT_EQ(*info.swap_free_kib, 1024000u);
}

/// Test /proc/meminfo with missing fields.
void test_parse_meminfo_missing_fields() {
    // Only MemTotal present
    std::string content = "MemTotal:       8192000 kB\n";

    auto info = SystemMemory::parseMeminfo(content);

    ASSERT_TRUE(info.mem_total_kib.has_value());
    ASSERT_EQ(*info.mem_total_kib, 8192000u);
    ASSERT_FALSE(info.mem_free_kib.has_value());
    ASSERT_FALSE(info.mem_available_kib.has_value());
    ASSERT_FALSE(info.buffers_kib.has_value());
    ASSERT_FALSE(info.cached_kib.has_value());
    ASSERT_FALSE(info.swap_total_kib.has_value());
    ASSERT_FALSE(info.swap_free_kib.has_value());
}

/// Test empty /proc/meminfo.
void test_parse_meminfo_empty() {
    auto info = SystemMemory::parseMeminfo("");

    ASSERT_FALSE(info.mem_total_kib.has_value());
    ASSERT_FALSE(info.mem_free_kib.has_value());
}

/// Test /proc/meminfo with malformed values.
void test_parse_meminfo_malformed() {
    std::string content =
        "MemTotal:       abc kB\n"
        "MemFree:        1024 kB\n"
        "SwapTotal:      -500 kB\n";

    auto info = SystemMemory::parseMeminfo(content);

    ASSERT_FALSE(info.mem_total_kib.has_value());  // "abc" is not a number
    ASSERT_TRUE(info.mem_free_kib.has_value());
    ASSERT_EQ(*info.mem_free_kib, 1024u);
    ASSERT_FALSE(info.swap_total_kib.has_value()); // negative rejected
}

// ===================================================================
// Section 3: Formatter tests
// ===================================================================

/// Test memory formatting at different magnitudes.
void test_format_memory_kib() {
    ASSERT_EQ(Formatter::formatMemoryKiB(0), "0 KiB");
    ASSERT_EQ(Formatter::formatMemoryKiB(512), "512 KiB");
    ASSERT_EQ(Formatter::formatMemoryKiB(1023), "1023 KiB");
    ASSERT_EQ(Formatter::formatMemoryKiB(1024), "1.0 MiB");
    ASSERT_EQ(Formatter::formatMemoryKiB(2048), "2.0 MiB");
    ASSERT_EQ(Formatter::formatMemoryKiB(1536), "1.5 MiB");
    ASSERT_EQ(Formatter::formatMemoryKiB(1048576), "1.0 GiB");
    ASSERT_EQ(Formatter::formatMemoryKiB(2097152), "2.0 GiB");
}

/// Test formatting of optional values.
void test_format_optional_memory() {
    ASSERT_EQ(Formatter::formatOptionalMemory(std::nullopt), "N/A");
    ASSERT_EQ(Formatter::formatOptionalMemory(std::optional<uint64_t>(1024)), "1.0 MiB");
}

void test_format_optional_int() {
    ASSERT_EQ(Formatter::formatOptionalInt(std::nullopt), "N/A");
    ASSERT_EQ(Formatter::formatOptionalInt(std::optional<int>(42)), "42");
}

void test_format_optional_state() {
    ASSERT_EQ(Formatter::formatOptionalState(std::nullopt), "?");
    ASSERT_EQ(Formatter::formatOptionalState(std::optional<char>('R')), "R");
}

/// Test string padding.
void test_pad_right() {
    ASSERT_EQ(Formatter::padRight("abc", 6), "abc   ");
    ASSERT_EQ(Formatter::padRight("abcdef", 6), "abcdef");
    ASSERT_EQ(Formatter::padRight("abcdefgh", 6), "abcdefgh"); // wider than width
}

void test_pad_left() {
    ASSERT_EQ(Formatter::padLeft("42", 6), "    42");
    ASSERT_EQ(Formatter::padLeft("123456", 6), "123456");
}

// ===================================================================
// Section 4: Edge-case parsing tests
// ===================================================================

/// Test that a State field with only a single character (no description) works.
void test_parse_state_single_char() {
    std::string content = "Name:\ttest\nState:\tZ\n";
    auto info = ProcReader::parseStatusContent(900, content);
    ASSERT_TRUE(info.state.has_value());
    ASSERT_EQ(*info.state, 'Z');
}

/// Test multiple colons in a line (key is before the first colon).
void test_parse_multiple_colons() {
    std::string content = "Name:\tproc:with:colons\nState:\tS (sleeping)\n";
    auto info = ProcReader::parseStatusContent(901, content);
    ASSERT_EQ(info.name, "proc:with:colons");
}

/// Test that the PID argument is correctly set on the returned struct.
void test_parse_pid_passthrough() {
    auto info = ProcReader::parseStatusContent(12345, "Name:\ttest\n");
    ASSERT_EQ(info.pid, 12345);
}

// ===================================================================
// Main test runner
// ===================================================================

int main() {
    std::cout << "=== Unit Tests: Proc Parsing & Formatting ===\n\n";

    std::cout << "--- ProcReader::parseStatusContent ---\n";
    TEST(parse_normal_status);
    TEST(parse_missing_optional_fields);
    TEST(parse_malformed_numeric);
    TEST(parse_empty_content);
    TEST(parse_name_with_spaces);
    TEST(parse_extra_whitespace);
    TEST(parse_truncated_input);
    TEST(parse_overflow_value);
    TEST(parse_zero_vs_absent);
    TEST(parse_no_colon_lines);
    TEST(parse_state_single_char);
    TEST(parse_multiple_colons);
    TEST(parse_pid_passthrough);

    std::cout << "\n--- SystemMemory::parseMeminfo ---\n";
    TEST(parse_meminfo_normal);
    TEST(parse_meminfo_missing_fields);
    TEST(parse_meminfo_empty);
    TEST(parse_meminfo_malformed);

    std::cout << "\n--- Formatter ---\n";
    TEST(format_memory_kib);
    TEST(format_optional_memory);
    TEST(format_optional_int);
    TEST(format_optional_state);
    TEST(pad_right);
    TEST(pad_left);

    std::cout << "\n=== Results: " << g_tests_passed << "/" << g_tests_run
              << " passed, " << g_tests_failed << " failed ===\n";

    return (g_tests_failed == 0) ? 0 : 1;
}
