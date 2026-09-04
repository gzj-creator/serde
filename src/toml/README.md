# `toml` module

This directory contains a dependency-free TOML encoder/decoder and a
declaration-time reflection helper. It does not use `reflect-cpp`, `toml++`,
or compiler-specific reflection extensions.

```cpp
import toml;
// 将 src/toml 加入头文件搜索路径；也可以像相邻的 src/config 模块一样使用相对路径。
// #include "../toml/toml_reflect.hpp"
#include "toml_reflect.hpp"

namespace config {
struct server {
    std::string host;
    int port{};
};

TOML_REFLECT_MEMBERS(server, host, port)
} // 命名空间 config

config::server value{"localhost", 8080};
const auto text = toml::serialize(value);
if (!text) {
    // text.error() 返回便于阅读的错误信息。
}

const auto parsed = text ? toml::deserialize<config::server>(*text)
                         : toml::result<config::server>{std::unexpected(text.error())};
if (parsed) {
    // 可通过 parsed->host 和 parsed->port 访问解析后的字段。
}
```

`TOML_REFLECT_MEMBERS` is the compact form and accepts up to 16 members. For a
reusable or longer field list, use an X-macro instead:

```cpp
#define SERVER_FIELDS(X) \
    X(host)                \
    X(port)
TOML_REFLECT(server, SERVER_FIELDS)
#undef SERVER_FIELDS
```

When a TOML key differs from the C++ member name, use the named X-macro form.
This is also how to represent keys containing whitespace or literal dots:

```cpp
#define SERVER_FIELDS(X) \
    X(host_name, "host name") \
    X(port, "port")
TOML_REFLECT_NAMED(server, SERVER_FIELDS)
#undef SERVER_FIELDS
```

The generated `toml_reflect` function must be declared in the type's associated
namespace so that argument-dependent lookup can find it. C++23 cannot enumerate
members of an existing struct, so the member names still have to be listed once
at the declaration site.

The canonical symmetric API is:

```cpp
toml::result<std::string> toml::serialize(const T&);
toml::result<T> toml::deserialize<T>(std::string_view);
```

`toml::result<T>` is exactly `std::expected<T, std::string>`. Therefore both
directions report failures without throwing. `try_serialize` is an equivalent
compatibility alias for `serialize`; `deserializee` and `deSerialize` are
equivalent compatibility aliases for `deserialize`.

Unknown TOML fields are ignored. Missing non-optional fields and type/range
mismatches are returned as errors.

The decoder accepts comments; bare, quoted, and dotted keys; basic, literal,
and multiline strings; integers in decimal, hexadecimal, octal, and binary;
floating-point, `inf`, and `nan`; booleans; local and offset temporal values;
single-line and multiline arrays; inline tables; regular tables; nested tables;
and array-of-tables.

This is a broadly covered self-contained implementation, not a claim of full
TOML 1.0 conformance-suite coverage. In particular, unusual lexical edge cases
outside the documented fixture should be validated against the TOML
specification before relying on them in an interchange boundary.

The supported C++ value types are `std::string`, booleans, signed and unsigned
integers that fit TOML's signed 64-bit range, floating-point values, enums,
`std::optional`, `std::vector`, `std::array`, string-keyed `std::map` and
`std::unordered_map`, recursively reflected structures, and the public
temporal types `toml::date`, `toml::time`, `toml::local_date_time`, and
`toml::offset_date_time`. Wrap a reflected member in `toml::inline_table<T>`
when it must be written as `{ key = value }`; a `std::vector` of reflected
structures is written as an array-of-tables.

The serializer is deliberately canonical: it emits valid deterministic TOML,
but cannot preserve purely lexical input details such as comments, quote style,
numeric underscores or bases, whitespace, key ordering, and original table
layout. The parser and writer are tested independently so parsing does not only
validate the library's own output.
