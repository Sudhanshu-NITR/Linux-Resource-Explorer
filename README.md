# Linux Process and Memory Explorer

A command-line Linux utility that inspects running processes and reports useful process and memory statistics by reading the Linux `/proc` pseudo-filesystem directly.

Built in C++17 to demonstrate practical understanding of Linux process management, memory management, system interfaces, and modern C++ programming — suitable for a systems software engineering interview.

## Motivation

Systems software engineers need to understand how the Linux kernel exposes runtime information to user-space programs. This project reads `/proc` files directly — the same data source that tools like `ps`, `top`, and `free` use internally — and demonstrates:

- How the Linux `/proc` pseudo-filesystem works
- How to parse kernel-exported data defensively
- How virtual memory, resident memory, and system memory statistics relate
- How to handle race conditions (processes exiting mid-inspection)
- Modern C++17 patterns: `std::optional`, RAII, `const` correctness

## Features

| Command | Description |
|---------|-------------|
| `list` | List all processes with PID, name, state, VSZ, and RSS |
| `inspect <PID>` | Show detailed info for a specific process |
| `top-mem [N]` | Show the N processes with the highest RSS (default: 10) |
| `memory` | Show system-wide memory statistics from `/proc/meminfo` |
| `help` / `--help` | Show help and usage information |

## Architecture Overview

```
┌──────────────────────────────────────────────────────┐
│                     main.cpp                         │
│  (CLI argument parsing + command dispatch)           │
├──────────────────┬───────────────┬────────────────────┤
│   ProcReader     │ SystemMemory  │    Formatter       │
│  (reads /proc/   │ (reads /proc/ │  (formats memory   │
│   PID/status)    │  meminfo)     │   & optional vals) │
├──────────────────┴───────────────┴────────────────────┤
│                   ProcessInfo.hpp                     │
│          (data model with std::optional)              │
└──────────────────────────────────────────────────────┘
         ▼                     ▼
    /proc/<PID>/status    /proc/meminfo
```

## Prerequisites

- **Linux** (tested on Fedora 44)
- **C++17 compiler** (GCC 10+ or Clang 10+)
- **CMake** 3.16+

No external libraries, root privileges, or package installations required.

## Build Instructions

```bash
# Create build directory and configure
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Compile
cmake --build .
```

The executable is at `build/proc_explorer`.

## Usage

### List all processes

```bash
./build/proc_explorer list
```

```
PID      NAME                     STATE   VSZ           RSS
1        systemd                  S       44.3 MiB      22.7 MiB
2        kthreadd                 S       N/A           N/A
842      brave                    S       1448.7 GiB    546.7 MiB
...
Total: 450 processes
```

### Inspect a specific process

```bash
./build/proc_explorer inspect 1
```

```
Process Details for PID 1
=======================================
  Name:        systemd
  PID:         1
  State:       S
  PPID:        0
  VmSize:      44.3 MiB
  VmRSS:       22.7 MiB
  VmPeak:      44.3 MiB
  VmHWM:       22.7 MiB
  Threads:     1
```

### Top processes by memory

```bash
./build/proc_explorer top-mem 5
```

```
Top 5 processes by RSS (Resident Set Size):

RANK   PID      NAME                     RSS           VSZ
1      30170    brave                    713.8 MiB     1473.2 GiB
2      3393     brave                    646.2 MiB     53.8 GiB
3      25630    antigravity-ide          549.7 MiB     1411.9 GiB
...

Note: RSS includes shared pages (libraries, etc.) and does not
represent the private physical memory used by each process.
```

### System memory summary

```bash
./build/proc_explorer memory
```

```
System Memory Summary
=====================
  MemTotal:       15.0 GiB
  MemFree:        430.8 MiB
  MemAvailable:   3.9 GiB
  Buffers:        628 KiB
  Cached:         4.0 GiB
  SwapTotal:      8.0 GiB
  SwapFree:       7.8 GiB
```

### Help

```bash
./build/proc_explorer help
./build/proc_explorer --help
```

## Testing

```bash
cd build

# Run all tests via CTest
ctest --output-on-failure

# Or run individually
./test_proc_parsing     # 23 unit tests (parsing, formatting)
./test_integration      # 10 integration tests (live /proc)
```

**Unit tests** use controlled text fixtures — no live system dependency.
**Integration tests** run against the live `/proc` filesystem without depending on specific PIDs or memory values.

## Project Structure

```
├── CMakeLists.txt              # Build configuration
├── README.md                   # This file
├── .gitignore
├── docs/
│   ├── ARCHITECTURE.md         # Design and execution flow
│   ├── CONCEPTS.md             # Linux/C++ concepts explained
│   ├── INTERVIEW_QUESTIONS.md  # 30 interview Q&A
│   ├── DEBUGGING_GUIDE.md      # Debugging techniques
│   └── CODE_WALKTHROUGH.md     # Source code walkthrough
├── include/
│   ├── ProcessInfo.hpp         # Process data model
│   ├── ProcReader.hpp          # /proc reader API
│   ├── SystemMemory.hpp        # System memory API
│   └── Formatter.hpp           # Output formatting
├── src/
│   ├── main.cpp                # CLI entry point and command handlers
│   ├── ProcReader.cpp          # Process reading and parsing
│   ├── SystemMemory.cpp        # /proc/meminfo parsing
│   └── Formatter.cpp           # Memory and value formatting
└── tests/
    ├── test_proc_parsing.cpp   # 23 unit tests
    └── test_integration.cpp    # 10 integration tests
```

## Limitations

1. **RSS is not private memory.** RSS includes shared library pages. Multiple processes sharing `libc.so` each count its pages in their RSS.
2. **Snapshot, not real-time.** All values are point-in-time snapshots from `/proc`. Values change between reads.
3. **TOCTOU race conditions.** A process may exit between directory listing and file reading. The tool handles this gracefully (skips the process) but cannot eliminate the race.
4. **No CPU usage.** CPU utilization requires sampling `/proc/<PID>/stat` over time intervals; this is not implemented.
5. **No per-thread stats.** Thread-level information is available under `/proc/<PID>/task/` but not currently read.
6. **Kernel threads have no memory data.** Kernel threads (kworker, ksoftirqd, etc.) lack VmSize/VmRSS — displayed as "N/A".
7. **Linux-only.** The `/proc` filesystem is a Linux-specific interface.

## What This Project Demonstrates

✅ Reading Linux `/proc` pseudo-filesystem  
✅ Defensive parsing of OS-provided data  
✅ `std::optional` for representing missing values  
✅ RAII resource management  
✅ Structured CLI with input validation and error handling  
✅ Separation of I/O from parsing for testability  
✅ Unit and integration testing without external frameworks  
✅ Clear, maintainable C++17 code  

<!-- ## What This Project Does NOT Demonstrate

❌ Kernel module development  
❌ GPU/CUDA programming  
❌ Real-time monitoring or daemons  
❌ Multi-threaded data collection  
❌ Network or IPC interfaces  

## Documentation

| Document | Purpose |
|----------|---------|
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Design decisions, execution flow, error propagation |
| [CONCEPTS.md](docs/CONCEPTS.md) | Linux and C++ concepts explained for beginners |
| [INTERVIEW_QUESTIONS.md](docs/INTERVIEW_QUESTIONS.md) | 30 technical interview questions with answers |
| [DEBUGGING_GUIDE.md](docs/DEBUGGING_GUIDE.md) | How to debug, inspect warnings, and diagnose issues |
| [CODE_WALKTHROUGH.md](docs/CODE_WALKTHROUGH.md) | Line-by-line explanation of every major source file | -->
