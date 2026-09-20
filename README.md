# serde

C++ 序列化库，提供 TOML 和 JSON 编解码。JSON 解析由项目内的 simdjson 完成。

## 特性

- **内置 JSON 后端**：vendored simdjson 4.6.9 直接参与库构建，对外只暴露 `json::Json` 包装接口
- **编译器无关**：基于编译器无关的反射模块，支持 LLVM/Clang 和 GCC
- **确定性序列化**：输出格式规范、可预测
- **资源限制**：默认校验字节、深度、节点、字符串和容器上限；可用 `enforce_document_limits = false` 关闭整树校验
- **C++23**：语法和限制错误走 `std::expected`。库以 `-fno-exceptions` 构建，内存耗尽会终止进程

## 模块

| 模块 | 说明 | 文档 |
|------|------|------|
| `serde_common` | toml/json 共享的日期时间值类型与内联表包装（`common::date` 等） | — |
| `reflect` | 序列化器中立的反射词汇 | [reflect-README.md](docs/reflect-README.md) |
| `toml` | TOML 编码器/解码器 | [toml-README.md](docs/toml-README.md) |
| `json` | JSON 编码器/解码器 | [json-README.md](docs/json-README.md) |

`toml` 与 `json` 相互独立，均只依赖 `serde_common` 与 `reflect`；共享的
`common::date` / `common::time` / `common::local_date_time` /
`common::offset_date_time` / `common::InlineTable` 经别名（`toml::date`、
`json::date` 等）在各自命名空间暴露。

## 快速开始

```cpp
import toml;

namespace config {
struct server {
    std::string host;
    int port{};
};

#define SERVER_FIELDS(X) X(host) X(port)
REFLECT_FIELDS(server, SERVER_FIELDS)
#undef SERVER_FIELDS
}

config::server value{"localhost", 8080};
auto text = toml::serialize(value);  // 序列化
auto parsed = toml::deserialize<config::server>(*text);  // 反序列化
```

## 项目结构

每个模块采用「实现头 + .cppm 门面」的双形态结构：实现全部在头文件中，
经典 TU 直接 `#include`（无需模块工具链，低标准 C++ 编译器也能消费）；
`.cppm` 是薄门面，供支持 C++23 模块的消费者 `import`。同一程序内
include 与 import 二选一，不要混用。

```
src/
├── module_prelude.hpp   # 门面共享的全局模块片段预置头（标准库/SSE2）
├── common/
│   ├── common.hpp           # 公共词汇实现头（date/time/.../InlineTable）
│   └── common.cppm          # `import serde_common` 门面
├── reflect/
│   ├── reflect.hpp          # 反射实现头（经典 TU 直接 include）
│   ├── reflect_macros.hpp   # REFLECT_FIELDS / REFLECT_EMPTY 注册宏
│   └── reflect.cppm         # `import reflect` 门面
├── toml/
│   ├── toml.hpp             # TOML 实现头
│   └── toml.cppm            # `import toml` 门面
└── json/
    ├── json.hpp             # JSON 实现头（simdjson 后端）
    └── json.cppm            # `import json` 门面

third_party/
└── simdjson/   # 内置 JSON 后端，直接参与库构建

cmake/          # 包配置模板（serdeConfig.cmake.in）
test/           # CMake 冒烟测试（头文件 + 模块两条消费路径）
```

## 构建与安装

### mcpp（开发默认）

```bash
mcpp build            # 模块形态，默认 LLVM 22.1.8 工具链 + debug profile
mcpp test             # 全部 9 个测试
mcpp build --profile release   # 基准测试请用 release 或 benchmark/run.sh
```

### CMake（可安装）

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure   # 头文件 + 模块冒烟测试
cmake --install build --prefix /your/prefix
```

模块门面需要模块工具链（Clang >= 17 / GCC >= 15 + Ninja/VS），不满足时
CMake 自动退化为纯头文件目标（`-DSERDE_BUILD_CPP23_MODULES=OFF` 可强制关闭）。

simdjson 后端默认构建为启用 PIC 的静态库，也可链接到下游共享库。
`-DSERDE_BUILD_SHARED_LIBS=ON` 将后端构建为共享库。

### Bazel

`BUILD.bazel` 提供 `//:serde` 头文件 target；`//third_party/simdjson:simdjson`
提供 JSON 后端。`.cppm` 门面无法用 rules_cc 0.1.1 编译，Bazel 消费者走头文件。

```bash
bazel test //:header_smoke
```

## 消费方式

### 头文件（任何 C++23 编译器）

```cpp
#include <serde/reflect/reflect.hpp>
#include <serde/reflect/reflect_macros.hpp>
#include <serde/toml/toml.hpp>   // 或 <serde/json/json.hpp>
```

```cmake
find_package(serde REQUIRED)
target_link_libraries(app PRIVATE serde::serde)
```

### C++23 模块

```cpp
import std;
import toml;   // 或 import json；二者互不依赖，可同时导入
```

```cmake
find_package(serde REQUIRED)
target_link_libraries(app PRIVATE serde::serde-cpp23-modules serde::serde_simdjson)
```

> `import toml` / `import json` 各自经 `export import` 复发布 `reflect` 与
> `serde_common` 依赖；`toml` 与 `json` 相互独立，公共的日期时间类型
> （`common::date` 等）经 `toml::date` / `json::date` 别名暴露。

## 许可证

Apache-2.0。vendored simdjson 使用其上游许可证，见 `third_party/simdjson/LICENSE.md`。
