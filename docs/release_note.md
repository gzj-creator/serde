# 发版说明

本文件按时间顺序记录已发布版本，最新版本追加在文件末尾。

## v0.2.0 - 2026-09-19

- 版本级别：次版本（minor）
- Git 提交消息：`feat: 模块改为实现头+门面双形态，新增 CMake 安装与 Bazel 支持`
- git tag：`v0.2.0`

## v0.2.1 - 2026-09-20

- 版本级别：小版本（trivial）
- Git 提交消息：`feat: simdjson 后端支持共享库链接并新增冒烟测试`
- git tag：`v0.2.1`

### 摘要

自 `v0.2.0` 以来的累计变更：

- **共享库链接支持**：`serde_simdjson` 后端改为启用 PIC 的库，可通过
  `-DSERDE_BUILD_SHARED_LIBS=ON` 构建为共享库；Windows 上自动带上
  `SIMDJSON_BUILDING/USING_WINDOWS_DYNAMIC_LIBRARY` 相关宏。
- **共享库冒烟测试**：新增 `test/shared_consumer.cpp` 与 `test/shared_smoke.cpp`，
  覆盖静态库与共享库两种构建下把后端链接进下游共享库的路径。
- **Bazel 测试入口**：新增 `//:header_smoke` 目标，支持 `bazel test` 冒烟验证。
- **构建兼容性**：`src/json/json.hpp` 用 `__has_include` 探测 simdjson 头文件，
  兼容 Bazel 虚拟 include 目录；`test/module_smoke.cpp` 调整 include/import 顺序以兼容 GCC。
- **文档**：README 补充后端 PIC / 共享库构建说明与 Bazel 测试用法。

### 摘要

首个可安装版本的累计变更：

- **双形态模块结构**：`reflect`/`toml`/`json` 实现全部迁入头文件
  （`reflect.hpp`/`toml.hpp`/`json.hpp`），经典 TU 直接 include 即可使用，
  无需模块工具链；`.cppm` 改为薄门面，经 `export extern "C++"` 复发布同名模块，
  共享 `module_prelude.hpp` 全局模块片段。
- **json 与 toml 解耦**：新增共享 `common` 词汇模块（`serde_common`：
  `common::date`/`time`/`local_date_time`/`offset_date_time`/`InlineTable`），
  两个格式经别名（`toml::date`、`json::date`）同源暴露、互不依赖。
- **CMake 安装支持**：`serde::serde` 头文件目标 + `serde_simdjson` 静态库 +
  可选 `serde_cpp23_modules` 模块目标，含 install/export 与 `serdeConfig.cmake`
  包配置；模块门面需 Clang >= 17 / GCC >= 15 + Ninja，不满足时自动退化为纯头文件。
- **Bazel 支持**：`MODULE.bazel`、根 `BUILD.bazel` 头文件 target 与
  `third_party/simdjson/BUILD.bazel`。
- **核心能力**：内置 simdjson JSON 后端、编译器无关反射与 `REFLECT_FIELDS` 宏、
  JSON/TOML 资源限制与确定性序列化、SSE2 扫描快速路径、静态字段查找表、
  基准测试套件（对比 simdjson 与 toml++）。
- 版本号由 `0.1.0` 升至 `0.2.0`，`mcpp.toml`、`CMakeLists.txt`、`MODULE.bazel` 同步。