#include <serde/reflect/reflect.hpp>
#include <serde/reflect/reflect_macros.hpp>
#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace fixture {

struct Account {
    std::uint64_t id{};
    std::string name;
    std::optional<int> age = 25;
    std::vector<int> values;
};

#define ACCOUNT_FIELDS(X) \
    X(id, "external-id", (reflect::field_options<std::uint64_t>{.description = "Exact identifier", .minimum = std::uint64_t{9007199254740993ULL}, .maximum = std::numeric_limits<std::uint64_t>::max()})) \
    X(name, "name", (reflect::field_options<std::string>{.description = "Display name", .min_length = 1, .max_length = 3})) \
    X(age, "age", (reflect::field_options<std::optional<int>>{.minimum = 18, .maximum = 120})) \
    X(values)
REFLECT_FIELDS(Account, ACCOUNT_FIELDS)
#undef ACCOUNT_FIELDS

struct Plain { int value{}; };
struct PlainLayout {
    std::string_view name;
    int Plain::*pointer;
};
#define PLAIN_FIELDS(X) X(value, "renamed")
REFLECT_FIELDS(Plain, PLAIN_FIELDS)
#undef PLAIN_FIELDS

enum class Mode { Active = 1, Disabled = 2 };
constexpr auto reflect_enum(std::type_identity<Mode>) {
    return reflect::enum_descriptor<Mode, 2>{
        reflect::enum_encoding::string,
        {{{Mode::Active, "active"}, {Mode::Disabled, "disabled"}}}
    };
}

enum class Code : std::uint64_t { Large = 9007199254740993ULL, Maximum = UINT64_MAX };
constexpr auto reflect_enum(std::type_identity<Code>) {
    return reflect::enum_descriptor<Code, 2>{
        reflect::enum_encoding::underlying,
        {{{Code::Large, "large"}, {Code::Maximum, "maximum"}}}
    };
}

enum class Flag : bool { Off = false, On = true };
constexpr auto reflect_enum(std::type_identity<Flag>) {
    return reflect::enum_descriptor<Flag, 2>{
        reflect::enum_encoding::underlying,
        {{{Flag::Off, "off"}, {Flag::On, "on"}}}
    };
}

enum class PlainEnum : int { Known = 1 };
enum class Empty { Value };
constexpr auto reflect_enum(std::type_identity<Empty>) {
    return reflect::enum_descriptor<Empty, 0>{reflect::enum_encoding::string, {}};
}
enum class DuplicateValue { One, Two };
constexpr auto reflect_enum(std::type_identity<DuplicateValue>) {
    return reflect::enum_descriptor<DuplicateValue, 2>{
        reflect::enum_encoding::string,
        {{{DuplicateValue::One, "one"}, {DuplicateValue::One, "other"}}}
    };
}
enum class DuplicateName { One, Two };
constexpr auto reflect_enum(std::type_identity<DuplicateName>) {
    return reflect::enum_descriptor<DuplicateName, 2>{
        reflect::enum_encoding::string,
        {{{DuplicateName::One, "same"}, {DuplicateName::Two, "same"}}}
    };
}
enum class InvalidName { Value };
constexpr auto reflect_enum(std::type_identity<InvalidName>) {
    return reflect::enum_descriptor<InvalidName, 1>{
        reflect::enum_encoding::string,
        {{{InvalidName::Value, "\xED\xA0\x80"}}}
    };
}
enum class EmptyName { Value };
constexpr auto reflect_enum(std::type_identity<EmptyName>) {
    return reflect::enum_descriptor<EmptyName, 1>{
        reflect::enum_encoding::underlying,
        {{{EmptyName::Value, ""}}}
    };
}
enum class InvalidEncoding { Value };
constexpr auto reflect_enum(std::type_identity<InvalidEncoding>) {
    return reflect::enum_descriptor<InvalidEncoding, 1>{
        static_cast<reflect::enum_encoding>(99),
        {{{InvalidEncoding::Value, "value"}}}
    };
}

enum class RuntimeMode { One, Two };
inline bool invalid_runtime_descriptor = false;
inline unsigned runtime_descriptor_calls = 0;
auto reflect_enum(std::type_identity<RuntimeMode>) {
    ++runtime_descriptor_calls;
    return reflect::enum_descriptor<RuntimeMode, 2>{
        reflect::enum_encoding::string,
        {{{RuntimeMode::One, "one"},
          {RuntimeMode::Two, invalid_runtime_descriptor ? "one" : "two"}}}
    };
}

enum class MutableName { One, Two };
inline char mutable_enum_name[] = "two";
constexpr auto reflect_enum(std::type_identity<MutableName>) {
    return reflect::enum_descriptor<MutableName, 2>{
        reflect::enum_encoding::string,
        {{{MutableName::One, "one"}, {MutableName::Two, {mutable_enum_name, 3}}}}
    };
}

