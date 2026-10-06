cmake_minimum_required(VERSION 3.28)
project(serde_installed_consumer LANGUAGES CXX)

find_package(serde CONFIG REQUIRED)
enable_testing()

if(MSVC)
    add_compile_options(/UNDEBUG)
else()
    add_compile_options(-UNDEBUG)
endif()

add_executable(header_smoke header_smoke.cpp)
target_link_libraries(header_smoke PRIVATE serde::serde)
add_test(NAME installed.header_smoke COMMAND header_smoke)

if(TARGET serde::serde-cpp23-modules)
    add_executable(module_smoke module_smoke.cpp)
    set_target_properties(module_smoke PROPERTIES
        CXX_EXTENSIONS OFF CXX_SCAN_FOR_MODULES ON)
    target_link_libraries(module_smoke PRIVATE serde::serde-cpp23-modules)
    add_test(NAME installed.module_smoke COMMAND module_smoke)
    add_executable(module_smoke_second module_smoke.cpp)
    set_target_properties(module_smoke_second PROPERTIES
        CXX_EXTENSIONS OFF CXX_SCAN_FOR_MODULES ON)
    target_link_libraries(module_smoke_second PRIVATE serde::serde-cpp23-modules)
    add_test(NAME installed.module_smoke_second COMMAND module_smoke_second)
endif()
