#include "Formatter.hpp"
#include "ProcReader.hpp"
#include "SystemMemory.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Command Handlers
// ---------------------------------------------------------------------------

/// Print usage information for every supported command.
static void printHelp(const std::string& program_name) {
    std::cout
        << "Linux Process and Memory Explorer\n"
        << "==================================\n\n"
        << "Usage: " << program_name << " <command> [arguments]\n\n"
        << "Commands:\n"
        << "  list              List all processes with PID, name, state, VSZ, and RSS.\n"
        << "  inspect <PID>     Show detailed information for a specific process.\n"
        << "  top-mem [N]       Show the N processes with the highest RSS (default: 10).\n"
        << "  memory            Show system-wide memory statistics from /proc/meminfo.\n"
        << "  help, --help      Show this help message.\n\n"
        << "Examples:\n"
        << "  " << program_name << " list\n"
        << "  " << program_name << " inspect 1\n"
        << "  " << program_name << " top-mem 5\n"
        << "  " << program_name << " memory\n\n"
        << "Notes:\n"
        << "  - All data is read from the Linux /proc pseudo-filesystem.\n"
        << "  - Values are snapshots and may change between reads.\n"
        << "  - Memory sizes from /proc use KiB (1024 bytes), displayed in\n"
        << "    human-readable units (KiB, MiB, GiB).\n"
        << "  - Processes that exit during inspection are skipped gracefully.\n"
        << "  - No root privileges are required.\n";
}

/// Feature A: List all processes.
static int cmdList() {
    auto processes = ProcReader::listAllProcesses();

    if (processes.empty()) {
        std::cerr << "Warning: no processes could be read from /proc.\n";
        return 1;
    }

    // Print table header
    std::cout << Formatter::padRight("PID", 9)
              << Formatter::padRight("NAME", 25)
              << Formatter::padRight("STATE", 8)
              << Formatter::padRight("VSZ", 14)
              << Formatter::padRight("RSS", 14)
              << "\n";

    for (const auto& p : processes) {
        std::cout << Formatter::padRight(std::to_string(p.pid), 9)
                  << Formatter::padRight(p.name, 25)
                  << Formatter::padRight(Formatter::formatOptionalState(p.state), 8)
                  << Formatter::padRight(Formatter::formatOptionalMemory(p.vm_size_kib), 14)
                  << Formatter::padRight(Formatter::formatOptionalMemory(p.vm_rss_kib), 14)
                  << "\n";
    }

    std::cout << "\nTotal: " << processes.size() << " processes\n";
    return 0;
}

/// Feature B: Inspect a single process.
static int cmdInspect(const std::string& pid_str) {
    // Validate PID argument
    int pid = 0;
    try {
        size_t pos = 0;
        long val = std::stol(pid_str, &pos);
        if (pos != pid_str.size() || val <= 0) {
            std::cerr << "Error: '" << pid_str << "' is not a valid PID. "
                      << "PID must be a positive integer.\n";
            return 1;
        }
        if (val > std::numeric_limits<int>::max()) {
            std::cerr << "Error: PID value " << pid_str << " is too large.\n";
            return 1;
        }
        pid = static_cast<int>(val);
    } catch (const std::exception&) {
        std::cerr << "Error: '" << pid_str << "' is not a valid PID.\n";
        return 1;
    }

    auto info = ProcReader::readProcess(pid);
    if (!info) {
        std::cerr << "Error: could not read process " << pid << ". "
                  << "The process may not exist or may have exited.\n";
        return 1;
    }

    std::cout << "Process Details for PID " << info->pid << "\n";
    std::cout << "=======================================\n";
    std::cout << "  Name:        " << info->name << "\n";
    std::cout << "  PID:         " << info->pid << "\n";
    std::cout << "  State:       " << Formatter::formatOptionalState(info->state) << "\n";
    std::cout << "  PPID:        " << Formatter::formatOptionalInt(info->ppid) << "\n";
    std::cout << "  VmSize:      " << Formatter::formatOptionalMemory(info->vm_size_kib) << "\n";
    std::cout << "  VmRSS:       " << Formatter::formatOptionalMemory(info->vm_rss_kib) << "\n";
    std::cout << "  VmPeak:      " << Formatter::formatOptionalMemory(info->vm_peak_kib) << "\n";
    std::cout << "  VmHWM:       " << Formatter::formatOptionalMemory(info->vm_hwm_kib) << "\n";
    std::cout << "  Threads:     " << Formatter::formatOptionalInt(info->threads) << "\n";

    return 0;
}