enum class EvaluationMode { One, Two };
inline char evaluation_name[] = "two";
constexpr auto reflect_enum(std::type_identity<EvaluationMode>) {
    if consteval {
        return reflect::enum_descriptor<EvaluationMode, 2>{
            reflect::enum_encoding::string,
            {{{EvaluationMode::One, "one"}, {EvaluationMode::Two, "two"}}}};
    } else {
        return reflect::enum_descriptor<EvaluationMode, 2>{
            reflect::enum_encoding::string,
            {{{EvaluationMode::One, "one"}, {EvaluationMode::Two, {evaluation_name, 3}}}}};
    }
}

template<class T>
struct Box { T value; };

} // namespace fixture

namespace {

bool expect(bool condition, std::string_view label) {
    if (!condition) std::cerr << "field_contract: " << label << '\n';
    return condition;
}

template<class T>
bool accepts(const T& value, reflect::field_options<T> options) {
    const auto descriptor = reflect::make_field("value", &fixture::Box<T>::value, options);
    return reflect::validate_field(descriptor, value).has_value();
}

template<class T>
bool enumDescriptorFails(std::string_view label) {
    const auto result = reflect::validate_enum_descriptor<T>();
    return expect(!result && !result.error().empty(), label);
}

} // namespace

int main() {
    static_assert(reflect::StaticReflectable<fixture::Account>);
    static_assert(reflect::StaticReflectable<fixture::Plain>);
    static_assert(reflect::is_optional_v<std::optional<int>>);
    static_assert(reflect::is_optional_v<const std::optional<int>&>);
    static_assert(!reflect::is_optional_v<int>);
    static_assert(sizeof(reflect::field<fixture::Plain, int>) == sizeof(fixture::PlainLayout));
    static_assert(reflect::field<fixture::Plain, int>::options.empty());
    static_assert(!std::is_convertible_v<reflect::field<fixture::Plain, int, true>,
                                         reflect::field<fixture::Plain, int>>);
    static_assert(!std::is_base_of_v<reflect::field<fixture::Plain, int>,
                                     reflect::field<fixture::Plain, int, true>>);
    constexpr auto fields = reflect::static_fields<fixture::Account>();
    static_assert(std::get<0>(fields).name == "external-id");
    static_assert(std::get<0>(fields).options.description == "Exact identifier");
    static_assert(*std::get<0>(fields).options.minimum == 9007199254740993ULL);
    static_assert(std::same_as<decltype(std::get<0>(fields).options.minimum), std::optional<std::uint64_t>>);
    static_assert(std::same_as<decltype(std::get<2>(fields).options.minimum), std::optional<int>>);
    static_assert(reflect::EnumReflectable<fixture::Mode>);
    static_assert(reflect::EnumReflectable<fixture::Flag>);
    static_assert(!reflect::EnumReflectable<fixture::PlainEnum>);
    static_assert(reflect::enum_descriptor_for<fixture::Mode>().values[0].name == "active");
    static_assert(reflect::enum_descriptor_for<fixture::Code>().values[1].value == fixture::Code::Maximum);
    static_assert(reflect::validate_enum_descriptor<fixture::Mode>().has_value());
    static_assert(reflect::validate_enum(fixture::Mode::Active).has_value());

    bool passed = true;
    fixture::Account account{std::numeric_limits<std::uint64_t>::max(), "Ada", 25, {1}};
    passed &= expect(reflect::validate_field(std::get<0>(fields), account.id).has_value(), "uint64 maximum accepted");
    passed &= expect(!reflect::validate_field(std::get<0>(fields), std::uint64_t{9007199254740992ULL}), "uint64 boundary keeps precision");
    const auto plain = reflect::static_fields<fixture::Plain>();
    passed &= expect(reflect::validate_field(std::get<0>(plain), -999).has_value(), "default options use same field path");

    passed &= expect(accepts<std::int64_t>(INT64_MIN, {.minimum = INT64_MIN, .maximum = INT64_MAX}), "signed boundary accepted");
    passed &= expect(!accepts<int>(5, {.minimum = 10, .maximum = 1}), "reversed numeric bounds");
    passed &= expect(!accepts<double>(1, {.minimum = std::numeric_limits<double>::quiet_NaN()}), "NaN bound rejected");
    passed &= expect(!accepts<double>(1, {.maximum = std::numeric_limits<double>::infinity()}), "infinite bound rejected");
    passed &= expect(!accepts<double>(std::numeric_limits<double>::infinity(), {.minimum = 0}), "non-finite constrained value rejected");
    passed &= expect(!accepts<double>(std::numeric_limits<double>::quiet_NaN(), {.maximum = 10}), "NaN constrained value rejected");
    passed &= expect(!accepts<bool>(true, {.minimum = 1}), "bool numeric options rejected");
    passed &= expect(!accepts<std::string>("x", {.minimum = 1}), "string numeric options rejected");
    passed &= expect(!accepts<int>(1, {.min_length = 1}), "numeric string options rejected");
    passed &= expect(!accepts<int>(1, {.min_items = 1}), "numeric container options rejected");
    passed &= expect(!accepts<std::string>("x", {.min_items = 1}), "string is not item container");

    passed &= expect(accepts<std::string>("abc", {.min_length = 3, .max_length = 3}), "ASCII scalar count");
    passed &= expect(accepts<std::string>("\xC3\xA9\xE4\xB8\xAD\xF0\x9F\x98\x80", {.min_length = 3, .max_length = 3}), "UTF8 two/three/four-byte scalar count");
    passed &= expect(accepts<std::string>("e\xCC\x81", {.min_length = 2, .max_length = 2}), "combining mark counts as scalar");
    passed &= expect(accepts<std::string>(std::string("a\0b", 3), {.min_length = 3, .max_length = 3}), "NUL is valid scalar");
    passed &= expect(accepts<std::string_view>("\xF4\x8F\xBF\xBF", {.min_length = 1, .max_length = 1}), "maximum Unicode scalar");
    passed &= expect(!accepts<std::string>("abc", {.min_length = 4}), "string minimum");
    passed &= expect(!accepts<std::string>("abc", {.max_length = 2}), "string maximum");
    passed &= expect(!accepts<std::string>("abc", {.min_length = 3, .max_length = 2}), "reversed string bounds");
    for (const auto invalid : {"\xC0\xAF", "\xE0\x80\x80", "\xF0\x80\x80\x80", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xF5\x80\x80\x80", "\x80", "\xC2", "\xE2\x82", "\xF0\x90\x80", "\xE2x\xA1"}) {
        passed &= expect(!accepts<std::string>(invalid, {.min_length = 0}), "invalid UTF8 string rejected");
    }
    passed &= expect(!accepts<int>(1, {.description = "\xFF"}), "invalid UTF8 description rejected");

    passed &= expect(accepts<std::vector<int>>({1, 2}, {.min_items = 2, .max_items = 2}), "vector count");
    passed &= expect(!accepts<std::vector<int>>({1}, {.min_items = 2}), "vector minimum");
    passed &= expect(accepts<std::array<int, 2>>({1, 2}, {.min_items = 2, .max_items = 2}), "array count");
    passed &= expect(accepts<std::map<int, int>>({{1, 2}}, {.min_items = 1, .max_items = 1}), "protocol-neutral map count");
    passed &= expect(accepts<std::unordered_map<std::string, int>>({{"one", 1}}, {.max_items = 1}), "unordered map count");
    passed &= expect(!accepts<std::vector<int>>({}, {.min_items = 2, .max_items = 1}), "reversed container bounds");

    passed &= expect(accepts<std::optional<int>>(std::nullopt, {.minimum = 18, .maximum = 120}), "empty optional skips value bounds");
    passed &= expect(!accepts<std::optional<int>>(std::nullopt, {.minimum = 120, .maximum = 18}), "empty optional still validates metadata");
    passed &= expect(!accepts<std::optional<int>>(std::nullopt, {.min_length = 1}), "empty optional rejects incompatible metadata");
    passed &= expect(!accepts<std::optional<double>>(std::nullopt, {.minimum = std::numeric_limits<double>::quiet_NaN()}), "empty optional rejects non-finite metadata");
    passed &= expect(!accepts<std::optional<bool>>(std::nullopt, {.minimum = 0}), "empty optional bool rejects numeric metadata");
    passed &= expect(!reflect::validate_field(std::get<2>(fields), std::optional<int>{12}), "optional value validation");
    passed &= expect(reflect::validate_field(std::get<2>(fields), account.age).has_value(), "optional default validation");

    const auto active_name = reflect::enum_to_string(fixture::Mode::Active);
    const auto disabled_value = reflect::enum_from_string<fixture::Mode>("disabled");
    passed &= expect(active_name && *active_name == "active", "enum external string");
    passed &= expect(disabled_value && *disabled_value == fixture::Mode::Disabled, "enum parse string");
    passed &= expect(!reflect::enum_from_string<fixture::Mode>("Active"), "enum case-sensitive external name");
    passed &= expect(!reflect::enum_from_string<fixture::Mode>("1"), "enum does not infer numeric text");
    passed &= expect(!reflect::validate_enum(static_cast<fixture::Mode>(99)), "unknown registered enum value");
    passed &= expect(!reflect::enum_to_string(static_cast<fixture::Mode>(99)), "unknown enum string encode");
    passed &= expect(reflect::validate_enum(fixture::Code::Maximum).has_value(), "registered uint64 enum");
    passed &= expect(reflect::validate_enum(fixture::Flag::On).has_value(), "registered bool enum");
    passed &= expect(reflect::validate_enum(static_cast<fixture::PlainEnum>(99)).has_value(), "unregistered enum retains underlying semantics");
    passed &= expect(!reflect::enum_to_string(fixture::PlainEnum::Known), "unregistered enum has no string mapping");
    passed &= expect(!reflect::enum_from_string<fixture::PlainEnum>("Known"), "unregistered enum has no string parser");
    passed &= enumDescriptorFails<fixture::Empty>("empty enum descriptor");
    passed &= enumDescriptorFails<fixture::DuplicateValue>("duplicate enum value");
    passed &= enumDescriptorFails<fixture::DuplicateName>("duplicate enum name");
    passed &= enumDescriptorFails<fixture::InvalidName>("invalid enum UTF8 name");
    passed &= enumDescriptorFails<fixture::EmptyName>("empty enum name");
    passed &= enumDescriptorFails<fixture::InvalidEncoding>("invalid enum encoding");
    passed &= expect(!reflect::enum_to_string(fixture::DuplicateValue::One), "enum encoding validates descriptor");
    passed &= expect(!reflect::enum_from_string<fixture::DuplicateName>("same"), "enum decoding validates descriptor");
    passed &= expect(!accepts<fixture::Mode>(fixture::Mode::Active, {.minimum = 0}), "string enum numeric constraints rejected");
    passed &= expect(!accepts<fixture::Flag>(fixture::Flag::On, {.minimum = false}), "boolean enum numeric constraints rejected");
    passed &= expect(!accepts<fixture::Mode>(static_cast<fixture::Mode>(99), {}), "default field options enforce enum value set");
    passed &= expect(accepts<fixture::Code>(fixture::Code::Maximum, {.minimum = std::uint64_t{9007199254740993ULL}, .maximum = UINT64_MAX}), "enum underlying bound precision");
    passed &= expect(!accepts<std::optional<fixture::Empty>>(std::nullopt, {}), "empty optional enum validates descriptor");
    fixture::runtime_descriptor_calls = 0;
    const auto runtime_name = reflect::enum_to_string(fixture::RuntimeMode::Two);
    passed &= expect(runtime_name && *runtime_name == "two" && fixture::runtime_descriptor_calls == 1,
                     "enum string encoding validates one metadata snapshot");
    fixture::runtime_descriptor_calls = 0;
    const auto runtime_value = reflect::enum_from_string<fixture::RuntimeMode>("one");
    passed &= expect(runtime_value && *runtime_value == fixture::RuntimeMode::One &&
                     fixture::runtime_descriptor_calls == 1,
                     "enum string decoding validates one metadata snapshot");
    fixture::runtime_descriptor_calls = 0;
    const auto runtime_checked = reflect::validate_enum(fixture::RuntimeMode::One);
    passed &= expect(runtime_checked && fixture::runtime_descriptor_calls == 1,
                     "enum value validation reads one metadata snapshot");
    fixture::invalid_runtime_descriptor = true;
    passed &= expect(!reflect::enum_to_string(fixture::RuntimeMode::One) &&
                     !reflect::enum_from_string<fixture::RuntimeMode>("one") &&
                     !reflect::validate_enum(fixture::RuntimeMode::One),
                     "runtime descriptor mutation is revalidated rather than cached");
    passed &= expect(reflect::validate_enum_descriptor<fixture::MutableName>().has_value(),
                     "constexpr descriptor may borrow mutable name storage");
    fixture::mutable_enum_name[0] = 'o';
    fixture::mutable_enum_name[1] = 'n';
    fixture::mutable_enum_name[2] = 'e';
    passed &= expect(!reflect::validate_enum_descriptor<fixture::MutableName>() &&
                     !reflect::enum_to_string(fixture::MutableName::Two),
                     "mutable name storage is never assumed to be compile-time valid");
    static_assert(reflect::validate_enum_descriptor<fixture::EvaluationMode>().has_value());
    passed &= expect(reflect::validate_enum_descriptor<fixture::EvaluationMode>().has_value(),
                     "runtime enum snapshot may differ from constant evaluation");
    fixture::evaluation_name[0] = 'o';
    fixture::evaluation_name[1] = 'n';
    fixture::evaluation_name[2] = 'e';
    passed &= expect(!reflect::validate_enum_descriptor<fixture::EvaluationMode>() &&
                     !reflect::validate_enum(fixture::EvaluationMode::One) &&
                     !reflect::enum_to_string(fixture::EvaluationMode::Two) &&
                     !reflect::enum_from_string<fixture::EvaluationMode>("one"),
                     "constant evaluation cannot bypass invalid runtime enum metadata");
    return passed ? 0 : 1;
}
