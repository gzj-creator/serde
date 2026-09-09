# serde

无依赖的 C++ 序列化库，提供 TOML 和 JSON 编解码功能。

## 特性

- **无第三方依赖**：完全自包含，不依赖 simdjson、nlohmann/json、toml++ 等
- **编译器无关**：基于编译器无关的反射模块，支持 LLVM/Clang 和 GCC
- **确定性序列化**：输出格式规范、可预测
- **资源限制**：内置字节、深度、节点等限制，防止资源滥用
- **C++23**：使用 `std::expected` 进行错误处理，不抛出异常

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
├── json/       # JSON 编解码
└── main.cpp    # 示例入口

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

默认使用 LLVM 22.1.8 工具链。

## 许可证

Apache-2.0
