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

## v0.2.2 - 2026-09-23

- 版本级别：修订版本（patch）
- Git 提交消息：`fix: 修复外部工程消费 serde 的构建与头文件导出，并发布 v0.2.2`
- git tag：`v0.2.2`

### 摘要

自 `v0.2.1` 以来的累计变更：

- **mcpp 外部消费**：补齐公开头文件搜索路径与 `include/serde` 源码链接，使外部消费者
  能直接使用反射注册宏和内置 simdjson；新增独立 mcpp 消费工程，覆盖结构体
  JSON/TOML 往返与缺失字段错误。
- **CMake 消费契约**：头文件与模块 target 不再强制向消费方传递 `-fno-exceptions`，
  模块跟随所在工程异常策略，simdjson 后端仍使用无异常接口；导出后端标准头文件根路径，
  支持与其他库共用同一份实现。
- **文档**：说明外部依赖头文件路径、异常编译契约和 mcpp 消费回归测试入口。
- **版本同步**：`CMakeLists.txt`、`MODULE.bazel`、`mcpp.toml` 包版本声明更新为 `0.2.2`。

## v0.3.0 - 2026-09-24

- 版本级别：次版本（minor）
- Git 提交消息：`feat: 新增 JSON 流式输出并缩短默认反序列化检查，发布 v0.3.0`
- git tag：`v0.3.0`

### 摘要

自 `v0.2.2` 以来的累计变更：

- **JSON 流式输出**：新增 `json::stream::StreamWriter`，通过同步 sink 逐段输出对象、数组、标量、经过校验的原始 JSON 和 DOM 子节点；`json::stream::serialize` 可直接遍历 C++ 值，无需构造中间 JSON 树。输出保留输入顺序，错误会停止后续写入，并支持现有序列化资源限制、pretty 格式和 writer 重置复用。
- **回归测试与示例**：补充 MCP JSON-RPC 使用示例，以及头文件和模块两条路径的流式输出回归测试。
- **反序列化检查**：输入放得进默认节点、字符串、键、数组和成员上限时，不再对整篇 DOM 做完整限制遍历；重复键和可能超深度的文档改为只扫描对象键与嵌套深度。更紧的上限仍走完整检查，`json::parse` 不变。成功解码时不再预先构造错误路径字符串 `"value"`。
- **基准记录**：`benchmark/README.md` 记录相对 `v0.2.2` 的反序列化探针数据。样本按 `config.json` 的形状生成，200 台服务器为 12315 字节，平均时间从 73 µs 降到 49 µs。
- **版本同步**：`CMakeLists.txt`、`MODULE.bazel`、`mcpp.toml` 包版本声明更新为 `0.3.0`。

## v0.4.0 - 2026-10-06

- 版本级别：次版本（minor）
- Git 提交消息：`feat: 新增跨格式字段契约与枚举编码，发布 v0.4.0`
- git tag：`v0.4.0`

### 摘要

自 `v0.3.0` 以来的累计变更：

- **通用字段契约**：新增 `field_options<Member>`、三参数字段登记和公开校验，
  支持描述、精确数值范围、Unicode 标量长度及容器数量；无约束字段仍使用轻量
  name+pointer 布局和静态空 options，带约束描述符不能隐式切片。
- **枚举编码**：新增 ADL descriptor、字符串或实际底层值编码和合法集合，
  公开映射 helper 用同一快照校验与查找。非法/重复/空/无效 UTF-8 元数据和
  已登记枚举中的未知值显式失败，不缓存或忽略运行期损坏。
- **跨格式执行**：JSON DOM、TOML 编解码及 JSON StreamWriter 实际执行约束；
  optional 缺失保留并校验对象值，JSON null 清空，TOML 空值省略。
  所有校验失败返回 `std::expected`，流式首个错误在后续写入/status/finish 保留。
- **公开 JSON 绑定接口**：新增 `decode_field` 和 `decode_fields_into`，
  全字段与选择解码共享既有索引查找，严格策略仅允许选择集合；动态描述符重排
  和 name/get 自定义描述符均有跨格式回归。失败对象可能已部分赋值，需丢弃。
