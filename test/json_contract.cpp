#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include <serde/json/json.hpp>
#include <serde/reflect/reflect_macros.hpp>

namespace {

void require(bool condition, std::string_view message) {
    if (!condition) {
        std::fprintf(stderr, "json_contract: %.*s\n", static_cast<int>(message.size()), message.data());
        std::abort();
    }
}

template <class T>
void require_error(const json::result<T>& result, std::string_view fragment) {
    require(!result, "operation must fail");
    if (result.error().find(fragment) == std::string::npos) {
        std::fprintf(stderr, "json_contract: expected '%.*s', actual '%s'\n",
                     static_cast<int>(fragment.size()), fragment.data(), result.error().c_str());
        std::abort();
    }
}

enum class Mode : std::uint8_t { active = 1, disabled = 2 };
constexpr auto reflect_enum(std::type_identity<Mode>) {
    return reflect::enum_descriptor<Mode, 2>{reflect::enum_encoding::string,
        {{{Mode::active, "active"}, {Mode::disabled, "disabled"}}}};
}

enum class Limit : std::uint64_t { low = 0, high = UINT64_MAX };
constexpr auto reflect_enum(std::type_identity<Limit>) {
    return reflect::enum_descriptor<Limit, 2>{reflect::enum_encoding::underlying,
        {{{Limit::low, "low"}, {Limit::high, "high"}}}};
}

enum class Toggle : bool { off = false, on = true };
constexpr auto reflect_enum(std::type_identity<Toggle>) {
    return reflect::enum_descriptor<Toggle, 2>{reflect::enum_encoding::underlying,
        {{{Toggle::off, "off"}, {Toggle::on, "on"}}}};
}

enum class Plain : std::int16_t { known = 1 };

enum class RuntimeMode { active = 1, disabled = 2 };
auto reflect_enum(std::type_identity<RuntimeMode>) {
    return reflect::enum_descriptor<RuntimeMode, 2>{reflect::enum_encoding::string,
        {{{RuntimeMode::active, "active"}, {RuntimeMode::disabled, "disabled"}}}};
}

enum class RuntimeLevel { low = 1, high = 2 };
auto reflect_enum(std::type_identity<RuntimeLevel>) {
    return reflect::enum_descriptor<RuntimeLevel, 2>{reflect::enum_encoding::underlying,
        {{{RuntimeLevel::low, "low"}, {RuntimeLevel::high, "high"}}}};
}

enum class Duplicated { first, second };
constexpr auto reflect_enum(std::type_identity<Duplicated>) {
    return reflect::enum_descriptor<Duplicated, 2>{reflect::enum_encoding::string,
        {{{Duplicated::first, "same"}, {Duplicated::second, "same"}}}};
}

enum class DuplicateValue { first, second };
constexpr auto reflect_enum(std::type_identity<DuplicateValue>) {
    return reflect::enum_descriptor<DuplicateValue, 2>{reflect::enum_encoding::underlying,
        {{{DuplicateValue::first, "first"}, {DuplicateValue::first, "second"}}}};
}

enum class Empty { value };
constexpr auto reflect_enum(std::type_identity<Empty>) {
    return reflect::enum_descriptor<Empty, 0>{reflect::enum_encoding::string, {}};
}

enum class InvalidEncoding { value };
constexpr auto reflect_enum(std::type_identity<InvalidEncoding>) {
    return reflect::enum_descriptor<InvalidEncoding, 1>{static_cast<reflect::enum_encoding>(99),
        {{{InvalidEncoding::value, "value"}}}};
}

enum class InvalidUtf8 { value };
constexpr auto reflect_enum(std::type_identity<InvalidUtf8>) {
    return reflect::enum_descriptor<InvalidUtf8, 1>{reflect::enum_encoding::string,
        {{{InvalidUtf8::value, "\xc0\x80"}}}};
}

struct Numeric {
    std::int64_t signed_value{};
    std::uint64_t unsigned_value{};
    double ratio{};
};
#define NUMERIC_FIELDS(X) \
    X(signed_value, "signed", (reflect::field_options<std::int64_t>{ \
        .minimum = INT64_MIN, .maximum = INT64_MIN + 1})) \
    X(unsigned_value, "unsigned", (reflect::field_options<std::uint64_t>{ \
        .minimum = UINT64_MAX - 1, .maximum = UINT64_MAX})) \
    X(ratio, "ratio", (reflect::field_options<double>{.minimum = 0.0, .maximum = 1.0}))
REFLECT_FIELDS(Numeric, NUMERIC_FIELDS)
#undef NUMERIC_FIELDS

struct Text {
    std::string name;
};
#define TEXT_FIELDS(X) \
    X(name, "display-name", (reflect::field_options<std::string>{.min_length = 2, .max_length = 3}))
REFLECT_FIELDS(Text, TEXT_FIELDS)
#undef TEXT_FIELDS

struct Containers {
    std::vector<int> list;
    std::array<int, 2> pair{};
    std::map<std::string, int> map;
};
#define CONTAINER_FIELDS(X) \
    X(list, "list", (reflect::field_options<std::vector<int>>{.min_items = 1, .max_items = 2})) \
    X(pair, "pair", (reflect::field_options<std::array<int, 2>>{.min_items = 2, .max_items = 2})) \
    X(map, "map", (reflect::field_options<std::map<std::string, int>>{.min_items = 1, .max_items = 2}))
REFLECT_FIELDS(Containers, CONTAINER_FIELDS)
#undef CONTAINER_FIELDS

struct Nested {
    std::vector<Text> names;
};
#define NESTED_FIELDS(X) X(names)
REFLECT_FIELDS(Nested, NESTED_FIELDS)
#undef NESTED_FIELDS

struct OptionalDefault {
    std::optional<int> age = 20;
};
#define OPTIONAL_FIELDS(X) \
    X(age, "age", (reflect::field_options<std::optional<int>>{.minimum = 18, .maximum = 30}))
REFLECT_FIELDS(OptionalDefault, OPTIONAL_FIELDS)
#undef OPTIONAL_FIELDS

struct InvalidDefault {
    std::optional<int> age = 12;
};
#define INVALID_DEFAULT_FIELDS(X) \
    X(age, "age", (reflect::field_options<std::optional<int>>{.minimum = 18}))
REFLECT_FIELDS(InvalidDefault, INVALID_DEFAULT_FIELDS)
#undef INVALID_DEFAULT_FIELDS

struct InvalidOptions {
    std::optional<int> value;
};
#define INVALID_OPTIONS_FIELDS(X) \
    X(value, "value", (reflect::field_options<std::optional<int>>{.minimum = 5, .maximum = 1}))
REFLECT_FIELDS(InvalidOptions, INVALID_OPTIONS_FIELDS)
#undef INVALID_OPTIONS_FIELDS

struct InapplicableOptions {
    int value{};
};
#define INAPPLICABLE_FIELDS(X) \
    X(value, "value", (reflect::field_options<int>{.min_length = 1}))
REFLECT_FIELDS(InapplicableOptions, INAPPLICABLE_FIELDS)
#undef INAPPLICABLE_FIELDS

struct NonFiniteOptions {
    double value{};
};
#define NON_FINITE_FIELDS(X) \
    X(value, "value", (reflect::field_options<double>{.minimum = std::numeric_limits<double>::quiet_NaN()}))
REFLECT_FIELDS(NonFiniteOptions, NON_FINITE_FIELDS)
#undef NON_FINITE_FIELDS

struct InvalidOptionalEnum {
    std::optional<Duplicated> value;
};
#define INVALID_ENUM_FIELDS(X) X(value)
REFLECT_FIELDS(InvalidOptionalEnum, INVALID_ENUM_FIELDS)
#undef INVALID_ENUM_FIELDS

struct Input {
    int id = 77;
    std::string name;
    std::optional<int> age = 20;
    Mode mode = Mode::disabled;
};
#define INPUT_FIELDS(X) \
    X(id) \
    X(name, "display-name", (reflect::field_options<std::string>{.min_length = 2})) \
    X(age, "age", (reflect::field_options<std::optional<int>>{.minimum = 18})) \
    X(mode)
REFLECT_FIELDS(Input, INPUT_FIELDS)
#undef INPUT_FIELDS

struct WideInput {
    int f00{}, f01{}, f02{}, f03{}, f04{}, f05{}, f06{}, f07{}, f08{}, f09{};
    int f10{}, f11{}, f12{}, f13{}, f14{}, f15{}, f16{}, f17{}, f18{}, f19{};
};
#define WIDE_FIELDS(X) \
    X(f00) X(f01) X(f02) X(f03) X(f04) X(f05) X(f06) X(f07) X(f08) X(f09) \
    X(f10) X(f11) X(f12) X(f13) X(f14) X(f15) X(f16) X(f17) X(f18) \
    X(f19, "f19", (reflect::field_options<int>{.minimum = 18}))
REFLECT_FIELDS(WideInput, WIDE_FIELDS)
#undef WIDE_FIELDS

struct ReferenceInput {
    int id = 77;
    std::optional<int> age = 20;
};
const auto& reflect_fields(const ReferenceInput&) {
    static constexpr auto fields = std::tuple{
        reflect::make_field("id", &ReferenceInput::id),
        reflect::make_field("age", &ReferenceInput::age,
                            reflect::field_options<std::optional<int>>{.minimum = 18})};
    return fields;
}

struct CustomOptions {
    int age = 20;
};
constexpr auto reflect_fields(std::type_identity<CustomOptions>) {
    return std::tuple{reflect::make_field("age", &CustomOptions::age)};
}
auto reflect_fields(const CustomOptions&) {
    return std::tuple{reflect::make_field(
        "age", &CustomOptions::age, reflect::field_options<int>{.minimum = 18})};
}

struct ReorderedFields {
    int order{};
    int left{};
    int right = 99;
};
auto reflect_fields(const ReorderedFields& value) {
    const auto left = reflect::make_field("left", &ReorderedFields::left);
    const auto right = reflect::make_field("right", &ReorderedFields::right);
    return std::tuple{reflect::make_field("order", &ReorderedFields::order),
                      value.order ? right : left, value.order ? left : right};
}

struct NonDefaultInput {
    explicit NonDefaultInput(int id_value) : id(id_value) {}
    int id;
    std::string name;
};
#define NON_DEFAULT_FIELDS(X) X(id) X(name)
REFLECT_FIELDS(NonDefaultInput, NON_DEFAULT_FIELDS)
#undef NON_DEFAULT_FIELDS

json::Json parsed(std::string_view text) {
    auto result = json::parse(text);
    require(result.has_value(), "JSON fixture must parse");
    return std::move(*result);
}

void test_numeric_constraints() {
    Numeric value{INT64_MIN, UINT64_MAX, 0.5};
    auto encoded = json::serialize(value);
    require(encoded.has_value(), "exact integer boundaries must encode");
    require(encoded->find("18446744073709551615") != std::string::npos,
            "uint64 maximum must be exact");
    auto decoded = json::deserialize<Numeric>(*encoded);
    require(decoded && decoded->signed_value == INT64_MIN && decoded->unsigned_value == UINT64_MAX,
            "integer boundaries must roundtrip");
    value.unsigned_value = UINT64_MAX - 2;
    require_error(json::serialize(value), "unsigned");
    require_error(json::deserialize<Numeric>(
        R"({"signed":-9223372036854775808,"unsigned":18446744073709551613,"ratio":0.5})"), "unsigned");
    value.unsigned_value = UINT64_MAX;
    value.ratio = 1.5;
    require_error(json::serialize(value), "ratio");
    require_error(json::deserialize<Numeric>(
        R"({"signed":-9223372036854775808,"unsigned":18446744073709551615,"ratio":-0.1})"), "ratio");
    require_error(json::serialize(InapplicableOptions{}), "value");
    require_error(json::deserialize<InapplicableOptions>(R"({"value":5})"), "value");
    require_error(json::serialize(NonFiniteOptions{}), "value");
    require_error(json::deserialize<NonFiniteOptions>(R"({"value":0})"), "value");
}

void test_text_and_container_constraints() {
    const std::string two_scalars = "\xe4\xbd\xa0\xf0\x9f\x98\x80";
    auto encoded = json::serialize(Text{two_scalars});
    require(encoded.has_value(), "UTF-8 scalar length is not byte length");
    auto decoded = json::deserialize<Text>(*encoded);
    require(decoded && decoded->name == two_scalars, "UTF-8 text must roundtrip");
    require(json::serialize(Text{"e\xcc\x81"}).has_value(), "combining marks count as scalars");
    require_error(json::serialize(Text{"a"}), "display-name");
    require_error(json::deserialize<Text>(R"({"display-name":"abcd"})"), "display-name");
    require_error(json::serialize(Text{"\xc0\x80"}), "display-name");

    Containers value{{1}, {2, 3}, {{"a", 4}}};
    auto container_text = json::serialize(value);
    require(container_text.has_value(), "container constraints must accept boundaries");
    require(json::deserialize<Containers>(*container_text).has_value(), "container constraints decode");
    value.list.clear();
    require_error(json::serialize(value), "list");
    require_error(json::deserialize<Containers>(R"({"list":[],"pair":[2,3],"map":{"a":4}})"), "list");
    value.list = {1};
    value.map.clear();
    require_error(json::serialize(value), "map");
    require_error(json::deserialize<Containers>(R"({"list":[1],"pair":[2,3],"map":{}})"), "map");
    require_error(json::deserialize<Nested>(R"({"names":[{"display-name":"a"}]})"),
                  "value.names[0].display-name");
    require(!json::serialize(Nested{{Text{"a"}}}), "nested constraints execute during encoding");
}

void test_enums() {
    auto text = json::serialize(Mode::active);
    require(text && *text == "\"active\"", "registered string enum encoding");
    auto mode = json::deserialize<Mode>("\"disabled\"");
    require(mode && *mode == Mode::disabled, "registered string enum decoding");
    require_error(json::deserialize<Mode>("1"), "string");
    require_error(json::deserialize<Mode>("\"other\""), "enum");
    require_error(json::serialize(static_cast<Mode>(3)), "enum");

    auto high = json::serialize(Limit::high);
    require(high && *high == "18446744073709551615", "underlying enum uint64 encoding");
    auto limit = json::deserialize<Limit>(*high);
    require(limit && *limit == Limit::high, "underlying enum uint64 decoding");
    require_error(json::deserialize<Limit>("1"), "enum");
    require_error(json::deserialize<Limit>("\"high\""), "integer");
    auto toggle = json::serialize(Toggle::on);
    require(toggle && *toggle == "true", "bool enum encoding uses boolean");
    auto off = json::deserialize<Toggle>("false");
    require(off && *off == Toggle::off, "bool enum decoding uses boolean");
    require_error(json::deserialize<Toggle>("1"), "boolean");
    auto plain = json::deserialize<Plain>("123");
    require(plain && static_cast<std::int16_t>(*plain) == 123, "unregistered enum retains underlying rules");
    auto runtime_mode = json::serialize(RuntimeMode::active);
    require(runtime_mode && *runtime_mode == "\"active\"", "nonconstexpr string enum metadata encodes");
    auto runtime_mode_back = json::deserialize<RuntimeMode>("\"disabled\"");
    require(runtime_mode_back && *runtime_mode_back == RuntimeMode::disabled,
            "nonconstexpr string enum metadata decodes");
    require_error(json::deserialize<RuntimeMode>("\"other\""), "enum");
    require_error(json::serialize(static_cast<RuntimeMode>(3)), "enum");
    auto runtime_level = json::serialize(RuntimeLevel::high);
    require(runtime_level && *runtime_level == "2", "nonconstexpr integer enum metadata encodes");
    auto runtime_level_back = json::deserialize<RuntimeLevel>("1");
    require(runtime_level_back && *runtime_level_back == RuntimeLevel::low,
            "nonconstexpr integer enum metadata decodes");
    require_error(json::deserialize<RuntimeLevel>("3"), "enum");
    require_error(json::serialize(static_cast<RuntimeLevel>(3)), "enum");
    const std::map<std::string, std::vector<Mode>> grouped{{"modes", {Mode::active, Mode::disabled}}};
    auto grouped_json = json::serialize(grouped);
    require(grouped_json.has_value(), "enum codec runs inside maps and vectors");
    auto grouped_back = json::deserialize<std::map<std::string, std::vector<Mode>>>(*grouped_json);
    require(grouped_back && *grouped_back == grouped, "nested enum codec roundtrip");
    require_error(json::deserialize<std::vector<Mode>>(R"(["active","other"])"), "value[1]");

    require(!json::serialize(Duplicated::first), "duplicate enum names must fail encode");
    require(!json::deserialize<Duplicated>("\"same\""), "duplicate enum names must fail decode");
    require(!json::serialize(DuplicateValue::first), "duplicate enum values must fail encode");
    require(!json::deserialize<DuplicateValue>("0"), "duplicate enum values must fail decode");
    require(!json::serialize(Empty::value), "empty enum descriptor must fail encode");
    require(!json::deserialize<Empty>("\"value\""), "empty enum descriptor must fail decode");
    require(!json::serialize(InvalidEncoding::value), "invalid enum encoding must fail encode");
    require(!json::deserialize<InvalidEncoding>("0"), "invalid enum encoding must fail decode");
    require(!json::serialize(InvalidUtf8::value), "invalid enum name UTF-8 must fail encode");
    require(!json::deserialize<InvalidUtf8>("\"value\""), "invalid enum name UTF-8 must fail decode");
    require(!json::serialize(InvalidOptionalEnum{}), "empty optional validates enum metadata");
    require(!json::deserialize<InvalidOptionalEnum>("{}"), "missing optional validates enum metadata");
    require(!json::deserialize<InvalidOptionalEnum>(R"({"value":null})"), "null optional validates enum metadata");
    require(!json::serialize(std::optional<Duplicated>{}), "root optional validates enum metadata");
    require(!json::deserialize<std::optional<Duplicated>>("null"), "root null validates enum metadata");
    require(!json::serialize(std::vector<std::optional<Duplicated>>{std::nullopt}),
            "container optional validates enum metadata");
    require(!json::deserialize<std::vector<std::optional<Duplicated>>>("[null]"),
            "container null validates enum metadata");
}

void test_optional_and_decode_field() {
    auto missing = json::deserialize<OptionalDefault>("{}");
    require(missing && missing->age == 20, "missing optional preserves DTO default");
    auto null = json::deserialize<OptionalDefault>(R"({"age":null})");
    require(null && !null->age, "explicit null clears optional");
    require_error(json::deserialize<OptionalDefault>(R"({"age":17})"), "age");
    require_error(json::deserialize<InvalidDefault>("{}"), "age");
    require(json::deserialize<InvalidDefault>(R"({"age":null})").has_value(), "null skips value range");
    require_error(json::deserialize<InvalidOptions>("{}"), "value");
    require_error(json::deserialize<InvalidOptions>(R"({"value":null})"), "value");
    require_error(json::serialize(InvalidOptions{}), "value");
    static_assert(reflect::StaticReflectable<CustomOptions>);
    require_error(json::deserialize<CustomOptions>(R"({"age":17})"), "age");
    require_error(json::serialize(CustomOptions{17}), "age");

    constexpr auto optional_field = std::get<0>(reflect::static_fields<OptionalDefault>());
    auto standalone_missing = json::decode_field(optional_field, json::Json{});
    require(standalone_missing && !*standalone_missing, "standalone missing optional has no owner default");
    auto standalone_null = json::decode_field(optional_field, parsed("null"));
    require(standalone_null && !*standalone_null, "standalone null optional clears");
    require_error(json::decode_field(optional_field, parsed("17")), "age");
    auto valid = json::decode_field(optional_field, parsed("22"));
    require(valid && *valid == 22, "standalone field enforces metadata");

    constexpr auto name_field = std::get<0>(reflect::static_fields<Text>());
    require_error(json::decode_field(name_field, json::Json{}), "missing");
    require_error(json::decode_field(name_field, parsed("null")), "display-name");
    require_error(json::decode_field(name_field, parsed("\"a\"")), "display-name");
    constexpr auto invalid_field = std::get<0>(reflect::static_fields<InvalidOptions>());
    require_error(json::decode_field(invalid_field, json::Json{}), "value");
    require_error(json::decode_field(invalid_field, parsed("null")), "value");
}

void test_selected_fields() {
    Input input;
    const auto body_fields = [](const auto& descriptor) { return descriptor.name != "id"; };
    auto result = json::decode_fields_into(parsed(R"({"display-name":"Ada","mode":"active"})"), input, body_fields);
    require(result && input.id == 77 && input.name == "Ada" && input.age == 20 && input.mode == Mode::active,
            "selection does not require or overwrite excluded fields");
    result = json::decode_fields_into(parsed(R"({"display-name":"Ada","mode":"disabled","age":null})"), input, body_fields);
    require(result && !input.age, "selected null optional clears existing value");
    require_error(json::decode_fields_into(parsed(R"({"mode":"active"})"), input, body_fields), "display-name");
    require_error(json::decode_fields_into(parsed(R"({"display-name":"x","mode":"active"})"), input, body_fields), "display-name");
    require_error(json::decode_fields_into(parsed(R"({"display-name":"Ada","mode":"other"})"), input, body_fields), "mode");
    require_error(json::decode_fields_into(parsed("[]"), input, body_fields), "object");

    json::ParseOptions strict;
    strict.unknown_fields = json::UnknownFieldPolicy::reject;
    require_error(json::decode_fields_into(parsed(R"({"id":99,"display-name":"Ada","mode":"active"})"), input, body_fields, strict), "id");
    require_error(json::decode_fields_into(parsed(R"({"extra":99,"display-name":"Ada","mode":"active"})"), input, body_fields, strict), "extra");
    auto none = [](const auto&) { return false; };
    require(json::decode_fields_into(parsed("{}"), input, none, strict).has_value(), "empty selection accepts empty object");
    require_error(json::decode_fields_into(parsed(R"({"id":99})"), input, none, strict), "id");
    input.age = 12;
    auto age_only = [](const auto& descriptor) { return descriptor.name == "age"; };
    require_error(json::decode_fields_into(parsed("{}"), input, age_only), "age");

    WideInput wide;
    auto last_only = [](const auto& descriptor) { return descriptor.name == "f19"; };
    require(json::decode_fields_into(parsed(R"({"f19":19})"), wide, last_only, strict).has_value() && wide.f19 == 19,
            "wide selected object uses only selected fields");
    require_error(json::decode_fields_into(parsed(R"({"f19":17})"), wide, last_only), "f19");
    std::string wide_json = "{";
    for (int index = 0; index < 20; ++index) {
        if (index != 0) wide_json += ',';
        wide_json += "\"f" + std::string(index < 10 ? "0" : "") + std::to_string(index) + "\":" + std::to_string(index);
    }
    wide_json += '}';
    require(json::deserialize<WideInput>(wide_json).has_value(), "wide ordinary decode retains field index behavior");
    wide_json.replace(wide_json.rfind("19"), 2, "17");
    require_error(json::deserialize<WideInput>(wide_json), "f19");

    ReferenceInput reference;
    require(json::decode_fields_into(parsed(R"({"age":22})"), reference, age_only, strict).has_value() &&
            reference.id == 77 && reference.age == 22,
            "reference-returning descriptors support selected decode");
    auto reference_back = json::deserialize<ReferenceInput>(R"({"id":12})");
    require(reference_back && reference_back->id == 12 && reference_back->age == 20,
            "runtime descriptors preserve missing optional defaults");
    NonDefaultInput nondefault(42);
    auto name_only = [](const auto& descriptor) { return descriptor.name == "name"; };
    require(json::decode_fields_into(parsed(R"({"name":"Ada"})"), nondefault, name_only, strict).has_value() &&
            nondefault.id == 42 && nondefault.name == "Ada",
            "selected decode uses supplied object without default constructing its owner");

    ReorderedFields reordered;
    auto exclude_right = [](const auto& field) { return field.name != "right"; };
    auto reordered_result = json::decode_fields_into(
        parsed(R"({"order":1,"left":7})"), reordered, exclude_right, strict);
    require(reordered_result && reordered.order == 1 && reordered.left == 7 && reordered.right == 99,
            "strict selection uses the same descriptor snapshot when runtime fields reorder");
    reordered = {};
    require_error(json::decode_fields_into(parsed(R"({"order":1,"left":7,"right":8})"),
                                          reordered, exclude_right, strict), "right");
}

} // namespace

int main() {
    test_numeric_constraints();
    test_text_and_container_constraints();
    test_enums();
    test_optional_and_decode_field();
    test_selected_fields();
    return 0;
}
