cmake_minimum_required(VERSION 3.28)

project(serde VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# 选项
# ---------------------------------------------------------------------------
option(SERDE_BUILD_CPP23_MODULES "Build the C++23 module interfaces (import reflect/toml/json)" ON)
option(SERDE_INSTALL_CPP23_MODULE_INTERFACES
    "Install the .cppm module interface sources and module_prelude.hpp" ON)
option(SERDE_BUILD_SHARED_LIBS "Build shared libraries" OFF)
set(BUILD_SHARED_LIBS OFF)
if(SERDE_BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS ON)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

# 仅模块目标开启依赖扫描；头文件 TU 无需扫描（GCC 的 module mapper 会
# 因 build-tree 的 include 软链误报 include-translate，见 test 目录）。
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# ---------------------------------------------------------------------------
# Build-tree include shim
#
# 源码树与安装树使用同一相对布局，消费方代码在两种布局下都写
# `#include <serde/reflect/reflect.hpp>`：
#   - BUILD_INTERFACE:  ${CMAKE_BINARY_DIR}/include/serde -> src
#   - INSTALL_INTERFACE: <prefix>/${CMAKE_INSTALL_INCLUDEDIR}/serde
# json.hpp 经相对路径引用 simdjson（../../third_party/simdjson/simdjson.h），
# 安装时 simdjson 落在 include/third_party/simdjson，与源码树几何一致。
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/include)
file(REMOVE_RECURSE ${CMAKE_BINARY_DIR}/include/serde)
file(CREATE_LINK
    ${PROJECT_SOURCE_DIR}/src
    ${CMAKE_BINARY_DIR}/include/serde
    SYMBOLIC
    COPY_ON_ERROR
)

# ---------------------------------------------------------------------------
# simdjson 后端
# ---------------------------------------------------------------------------
add_library(serde_simdjson third_party/simdjson/simdjson.cpp)
add_library(serde::serde_simdjson ALIAS serde_simdjson)
# Static consumers may also link this backend into a shared library.
set_target_properties(serde_simdjson PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(WIN32 AND SERDE_BUILD_SHARED_LIBS)
    target_compile_definitions(serde_simdjson
        PRIVATE SIMDJSON_BUILDING_WINDOWS_DYNAMIC_LIBRARY=1
        INTERFACE SIMDJSON_USING_WINDOWS_DYNAMIC_LIBRARY=1
    )
endif()
target_compile_features(serde_simdjson PUBLIC cxx_std_17)
target_compile_options(serde_simdjson PRIVATE -fno-exceptions)
target_compile_definitions(serde_simdjson PUBLIC
    SIMDJSON_EXCEPTIONS=0
    nssv_CONFIG_NO_EXCEPTIONS=1
)
target_include_directories(serde_simdjson
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/third_party/simdjson>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/third_party/simdjson>
)

# ---------------------------------------------------------------------------
# serde 头文件目标（经典 TU 直接 include）
# ---------------------------------------------------------------------------
add_library(serde INTERFACE)
add_library(serde::serde ALIAS serde)
target_compile_features(serde INTERFACE cxx_std_23)
# 库以 -fno-exceptions 构建（mcpp.toml 同），异常配置必须向消费方传播，
# 否则模块消费者加载 BMI 时触发 module-file-config-mismatch。
target_compile_options(serde INTERFACE -fno-exceptions)
target_include_directories(serde
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(serde
    INTERFACE
        serde_simdjson
)

# ---------------------------------------------------------------------------
# C++23 模块目标（import reflect / import toml / import json）
# ---------------------------------------------------------------------------
set(SERDE_CPP23_MODULES_EFFECTIVE OFF)
if(SERDE_BUILD_CPP23_MODULES)
    if(CMAKE_VERSION VERSION_LESS 3.28)
        message(WARNING "SERDE_BUILD_CPP23_MODULES=ON requires CMake >= 3.28; skipping module file set")
    elseif(NOT CMAKE_GENERATOR MATCHES "Ninja|Visual Studio")
        message(WARNING "SERDE_BUILD_CPP23_MODULES=ON requires a C++ module-capable generator; skipping module file set")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        message(WARNING "SERDE_BUILD_CPP23_MODULES=ON is not enabled for AppleClang; skipping module file set")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 17)
        message(WARNING "serde module interfaces need Clang >= 17; skipping module file set")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15)
        message(WARNING "serde module interfaces need GCC >= 15; skipping module file set")
    else()
        set(SERDE_CPP23_MODULES_EFFECTIVE ON)
    endif()
endif()

