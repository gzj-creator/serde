# `toml` 模块

本目录提供自包含的 TOML 编码器/解码器。普通 C++23 简单聚合结构体可
[免注册读写](../README.md#快速开始)，不需要安装 Boost 或外部 TOML 解析器。

```cpp
#include <string>
#include <serde/toml/toml.hpp>

struct Server {
    std::string host;
    int port{};
};

int main() {
    const auto text = toml::serialize(Server{"localhost", 8080});
    if (!text) return 1;
    const auto parsed = toml::deserialize<Server>(*text);
    return parsed && parsed->host == "localhost" && parsed->port == 8080 ? 0 : 1;
}
```

需要固定字段名单、重命名或约束时，使用 `reflect_fields` 或 `REFLECT_FIELDS` 显式声明。
使用宏时包含 `<serde/reflect/reflect_macros.hpp>`，并将字段列表放在类型声明附近：

```cpp
#define SERVER_FIELDS(X) \
    X(host)              \
    X(port)
REFLECT_FIELDS(Server, SERVER_FIELDS)
#undef SERVER_FIELDS
```

当 TOML 键与 C++ 成员名不同时，使用命名 X-macro 形式。这也是表示包含空格或字面点号的键的方式：

```cpp
#define SERVER_FIELDS(X) \
    X(host, "host name") \
    X(port, "port")
REFLECT_FIELDS(Server, SERVER_FIELDS)
#undef SERVER_FIELDS
```

生成的 `reflect_fields` 函数必须在类型的关联命名空间中声明，以便参数相关查找能找到它。`REFLECT_FIELDS` 及其兼容别名 `REFLECT_MEMBERS` 生成通用定制点，可通过 `import reflect` 使用。

默认 `reflect::field` 只保存外部字段名和成员指针，带选项版本另保存协议无关的字段约束。两者可以在编译期构造，LLVM/Clang 和 GCC 消费者均可使用。字段约束和显式枚举编码的用法见 [字段契约](field-contract-README.md)，TOML 编解码与 JSON/流式 JSON 使用相同的校验规则。

标准对称 API：

```cpp
toml::result<std::string> toml::serialize(const T&);
toml::result<T> toml::deserialize<T>(std::string_view);
```

`toml::result<T>` 就是 `std::expected<T, std::string>`。因此两个方向报告失败时均不抛出异常。

未知 TOML 字段默认被忽略，设置 `ParseOptions::unknown_fields = toml::UnknownFieldPolicy::reject` 可拒绝。缺少非可选字段和类型/范围不匹配作为错误返回。缺少 optional 键保留初值；空 optional 不输出，因此如果主动清空一个带非空默认值的 optional，往返后会恢复默认值。

解码器接受注释；裸键、带引号键和点号键；基本字符串、字面量字符串和多行字符串；十进制、十六进制、八进制和二进制整数；浮点数、`inf` 和 `nan`；布尔值；本地和偏移时间值；单行和多行数组；内联表；常规表；嵌套表；以及表数组。拒绝格式错误的数字词素、重复表定义、重新打开内联表、无效 UTF-8、无效原始控制字符和超出范围的时间值。TOML 数组可以是异构的；仅当元素无法转换为请求的 C++ 元素类型时，解码器才报告类型错误。

这是一个广泛覆盖的自包含实现，并非声称完整的 TOML 1.0 合规套件覆盖。特别是，在依赖超出文档固定用例的不寻常词法边缘情况之前，应对照 TOML 规范进行验证。

支持的 C++ 值类型包括：`std::string`、布尔值、适合 TOML 有符号 64 位范围的有符号和无符号整数、浮点值、枚举、`std::optional`、`std::vector`、`std::array`、字符串键的 `std::map` 和 `std::unordered_map`、嵌套结构体，以及公共时间类型 `toml::date`、`toml::time`、`toml::local_date_time` 和 `toml::offset_date_time`。子结构体默认写作表；需要 `{ key = value }` 时用 `toml::InlineTable<T>` 包装。非空的结构体 `std::vector` 和 `std::array` 写为表数组，空数组仍输出 `[]`。

序列化器刻意保持规范：生成有效的确定性 TOML，但无法保留纯词法输入细节，如注释、引号样式、数字下划线或进制、空白、键排序和原始表布局。解析器和写入器独立测试，因此解析不仅验证库自身的输出。

## GCC 16.1 静态反射

捆绑的 GCC 16.1 工具链通过 `-std=c++2c -freflection` 暴露 C++26 反射功能（`std::meta`、`^^T`、`template for` 和 splice 表达式）。由于 Clang 不提供 `<meta>`，GCC 原生反射不属于默认 LLVM 测试路径。

原生反射可以在 GCC 构建中生成 `reflect_fields` 适配器，但生成的 `std::meta::info` 值和编译器生成的 BMI 不是可移植 ABI。GCC 构建的静态或共享库仅在其导出 ABI 使用普通、匹配的 C++/C 类型和兼容的标准库/ABI 时，才能被 LLVM 消费者链接。LLVM 不能导入 GCC BMI，也不能在同一二进制边界中暴露来自 libstdc++ 和 libc++ 的 `std::meta::info`、`std::string`。因此推荐的生产拆分方案：

1. 保持通用 `reflect` API 和序列化器模板为仅头文件/模块。
2. 在仅 GCC 的适配器目标中生成 GCC 原生元数据。
3. 使用相同工具链和标准库编译 TOML/JSON 消费者，或通过 C ABI/序列化线路格式通信。
