cmake_minimum_required(VERSION 3.16)
project(LinuxProcessExplorer VERSION 1.0.0 LANGUAGES CXX)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable comprehensive compiler warnings
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -Wnon-virtual-dtor
    -Woverloaded-virtual
    -Wcast-align
    -Wunused
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
)

# Main executable
add_executable(proc_explorer
    src/main.cpp
    src/ProcReader.cpp
    src/SystemMemory.cpp
    src/Formatter.cpp
)

target_include_directories(proc_explorer PRIVATE include)

# Enable testing
enable_testing()

# Unit tests
add_executable(test_proc_parsing tests/test_proc_parsing.cpp src/ProcReader.cpp src/SystemMemory.cpp src/Formatter.cpp)
target_include_directories(test_proc_parsing PRIVATE include)
add_test(NAME UnitTests_ProcParsing COMMAND test_proc_parsing)

# Integration tests
add_executable(test_integration tests/test_integration.cpp src/ProcReader.cpp src/SystemMemory.cpp src/Formatter.cpp)
target_include_directories(test_integration PRIVATE include)
add_test(NAME IntegrationTests COMMAND test_integration)
