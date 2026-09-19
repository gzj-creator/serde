# 变更日志

本文档记录项目中值得关注的变更。

## 维护说明

- 版本号遵循语义化版本规则：不兼容变更升级主版本，新增功能升级次版本，修复和小范围维护升级修订版本。
- 每次提交前更新本文件；尚未发布的内容记录在 `Unreleased` 节，发布时再归入带日期的版本节。
- 发布版本使用 `## [vX.Y.Z] - YYYY-MM-DD` 作为标题格式。
- 按新增、变更、修复、文档或维护归纳重要内容，不逐行罗列代码差异。

## [Unreleased]

## [v0.2.0] - 2026-09-19

### 新增

- 抽出可复用的 `reflect` module 与 `REFLECT_FIELDS` 宏，TOML 和后续 JSON 等序列化器共享同一字段描述符。
- 增加 `json` module，复用反射字段描述，提供 RFC 8259 编解码、资源限制和测试；解析后端为内置 simdjson。
- 移除 TOML 专用 `toml_reflect.hpp` 和 `TOML_REFLECT_*` 兼容入口，统一使用通用反射接口。
- 增加嵌套数组表、严格数字词法、重复表定义、内联表重开、UTF-8/控制字符和 GCC 16.1 静态反射探针测试。
- 新增基准测试套件，对比 serde 与 simdjson 4.6.9、toml++ 3.4.0 的性能表现。
- 新增 JSON 解析器边界回归测试与 TOML 解析器边界回归测试。
- 新增 `json::Parser::reset` 与 `json::reset_thread_parser()`，用于释放复用 parser 的容量并作废旧文档。
- 新增项目级 README.md 与 docs/ 文档目录（含 json、reflect、toml 模块说明）。
- 新增 `reflect::StaticReflectable` 概念和 `static_fields<T>()` 接口，支持编译期字段名校验。
- 新增 JSON 反序列化静态字段查找表（`StaticFieldLookup`）和 FNV-1a 哈希函数，优化大型结构体的反序列化性能。
- 新增 `benchmark/compare.mjs` 脚本，支持对比两个 release 构建的性能差异。
- 新增 `benchmark/serde_json_wide.cpp`，测试 64 字段结构体的反序列化性能。
- 新增 TOML 赋值解析快速路径，提取 `bare_key_character()` 和 `parse_assignment_value()` 函数。
- 新增「实现头 + .cppm 门面」双形态结构：`reflect`/`toml`/`json` 实现全部迁入头文件（`reflect.hpp`/`toml.hpp`/`json.hpp`），经典 TU 直接 include 即可使用，无需模块工具链；`.cppm` 改为薄门面，经 `export extern "C++"` 复发布同名模块。
- 新增共享 `common` 词汇模块（`serde_common` 模块 / `common::date`、`common::time`、`common::local_date_time`、`common::offset_date_time`、`common::InlineTable`），TOML 与 JSON 经别名（`toml::date`、`json::date` 等）同源暴露。
- 新增 CMake 安装支持：`CMakeLists.txt` 提供头文件目标 `serde::serde`、simdjson 静态库 `serde::serde_simdjson`、可选 C++23 模块目标 `serde::serde_cpp23_modules`，含 install/export、`serdeConfig.cmake` 包配置与模块文件集；模块门面需 Clang >= 17 / GCC >= 15 + Ninja，不满足时自动退化为纯头文件目标。
- 新增 Bazel 支持：`MODULE.bazel`、根 `BUILD.bazel` 头文件 target 与 `third_party/simdjson/BUILD.bazel`。
- 新增 CMake 冒烟测试（头文件与模块两条消费路径）与共享 `module_prelude.hpp` 全局模块片段预置头。

### 修复

- 加强 TOML 日期时间、字符串、键、数组和表状态校验，拒绝超出规范的词法与结构输入。
- JSON 解析不再手写越过 `std::string::size()` 的 padding，改由 simdjson 拷贝输入；`Parser` 再次解析或 `reset` 后旧 `Json` 会失效。

### 变更

- 将 `reflect` 模块中的 concept 和函数重命名为 PascalCase（`hasReflectFields`、`Reflectable`、`getFields`），保持与 C++ 标准库命名风格一致。
- 将 `toml` 模块中的枚举、结构体和 concept 重命名为 PascalCase（`UnknownFieldPolicy`、`ParseOptions`、`SerializeOptions`、`InlineTable`、`Reflectable`），不再保留旧名称的兼容别名。
- JSON 解析改为 `json::Json` 包装 simdjson DOM；`ParseOptions` 以独立的 `duplicate_keys` 与 `enforce_document_limits` 控制重复键和文档上限。
- 库以 `-fno-exceptions` 构建；JSON/TOML 不再把内存耗尽转成 `expected`，TOML 编解码去掉已失效的 try/catch。
- `json::detail` 不再随模块导出，对外只保留公开 API。
- 为 JSON/TOML 的空白符、字符串和 ASCII UTF-8 扫描增加 SSE2 快速路径。
- 模块实现由纯 `.cppm` 改为「实现头 + `.cppm` 门面」，源码树与安装树保持同一相对布局；非模板外联定义补 `inline`，避免多 TU 包含触发 ODR。
- `json` 模块不再依赖 `toml` 模块，公共日期时间词汇统一收口到 `common` 模块，两个格式互不依赖。
- 版本号由 `0.1.0` 升至 `0.2.0`，`mcpp.toml`、`CMakeLists.txt` 与 `MODULE.bazel` 同步更新。
- 删除 `probes/gcc_reflection.cpp`、`src/reflect/README.md`、`src/toml/README.md` 等不再使用的文件。
- 删除 `reflect`、`toml`、`json` 的旧拼写兼容别名（`native_reflection_available`、`inline_table`、`parse_options`、`try_serialize`、`deserializee`、`deSerialize` 等），全量重构后统一使用 PascalCase 与标准命名。

### 文档

- 记录 GCC 16.1 `std::meta`/`-freflection` 的可行性，以及 GCC BMI、libstdc++/libc++ 与 LLVM 消费边界。
- 将源码、反射宏、示例、测试和 TOML 样例中的说明性注释统一为中文。
- 为核心函数、模板函数、解析器、序列化与反序列化 API 以及测试入口补充 Doxygen 规范标注。
- README.md 补充双形态结构、CMake 安装与 Bazel 消费说明。

### 维护

- 将 `mcpp.toml` 中的包名从 `demo` 修正为 `serde`，使包元数据与项目名称保持一致。
- 移除未被其他目标使用的 `demo.common` 示例模块和应用入口 `src/main.cpp`，包以静态库形式交付。
- 默认构建 profile 改回 debug；release 留给基准测试。
- 忽略编辑器配置目录和本地生成的编译命令数据库，避免将开发环境文件纳入版本控制。
- `.gitignore` 增加 `build/`，排除 CMake 构建产物。