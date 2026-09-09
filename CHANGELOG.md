# 变更日志

本文档记录项目中值得关注的变更。

## 维护说明

- 版本号遵循语义化版本规则：不兼容变更升级主版本，新增功能升级次版本，修复和小范围维护升级修订版本。
- 每次提交前更新本文件；尚未发布的内容记录在 `Unreleased` 节，发布时再归入带日期的版本节。
- 发布版本使用 `## [vX.Y.Z] - YYYY-MM-DD` 作为标题格式。
- 按新增、变更、修复、文档或维护归纳重要内容，不逐行罗列代码差异。

## [Unreleased]

### 新增

- 抽出可复用的 `reflect` module 与 `REFLECT_*` 宏，TOML 和后续 JSON 等序列化器共享同一字段描述符。
- 增加无第三方依赖的 `json` module，复用反射字段描述，提供严格 RFC 8259 解析、确定性序列化、资源限制和完整测试。
- 移除 TOML 专用 `toml_reflect.hpp` 和 `TOML_REFLECT_*` 兼容入口，统一使用通用反射接口。
- 增加嵌套数组表、严格数字词法、重复表定义、内联表重开、UTF-8/控制字符和 GCC 16.1 静态反射探针测试。
- 新增基准测试套件，对比 serde 与 simdjson 4.6.9、toml++ 3.4.0 的性能表现。
- 新增 JSON 解析器边界回归测试与 TOML 解析器边界回归测试。
- 新增项目级 README.md 与 docs/ 文档目录（含 json、reflect、toml 模块说明）。

### 修复

- 加强 TOML 日期时间、字符串、键、数组和表状态校验，拒绝超出规范的词法与结构输入。

### 变更

- 将 `reflect` 模块中的 concept 和函数重命名为 PascalCase（`hasReflectFields`、`Reflectable`、`getFields`），保持与 C++ 标准库命名风格一致。
- 将 `toml` 模块中的枚举、结构体和 concept 重命名为 PascalCase（`UnknownFieldPolicy`、`ParseOptions`、`SerializeOptions`、`InlineTable`、`Reflectable`），同时保留旧名称的 type alias 以维持向后兼容。
- 为 JSON/TOML 的空白符、字符串和 ASCII UTF-8 扫描增加 SSE2 快速路径。
- 删除 `probes/gcc_reflection.cpp`、`src/reflect/README.md`、`src/toml/README.md` 等不再使用的文件。

### 文档

- 记录 GCC 16.1 `std::meta`/`-freflection` 的可行性，以及 GCC BMI、libstdc++/libc++ 与 LLVM 消费边界。
- 将源码、反射宏、示例、测试和 TOML 样例中的说明性注释统一为中文。
- 为核心函数、模板函数、解析器、序列化与反序列化 API 以及测试入口补充 Doxygen 规范标注。

### 维护

- 将 `mcpp.toml` 中的包名从 `demo` 修正为 `serde`，使包元数据与项目名称保持一致。
- 移除未被其他目标使用的 `demo.common` 示例模块，并将应用入口精简为无业务逻辑的最小程序。
- 忽略编辑器配置目录和本地生成的编译命令数据库，避免将开发环境文件纳入版本控制。
