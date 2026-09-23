# 头文件消费路径：任何 C++23 编译器可用，无需模块工具链。
add_executable(serde_header_smoke header_smoke.cpp)
target_link_libraries(serde_header_smoke PRIVATE serde::serde)
add_test(NAME serde_header_smoke COMMAND serde_header_smoke)

add_executable(serde_json_stream json_stream.cpp)
target_link_libraries(serde_json_stream PRIVATE serde::serde)
add_test(NAME serde_json_stream COMMAND serde_json_stream)

# Exercise backend linkage from a shared library in both static and shared builds.
add_library(serde_shared_consumer SHARED shared_consumer.cpp)
set_target_properties(serde_shared_consumer PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
target_link_libraries(serde_shared_consumer PRIVATE serde::serde)
add_executable(serde_shared_smoke shared_smoke.cpp)
target_link_libraries(serde_shared_smoke PRIVATE serde_shared_consumer)
add_test(NAME serde_shared_smoke COMMAND serde_shared_smoke)

# 模块消费路径：仅模块文件集生效时编译（Clang >= 17 / GCC >= 15 + Ninja）。
if(SERDE_CPP23_MODULES_EFFECTIVE)
    add_executable(serde_module_smoke module_smoke.cpp)
    set_target_properties(serde_module_smoke PROPERTIES CXX_SCAN_FOR_MODULES ON)
    target_link_libraries(serde_module_smoke PRIVATE
        serde::serde_cpp23_modules serde::serde_simdjson)
    add_test(NAME serde_module_smoke COMMAND serde_module_smoke)
endif()
