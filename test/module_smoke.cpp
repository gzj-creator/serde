// 模块门面冒烟：json 与 toml 相互独立（各经 export import 复发布
// reflect/serde_common），同一 TU 内可同时导入。
#include <cassert>
#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
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

enum class mode : std::uint8_t { active = 1, disabled = 2 };

constexpr auto reflect_enum(std::type_identity<mode>) {
    return reflect::enum_descriptor<mode, 2>{
        reflect::enum_encoding::string,
        {{{mode::active, "active"}, {mode::disabled, "disabled"}}}
    };
}

struct settings {
    std::string name;
    int retries{};
    mode state = mode::active;
    bool operator==(const settings&) const = default;
};

#define SETTINGS_FIELDS(X) \
    X(name, "display-name", (reflect::field_options<std::string>{.min_length = 1, .max_length = 4})) \
    X(retries, "retries", (reflect::field_options<int>{.minimum = 1, .maximum = 3})) \
    X(state)
REFLECT_FIELDS(settings, SETTINGS_FIELDS)
#undef SETTINGS_FIELDS

bool contract_roundtrip() {
    static_assert(reflect::EnumReflectable<mode>);
    constexpr auto fields = reflect::static_fields<settings>();
    static_assert(std::get<0>(fields).options.max_length == 4);
    const settings value{"demo", 2, mode::active};
    const auto encoded_json = json::serialize(value);
    const auto encoded_toml = toml::serialize(value);
    if (!encoded_json || !encoded_toml) return false;
    const auto json_back = json::deserialize<settings>(*encoded_json);
    const auto toml_back = toml::deserialize<settings>(*encoded_toml);
    if (!json_back || *json_back != value || !toml_back || *toml_back != value) return false;

    const auto partial = json::parse(R"({"retries":3})");
    if (!partial) return false;
    auto merged = value;
    const auto selected = json::decode_fields_into(
        *partial, merged, [](const auto& field) { return field.name == "retries"; });
    if (!selected || merged.retries != 3 || merged.name != value.name) return false;
    const auto decoded_field = json::decode_field(std::get<1>(fields), partial->at("retries"));
    if (!decoded_field || *decoded_field != 3) return false;

    const settings invalid{"demo", 4, mode::active};
    const auto invalid_json = json::serialize(invalid);
    const auto invalid_toml = toml::serialize(invalid);
    if (invalid_json || invalid_toml) return false;
    const auto invalid_enum = json::deserialize<mode>(R"("unknown")");
    if (invalid_enum) return false;
    std::string discarded;
    json::stream::StreamWriter writer([&](std::string_view part) -> json::result<void> {
        discarded.append(part);
        return {};
    });
    const auto written = writer.value(invalid);
    const auto finished = writer.finish();
    return !written && !finished && written.error() == finished.error();
}

}  // namespace

int main() {
    json::SchemaBuilder schema;
    schema.add_string("name", "Name", true);
    const auto schema_text = schema.encode();
    if (!schema_text) return 1;
    const auto schema_fields = json::deserialize<json::Object>(*schema_text);
    if (!schema_fields || !schema_fields->contains("properties")) return 1;

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
    return contract_roundtrip() ? 0 : 1;
}
