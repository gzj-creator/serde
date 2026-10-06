#include <array>
#include <cstdio>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <serde/json/stream.hpp>
#include <serde/reflect/reflect_macros.hpp>
#include <serde/toml/toml.hpp>

namespace contract_tests {

bool expect(bool condition, const char* message) {
    if (!condition) std::fprintf(stderr, "toml_stream_contract: %s\n", message);
    return condition;
}

json::stream::StreamWriter::Sink collect(std::string& output) {
    return [&](std::string_view part) -> json::result<void> {
        output.append(part);
        return {};
    };
}

template<class T>
bool rejects_encoding(const T& value, const char* message) {
    std::string output;
    const auto streamed = json::stream::serialize(value, collect(output));
    const auto encoded = toml::serialize(value);
    return expect(!streamed && !encoded, message);
}

enum class Mode { active = 2, disabled = 7 };
constexpr auto reflect_enum(std::type_identity<Mode>) {
    return reflect::enum_descriptor<Mode, 2>{
        reflect::enum_encoding::string,
        {{{Mode::active, "enabled"}, {Mode::disabled, "disabled"}}}
    };
}

enum class Level : std::uint64_t { low = 3, high = 9 };
constexpr auto reflect_enum(std::type_identity<Level>) {
    return reflect::enum_descriptor<Level, 2>{
        reflect::enum_encoding::underlying,
        {{{Level::low, "low"}, {Level::high, "high"}}}
    };
}

enum class Switch : bool { off = false, on = true };
constexpr auto reflect_enum(std::type_identity<Switch>) {
    return reflect::enum_descriptor<Switch, 2>{
        reflect::enum_encoding::underlying,
        {{{Switch::off, "off"}, {Switch::on, "on"}}}
    };
}

enum class Plain : unsigned { ready = 4 };
enum class PlainSwitch : bool { off = false, on = true };

struct Config {
    int age = 18;
    std::string title = "ok";
    std::vector<int> samples{1};
    std::map<std::string, int> labels{{"a", 1}};
    std::optional<int> retries;
    Mode mode = Mode::active;
    Level level = Level::low;
    Switch flag = Switch::on;
    bool operator==(const Config&) const = default;
};

#define CONFIG_FIELDS(X) \
    X(age, "age", (reflect::field_options<int>{.minimum = 18, .maximum = 150})) \
    X(title, "display-name", (reflect::field_options<std::string>{.min_length = 2, .max_length = 3})) \
    X(samples, "samples", (reflect::field_options<std::vector<int>>{.min_items = 1, .max_items = 2})) \
    X(labels, "labels", (reflect::field_options<std::map<std::string, int>>{.min_items = 1, .max_items = 2})) \
    X(retries, "retries", (reflect::field_options<std::optional<int>>{.minimum = 1, .maximum = 4})) \
    X(mode) X(level) X(flag)
REFLECT_FIELDS(Config, CONFIG_FIELDS)
#undef CONFIG_FIELDS

struct OptionalDefault {
    std::optional<int> retries = 0;
};
#define OPTIONAL_DEFAULT_FIELDS(X) \
    X(retries, "retries", (reflect::field_options<std::optional<int>>{.minimum = 1}))
REFLECT_FIELDS(OptionalDefault, OPTIONAL_DEFAULT_FIELDS)
#undef OPTIONAL_DEFAULT_FIELDS

struct InvalidOptional {
    std::optional<std::string> note;
};
#define INVALID_OPTIONAL_FIELDS(X) \
    X(note, "note", (reflect::field_options<std::optional<std::string>>{.min_length = 3, .max_length = 1}))
REFLECT_FIELDS(InvalidOptional, INVALID_OPTIONAL_FIELDS)
#undef INVALID_OPTIONAL_FIELDS

struct Inapplicable {
    int count = 2;
};
#define INAPPLICABLE_FIELDS(X) \
    X(count, "count", (reflect::field_options<int>{.min_items = 1}))
REFLECT_FIELDS(Inapplicable, INAPPLICABLE_FIELDS)
#undef INAPPLICABLE_FIELDS

struct NonFiniteBoundary {
    double ratio = 1;
};
#define NONFINITE_FIELDS(X) \
    X(ratio, "ratio", (reflect::field_options<double>{.maximum = std::numeric_limits<double>::infinity()}))
REFLECT_FIELDS(NonFiniteBoundary, NONFINITE_FIELDS)
#undef NONFINITE_FIELDS

struct BoundedFloat {
    double ratio = 0.5;
};
#define BOUNDED_FLOAT_FIELDS(X) \
    X(ratio, "ratio", (reflect::field_options<double>{.minimum = 0.0, .maximum = 1.0}))
REFLECT_FIELDS(BoundedFloat, BOUNDED_FLOAT_FIELDS)
#undef BOUNDED_FLOAT_FIELDS

struct WideInteger {
    std::uint64_t value = std::numeric_limits<std::uint64_t>::max();
};
#define WIDE_FIELDS(X) \
    X(value, "value", (reflect::field_options<std::uint64_t>{.minimum = std::numeric_limits<std::uint64_t>::max()}))
REFLECT_FIELDS(WideInteger, WIDE_FIELDS)
#undef WIDE_FIELDS

struct PlainEnums {
    Plain mode = static_cast<Plain>(100);
    PlainSwitch flag = PlainSwitch::on;
};
#define PLAIN_FIELDS(X) X(mode) X(flag)
REFLECT_FIELDS(PlainEnums, PLAIN_FIELDS)
#undef PLAIN_FIELDS

struct Nested {
    std::vector<Config> configs{Config{}};
};
#define NESTED_FIELDS(X) X(configs)
REFLECT_FIELDS(Nested, NESTED_FIELDS)
#undef NESTED_FIELDS

enum class Duplicate { first, second };
constexpr auto reflect_enum(std::type_identity<Duplicate>) {
    return reflect::enum_descriptor<Duplicate, 2>{
        reflect::enum_encoding::string,
        {{{Duplicate::first, "same"}, {Duplicate::second, "same"}}}
    };
}

enum class Empty { value };
constexpr auto reflect_enum(std::type_identity<Empty>) {
    return reflect::enum_descriptor<Empty, 0>{reflect::enum_encoding::underlying, {}};
}

enum class InvalidName { value };
constexpr auto reflect_enum(std::type_identity<InvalidName>) {
    return reflect::enum_descriptor<InvalidName, 1>{
        reflect::enum_encoding::string,
        {{{InvalidName::value, std::string_view("\xff", 1)}}}
    };
}

enum class InvalidEncoding { value };
constexpr auto reflect_enum(std::type_identity<InvalidEncoding>) {
    return reflect::enum_descriptor<InvalidEncoding, 1>{
        static_cast<reflect::enum_encoding>(99), {{{InvalidEncoding::value, "value"}}}
    };
}

enum class DuplicateValue { first, second };
constexpr auto reflect_enum(std::type_identity<DuplicateValue>) {
    return reflect::enum_descriptor<DuplicateValue, 2>{
        reflect::enum_encoding::underlying,
        {{{DuplicateValue::first, "first"}, {DuplicateValue::first, "second"}}}
    };
}

template<class E>
struct EnumBox {
    E value{};
};
template<class E>
constexpr auto reflect_fields(std::type_identity<EnumBox<E>>) {
    return std::tuple{reflect::make_field("value", &EnumBox<E>::value)};
}
template<class E>
constexpr auto reflect_fields(const EnumBox<E>&) {
    return reflect_fields(std::type_identity<EnumBox<E>>{});
}

struct RuntimeDescriptorOwner {
    int count = 2;
};
template<class Member>
struct RuntimeDescriptor {
    std::string_view name;
    Member RuntimeDescriptorOwner::*pointer;
    const Member& get(const RuntimeDescriptorOwner& value) const { return value.*pointer; }
    Member& get(RuntimeDescriptorOwner& value) const { return value.*pointer; }
};
auto reflect_fields(const RuntimeDescriptorOwner&) {
    return std::tuple{RuntimeDescriptor<int>{"count", &RuntimeDescriptorOwner::count}};
}

struct RuntimeOptionalOwner { std::optional<Duplicate> value; };
struct RuntimeOptionalDescriptor {
    std::string_view name = "value";
    const auto& get(const RuntimeOptionalOwner& owner) const { return owner.value; }
    auto& get(RuntimeOptionalOwner& owner) const { return owner.value; }
};
auto reflect_fields(const RuntimeOptionalOwner&) {
    return std::tuple{RuntimeOptionalDescriptor{}};
}

bool custom_descriptors() {
    const RuntimeDescriptorOwner value;
    auto encoded = toml::serialize(value);
    bool ok = expect(encoded.has_value(), "custom name/get descriptor encodes TOML");
    if (encoded) {
        auto decoded = toml::deserialize<RuntimeDescriptorOwner>(*encoded);
        ok &= expect(decoded && decoded->count == value.count, "custom descriptor decodes TOML");
    }
    std::string output;
    auto streamed = json::stream::serialize(value, collect(output));
    ok &= expect(streamed && output == R"({"count":2})", "custom descriptor encodes JSON stream");
    auto dom = json::serialize(value);
    ok &= expect(dom && *dom == output, "custom descriptor encodes JSON DOM consistently");
    if (dom) {
        auto decoded = json::deserialize<RuntimeDescriptorOwner>(*dom);
        ok &= expect(decoded && decoded->count == value.count, "custom descriptor decodes JSON DOM");
    }
    ok &= expect(!toml::deserialize<RuntimeOptionalOwner>(""),
                 "missing custom optional still validates enum metadata");
    ok &= expect(!json::deserialize<RuntimeOptionalOwner>("{}"),
                 "missing JSON custom optional still validates enum metadata");
    ok &= rejects_encoding(RuntimeOptionalOwner{}, "empty custom optional validates enum metadata in both encoders");
    return ok;
}

bool valid_roundtrips() {
    Config config;
    config.title = "\xe4\xb8\xad\xf0\x9f\x98\x80";
    config.samples = {1, 2};
    config.retries = 4;
    auto encoded = toml::serialize(config);
    bool ok = expect(encoded.has_value(), "TOML accepts inclusive constraints and Unicode scalar length");
    if (encoded) {
        ok &= expect(encoded->find("mode = \"enabled\"") != std::string::npos &&
                     encoded->find("level = 3") != std::string::npos &&
                     encoded->find("flag = true") != std::string::npos,
                     "TOML uses explicit enum string, integer and boolean encoding");
        const auto decoded = toml::deserialize<Config>(*encoded);
        ok &= expect(decoded && *decoded == config, "constrained TOML object round trips");
    }
    std::string output;
    auto streamed = json::stream::serialize(config, collect(output));
    ok &= expect(streamed.has_value(), "stream accepts the same constrained object");
    const auto parsed = json::parse(output);
    ok &= expect(parsed && parsed->at("mode").as_string() == "enabled" &&
                 parsed->at("level").as_uint64() == 3 &&
                 parsed->at("flag").as_bool() == true,
                 "stream enum encoding matches wire types");

    config.retries.reset();
    encoded = toml::serialize(config);
    ok &= expect(encoded && encoded->find("retries") == std::string::npos,
                 "TOML omits empty optional constrained fields");
    if (encoded) {
        const auto decoded = toml::deserialize<Config>(*encoded);
        ok &= expect(decoded && !decoded->retries, "missing TOML optional remains empty");
    }
    output.clear();
    streamed = json::stream::serialize(config, collect(output));
    const auto optional_json = json::parse(output);
    ok &= expect(streamed && optional_json && optional_json->at("retries").is_null(),
                 "stream preserves JSON null for an empty optional");

    output.clear();
    streamed = json::stream::serialize(WideInteger{}, collect(output));
    ok &= expect(streamed && output == "{\"value\":18446744073709551615}",
                 "stream checks uint64 bounds without loss of precision");
    ok &= expect(!toml::serialize(WideInteger{}),
                 "TOML still rejects integers beyond its signed 64-bit range");
    return ok;
}

bool field_failures() {
    bool ok = true;
    const auto fail_value = [&](auto change, const char* message) {
        Config value;
        change(value);
        return rejects_encoding(value, message);
    };
    ok &= fail_value([](auto& c) { c.age = 17; }, "both encoders reject values below minimum");
    ok &= fail_value([](auto& c) { c.age = 151; }, "both encoders reject values above maximum");
    ok &= fail_value([](auto& c) { c.title = "x"; }, "both encoders reject short strings");
    ok &= fail_value([](auto& c) { c.title = "four"; }, "both encoders reject long strings");
    ok &= fail_value([](auto& c) { c.title = std::string("\xff", 1); }, "both encoders reject invalid UTF-8");
    ok &= fail_value([](auto& c) { c.samples.clear(); }, "both encoders reject too few sequence items");
    ok &= fail_value([](auto& c) { c.samples = {1, 2, 3}; }, "both encoders reject too many sequence items");
    ok &= fail_value([](auto& c) { c.labels.clear(); }, "both encoders reject too few map entries");
    ok &= fail_value([](auto& c) { c.labels = {{"a", 1}, {"b", 2}, {"c", 3}}; },
                     "both encoders reject too many map entries");
    ok &= fail_value([](auto& c) { c.retries = 0; }, "both encoders unwrap optional for minimum");
    ok &= fail_value([](auto& c) { c.retries = 5; }, "both encoders unwrap optional for maximum");
    ok &= rejects_encoding(InvalidOptional{}, "empty optional cannot conceal invalid options");
    ok &= rejects_encoding(Inapplicable{}, "inapplicable options reject encoding");
    ok &= rejects_encoding(NonFiniteBoundary{}, "non-finite constraint rejects encoding");
    ok &= rejects_encoding(BoundedFloat{std::numeric_limits<double>::quiet_NaN()},
                           "NaN cannot satisfy a declared floating-point interval");
    ok &= expect(!toml::deserialize<BoundedFloat>("ratio = nan\n") &&
                 !toml::deserialize<BoundedFloat>("ratio = inf\n") &&
                 !toml::deserialize<BoundedFloat>("ratio = 1.5\n"),
                 "TOML decoding rejects non-finite and out-of-range constrained floats");
    ok &= expect(!toml::deserialize<InvalidOptional>(""),
                 "missing optional cannot conceal invalid options in TOML decoding");
    ok &= expect(!toml::deserialize<OptionalDefault>(""),
                 "missing optional retained default must satisfy the contract");
    ok &= expect(!toml::deserialize<Inapplicable>("count = 2\n") &&
                 !toml::deserialize<NonFiniteBoundary>("ratio = 1.0\n"),
                 "invalid metadata rejects TOML decoding");

    const std::string fields = "display-name = \"ok\"\nmode = \"enabled\"\nlevel = 3\nflag = true\n";
    for (const auto invalid : {"age = 17\nsamples = [1]\nlabels = {a = 1}\n",
                               "age = 151\nsamples = [1]\nlabels = {a = 1}\n",
                               "age = 18\nsamples = []\nlabels = {a = 1}\n",
                               "age = 18\nsamples = [1,2,3]\nlabels = {a = 1}\n",
                               "age = 18\nsamples = [1]\nlabels = {}\n",
                               "age = 18\nsamples = [1]\nlabels = {a = 1,b = 2,c = 3}\n",
                               "age = 18\nsamples = [1]\nretries = 0\nlabels = {a = 1}\n"}) {
        ok &= expect(!toml::deserialize<Config>(fields + invalid),
                     "TOML decoding enforces scalar, optional and container bounds");
    }
    const std::string other_fields = "age = 18\nsamples = [1]\nlabels = {a = 1}\n"
                                     "mode = \"enabled\"\nlevel = 3\nflag = true\n";
    ok &= expect(!toml::deserialize<Config>(other_fields + "display-name = \"x\"\n") &&
                 !toml::deserialize<Config>(other_fields + "display-name = \"four\"\n"),
                 "TOML decoding enforces renamed string length");
    const auto unicode = toml::deserialize<Config>(other_fields + "display-name = \"e\xcc\x81\"\n");
    ok &= expect(unicode.has_value(), "length counts Unicode scalars rather than graphemes or bytes");
    const auto error = toml::deserialize<Config>(fields + "age = 17\nsamples = [1]\nlabels = {a = 1}\n");
    ok &= expect(!error && error.error().find("age") != std::string::npos,
                 "TOML constraint error preserves the field path");
    Nested nested;
    nested.configs[0].age = 17;
    ok &= rejects_encoding(nested, "nested containers enforce member constraints in both encoders");
    const auto nested_error = toml::deserialize<Nested>("[[configs]]\n" + fields +
                                                       "age = 17\nsamples = [1]\nlabels = {a = 1}\n");
    ok &= expect(!nested_error && nested_error.error().find("configs[0].age") != std::string::npos,
                 "nested TOML constraint error preserves container index and field path");
    return ok;
}

bool enum_failures() {
    bool ok = rejects_encoding(EnumBox<Mode>{static_cast<Mode>(99)},
                               "unknown string enum value rejects encoding");
    ok &= rejects_encoding(EnumBox<Level>{static_cast<Level>(8)},
                           "unknown underlying enum value rejects encoding");
    ok &= expect(!toml::deserialize<EnumBox<Mode>>("value = \"active\"\n") &&
                 !toml::deserialize<EnumBox<Mode>>("value = 2\n") &&
                 !toml::deserialize<EnumBox<Level>>("value = 8\n") &&
                 !toml::deserialize<EnumBox<Level>>("value = \"low\"\n") &&
                 !toml::deserialize<EnumBox<Switch>>("value = 1\n"),
                 "TOML rejects unknown enum names, values and incorrect wire types");
    ok &= rejects_encoding(EnumBox<Duplicate>{}, "duplicate enum names reject both encoders");
    ok &= rejects_encoding(EnumBox<Empty>{}, "empty enum descriptor rejects both encoders");
    ok &= rejects_encoding(EnumBox<InvalidName>{}, "invalid UTF-8 enum descriptor rejects both encoders");
    ok &= rejects_encoding(EnumBox<InvalidEncoding>{}, "invalid enum encoding rejects both encoders");
    ok &= rejects_encoding(EnumBox<DuplicateValue>{}, "duplicate enum values reject both encoders");
    ok &= expect(!toml::deserialize<EnumBox<Duplicate>>("value = \"same\"\n") &&
                 !toml::deserialize<EnumBox<Empty>>("value = 0\n") &&
                 !toml::deserialize<EnumBox<InvalidName>>("value = \"bad\"\n") &&
                 !toml::deserialize<EnumBox<InvalidEncoding>>("value = 0\n") &&
                 !toml::deserialize<EnumBox<DuplicateValue>>("value = 0\n"),
                 "invalid enum descriptors reject TOML decoding");
    ok &= rejects_encoding(EnumBox<std::optional<Duplicate>>{},
                           "empty optional enum field cannot hide an invalid descriptor");
    ok &= expect(!toml::deserialize<EnumBox<std::optional<Duplicate>>>(""),
                 "absent TOML optional enum field cannot hide an invalid descriptor");
    std::string optional_output;
    const auto root_optional = json::stream::serialize(std::optional<Duplicate>{}, collect(optional_output));
    const std::map<std::string, std::optional<Duplicate>> optional_map{{"value", std::nullopt}};
    const auto map_optional = json::stream::serialize(optional_map, collect(optional_output));
    const auto map_toml = toml::serialize(optional_map);
    ok &= expect(!root_optional && !map_optional && !map_toml,
                 "root and map optional enum values cannot hide invalid descriptors");
    const auto plain = toml::serialize(PlainEnums{});
    std::string output;
    const auto streamed = json::stream::serialize(PlainEnums{}, collect(output));
    ok &= expect(plain && plain->find("mode = 100") != std::string::npos &&
                 plain->find("flag = true") != std::string::npos && streamed &&
                 output == "{\"mode\":100,\"flag\":true}",
                 "unregistered enums preserve actual underlying integer and boolean encodings");
    const auto decoded = toml::deserialize<PlainEnums>("mode = 100\nflag = true\n");
    ok &= expect(decoded && static_cast<unsigned>(decoded->mode) == 100 && decoded->flag == PlainSwitch::on,
                 "unregistered enums retain underlying TOML decode behavior");
    return ok;
}

bool sticky_failures() {
    std::string output;
    json::stream::StreamWriter writer(collect(output));
    Config invalid;
    invalid.age = 17;
    const auto encoded = writer.value(invalid);
    const auto status = writer.status();
    const auto finished = writer.finish();
    const auto bytes = output.size();
    const auto attempted = writer.value(1);
    bool ok = expect(!encoded && !status && !finished && !attempted &&
                     encoded.error() == status.error() && status.error() == finished.error() &&
                     finished.error() == attempted.error() && output.size() == bytes,
                     "stream constraint failure is sticky and stops later output");
    writer.reset();
    output.clear();
    const auto invalid_enum = writer.value(static_cast<Mode>(99));
    const auto enum_status = writer.status();
    const auto enum_finish = writer.finish();
    ok &= expect(!invalid_enum && !enum_status && !enum_finish && output.empty() &&
                 invalid_enum.error() == enum_status.error() && enum_status.error() == enum_finish.error(),
                 "root enum failure is sticky before emitting bytes");
    writer.reset();
    output.clear();
    const auto valid = writer.value(Mode::active);
    const auto completed = writer.finish();
    ok &= expect(valid && completed && output == "\"enabled\"", "reset recovers from metadata failure");
    return ok;
}

} // namespace contract_tests

int main() {
    bool ok = contract_tests::valid_roundtrips();
    ok &= contract_tests::custom_descriptors();
    ok &= contract_tests::field_failures();
    ok &= contract_tests::enum_failures();
    ok &= contract_tests::sticky_failures();
    if (ok) std::puts("toml_stream_contract PASS");
    return ok ? 0 : 1;
}
