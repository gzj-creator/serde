cmake_minimum_required(VERSION 3.28)
project(serde_benchmarks LANGUAGES CXX)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

find_package(serde CONFIG REQUIRED)

foreach(track IN ITEMS json json_alloc json_wide toml toml_alloc)
    add_executable("benchmark_serde_${track}" "serde_${track}.cpp")
    target_compile_definitions("benchmark_serde_${track}" PRIVATE SERDE_BENCHMARK_HEADERS)
    target_link_libraries("benchmark_serde_${track}" PRIVATE serde::serde)
endforeach()

# Feature tracks are explicit targets so this harness also builds against older serde.
foreach(track IN ITEMS contract contract_alloc)
    add_executable("benchmark_serde_${track}" EXCLUDE_FROM_ALL "serde_${track}.cpp")
    target_compile_definitions("benchmark_serde_${track}" PRIVATE SERDE_BENCHMARK_HEADERS)
    target_link_libraries("benchmark_serde_${track}" PRIVATE serde::serde)
endforeach()
