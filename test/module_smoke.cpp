// 模块门面冒烟：json 与 toml 相互独立（各经 export import 复发布
// reflect/serde_common），同一 TU 内可同时导入。
#include <cassert>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// GCC needs standard headers before imports to merge their declarations.
import json;
import toml;

#include <serde/reflect/reflect_macros.hpp>

namespace {

struct entry {
    std::string name;
    int count{};
    bool operator==(const entry&) const = default;
};

#define ENTRY_FIELDS(X) X(name) X(count)
REFLECT_FIELDS(entry, ENTRY_FIELDS)
#undef ENTRY_FIELDS

}  // namespace

int main() {
    entry value{"demo", 3};
    const auto text = json::serialize(value);
    assert(text);
    const auto back = json::deserialize<entry>(*text);
    assert(back);
    assert(*back == value);

    std::string streamed;
    json::stream::StreamWriter writer([&](std::string_view part) -> json::result<void> {
        streamed.append(part);
        return {};
    });
    if (!writer.value(value) || !writer.finish()) return 1;
    const auto streamed_back = json::deserialize<entry>(streamed);
    if (!streamed_back || *streamed_back != value) return 1;

    // json 与 toml 各自独立使用公共词汇（common::date 等），互不依赖。
    const auto toml_text = toml::serialize(value);
    assert(toml_text);
    const auto toml_back = toml::deserialize<entry>(*toml_text);
    assert(toml_back);
    assert(*toml_back == value);
    return 0;
}
