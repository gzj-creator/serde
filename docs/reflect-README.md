# `reflect` 模块

`reflect` 是 TOML 及 JSON 编解码器使用的序列化器中立反射词汇。公共 API 包含 `reflect::field`、成员指针、UTF-8 字段名、`reflect::fields`、`reflect::for_each_field`，以及编译期元数据接口 `reflect::StaticReflectable` 和 `reflect::static_fields<T>()`。没有编译器元对象类型或 BMI 格式是契约的一部分。

```cpp
import reflect;
#include "reflect_macros.hpp"

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
reflect::for_each_field(value, [](const auto& field, auto& object) {
    // field.name 和 field.get(object) 对任何编解码器可用。
});
```

`REFLECT_FIELDS`、`REFLECT_MEMBERS` 和 `REFLECT_EMPTY` 宏是可移植成员枚举之前编译器的声明时回退。它们保存在一个小头文件中，以便消费者可以使用模块而不依赖 TOML。

宏同时生成 `reflect_fields(std::type_identity<T>)` 重载。字段名可在编译期读取时，类型满足 `StaticReflectable<T>`，消费者可以通过 `static_fields<T>()` 获取元数据并生成常量查找索引，无需构造 `T`。已有的 `reflect_fields(const T&)` 定制仍然有效；运行时字段名按每次调用重新获取，不会被缓存为编译期常量。

GCC 16.1 额外实现了 C++26 静态反射提案，通过 `-std=c++2c -freflection` 启用。仅 GCC 的适配器可以枚举 `std::meta::nonstatic_data_members_of(^^T, ...)` 并生成相同的字段契约。将 `std::meta::info` 和 GCC BMI 保持在该适配器内：Clang 不能导入它们，且 libstdc++/libc++ C++ 对象类型不能跨混合工具链 ABI 边界。
