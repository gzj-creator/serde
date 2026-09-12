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
| `reflect` | 序列化器中立的反射词汇 | [reflect-README.md](docs/reflect-README.md) |
| `toml` | TOML 编码器/解码器 | [toml-README.md](docs/toml-README.md) |
| `json` | JSON 编码器/解码器 | [json-README.md](docs/json-README.md) |

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

```
src/
├── reflect/    # 反射模块
├── toml/       # TOML 编解码
└── json/       # JSON 编解码

third_party/
└── simdjson/   # 内置 JSON 后端，直接参与库构建

docs/
├── reflect-README.md
├── toml-README.md
├── json-README.md
└── plans/      # 开发计划
```

## 构建

使用 mcpp 构建：

```bash
mcpp build
```

默认使用 LLVM 22.1.8 工具链和 debug profile，生成 `serde` 静态库。基准测试请用 `mcpp build --profile release` 或 `benchmark/run.sh`。

## 许可证

Apache-2.0。vendored simdjson 使用其上游许可证，见 `third_party/simdjson/LICENSE.md`。
