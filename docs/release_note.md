# 发版说明

本文件按时间顺序记录已发布版本，最新版本追加在文件末尾。

## v0.2.0 - 2026-09-19

- 版本级别：次版本（minor）
- Git 提交消息：`feat: 模块改为实现头+门面双形态，新增 CMake 安装与 Bazel 支持`
- git tag：`v0.2.0`

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