// 模块门面冒烟：json 与 toml 相互独立（各经 export import 复发布
// reflect/serde_common），同一 TU 内可同时导入。
import json;
import toml;

#include <cassert>
#include <optional>
#include <string>
#include <vector>

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

    // json 与 toml 各自独立使用公共词汇（common::date 等），互不依赖。
    const auto toml_text = toml::serialize(value);
    assert(toml_text);
    const auto toml_back = toml::deserialize<entry>(*toml_text);
    assert(toml_back);
    assert(*toml_back == value);
    return 0;
}