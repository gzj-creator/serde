import std;
import json;

#include "../src/reflect/reflect_macros.hpp"

namespace {

enum class level : std::uint8_t { low = 1, high = 2 };

struct credentials {
    std::string user;
    std::string token;
    bool operator==(const credentials&) const = default;
};

#define CREDENTIAL_FIELDS(X) X(user) X(token)
REFLECT_FIELDS(credentials, CREDENTIAL_FIELDS)
#undef CREDENTIAL_FIELDS

struct document {
    std::string title;
    bool enabled{};
    std::int64_t signed_value{};
    std::uint64_t unsigned_value{};
    double ratio{};
    level priority{level::low};
    std::optional<std::string> note;
    std::vector<int> values;
    std::array<std::string, 2> labels{};
    std::map<std::string, int> counts;
    credentials auth;
    json::date day;
    json::time clock;
    json::local_date_time local_time;
    json::offset_date_time instant;
    bool operator==(const document&) const = default;
};

#define DOCUMENT_FIELDS(X) \
    X(title) X(enabled) X(signed_value) X(unsigned_value) X(ratio) \
    X(priority) X(note) X(values) X(labels) X(counts) X(auth) X(day) \
    X(clock) X(local_time) X(instant)
REFLECT_FIELDS(document, DOCUMENT_FIELDS)
#undef DOCUMENT_FIELDS

struct required {
    int value{};
};

#define REQUIRED_FIELDS(X) X(value)
REFLECT_FIELDS(required, REQUIRED_FIELDS)
#undef REQUIRED_FIELDS

struct OptionalValue {
    std::optional<int> value;
};

#define OPTIONAL_FIELDS(X) X(value)
REFLECT_FIELDS(OptionalValue, OPTIONAL_FIELDS)
#undef OPTIONAL_FIELDS

template <class T>
bool expect_error(const json::result<T>& value, std::string_view fragment,
                  std::string_view name) {
    if (!value && value.error().find(fragment) != std::string::npos) {
        return true;
    }
    std::println("test_json: {}: {}", name,
                 value ? "unexpected success" : value.error());
    return false;
}

}  // namespace

int main() {
    const document original{
        "JSON \"fixture\"\n", true, std::numeric_limits<std::int64_t>::min(),
        std::numeric_limits<std::uint64_t>::max(), 0.125, level::high, std::nullopt,
        {1, 2, 3}, {"left", "right"}, {{"critical", 90}, {"warning", 70}},
        {"Ada", "secret"}, {2026, 9, 8}, {9, 30, 15, "123"},
        {{2026, 9, 8}, {9, 30, 15, "456"}},
        {{{2026, 9, 8}, {9, 30, 15, "789"}}, -480}};

    bool passed = true;
    const auto serialized = json::serialize(original);
    passed &= static_cast<bool>(serialized);
    if (serialized) {
        const auto parsed = json::deserialize<document>(*serialized);
        passed &= parsed && *parsed == original;
        if (!parsed) std::println("test_json: round-trip: {}", parsed.error());
        passed &= *serialized ==
                  "{\"auth\":{\"token\":\"secret\",\"user\":\"Ada\"},\""
                  "clock\":\"09:30:15.123\",\"counts\":{\"critical\":90,\"warning\":70},\""
                  "day\":\"2026-09-08\",\"enabled\":true,\"instant\":\"2026-09-08T09:30:15.789-08:00\",\""
                  "labels\":[\"left\",\"right\"],\"local_time\":\"2026-09-08T09:30:15.456\",\"note\":null,\""
                  "priority\":2,\"ratio\":0.125,\"signed_value\":-9223372036854775808,\""
                  "title\":\"JSON \\\"fixture\\\"\\n\",\"unsigned_value\":18446744073709551615,\""
                  "values\":[1,2,3]}";
    }

    json::SerializeOptions pretty_options;
    pretty_options.pretty = true;
    const auto pretty = json::serialize(required{7}, pretty_options);
    passed &= pretty && *pretty == "{\n  \"value\": 7\n}";

    const auto unicode = json::deserialize<std::string>(R"("\uD83D\uDE80")");
    passed &= unicode && *unicode == "\xF0\x9F\x9A\x80";
    passed &= json::deserialize<std::string>(R"("\uD800")").has_value() == false;
    passed &= json::deserialize<required>(R"({"value":1,})").has_value() == false;
    passed &= json::deserialize<required>(R"({"value":1,"value":2})").has_value() == false;
    passed &= json::deserialize<required>(R"({"value":01})").has_value() == false;
    passed &= json::deserialize<required>(R"({"value":1} trailing)").has_value() == false;
    passed &= static_cast<bool>(json::deserialize<required>(R"({"value":1,"extra":2})"));
    passed &= json::deserialize<required>(std::string("{\"value\":\xFF}")).has_value() == false;
    passed &= json::deserialize<std::uint64_t>("18446744073709551616").has_value() == false;
    passed &= json::deserialize<double>("1e400").has_value() == false;
    passed &= json::deserialize<json::offset_date_time>(R"("2024-01-01T00:00:00+")").has_value() == false;

    json::ParseOptions strict;
    strict.unknown_fields = json::UnknownFieldPolicy::reject;
    passed &= expect_error(json::deserialize<required>(R"({"value":1,"extra":2})", strict),
                           "unknown field", "strict unknown field policy");
    passed &= expect_error(json::deserialize<required>(R"({})"), "missing field",
                           "missing required field");
    passed &= expect_error(json::deserialize<required>(R"({"value":"one"})"),
                           "integer", "type mismatch");
    const auto null_optional = json::deserialize<OptionalValue>(R"({"value":null})");
    passed &= null_optional && !null_optional->value;

    json::ParseOptions limited;
    limited.max_array_items = 2;
    passed &= expect_error(json::deserialize<std::vector<int>>("[1,2,3]", limited),
                           "item count", "array limit");
    limited = {};
    limited.max_depth = 2;
    passed &= expect_error(json::deserialize<std::vector<std::vector<int>>>("[[1]]", limited),
                           "depth", "depth limit");
    limited = {};
    limited.max_input_bytes = 2;
    passed &= expect_error(json::deserialize<required>(R"({"value":1})", limited),
                           "input", "input limit");

    passed &= expect_error(json::serialize(std::numeric_limits<double>::infinity()),
                           "non-finite", "non-finite serialization");
    json::SerializeOptions null_non_finite;
    null_non_finite.non_finite = json::NonFinitePolicy::null_value;
    const auto null_float = json::serialize(std::numeric_limits<double>::infinity(), null_non_finite);
    passed &= null_float && *null_float == "null";
    json::SerializeOptions limited_output;
    limited_output.max_output_bytes = 2;
    passed &= expect_error(json::serialize(required{123}, limited_output), "output",
                           "output limit");

    const auto leap_day = json::deserialize<json::date>(R"("2024-02-29")");
    passed &= leap_day && *leap_day == json::date{2024, 2, 29};
    passed &= json::deserialize<json::date>(R"("2024-02-30")").has_value() == false;
    return passed ? 0 : 1;
}
