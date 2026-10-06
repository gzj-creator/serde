cmake_minimum_required(VERSION 3.28)
project(serde_installed_consumer LANGUAGES CXX)

find_package(serde CONFIG REQUIRED)
if(SERDE_EXPECT_NO_MODULES AND TARGET serde::serde-cpp23-modules)
    message(FATAL_ERROR "Module provider must not be exported when interface installation is disabled")
endif()
enable_testing()

if(MSVC)
    add_compile_options(/UNDEBUG)
else()
    add_compile_options(-UNDEBUG)
endif()

add_executable(header_smoke header_smoke.cpp)
target_link_libraries(header_smoke PRIVATE serde::serde)
add_test(NAME installed.header_smoke COMMAND header_smoke)

foreach(contract_test IN ITEMS field_contract json_contract toml_stream_contract)
    add_executable("${contract_test}" "${contract_test}.cpp")
    target_link_libraries("${contract_test}" PRIVATE serde::serde)
    add_test(NAME "installed.${contract_test}" COMMAND "${contract_test}")
endforeach()

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
