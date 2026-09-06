# `reflect` module

`reflect` is the serializer-neutral reflection vocabulary used by TOML and
future JSON codecs. Its public API contains only `reflect::field`, a member
pointer, a UTF-8 field name, `reflect::fields`, and `reflect::for_each_field`.
No compiler metaobject type or BMI format is part of the contract.

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
    // field.name and field.get(object) are available to any codec.
});
```

The `REFLECT_FIELDS`, `REFLECT_MEMBERS`, and `REFLECT_EMPTY` macros are declaration-time
fallbacks for compilers before portable member enumeration. They are kept in a
small header so a consumer can use the module without depending on TOML.

GCC 16.1 additionally implements the C++26 static reflection proposal behind
`-std=c++2c -freflection`. A GCC-only adapter can enumerate
`std::meta::nonstatic_data_members_of(^^T, ...)` and produce the same field
contract. Keep `std::meta::info` and GCC BMIs inside that adapter: Clang cannot
import them, and libstdc++/libc++ C++ object types must not cross a mixed-toolchain
ABI boundary. See `../toml/README.md` and `../../probes/gcc_reflection.cpp` for
the build and ABI notes.
