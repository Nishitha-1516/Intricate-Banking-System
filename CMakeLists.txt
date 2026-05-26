cmake_minimum_required(VERSION 3.16)
project(IntricateBankingSystem VERSION 1.0.0 LANGUAGES CXX)

# ── C++17 standard ──────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Build type ───────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ── Compiler flags ───────────────────────────────────────────
set(CMAKE_CXX_FLAGS_DEBUG   "-g -O0 -Wall -Wextra -Wpedantic -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")

# ── Include directories ──────────────────────────────────────
include_directories(${PROJECT_SOURCE_DIR}/include)

# ── Source files (library) ───────────────────────────────────
set(IBS_SOURCES
    src/transaction.cpp
    src/account.cpp
    src/customer.cpp
    src/fraud_detector.cpp
    src/bank.cpp
    src/benchmarker.cpp
)

# ── Static library ───────────────────────────────────────────
add_library(ibs_lib STATIC ${IBS_SOURCES})
target_compile_features(ibs_lib PUBLIC cxx_std_17)

# Thread support
find_package(Threads REQUIRED)
target_link_libraries(ibs_lib PUBLIC Threads::Threads)

# Filesystem support (GCC < 9 needs -lstdc++fs)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "9")
    target_link_libraries(ibs_lib PUBLIC stdc++fs)
endif()

# ── Main executable ──────────────────────────────────────────
add_executable(ibs src/main.cpp)
target_link_libraries(ibs PRIVATE ibs_lib)

# ── Test executable ──────────────────────────────────────────
add_executable(ibs_tests tests/test_suite.cpp)
target_link_libraries(ibs_tests PRIVATE ibs_lib)

# ── Benchmark executable ─────────────────────────────────────
add_executable(ibs_bench benchmarks/bench_main.cpp)
target_link_libraries(ibs_bench PRIVATE ibs_lib)

# ── CTest integration ────────────────────────────────────────
enable_testing()
add_test(NAME UnitTests COMMAND ibs_tests)

# ── Output directories ───────────────────────────────────────
set_target_properties(ibs ibs_tests ibs_bench
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ── Install ──────────────────────────────────────────────────
install(TARGETS ibs DESTINATION bin)
install(DIRECTORY include/ DESTINATION include/ibs)

# ── Summary ──────────────────────────────────────────────────
message(STATUS "")
message(STATUS "=== Intricate Banking System Build Config ===")
message(STATUS "  Build type  : ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler    : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Source dir  : ${PROJECT_SOURCE_DIR}")
message(STATUS "  Binary dir  : ${CMAKE_BINARY_DIR}")
message(STATUS "==============================================")
message(STATUS "")