/// Feature C: Top processes by memory usage.
static int cmdTopMem(int limit) {
    auto top = ProcReader::topByMemory(limit);

    if (top.empty()) {
        std::cerr << "Warning: no processes with memory data found.\n";
        return 1;
    }

    std::cout << "Top " << top.size() << " processes by RSS (Resident Set Size):\n\n";

    std::cout << Formatter::padRight("RANK", 7)
              << Formatter::padRight("PID", 9)
              << Formatter::padRight("NAME", 25)
              << Formatter::padRight("RSS", 14)
              << Formatter::padRight("VSZ", 14)
              << "\n";

    int rank = 1;
    for (const auto& p : top) {
        std::cout << Formatter::padRight(std::to_string(rank), 7)
                  << Formatter::padRight(std::to_string(p.pid), 9)
                  << Formatter::padRight(p.name, 25)
                  << Formatter::padRight(Formatter::formatOptionalMemory(p.vm_rss_kib), 14)
                  << Formatter::padRight(Formatter::formatOptionalMemory(p.vm_size_kib), 14)
                  << "\n";
        ++rank;
    }

    std::cout << "\nNote: RSS includes shared pages (libraries, etc.) and does not\n"
              << "represent the private physical memory used by each process.\n";
    return 0;
}

/// Feature D: System memory summary.
static int cmdMemory() {
    auto mem = SystemMemory::readMemoryInfo();
    if (!mem) {
        std::cerr << "Error: could not read /proc/meminfo.\n";
        return 1;
    }

    std::cout << "System Memory Summary\n";
    std::cout << "=====================\n";
    std::cout << "  MemTotal:       " << Formatter::formatOptionalMemory(mem->mem_total_kib) << "\n";
    std::cout << "  MemFree:        " << Formatter::formatOptionalMemory(mem->mem_free_kib) << "\n";
    std::cout << "  MemAvailable:   " << Formatter::formatOptionalMemory(mem->mem_available_kib) << "\n";
    std::cout << "  Buffers:        " << Formatter::formatOptionalMemory(mem->buffers_kib) << "\n";
    std::cout << "  Cached:         " << Formatter::formatOptionalMemory(mem->cached_kib) << "\n";
    std::cout << "  SwapTotal:      " << Formatter::formatOptionalMemory(mem->swap_total_kib) << "\n";
    std::cout << "  SwapFree:       " << Formatter::formatOptionalMemory(mem->swap_free_kib) << "\n";

    std::cout << "\nNotes:\n"
              << "  - MemFree is RAM not used at all — it does NOT mean all other RAM\n"
              << "    is permanently consumed. The kernel uses free RAM for caches.\n"
              << "  - MemAvailable estimates how much memory is available for starting\n"
              << "    new applications, including reclaimable caches and buffers.\n"
              << "  - Do NOT compute 'used memory' as MemTotal - MemFree; much of the\n"
              << "    difference is cache that the kernel can release on demand.\n";

    return 0;
}

// ---------------------------------------------------------------------------
// Main entry point — command dispatch
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    // Default to help if no arguments given
    if (argc < 2) {
        printHelp(argv[0]);
        return 1;
    }

    std::string command = argv[1];

    // Feature E: help
    if (command == "help" || command == "--help" || command == "-h") {
        printHelp(argv[0]);
        return 0;
    }

    // Feature A: list
    if (command == "list") {
        if (argc > 2) {
            std::cerr << "Error: 'list' takes no arguments.\n";
            return 1;
        }
        return cmdList();
    }

    // Feature B: inspect
    if (command == "inspect") {
        if (argc < 3) {
            std::cerr << "Error: 'inspect' requires a PID argument.\n"
                      << "Usage: " << argv[0] << " inspect <PID>\n";
            return 1;
        }
        if (argc > 3) {
            std::cerr << "Error: 'inspect' accepts exactly one PID argument.\n";
            return 1;
        }
        return cmdInspect(argv[2]);
    }

    // Feature C: top-mem
    if (command == "top-mem") {
        int limit = 10; // sensible default
        if (argc > 3) {
            std::cerr << "Error: 'top-mem' accepts at most one argument (count).\n";
            return 1;
        }
        if (argc == 3) {
            std::string count_str = argv[2];
            try {
                size_t pos = 0;
                long val = std::stol(count_str, &pos);
                if (pos != count_str.size() || val <= 0) {
                    std::cerr << "Error: '" << count_str << "' is not a valid positive integer.\n";
                    return 1;
                }
                if (val > 10000) {
                    std::cerr << "Error: count " << val << " is unreasonably large. "
                              << "Maximum supported is 10000.\n";
                    return 1;
                }
                limit = static_cast<int>(val);
            } catch (const std::exception&) {
                std::cerr << "Error: '" << count_str << "' is not a valid number.\n";
                return 1;
            }
        }
        return cmdTopMem(limit);
    }

    // Feature D: memory
    if (command == "memory") {
        if (argc > 2) {
            std::cerr << "Error: 'memory' takes no arguments.\n";
            return 1;
        }
        return cmdMemory();
    }

    // Unknown command
    std::cerr << "Error: unknown command '" << command << "'.\n"
              << "Run '" << argv[0] << " help' for a list of commands.\n";
    return 1;
}
