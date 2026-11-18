# serde

C++ 序列化库，提供 TOML 和 JSON 编解码。JSON 解析由项目内的 simdjson 完成。

## 特性

- **内置 JSON 后端**：vendored simdjson 4.6.9 直接参与库构建，对外只暴露 `json::Json` 包装接口
- **普通结构体免注册**：同一个 C++23 简单聚合可直接读写 JSON 和 TOML
- **显式字段契约**：需要稳定协议时，用 `reflect_fields` 固定字段名单、外部名称与约束
- **确定性序列化**：输出格式规范、可预测
- **流式 JSON 输出**：`json::stream::StreamWriter` 按输入顺序向同步 sink 输出，支持手动构建、C++ 值和 DOM 遍历
- **资源限制**：默认校验字节、深度、节点、字符串和容器上限；可用 `enforce_document_limits = false` 关闭整树校验
- **C++23**：语法和限制错误走 `std::expected`。CMake 消费方保留自身异常编译策略；mcpp 独立开发默认 `-fno-exceptions`，此时内存耗尽会终止进程

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
#include <string>
#include <serde/json/json.hpp>
#include <serde/toml/toml.hpp>

struct Server {
    std::string host;
    int port{};
    bool operator==(const Server&) const = default;
};

int main() {
    const Server server{"localhost", 8080};
    const auto json_text = json::serialize(server);
    const auto toml_text = toml::serialize(server);
    if (!json_text || !toml_text) return 1;

    const auto from_json = json::deserialize<Server>(*json_text);
    const auto from_toml = toml::deserialize<Server>(*toml_text);
    return from_json == server && from_toml == server ? 0 : 1;
}
```

自动映射使用成员原名，适用于命名空间作用域、可默认构造的简单聚合。不支持基类、
引用成员、`const` 数据成员、位域、联合或 C 数组。成员仍须是对应格式支持的类型。
缺少非 optional 的字段会报错，即使该成员有初值；缺少 optional 时保留初值。
JSON 的空 optional 输出 `null`；TOML 省略空 optional 字段，数组中不允许空值。

自动映射不会排除敏感字段。对外协议、持久化契约或流式 JSON 请使用
[显式字段契约](docs/field-contract-README.md)。有 `reflect_fields` 时只使用列出的字段，
该对象的整个子树不启用自动映射；普通聚合仍可包含这种显式对象。JSON 的 wire 转换、
`decode_field` 和 `decode_fields_into` 也不启用自动映射。

## 项目结构

每个模块采用「实现头 + .cppm 门面」的双形态结构：实现全部在头文件中，
经典 TU 直接 `#include`（只需支持 C++23，不要求模块工具链）；
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
│   ├── detail/              # 两种文本格式共用的内部字段适配
│   └── reflect.cppm         # `import reflect` 门面
├── toml/
│   ├── toml.hpp             # TOML 实现头
│   └── toml.cppm            # `import toml` 门面
└── json/
    ├── json.hpp             # JSON 实现头（simdjson 后端）
    ├── stream.hpp           # json::stream::StreamWriter 流式输出
    └── json.cppm            # `import json` 门面

third_party/
├── boost_pfr/  # 固定版本的内部聚合字段发现头
└── simdjson/   # 内置 JSON 后端，直接参与库构建

cmake/          # 包配置模板（serdeConfig.cmake.in）
test/           # CMake 冒烟测试（头文件 + 模块两条消费路径）
```

## 构建与安装

### mcpp（开发默认）

```bash
mcpp build            # 模块形态，默认 LLVM 22.1.8 工具链 + debug profile
mcpp test             # 全部 10 个测试
mcpp build --profile release   # 基准测试请用 release 或 benchmark/run.sh
```

作为依赖时，`include/serde` 提供与 CMake/Bazel 相同的头文件路径，
如 `<serde/reflect/reflect_macros.hpp>`；simdjson 的公开路径是
`<third_party/simdjson/simdjson.h>`。模块与消费方必须采用相同异常编译策略。

依赖消费回归测试：在 `test/mcpp_consumer` 下运行 `mcpp build` 和 `mcpp run`。

### CMake（可安装）

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure   # 头文件 + 模块冒烟测试
cmake --install build --prefix /your/prefix
```

原生模块构建与安装后的模块消费者要求 CMake >= 3.31，并需要
Clang >= 17 / GCC >= 15 + Ninja/VS。编译器或生成器不支持时只构建头文件目标；
使用较旧 CMake 时必须设置 `-DSERDE_BUILD_CPP23_MODULES=OFF`。
安装后消费者使用自己的编译器和选项重建 BMI。

simdjson 后端默认构建为启用 PIC 的静态库，也可链接到下游共享库。
`-DSERDE_BUILD_SHARED_LIBS=ON` 将后端构建为共享库。

### Bazel

`BUILD.bazel` 提供 `//:serde` 头文件 target；`//third_party/simdjson:simdjson`
提供 JSON 后端。`.cppm` 门面无法用 rules_cc 0.1.1 编译，Bazel 消费者走头文件。

```bash
bazel test //:header_smoke //:aggregate_formats
```

## 消费方式

### 头文件

```cpp
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

Apache-2.0。内置 simdjson 和 Boost.PFR 使用各自的上游许可证，见
`third_party/simdjson/LICENSE.md` 与 `third_party/boost_pfr/LICENSE_1_0.txt`。