if(SERDE_CPP23_MODULES_EFFECTIVE)
    add_library(serde-cpp23-modules OBJECT)
    add_library(serde::serde_cpp23_modules ALIAS serde-cpp23-modules)
    set_target_properties(serde-cpp23-modules PROPERTIES
        POSITION_INDEPENDENT_CODE ${BUILD_SHARED_LIBS}
    )
    # 显式固定方言：模块 BMI 以扩展关闭的 -std=c++23 编译，消费方 TU 若仍走
    # gnu++23 会触发 module-file-config-mismatch（GNU extensions 不一致）。
    # cxx_std_23 feature 仍必须保留：CMake 对模块重编译目标校验需其含
    # cxx_std_20+；显式 -std=c++23 选项位于 feature 之后，最终覆盖 gnu++23。
    if(MSVC)
        target_compile_options(serde-cpp23-modules PUBLIC /std:c++23)
    else()
        target_compile_options(serde-cpp23-modules PUBLIC -std=c++23)
    endif()
    target_compile_features(serde-cpp23-modules PUBLIC cxx_std_23)
    # -fno-exceptions 必须向模块消费方传播，否则 TU 加载 -fno-exceptions 的 BMI
    # 时触发 module-file-config-mismatch。
    target_compile_options(serde-cpp23-modules PUBLIC -fno-exceptions)
    target_compile_definitions(serde-cpp23-modules PUBLIC
        SIMDJSON_EXCEPTIONS=0
        nssv_CONFIG_NO_EXCEPTIONS=1
    )
    target_include_directories(serde-cpp23-modules
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    target_sources(serde-cpp23-modules
        PUBLIC
            FILE_SET CXX_MODULES
            BASE_DIRS ${PROJECT_SOURCE_DIR}/src
            FILES
                src/common/common.cppm
                src/reflect/reflect.cppm
                src/toml/toml.cppm
                src/json/json.cppm
    )
    set_target_properties(serde-cpp23-modules PROPERTIES CXX_SCAN_FOR_MODULES ON)
    target_link_libraries(serde-cpp23-modules PRIVATE serde_simdjson)
endif()

# ---------------------------------------------------------------------------
# 安装
# ---------------------------------------------------------------------------
# 模块目标带有 PUBLIC CXX_MODULES 文件集，导出时必须完整安装其文件集，
# 故单独处理；头文件目标与 simdjson 走通用 install。
set(SERDE_INSTALL_TARGETS serde_simdjson serde)

install(TARGETS ${SERDE_INSTALL_TARGETS}
    EXPORT serdeTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 实现头 + 注册宏 + .cppm 门面 + 共享 prelude，目录结构镜像源码树。
install(DIRECTORY ${PROJECT_SOURCE_DIR}/src/common/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/serde/common
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.cppm"
)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/src/reflect/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/serde/reflect
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.cppm"
)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/src/toml/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/serde/toml
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.cppm"
)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/src/json/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/serde/json
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.cppm"
)
if(SERDE_INSTALL_CPP23_MODULE_INTERFACES)
    install(FILES ${PROJECT_SOURCE_DIR}/src/module_prelude.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/serde
    )
endif()
# simdjson 与 include/serde 的源码几何保持一致：json.hpp/json.cppm 相对
# 引用 ../../third_party/simdjson/simdjson.h，安装树解析到
# <prefix>/${CMAKE_INSTALL_INCLUDEDIR}/third_party/simdjson。
install(DIRECTORY ${PROJECT_SOURCE_DIR}/third_party/simdjson/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/third_party/simdjson
    FILES_MATCHING PATTERN "*.h" PATTERN "*.cpp" PATTERN "*.md"
)
install(FILES
    ${PROJECT_SOURCE_DIR}/third_party/simdjson/LICENSE.md
    ${PROJECT_SOURCE_DIR}/third_party/simdjson/README.md
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/third_party/simdjson
)

# 原生模块文件集安装（仅模块生效时）。
if(SERDE_CPP23_MODULES_EFFECTIVE AND SERDE_INSTALL_CPP23_MODULE_INTERFACES)
    install(TARGETS serde-cpp23-modules
        EXPORT serdeTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        FILE_SET CXX_MODULES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/serde
        CXX_MODULES_BMI DESTINATION ${CMAKE_INSTALL_LIBDIR}/serde/cxx-modules
    )
endif()

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/serdeConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/serdeConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/serde
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/serdeConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

if(SERDE_CPP23_MODULES_EFFECTIVE)
    install(EXPORT serdeTargets
        NAMESPACE serde::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/serde
        FILE serdeTargets.cmake
        CXX_MODULES_DIRECTORY cxx-modules
    )
else()
    install(EXPORT serdeTargets
        NAMESPACE serde::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/serde
        FILE serdeTargets.cmake
    )
endif()
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/serdeConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/serdeConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/serde
)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()