- **安装修复**：收束上个 tag 后的模块静态库、初始化符号、安装依赖元数据和
  多 consumer BMI 重建修复。原生模块消费要求 CMake >= 3.31；关闭模块接口
  安装时仍可用 CMake 3.28 消费公开头文件。
- **测试与文档**：新增字段/JSON/TOML-stream 契约测试、安装与 module 消费验证、
  使用说明及不依赖 import std 的 CMake 基准入口；三套包版本同步为 `0.4.0`。

### 验收

- GCC14 Release `-fno-exceptions` CTest 7/7，Clang22 module/header CTest 8/8，
  安装后包含两个 module consumer 的测试 6/6，Galay serde 消费回归 12/12。
- 候选安装包的头文件消费 ASan/UBSan 4/4 通过；未新增生产异常控制流或同步锁。
- 与 `bb02aeb` 用同一 GCC14 Release 夹具/编译选项配对，CPU0 绑定且不与构建并发。
  10轮全部合并中位数：既有 decode-only 变化为 -6.83% 至 +1.89%，端到端最坏
  +6.74%，均在预定10%门槛内；两项争议场景7轮加长测量也通过，未筛掉原样本。
- default/medium/large/4096键map 的 JSON/TOML 解码分配次数与字节完全不增加。
  6字段校验中位62.44ns，3值enum名称往返49.35ns，两项100万次成功路径均零堆分配。
  完整方法、波动及数据范围见 `benchmark/README.md`，不宣称任意负载绝对最优。
- 本次只发布 serde 数据契约扩展，HTTP/OpenAPI/HTTP2/WebSocket 尚未实施。
  Bazel 和 GCC15 module 未在本机验证；Clang22 验证使用系统 libstdc++及
  `--no-default-config`，不声称默认 mcpp 工具链或其他平台均已通过。

## v0.5.0 - 2026-10-10

- 版本级别：次版本（minor）
- Git 提交消息：`feat: 发布 JSON 协议字段映射与 Schema 构建 v0.5.0`
- git tag：`v0.5.0`

### 摘要

自 `v0.4.0` 以来的累计变更：

- **JSON 字段映射**：新增 `FieldPolicy` / `WireField`，支持原始 JSON、缺失字段、空值省略、对象/数组约束、布尔 presence-object 映射与 null 拒绝；原始 optional 值保留显式 null。
- **值与适配接口**：新增 `RawValue` / `Object`、variant 和动态 `Json` 编解码、`to_wire` / `wire_type` / `from_wire` 适配、`reflect_fields(std::type_identity<T>)` 登记，以及成员/路径解码、对象合并与遍历接口；普通反射与 TOML 保留既有字段契约。
- **JSON Schema 构建**：新增 `<serde/json/schema.hpp>` 的 `json::SchemaBuilder`，支持标量、数组、枚举、嵌套对象与必填字段，`encode()` 显式返回错误，并导出至 `serde_json` 模块。
- **生命周期与错误处理**：解码的动态 `Json` 独立持有存储，避免后续解析或借用子视图失效；动态值和原始数组编码执行资源限制，嵌套 Schema 构建错误继续向上报告。
- **版本同步**：`CMakeLists.txt`、`MODULE.bazel`、`mcpp.toml` 的包版本统一为 `0.5.0`。

### 验收

- Galay serde unit 13/13、安装消费者 1/1；上游无异常配置 6/6、Clang C++ modules 7/7（含 Schema 导出 smoke）通过，覆盖字段策略、variant、动态值生命周期、资源限制及 Schema。
- 构建与测试均串行执行；解析 benchmark 仅作小规模 smoke，不据此宣称性能改善。
- 发版时 CMake 重新配置确认生成的包版本为 `0.5.0`，无异常配置 6 项测试串行复跑通过，`git diff --check` 通过；本轮仅变更版本与发布记录，未重复大型构建。
- 未执行完整 Galay 测试、etcd shared-backend 集成、Bazel 或 mcpp 构建，也未验证其他平台。
