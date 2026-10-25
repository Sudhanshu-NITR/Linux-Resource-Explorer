#include "../include/Formatter.hpp"
#include "../include/ProcReader.hpp"
#include "../include/SystemMemory.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>  // getpid()

// ---------------------------------------------------------------------------
// Minimal test framework (same macros as unit tests)
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
// Integration tests — run against the live Linux /proc filesystem
// ===================================================================

/// Test that we can list processes and get at least one result.
void test_list_processes_returns_results() {
    auto procs = ProcReader::listAllProcesses();
    ASSERT_TRUE(!procs.empty());

    // At minimum, the current process (this test) should be in the list.
    pid_t my_pid = getpid();
    bool found_self = false;
    for (const auto& p : procs) {
        if (p.pid == static_cast<int>(my_pid)) {
            found_self = true;
            break;
        }
    }
    ASSERT_TRUE(found_self);
}

/// Test that we can read our own process information.
void test_read_own_process() {
    pid_t my_pid = getpid();
    auto info = ProcReader::readProcess(static_cast<int>(my_pid));

    ASSERT_TRUE(info.has_value());
    ASSERT_EQ(info->pid, static_cast<int>(my_pid));
    ASSERT_FALSE(info->name.empty());
    ASSERT_TRUE(info->state.has_value());
    // Our process must have some virtual memory
    ASSERT_TRUE(info->vm_size_kib.has_value());
    ASSERT_TRUE(*info->vm_size_kib > 0u);
    ASSERT_TRUE(info->vm_rss_kib.has_value());
    ASSERT_TRUE(*info->vm_rss_kib > 0u);
    // Thread count should be at least 1
    ASSERT_TRUE(info->threads.has_value());
    ASSERT_TRUE(*info->threads >= 1);
}

/// Test that reading PID 1 (init/systemd) succeeds — it always exists.
void test_read_pid_1() {
    auto info = ProcReader::readProcess(1);

    // PID 1 should always be readable (even if some fields are restricted)
    ASSERT_TRUE(info.has_value());
    ASSERT_EQ(info->pid, 1);
    ASSERT_FALSE(info->name.empty());
}

/// Test that reading a nonexistent PID returns nullopt.
void test_read_nonexistent_pid() {
    // PID 0 is the kernel scheduler — /proc/0 does not exist on Linux.
    // A very high PID is also unlikely to exist.
    auto info = ProcReader::readProcess(999999999);
    ASSERT_FALSE(info.has_value());
}

/// Test that reading a negative PID returns nullopt.
void test_read_negative_pid() {
    auto info = ProcReader::readProcess(-1);
    ASSERT_FALSE(info.has_value());
}

/// Test system memory info reading.
void test_read_system_memory() {
    auto mem = SystemMemory::readMemoryInfo();

    ASSERT_TRUE(mem.has_value());
    ASSERT_TRUE(mem->mem_total_kib.has_value());
    ASSERT_TRUE(*mem->mem_total_kib > 0u);
    // MemFree and MemAvailable should always be present on modern kernels
    ASSERT_TRUE(mem->mem_free_kib.has_value());
    ASSERT_TRUE(mem->mem_available_kib.has_value());
    // Sanity: MemFree <= MemTotal
    ASSERT_TRUE(*mem->mem_free_kib <= *mem->mem_total_kib);
    // Sanity: MemAvailable <= MemTotal
    ASSERT_TRUE(*mem->mem_available_kib <= *mem->mem_total_kib);
}

/// Test top-by-memory returns sorted results.
void test_top_by_memory_sorted() {
    auto top = ProcReader::topByMemory(5);

    ASSERT_TRUE(!top.empty());

    // Verify descending sort order
    for (size_t i = 1; i < top.size(); ++i) {
        ASSERT_TRUE(top[i - 1].vm_rss_kib.has_value());
        ASSERT_TRUE(top[i].vm_rss_kib.has_value());
        ASSERT_TRUE(*top[i - 1].vm_rss_kib >= *top[i].vm_rss_kib);
    }
}

/// Test top-by-memory respects limit.
void test_top_by_memory_limit() {
    auto top3 = ProcReader::topByMemory(3);
    auto top10 = ProcReader::topByMemory(10);

    ASSERT_TRUE(top3.size() <= 3);
    ASSERT_TRUE(top10.size() <= 10);
    ASSERT_TRUE(top3.size() <= top10.size());
}

/// Test that all processes in the list have valid PIDs (> 0).
void test_all_pids_positive() {
    auto procs = ProcReader::listAllProcesses();
    for (const auto& p : procs) {
        ASSERT_TRUE(p.pid > 0);
    }
}

/// Test that memory formatter produces consistent output with live data.
void test_formatter_with_live_data() {
    auto mem = SystemMemory::readMemoryInfo();
    ASSERT_TRUE(mem.has_value());
    ASSERT_TRUE(mem->mem_total_kib.has_value());

    std::string formatted = Formatter::formatMemoryKiB(*mem->mem_total_kib);
    // Should contain a unit
    ASSERT_TRUE(formatted.find("KiB") != std::string::npos ||
                formatted.find("MiB") != std::string::npos ||
                formatted.find("GiB") != std::string::npos);
}

// ===================================================================
// Main test runner
// ===================================================================

int main() {
    std::cout << "=== Integration Tests: Live Linux System ===\n\n";

    std::cout << "--- Process Listing ---\n";
    TEST(list_processes_returns_results);
    TEST(all_pids_positive);

    std::cout << "\n--- Process Reading ---\n";
    TEST(read_own_process);
    TEST(read_pid_1);
    TEST(read_nonexistent_pid);
    TEST(read_negative_pid);

    std::cout << "\n--- System Memory ---\n";
    TEST(read_system_memory);
    TEST(formatter_with_live_data);

    std::cout << "\n--- Top by Memory ---\n";
    TEST(top_by_memory_sorted);
    TEST(top_by_memory_limit);

    std::cout << "\n=== Results: " << g_tests_passed << "/" << g_tests_run
              << " passed, " << g_tests_failed << " failed ===\n";

    return (g_tests_failed == 0) ? 0 : 1;
}
