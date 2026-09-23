#ifndef SERDE_JSON_HPP
#define SERDE_JSON_HPP
// serde JSON 实现头（simdjson 后端）。经典 TU 直接 include；模块消费者
// 经 src/json/json.cppm 门面 import。

// simdjson 宏影响其头内联代码，必须对每个包含 simdjson.h 的 TU 一致；
// 构建目标以相同取值 PUBLIC 传递。此处守卫式自洽定义，若消费方以
// 异常模式预先包含过 simdjson.h 则显式报错。
#ifndef SIMDJSON_EXCEPTIONS
#define SIMDJSON_EXCEPTIONS 0
#endif
#if SIMDJSON_EXCEPTIONS != 0
#error serde json requires SIMDJSON_EXCEPTIONS=0 (provided by the serde::serde_simdjson target)
#endif
#ifndef nssv_CONFIG_NO_EXCEPTIONS
#define nssv_CONFIG_NO_EXCEPTIONS 1
#endif
// Bazel exposes dependencies through separate virtual include directories.
#if __has_include(<third_party/simdjson/simdjson.h>)
#include <third_party/simdjson/simdjson.h>
#else
#include "../../third_party/simdjson/simdjson.h"
#endif

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// 模块门面经 `export import reflect` / `export import common` 提供依赖词汇，
// 头文件形态自包含。JSON 不依赖 toml 模块。
#ifndef SERDE_JSON_MODULE_MODE
#include "../reflect/reflect.hpp"
#include "../common/common.hpp"
#endif

namespace json {

template <class T>
using result = std::expected<T, std::string>;

enum class UnknownFieldPolicy { ignore, reject };

enum class NonFinitePolicy { reject, null_value };

enum class DuplicateKeyPolicy { first_wins, reject };

struct ParseOptions {
    std::size_t max_input_bytes = 64ULL * 1024ULL * 1024ULL;
    std::size_t max_nodes = 1000000;
    std::size_t max_depth = 128;
    std::size_t max_string_bytes = 16ULL * 1024ULL * 1024ULL;
    std::size_t max_key_bytes = 1ULL * 1024ULL * 1024ULL;
    std::size_t max_array_items = 1000000;
    std::size_t max_object_members = 1000000;
    UnknownFieldPolicy unknown_fields = UnknownFieldPolicy::ignore;
    DuplicateKeyPolicy duplicate_keys = DuplicateKeyPolicy::reject;
    bool enforce_document_limits = true;
};

struct SerializeOptions {
    std::size_t max_output_bytes = 64ULL * 1024ULL * 1024ULL;
    std::size_t max_nodes = 1000000;
    std::size_t max_depth = 128;
    std::size_t max_string_bytes = 16ULL * 1024ULL * 1024ULL;
    std::size_t max_key_bytes = 1ULL * 1024ULL * 1024ULL;
    std::size_t max_array_items = 1000000;
    std::size_t max_object_members = 1000000;
    bool pretty = false;
    unsigned indent_width = 2;
    NonFinitePolicy non_finite = NonFinitePolicy::reject;
};

enum class ValueType { invalid, null_value, boolean, signed_integer,
                              unsigned_integer, number, string, array, object };

class Parser;

/// simdjson DOM 的对外包装：持有解析文档并提供只读访问接口。
class Json {
public:
    Json() = default;

    static result<Json> parse(std::string_view text, const ParseOptions& options = {});

    bool valid() const noexcept;
    ValueType type() const noexcept {
        if (!valid()) return ValueType::invalid;
        switch (element_.type()) {
            case simdjson::dom::element_type::NULL_VALUE: return ValueType::null_value;
            case simdjson::dom::element_type::BOOL: return ValueType::boolean;
            case simdjson::dom::element_type::INT64: return ValueType::signed_integer;
            case simdjson::dom::element_type::UINT64: return ValueType::unsigned_integer;
            case simdjson::dom::element_type::DOUBLE: return ValueType::number;
            case simdjson::dom::element_type::STRING: return ValueType::string;
            case simdjson::dom::element_type::ARRAY: return ValueType::array;
            case simdjson::dom::element_type::OBJECT: return ValueType::object;
            default: return ValueType::invalid;
        }
    }
    bool is_null() const noexcept { return valid() && element_.is_null(); }
    bool is_bool() const noexcept { return valid() && element_.is_bool(); }
    bool is_number() const noexcept {
        return valid() && (element_.is_int64() || element_.is_uint64() || element_.is_double());
    }
    bool is_string() const noexcept { return valid() && element_.is_string(); }
    bool is_array() const noexcept { return valid() && element_.is_array(); }
    bool is_object() const noexcept { return valid() && element_.is_object(); }
    bool contains(std::string_view key) const noexcept { return at(key).valid(); }
    std::size_t size() const noexcept {
        if (!valid()) return 0;
        switch (element_.type()) {
            case simdjson::dom::element_type::STRING:
                return element_.get_string().value_unsafe().size();
            case simdjson::dom::element_type::ARRAY:
                return element_.get_array().value_unsafe().size();
            case simdjson::dom::element_type::OBJECT:
                return element_.get_object().value_unsafe().size();
            default:
                return 0;
        }
    }
    Json at(std::string_view key) const noexcept {
        if (!is_object()) return {};
        auto value = element_[key];
        return value.error() ? Json{} : Json{state_, generation_, value.value_unsafe()};
    }
    Json at(std::size_t index) const noexcept {
        if (!is_array()) return {};
        auto value = element_.at(index);
        return value.error() ? Json{} : Json{state_, generation_, value.value_unsafe()};
    }
    Json operator[](std::string_view key) const noexcept { return at(key); }
    Json operator[](std::size_t index) const noexcept { return at(index); }
    result<std::string_view> as_string() const {
        if (!valid()) return std::unexpected(std::string("invalid JSON value"));
        auto value = element_.get_string();
        if (value.error()) return std::unexpected(std::string("JSON value is not a string"));
        return value.value_unsafe();
    }
    result<bool> as_bool() const {
        if (!valid()) return std::unexpected(std::string("invalid JSON value"));
        auto value = element_.get_bool();
        if (value.error()) return std::unexpected(std::string("JSON value is not a boolean"));
        return value.value_unsafe();
    }
    result<std::int64_t> as_int64() const {
        if (!valid()) return std::unexpected(std::string("invalid JSON value"));
        auto value = element_.get_int64();
        if (value.error()) return std::unexpected(std::string("JSON value is not an int64"));
        return value.value_unsafe();
    }
    result<std::uint64_t> as_uint64() const {
        if (!valid()) return std::unexpected(std::string("invalid JSON value"));
        auto value = element_.get_uint64();
        if (value.error()) return std::unexpected(std::string("JSON value is not a uint64"));
        return value.value_unsafe();
    }
    result<double> as_double() const {
        if (!valid()) return std::unexpected(std::string("invalid JSON value"));
        auto value = element_.get_double();
        if (value.error()) return std::unexpected(std::string("JSON value is not a number"));
        return value.value_unsafe();
    }

    template <class Function>
    result<void> for_each_element(Function&& function) const {
        if (!is_array()) {
            return std::unexpected(std::string("JSON value is not an array"));
        }
        auto values = element_.get_array();
        if (values.error()) {
            return std::unexpected(std::string("JSON value is not an array"));
        }
        for (const auto child : values.value_unsafe()) {
            if (auto status = function(Json{state_, generation_, child}); !status) {
                return status;
            }
        }
        return {};
    }

    template <class Function>
    result<void> for_each_member(Function&& function) const {
        if (!is_object()) {
            return std::unexpected(std::string("JSON value is not an object"));
        }
        auto values = element_.get_object();
        if (values.error()) {
            return std::unexpected(std::string("JSON value is not an object"));
        }
        for (const auto field : values.value_unsafe()) {
            if (auto status = function(std::string_view(field.key),
                                       Json{state_, generation_, field.value});
                !status) {
                return status;
            }
        }
        return {};
    }

private:
    struct State;
    std::shared_ptr<State> owner_;
    State* state_ = nullptr;
    std::uint64_t generation_ = 0;
    simdjson::dom::element element_{};
    Json(std::shared_ptr<State> owner, simdjson::dom::element element);
    Json(State* state, std::uint64_t generation, simdjson::dom::element element)
        : state_(state), generation_(generation), element_(element) {}
    static result<Json> parse_with_state(const std::shared_ptr<State>& state,
                                         std::string_view text,
                                         const ParseOptions& options);
    friend class Parser;
};

/// 复用同一份 simdjson parser 容量；下一次 parse 或 reset 会使此前返回的 Json 失效。
class Parser {
public:
    Parser() = default;
    Parser(Parser&&) noexcept = default;
    Parser& operator=(Parser&&) noexcept = default;
    Parser(const Parser&) = delete;
    Parser& operator=(const Parser&) = delete;
    result<Json> parse(std::string_view text, const ParseOptions& options = {});
    void reset();

private:
    std::shared_ptr<Json::State> state_;
};

inline result<Json> parse(std::string_view text, const ParseOptions& options = {}) {
    return Json::parse(text, options);
}

void reset_thread_parser();
// JSON 没有原生时间值。公共的日期时间词汇（common::date 等）作为
// RFC 3339 JSON 字符串使用，与 toml 模块同源、互不依赖。
using date = common::date;
using time = common::time;
using local_date_time = common::local_date_time;
using offset_date_time = common::offset_date_time;
template <class T>
using InlineTable = common::InlineTable<T>;

template <class Owner, class Member>
using field = reflect::field<Owner, Member>;
using reflect::make_field;

template <class T>
concept Reflectable = reflect::Reflectable<T>;

template <class T, class Function>
    requires Reflectable<T>
constexpr void for_each_field(T& value, Function&& function) {
    reflect::for_each_field(value, std::forward<Function>(function));
}

}  // namespace json

namespace json::detail {

template <class T>
using BareT = std::remove_cvref_t<T>;

template <class T>
struct OptionalTraits {
    static constexpr bool value = false;
};

template <class T>
struct OptionalTraits<std::optional<T>> {
    static constexpr bool value = true;
    using value_type = T;
};

template <class T>
struct VectorTraits {
    static constexpr bool value = false;
};

template <class T, class Allocator>
struct VectorTraits<std::vector<T, Allocator>> {
    static constexpr bool value = true;
    using value_type = T;
};

template <class T>
struct ArrayTraits {
    static constexpr bool value = false;
};

template <class T, std::size_t Size>
struct ArrayTraits<std::array<T, Size>> {
    static constexpr bool value = true;
    using value_type = T;
    static constexpr std::size_t size = Size;
};

template <class T>
struct MapTraits {
    static constexpr bool value = false;
};

template <class Value, class Compare, class Allocator>
struct MapTraits<std::map<std::string, Value, Compare, Allocator>> {
    static constexpr bool value = true;
    using mapped_type = Value;
};

template <class Value, class Hash, class Equal, class Allocator>
struct MapTraits<std::unordered_map<std::string, Value, Hash, Equal, Allocator>> {
    static constexpr bool value = true;
    using mapped_type = Value;
};

template <class T>
struct InlineTableTraits {
    static constexpr bool value = false;
};

template <class T>
struct InlineTableTraits<InlineTable<T>> {
    static constexpr bool value = true;
    using value_type = T;
};

struct Node {
    using array = std::vector<Node>;
    using object = std::map<std::string, Node>;
    using storage = std::variant<std::monostate, bool, std::int64_t, std::uint64_t,
                                 double, std::string, array, object>;

    storage value{};

    Node() = default;
    Node(const Node&) = default;
    Node(Node&&) noexcept = default;
    Node& operator=(const Node&) = default;
    Node& operator=(Node&&) noexcept = default;

    template <class T>
        requires(!std::same_as<BareT<T>, Node>)
    Node(T&& input) : value(std::forward<T>(input)) {}
};

using node = Node;

inline bool ascii_digit(char value) {
    return value >= '0' && value <= '9';
}

inline bool valid_utf8(std::string_view text) {
    return simdjson::validate_utf8(text);
}

inline bool valid_date_value(const date& value) {
    if (value.year < 0 || value.year > 9999 || value.month == 0 || value.month > 12 ||
        value.day == 0) {
        return false;
    }
    constexpr std::array<unsigned, 12> days_per_month{
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const auto year = static_cast<unsigned>(value.year);
    const bool leap_year = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return value.day <= days_per_month[value.month - 1] +
                            (value.month == 2 && leap_year ? 1 : 0);
}

inline bool valid_time_value(const time& value) {
    if (value.hour > 23 || value.minute > 59 || value.second > 59) {
        return false;
    }
    for (const char character : value.fractional_second) {
        if (!ascii_digit(character)) {
            return false;
        }
    }
    return true;
}

inline bool parse_fixed_decimal(std::string_view text, std::size_t offset,
                                std::size_t width, unsigned& output) {
    if (offset + width > text.size()) {
        return false;
    }
    unsigned value = 0;
    for (std::size_t index = 0; index < width; ++index) {
        if (!ascii_digit(text[offset + index])) {
            return false;
        }
        value = value * 10 + static_cast<unsigned>(text[offset + index] - '0');
    }
    output = value;
    return true;
}

inline result<date> parse_date_text(std::string_view text) {
    unsigned year = 0;
    unsigned month = 0;
    unsigned day = 0;
    if (text.size() != 10 || text[4] != '-' || text[7] != '-' ||
        !parse_fixed_decimal(text, 0, 4, year) ||
        !parse_fixed_decimal(text, 5, 2, month) ||
        !parse_fixed_decimal(text, 8, 2, day)) {
        return std::unexpected(std::string("invalid JSON date"));
    }
    date value{static_cast<int>(year), month, day};
    if (!valid_date_value(value)) {
        return std::unexpected(std::string("JSON date is outside the calendar range"));
    }
    return value;
}

inline result<std::pair<time, std::size_t>> parse_time_prefix(std::string_view text) {
    unsigned hour = 0;
    unsigned minute = 0;
    unsigned second = 0;
    if (text.size() < 8 || text[2] != ':' || text[5] != ':' ||
        !parse_fixed_decimal(text, 0, 2, hour) ||
        !parse_fixed_decimal(text, 3, 2, minute) ||
        !parse_fixed_decimal(text, 6, 2, second) || hour > 23 || minute > 59 ||
        second > 59) {
        return std::unexpected(std::string("invalid JSON time"));
    }
    std::size_t position = 8;
    std::string fraction;
    if (position < text.size() && text[position] == '.') {
        const auto start = ++position;
        while (position < text.size() && ascii_digit(text[position])) {
            ++position;
        }
        if (position == start) {
            return std::unexpected(std::string("JSON time has an empty fractional second"));
        }
        fraction = std::string(text.substr(start, position - start));
    }
    return std::pair{time{hour, minute, second, std::move(fraction)}, position};
}

inline result<local_date_time> parse_local_date_time_text(std::string_view text) {
    if (text.size() < 19 || (text[10] != 'T' && text[10] != 't' && text[10] != ' ')) {
        return std::unexpected(std::string("invalid JSON local date-time"));
    }
    auto parsed_date = parse_date_text(text.substr(0, 10));
    if (!parsed_date) {
        return std::unexpected(parsed_date.error());
    }
    auto parsed_time = parse_time_prefix(text.substr(11));
    if (!parsed_time || 11 + parsed_time->second != text.size()) {
        return std::unexpected(parsed_time ? std::string("invalid JSON local date-time")
                                           : parsed_time.error());
    }
    return local_date_time{std::move(*parsed_date), std::move(parsed_time->first)};
}

inline result<offset_date_time> parse_offset_date_time_text(std::string_view text) {
    if (text.size() < 20 || (text[10] != 'T' && text[10] != 't' && text[10] != ' ')) {
        return std::unexpected(std::string("invalid JSON offset date-time"));
    }
    auto parsed_date = parse_date_text(text.substr(0, 10));
    if (!parsed_date) {
        return std::unexpected(parsed_date.error());
    }
    auto parsed_time = parse_time_prefix(text.substr(11));
    if (!parsed_time) {
        return std::unexpected(parsed_time.error());
    }
    const auto offset = 11 + parsed_time->second;
    if (offset >= text.size()) {
        return std::unexpected(std::string("invalid JSON offset date-time"));
    }
    int offset_minutes = 0;
    const auto suffix = text.substr(offset);
    if (suffix == "Z" || suffix == "z") {
        offset_minutes = 0;
    } else {
        unsigned hour = 0;
        unsigned minute = 0;
        if (suffix.size() != 6 || (suffix[0] != '+' && suffix[0] != '-') ||
            suffix[3] != ':' || !parse_fixed_decimal(suffix, 1, 2, hour) ||
            !parse_fixed_decimal(suffix, 4, 2, minute) || hour > 23 || minute > 59) {
            return std::unexpected(std::string("invalid JSON offset date-time"));
        }
        offset_minutes = static_cast<int>(hour * 60 + minute);
        if (suffix[0] == '-') {
            offset_minutes = -offset_minutes;
        }
    }
    return offset_date_time{
        local_date_time{std::move(*parsed_date), std::move(parsed_time->first)}, offset_minutes};
}

inline void append_padded_decimal(std::string& output, unsigned value, unsigned width) {
    std::array<char, 16> digits{};
    for (unsigned index = 0; index < width; ++index) {
        digits[width - index - 1] = static_cast<char>('0' + value % 10);
        value /= 10;
    }
    output.append(digits.data(), width);
}

inline std::string temporal_text(const date& value) {
    std::string output;
    append_padded_decimal(output, static_cast<unsigned>(value.year), 4);
    output.push_back('-');
    append_padded_decimal(output, value.month, 2);
    output.push_back('-');
    append_padded_decimal(output, value.day, 2);
    return output;
}

inline std::string temporal_text(const time& value) {
    std::string output;
    append_padded_decimal(output, value.hour, 2);
    output.push_back(':');
    append_padded_decimal(output, value.minute, 2);
    output.push_back(':');
    append_padded_decimal(output, value.second, 2);
    if (!value.fractional_second.empty()) {
        output.push_back('.');
        output += value.fractional_second;
    }
    return output;
}

inline std::string temporal_text(const local_date_time& value) {
    std::string output = temporal_text(value.date_part);
    output.push_back('T');
    output += temporal_text(value.time_part);
    return output;
}

inline std::string temporal_text(const offset_date_time& value) {
    std::string output = temporal_text(value.local);
    if (value.offset_minutes == 0) {
        output.push_back('Z');
        return output;
    }
    const int magnitude = std::abs(value.offset_minutes);
    output.push_back(value.offset_minutes < 0 ? '-' : '+');
    append_padded_decimal(output, static_cast<unsigned>(magnitude / 60), 2);
    output.push_back(':');
    append_padded_decimal(output, static_cast<unsigned>(magnitude % 60), 2);
    return output;
}

struct EncodeContext {
    const SerializeOptions& options;
    std::size_t nodes = 0;
};

template <class T>
result<Node> encodeValue(const T& input, EncodeContext& context, std::size_t depth);

template <class T>
result<Node> encodeValue(const T& input, EncodeContext& context, std::size_t depth) {
    using U = BareT<T>;
    if (depth >= context.options.max_depth) {
        return std::unexpected(std::string("JSON nesting depth exceeds configured limit"));
    }
    if (context.nodes >= context.options.max_nodes) {
        return std::unexpected(std::string("JSON node count exceeds configured limit"));
    }
    ++context.nodes;

    if constexpr (OptionalTraits<U>::value) {
        if (!input) {
            return Node{};
        }
        return encodeValue(*input, context, depth + 1);
    } else if constexpr (InlineTableTraits<U>::value) {
        return encodeValue(input.value, context, depth + 1);
    } else if constexpr (std::same_as<U, date>) {
        if (!valid_date_value(input)) {
            return std::unexpected(std::string("invalid json::date value"));
        }
        return Node{temporal_text(input)};
    } else if constexpr (std::same_as<U, time>) {
        if (!valid_time_value(input)) {
            return std::unexpected(std::string("invalid json::time value"));
        }
        return Node{temporal_text(input)};
    } else if constexpr (std::same_as<U, local_date_time>) {
        if (!valid_date_value(input.date_part) || !valid_time_value(input.time_part)) {
            return std::unexpected(std::string("invalid json::local_date_time value"));
        }
        return Node{temporal_text(input)};
    } else if constexpr (std::same_as<U, offset_date_time>) {
        if (!valid_date_value(input.local.date_part) || !valid_time_value(input.local.time_part) ||
            input.offset_minutes < -1439 || input.offset_minutes > 1439) {
            return std::unexpected(std::string("invalid json::offset_date_time value"));
        }
        return Node{temporal_text(input)};
    } else if constexpr (std::same_as<U, std::string>) {
        if (input.size() > context.options.max_string_bytes || !valid_utf8(input)) {
            return std::unexpected(std::string("invalid or oversized UTF-8 JSON string"));
        }
        return Node{input};
    } else if constexpr (std::same_as<U, std::string_view>) {
        if (input.size() > context.options.max_string_bytes || !valid_utf8(input)) {
            return std::unexpected(std::string("invalid or oversized UTF-8 JSON string"));
        }
        return Node{std::string(input)};
    } else if constexpr (std::same_as<U, std::nullptr_t>) {
        return Node{};
    } else if constexpr (std::same_as<U, bool>) {
        return Node{input};
    } else if constexpr (std::is_integral_v<U>) {
        if constexpr (std::is_signed_v<U>) {
            if constexpr (std::numeric_limits<U>::digits >
                          std::numeric_limits<std::int64_t>::digits) {
                if (input < static_cast<U>(std::numeric_limits<std::int64_t>::min()) ||
                    input > static_cast<U>(std::numeric_limits<std::int64_t>::max())) {
                    return std::unexpected(std::string("signed integer is outside JSON int64 range"));
                }
            }
            return Node{static_cast<std::int64_t>(input)};
        } else {
            if constexpr (std::numeric_limits<U>::digits >
                          std::numeric_limits<std::uint64_t>::digits) {
                if (input > static_cast<U>(std::numeric_limits<std::uint64_t>::max())) {
                    return std::unexpected(std::string("unsigned integer is outside JSON uint64 range"));
                }
            }
            return Node{static_cast<std::uint64_t>(input)};
        }
    } else if constexpr (std::is_floating_point_v<U>) {
        const double value = static_cast<double>(input);
        if (!std::isfinite(value)) {
            if (context.options.non_finite == NonFinitePolicy::null_value) {
                return Node{};
            }
            return std::unexpected(std::string("JSON does not permit non-finite numbers"));
        }
        return Node{value};
    } else if constexpr (std::is_enum_v<U>) {
        return encodeValue(static_cast<std::underlying_type_t<U>>(input), context, depth + 1);
    } else if constexpr (VectorTraits<U>::value) {
        if (input.size() > context.options.max_array_items) {
            return std::unexpected(std::string("JSON array item count exceeds configured limit"));
        }
        Node::array values;
        values.reserve(input.size());
        for (const auto& element : input) {
            using Element = typename VectorTraits<U>::value_type;
            auto encoded = [&]() {
                if constexpr (std::constructible_from<Element, decltype(element)>) {
                    return encodeValue(static_cast<Element>(element), context, depth + 1);
                } else {
                    return encodeValue(element, context, depth + 1);
                }
            }();
            if (!encoded) {
                return std::unexpected(encoded.error());
            }
            values.push_back(std::move(*encoded));
        }
        return Node{std::move(values)};
    } else if constexpr (ArrayTraits<U>::value) {
        if (ArrayTraits<U>::size > context.options.max_array_items) {
            return std::unexpected(std::string("JSON array item count exceeds configured limit"));
        }
        Node::array values;
        values.reserve(ArrayTraits<U>::size);
        for (const auto& element : input) {
            auto encoded = encodeValue(element, context, depth + 1);
            if (!encoded) {
                return std::unexpected(encoded.error());
            }
            values.push_back(std::move(*encoded));
        }
        return Node{std::move(values)};
    } else if constexpr (MapTraits<U>::value) {
        if (input.size() > context.options.max_object_members) {
            return std::unexpected(std::string("JSON object member count exceeds configured limit"));
        }
        Node::object values;
        for (const auto& [key, element] : input) {
            if (key.size() > context.options.max_key_bytes || !valid_utf8(key)) {
                return std::unexpected(std::string("invalid or oversized UTF-8 JSON object key"));
            }
            auto encoded = encodeValue(element, context, depth + 1);
            if (!encoded) {
                return std::unexpected(encoded.error());
            }
            values.emplace(key, std::move(*encoded));
        }
        return Node{std::move(values)};
    } else if constexpr (Reflectable<U>) {
        Node::object values;
        std::string failure;
        bool failed = false;
        for_each_field(input, [&](const auto& descriptor, const auto& object) {
            if (failed) {
                return;
            }
            if (descriptor.name.size() > context.options.max_key_bytes ||
                !valid_utf8(descriptor.name)) {
                failed = true;
                failure = "invalid or oversized UTF-8 reflected JSON key";
                return;
            }
            auto encoded = encodeValue(descriptor.get(object), context, depth + 1);
            if (!encoded) {
                failed = true;
                failure = encoded.error();
                return;
            }
            if (!values.emplace(std::string(descriptor.name), std::move(*encoded)).second) {
                failed = true;
                failure = "duplicate reflected field: " + std::string(descriptor.name);
            }
            if (values.size() > context.options.max_object_members) {
                failed = true;
                failure = "JSON object member count exceeds configured limit";
            }
        });
        if (failed) {
            return std::unexpected(std::move(failure));
        }
        return Node{std::move(values)};
    } else {
        return std::unexpected(std::string("unsupported type in JSON serializer"));
    }
}

struct Writer {
    std::string output;
    std::size_t limit;

    bool append(std::string_view text) {
        if (text.size() > limit || output.size() > limit - text.size()) {
            return false;
        }
        output.append(text);
        return true;
    }

    bool push(char character) {
        if (output.size() >= limit) {
            return false;
        }
        output.push_back(character);
        return true;
    }
};

inline bool appendIndent(Writer& output, std::size_t depth, const SerializeOptions& options) {
    if (!options.pretty) {
        return true;
    }
    if (options.indent_width != 0 && depth >
        (std::numeric_limits<std::size_t>::max() / options.indent_width)) {
        return false;
    }
    const auto spaces = depth * options.indent_width;
    for (std::size_t index = 0; index < spaces; ++index) {
        if (!output.push(' ')) {
            return false;
        }
    }
    return true;
}

inline bool appendString(std::string_view value, Writer& output) {
    if (!output.push('"')) {
        return false;
    }
    constexpr char hex[] = "0123456789abcdef";
    for (const char character : value) {
        switch (character) {
            case '"': if (!output.append("\\\"")) return false; break;
            case '\\': if (!output.append("\\\\")) return false; break;
            case '\b': if (!output.append("\\b")) return false; break;
            case '\f': if (!output.append("\\f")) return false; break;
            case '\n': if (!output.append("\\n")) return false; break;
            case '\r': if (!output.append("\\r")) return false; break;
            case '\t': if (!output.append("\\t")) return false; break;
            default:
                if (static_cast<unsigned char>(character) < 0x20) {
                    char escaped[6] = {'\\', 'u', '0', '0', '0', '0'};
                    const auto byte = static_cast<unsigned char>(character);
                    escaped[4] = hex[byte >> 4];
                    escaped[5] = hex[byte & 0x0f];
                    if (!output.append(std::string_view(escaped, 6))) return false;
                } else if (!output.push(character)) {
                    return false;
                }
                break;
        }
    }
    return output.push('"');
}

inline bool appendValue(const Node& value, Writer& output,
                          const SerializeOptions& options, std::size_t depth,
                          std::string& failure) {
    return std::visit(
        [&](const auto& item) -> bool {
            using Item = BareT<decltype(item)>;
            if constexpr (std::same_as<Item, std::monostate>) {
                return output.append("null");
            } else if constexpr (std::same_as<Item, bool>) {
                return output.append(item ? "true" : "false");
            } else if constexpr (std::same_as<Item, std::int64_t> ||
                                 std::same_as<Item, std::uint64_t>) {
                return output.append(std::to_string(item));
            } else if constexpr (std::same_as<Item, double>) {
                if (!std::isfinite(item)) {
                    if (options.non_finite == NonFinitePolicy::null_value) {
                        return output.append("null");
                    }
                    failure = "JSON does not permit non-finite numbers";
                    return false;
                }
                char buffer[64]{};
                const auto [end, error] = std::to_chars(
                    buffer, buffer + sizeof(buffer), item,
                    std::chars_format::general, 17);
                if (error != std::errc{}) {
                    failure = "failed to format JSON floating-point value";
                    return false;
                }
                return output.append(std::string_view(buffer, static_cast<std::size_t>(end - buffer)));
            } else if constexpr (std::same_as<Item, std::string>) {
                return appendString(item, output);
            } else if constexpr (std::same_as<Item, Node::array>) {
                if (!output.push('[')) return false;
                if (!item.empty() && options.pretty && !output.push('\n')) return false;
                for (std::size_t index = 0; index < item.size(); ++index) {
                    if (options.pretty && !appendIndent(output, depth + 1, options)) return false;
                    if (!appendValue(item[index], output, options, depth + 1, failure)) return false;
                    if (index + 1 != item.size()) {
                        if (!output.push(',')) return false;
                        if (options.pretty && !output.push('\n')) return false;
                    }
                }
                if (!item.empty() && options.pretty) {
                    if (!output.push('\n') || !appendIndent(output, depth, options)) return false;
                }
                return output.push(']');
            } else if constexpr (std::same_as<Item, Node::object>) {
                if (!output.push('{')) return false;
                if (!item.empty() && options.pretty && !output.push('\n')) return false;
                std::size_t index = 0;
                for (const auto& [key, child] : item) {
                    if (options.pretty && !appendIndent(output, depth + 1, options)) return false;
                    if (!appendString(key, output)) return false;
                    if (!output.push(':')) return false;
                    if (options.pretty && !output.push(' ')) return false;
                    if (!appendValue(child, output, options, depth + 1, failure)) return false;
                    if (++index != item.size()) {
                        if (!output.push(',')) return false;
                        if (options.pretty && !output.push('\n')) return false;
                    }
                }
                if (!item.empty() && options.pretty) {
                    if (!output.push('\n') || !appendIndent(output, depth, options)) return false;
                }
                return output.push('}');
            }
        },
        value.value);
}

inline bool needs_document_walk(const ParseOptions& options) noexcept;

inline result<void> enforce_limits(const Json& value, const ParseOptions& options,
                                   std::size_t depth, std::size_t& nodes);
inline result<void> enforce_contents(const Json& value, const ParseOptions& options,
                                     std::size_t depth, std::size_t& nodes);

// 小对象在遍历中立即拒绝重复键；大对象先走完子节点，再按键排序判定。
// 与原先的独立限制遍历保持同一顺序。小对象不分配、也不清零整块缓冲。
struct DuplicateKeyScan {
    static constexpr std::size_t inline_capacity = 16;
    const ParseOptions* options = nullptr;
    std::size_t member_count = 0;
    bool reject = false;
    std::size_t seen_count = 0;
    std::array<std::string_view, inline_capacity> seen_inline;
    std::vector<std::string_view> seen_heap;

    explicit DuplicateKeyScan(const ParseOptions& parse_options, std::size_t count)
        : options(&parse_options), member_count(count),
          reject(parse_options.duplicate_keys == DuplicateKeyPolicy::reject) {
        if (reject && count > inline_capacity) seen_heap.reserve(count);
    }

    result<void> observe(std::string_view key, bool& repeated) {
        repeated = false;
        if (options->enforce_document_limits && key.size() > options->max_key_bytes) {
            return std::unexpected(std::string("JSON object key exceeds configured size limit"));
        }
        if (member_count <= inline_capacity) {
            for (std::size_t index = 0; index < seen_count; ++index) {
                if (seen_inline[index] == key) {
                    repeated = true;
                    if (reject) {
                        return std::unexpected(std::string("JSON parse error: duplicate object key"));
                    }
                    return {};
                }
            }
            seen_inline[seen_count++] = key;
            return {};
        }
        if (reject) seen_heap.push_back(key);
        return {};
    }

    result<void> finish() {
        if (!reject || seen_heap.empty()) return {};
        std::sort(seen_heap.begin(), seen_heap.end());
        if (std::adjacent_find(seen_heap.begin(), seen_heap.end()) != seen_heap.end()) {
            return std::unexpected(std::string("JSON parse error: duplicate object key"));
        }
        return {};
    }
};

template <class T>
result<T> decodeValue(const Json& input, std::string_view path, const ParseOptions& options);

inline std::string rebaseDecodeError(std::string error, std::string_view path);
inline std::string indexedDecodePath(std::string_view path, std::size_t index);
inline std::string memberDecodePath(std::string_view path, std::string_view member);

inline std::string valuePath(std::string_view path) {
    return path.empty() ? std::string("value") : std::string(path);
}

// 成功路径不构造 "value"。只有真正格式化错误时才物化路径。
struct LazyWhere {
    std::string_view path;
    std::string_view view() const {
        return path.empty() ? std::string_view("value") : path;
    }
};

inline std::string operator+(const LazyWhere& where, std::string_view message) {
    const auto prefix = where.view();
    std::string out;
    out.reserve(prefix.size() + message.size());
    out.append(prefix);
    out.append(message);
    return out;
}

// 递归解码仅在报告错误时需要完整路径。将成功的子调用保持在根路径
// 避免了常见情况下的临时字符串，同时保留了失败时的现有诊断信息。
inline std::string rebaseDecodeError(std::string error, std::string_view path) {
    constexpr std::string_view root = "value";
    if (!error.starts_with(root)) return error;
    std::string rebased;
    rebased.reserve(path.size() + error.size() - root.size());
    rebased.append(path);
    rebased.append(error.substr(root.size()));
    return rebased;
}

inline std::string indexedDecodePath(std::string_view path, std::size_t index) {
    std::array<char, 32> digits{};
    const auto converted = std::to_chars(digits.data(), digits.data() + digits.size(), index);
    std::string result;
    result.reserve(path.size() + 2 + static_cast<std::size_t>(converted.ptr - digits.data()));
    result.append(path);
    result.push_back('[');
    result.append(digits.data(), converted.ptr);
    result.push_back(']');
    return result;
}

inline std::string memberDecodePath(std::string_view path, std::string_view member) {
    std::string result;
    result.reserve(path.size() + 1 + member.size());
    result.append(path);
    result.push_back('.');
    result.append(member);
    return result;
}

template <class Fields>
constexpr auto indexedFieldNames(const Fields& descriptors) {
    constexpr auto field_count = std::tuple_size_v<Fields>;
    std::array<std::pair<std::string_view, std::size_t>, field_count> names{};
    std::size_t index = 0;
    std::apply([&](const auto&... descriptor) {
        ((names[index] = {descriptor.name, index}, ++index), ...);
    }, descriptors);
    std::sort(names.begin(), names.end(), [](const auto& left, const auto& right) {
        return left.first < right.first;
    });
    return names;
}

template <class T>
    requires reflect::StaticReflectable<T>
inline constexpr auto staticFieldNames = indexedFieldNames(reflect::static_fields<T>());

constexpr std::size_t fieldNameHash(std::string_view name) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char character : name) {
        hash = (hash ^ character) * 1099511628211ULL;
    }
    return static_cast<std::size_t>(hash);
}

template <std::size_t Size>
struct StaticFieldLookup {
    static constexpr auto capacity = std::bit_ceil(Size * 2 + 1);
    std::array<std::size_t, capacity> buckets;

    constexpr explicit StaticFieldLookup(
        const std::array<std::pair<std::string_view, std::size_t>, Size>& names) {
        buckets.fill(Size);
        for (std::size_t index = 0; index < Size; ++index) {
            if (index != 0 && names[index - 1].first == names[index].first) continue;
            auto bucket = fieldNameHash(names[index].first) & (capacity - 1);
            while (buckets[bucket] != Size) bucket = (bucket + 1) & (capacity - 1);
            buckets[bucket] = index;
        }
    }

    constexpr std::size_t find(std::string_view key,
        const std::array<std::pair<std::string_view, std::size_t>, Size>& names) const {
        auto bucket = fieldNameHash(key) & (capacity - 1);
        for (std::size_t probes = 0; probes < capacity; ++probes) {
            const auto index = buckets[bucket];
            if (index == Size || names[index].first == key) return index;
            bucket = (bucket + 1) & (capacity - 1);
        }
        return Size;
    }
};

template <class T>
    requires reflect::StaticReflectable<T>
inline constexpr auto staticFieldLookup = StaticFieldLookup{staticFieldNames<T>};

template <class T>
result<T> decodeValue(const Json& input, std::string_view path, const ParseOptions& options) {
    using U = BareT<T>;
    const LazyWhere where{path};

    if constexpr (OptionalTraits<U>::value) {
        if (input.is_null()) {
            return U{};
        }
        auto decoded = decodeValue<typename OptionalTraits<U>::value_type>(input, path, options);
        if (!decoded) return std::unexpected(decoded.error());
        return U{std::move(*decoded)};
    } else if constexpr (InlineTableTraits<U>::value) {
        auto decoded = decodeValue<typename InlineTableTraits<U>::value_type>(input, path, options);
        if (!decoded) return std::unexpected(decoded.error());
        return U{std::move(*decoded)};
    } else if constexpr (std::is_enum_v<U>) {
        using Underlying = std::underlying_type_t<U>;
        auto decoded = decodeValue<Underlying>(input, path, options);
        if (!decoded) return std::unexpected(decoded.error());
        return static_cast<U>(*decoded);
    } else if constexpr (std::same_as<U, std::nullptr_t>) {
        if (input.is_null()) return nullptr;
        return std::unexpected(where + " must be JSON null");
    } else if constexpr (std::same_as<U, date>) {
        auto value = input.as_string();
        if (!value) return std::unexpected(where + " must be a JSON date string");
        auto parsed = parse_date_text(*value);
        if (!parsed) return std::unexpected(where + ": " + parsed.error());
        return *parsed;
    } else if constexpr (std::same_as<U, time>) {
        auto value = input.as_string();
        if (!value) return std::unexpected(where + " must be a JSON time string");
        auto parsed = parse_time_prefix(*value);
        if (!parsed || parsed->second != value->size()) {
            return std::unexpected(where + ": invalid JSON time");
        }
        return parsed->first;
    } else if constexpr (std::same_as<U, local_date_time>) {
        auto value = input.as_string();
        if (!value) return std::unexpected(where + " must be a JSON local date-time string");
        auto parsed = parse_local_date_time_text(*value);
        if (!parsed) return std::unexpected(where + ": " + parsed.error());
        return *parsed;
    } else if constexpr (std::same_as<U, offset_date_time>) {
        auto value = input.as_string();
        if (!value) return std::unexpected(where + " must be a JSON offset date-time string");
        auto parsed = parse_offset_date_time_text(*value);
        if (!parsed) return std::unexpected(where + ": " + parsed.error());
        return *parsed;
    } else if constexpr (std::same_as<U, std::string>) {
        auto value = input.as_string();
        if (!value) return std::unexpected(where + " must be a JSON string");
        return std::string(*value);
    } else if constexpr (std::same_as<U, bool>) {
        auto value = input.as_bool();
        if (!value) return std::unexpected(where + " must be a JSON boolean");
        return *value;
    } else if constexpr (std::is_integral_v<U>) {
        std::int64_t signed_value = 0;
        std::uint64_t unsigned_value = 0;
        bool is_unsigned_value = false;
        if (auto value = input.as_int64(); value) {
            signed_value = *value;
        } else if (auto value = input.as_uint64(); value) {
            unsigned_value = *value;
            is_unsigned_value = true;
        } else {
            return std::unexpected(where + " must be a JSON integer");
        }
        if constexpr (std::is_signed_v<U>) {
            if (is_unsigned_value) {
                if (unsigned_value > static_cast<std::uint64_t>(std::numeric_limits<U>::max())) {
                    return std::unexpected(where + " is outside the destination integer range");
                }
                return static_cast<U>(unsigned_value);
            }
            if constexpr (std::numeric_limits<U>::digits < std::numeric_limits<std::int64_t>::digits) {
                if (signed_value < static_cast<std::int64_t>(std::numeric_limits<U>::min()) ||
                    signed_value > static_cast<std::int64_t>(std::numeric_limits<U>::max())) {
                    return std::unexpected(where + " is outside the destination integer range");
                }
            }
            return static_cast<U>(signed_value);
        } else {
            if (is_unsigned_value) {
                if constexpr (std::numeric_limits<U>::digits < std::numeric_limits<std::uint64_t>::digits) {
                    if (unsigned_value > static_cast<std::uint64_t>(std::numeric_limits<U>::max())) {
                        return std::unexpected(where + " is outside the destination integer range");
                    }
                }
                return static_cast<U>(unsigned_value);
            }
            if (signed_value < 0) {
                return std::unexpected(where + " is outside the destination integer range");
            }
            if constexpr (std::numeric_limits<U>::digits < std::numeric_limits<std::int64_t>::digits + 1) {
                if (static_cast<std::uint64_t>(signed_value) >
                    static_cast<std::uint64_t>(std::numeric_limits<U>::max())) {
                    return std::unexpected(where + " is outside the destination integer range");
                }
            }
            return static_cast<U>(signed_value);
        }
    } else if constexpr (std::is_floating_point_v<U>) {
        double value = 0.0;
        if (auto item = input.as_double(); item) value = *item;
        else if (auto item = input.as_int64(); item) value = static_cast<double>(*item);
        else if (auto item = input.as_uint64(); item) value = static_cast<double>(*item);
        else return std::unexpected(where + " must be a JSON number");
        const U converted = static_cast<U>(value);
        if (!std::isfinite(static_cast<double>(converted))) {
            return std::unexpected(where + " is outside the destination floating-point range");
        }
        return converted;
    } else if constexpr (VectorTraits<U>::value) {
        if (!input.is_array()) return std::unexpected(where + " must be a JSON array");
        U output;
        if constexpr (requires { output.reserve(input.size()); }) output.reserve(input.size());
        std::size_t index = 0;
        std::string failure;
        const auto walked = input.for_each_element([&](const Json& child) -> result<void> {
            auto decoded = decodeValue<typename VectorTraits<U>::value_type>(child, {}, options);
            if (!decoded) {
                failure = rebaseDecodeError(decoded.error(), indexedDecodePath(where.view(), index));
                return std::unexpected(failure);
            }
            output.push_back(std::move(*decoded));
            ++index;
            return {};
        });
        if (!walked) return std::unexpected(std::move(failure));
        return output;
    } else if constexpr (ArrayTraits<U>::value) {
        if (!input.is_array() || input.size() != ArrayTraits<U>::size) {
            return std::unexpected(where + " has the wrong JSON array size");
        }
        U output{};
        std::size_t index = 0;
        std::string failure;
        const auto walked = input.for_each_element([&](const Json& child) -> result<void> {
            auto decoded = decodeValue<typename ArrayTraits<U>::value_type>(child, {}, options);
            if (!decoded) {
                failure = rebaseDecodeError(decoded.error(), indexedDecodePath(where.view(), index));
                return std::unexpected(failure);
            }
            output[index] = std::move(*decoded);
            ++index;
            return {};
        });
        if (!walked) return std::unexpected(std::move(failure));
        return output;
    } else if constexpr (MapTraits<U>::value) {
        if (!input.is_object()) return std::unexpected(where + " must be a JSON object");
        U output;
        std::string failure;
        const auto walked = input.for_each_member([&](std::string_view key, const Json& child) -> result<void> {
            auto decoded = decodeValue<typename MapTraits<U>::mapped_type>(child, {}, options);
            if (!decoded) {
                failure = rebaseDecodeError(decoded.error(), memberDecodePath(where.view(), key));
                return std::unexpected(failure);
            }
            output.emplace(std::string(key), std::move(*decoded));
            return {};
        });
        if (!walked) return std::unexpected(std::move(failure));
        return output;
    } else if constexpr (Reflectable<U>) {
        if (!input.is_object()) return std::unexpected(where + " must be a JSON object");
        if constexpr (!std::is_default_constructible_v<U>) {
            return std::unexpected(where + " is not default constructible");
        } else {
            U output{};
            auto descriptors = reflect::fields(output);
            constexpr auto field_count = std::tuple_size_v<decltype(descriptors)>;
            // Small records are faster without a scratch member array.
            constexpr bool use_index = field_count > 16 ||
                (field_count == 16 && reflect::StaticReflectable<U>);
            std::array<Json, use_index ? field_count : 0> members{};
            if constexpr (use_index) {
                const auto& names = [&]() -> decltype(auto) {
                    if constexpr (reflect::StaticReflectable<U>) return (staticFieldNames<U>);
                    else return indexedFieldNames(descriptors);
                }();
                const auto walked = input.for_each_member(
                    [&](std::string_view key, const Json& child) -> result<void> {
                        auto found = [&]() {
                            if constexpr (reflect::StaticReflectable<U>) {
                                return names.begin() + staticFieldLookup<U>.find(key, names);
                            } else {
                                return std::lower_bound(names.begin(), names.end(), key,
                                    [](const auto& field, std::string_view name) {
                                        return field.first < name;
                                    });
                            }
                        }();
                        // Multiple descriptors may intentionally read the same key.
                        for (; found != names.end() && found->first == key; ++found) {
                            auto& member = members[found->second];
                            if (!member.valid()) member = child;
                        }
                        return {};
                    });
                if (!walked) return std::unexpected(walked.error());
            }
            bool failed = false;
            std::string failure;
            std::size_t member_index = 0;
            auto decode_member = [&](const auto& descriptor) {
                if (failed) return;
                using Member = BareT<decltype(descriptor.get(output))>;
                if constexpr (!std::is_assignable_v<decltype(descriptor.get(output)), Member>) {
                    failed = true;
                    failure = where + " field '" + std::string(descriptor.name) + "' is not assignable";
                    return;
                }
                const auto& member = [&]() -> decltype(auto) {
                    if constexpr (use_index) return (members[member_index++]);
                    else return input.at(descriptor.name);
                }();
                if (!member.valid()) {
                    if constexpr (OptionalTraits<Member>::value) return;
                    failed = true;
                    failure = where + " is missing field '" + std::string(descriptor.name) + "'";
                    return;
                }
                auto decoded = decodeValue<Member>(member, {}, options);
                if (!decoded) {
                    failed = true;
                    failure = rebaseDecodeError(
                        decoded.error(), memberDecodePath(where.view(), descriptor.name));
                    return;
                }
                if constexpr (std::is_assignable_v<decltype(descriptor.get(output)), Member>) {
                    descriptor.get(output) = std::move(*decoded);
                }
            };
            std::apply([&](const auto&... descriptor) {
                (decode_member(descriptor), ...);
            }, descriptors);
            if (!failed && options.unknown_fields == UnknownFieldPolicy::reject) {
                // Runtime descriptors may depend on the decoded object.
                const auto known_descriptors = reflect::fields(output);
                const auto& known_names = [&]() -> decltype(auto) {
                    if constexpr (reflect::StaticReflectable<U>) return (staticFieldNames<U>);
                    else return indexedFieldNames(known_descriptors);
                }();
                const auto walked = input.for_each_member([&](std::string_view key, const Json&) -> result<void> {
                    const auto found = std::lower_bound(known_names.begin(), known_names.end(), key,
                        [](const auto& field, std::string_view name) {
                            return field.first < name;
                        });
                    const bool known = found != known_names.end() && found->first == key;
                    if (!known) {
                        failed = true;
                        failure = where + " contains unknown field '" + std::string(key) + "'";
                        return std::unexpected(failure);
                    }
                    return {};
                });
                static_cast<void>(walked);
            }
            if (failed) return std::unexpected(std::move(failure));
            return output;
        }
    } else {
        return std::unexpected(where + " has an unsupported C++ type");
    }
}


inline std::string parse_error(simdjson::error_code error) {
    switch (error) {
        case simdjson::UNESCAPED_CHARS:
            return "JSON parse error: unescaped control character in string";
        case simdjson::UTF8_ERROR:
            return "JSON parse error: input is not valid UTF-8";
        case simdjson::NUMBER_ERROR:
            return "JSON parse error: invalid number fraction or exponent";
        case simdjson::NUMBER_OUT_OF_RANGE:
            return "JSON parse error: number is outside the finite range";
        case simdjson::STRING_ERROR:
            return "JSON parse error: invalid string or unpaired low surrogate";
        case simdjson::DEPTH_ERROR:
            return "JSON parse error: nesting depth exceeds configured limit";
        default:
            return std::string("JSON parse error: ") + simdjson::error_message(error);
    }
}

inline bool needs_document_walk(const ParseOptions& options) noexcept {
    return options.enforce_document_limits ||
           options.duplicate_keys == DuplicateKeyPolicy::reject;
}

// 默认限制远大于输入时，字符串、节点、数组和成员上限不可能被触发。
// 深度由 simdjson 的 max_depth 在解析阶段处理，这里只剩重复键。
inline bool numeric_limits_implied_by_input(const ParseOptions& options, std::size_t bytes) noexcept {
    return !options.enforce_document_limits ||
           (bytes <= options.max_nodes && bytes <= options.max_string_bytes &&
            bytes <= options.max_key_bytes && bytes <= options.max_array_items &&
            bytes <= options.max_object_members);
}

inline result<void> scan_structure(const Json& value, const ParseOptions& options, std::size_t depth,
                                  bool check_depth, bool reject_duplicates) {
    if (check_depth && depth >= options.max_depth) {
        return std::unexpected(std::string("JSON parse error: nesting depth exceeds configured limit"));
    }
    const auto type = value.type();
    if (type == ValueType::array) {
        return value.for_each_element([&](const Json& child) {
            return scan_structure(child, options, depth + 1, check_depth, reject_duplicates);
        });
    }
    if (type != ValueType::object) return {};
    if (!reject_duplicates) {
        return value.for_each_member([&](std::string_view, const Json& child) {
            return scan_structure(child, options, depth + 1, check_depth, false);
        });
    }

    // 16 个以内的键放在栈上，避免每个小对象构造堆缓冲。
    std::array<std::string_view, DuplicateKeyScan::inline_capacity> seen_inline;
    std::size_t seen_count = 0;
    std::optional<std::vector<std::string_view>> seen_heap;
    const auto walked = value.for_each_member([&](std::string_view key, const Json& child) -> result<void> {
        if (!seen_heap) {
            for (std::size_t index = 0; index < seen_count; ++index) {
                if (seen_inline[index] == key) {
                    return std::unexpected(std::string("JSON parse error: duplicate object key"));
                }
            }
            if (seen_count < seen_inline.size()) seen_inline[seen_count++] = key;
            else {
                seen_heap.emplace();
                seen_heap->reserve(seen_count * 2);
                seen_heap->insert(seen_heap->end(), seen_inline.begin(), seen_inline.begin() + seen_count);
                seen_heap->push_back(key);
            }
        } else {
            seen_heap->push_back(key);
        }
        return scan_structure(child, options, depth + 1, check_depth, true);
    });
    if (!walked) return walked;
    if (seen_heap) {
        std::sort(seen_heap->begin(), seen_heap->end());
        if (std::adjacent_find(seen_heap->begin(), seen_heap->end()) != seen_heap->end()) {
            return std::unexpected(std::string("JSON parse error: duplicate object key"));
        }
    }
    return {};
}

inline result<void> enforce_contents(const Json& value, const ParseOptions& options,
                                     std::size_t depth, std::size_t& nodes) {
    // Read the tape type once. Calling each is_* predicate in sequence repeats
    // the validity and type checks for every scalar in a document.
    const auto type = value.type();
    if (type == ValueType::string) {
        auto text = value.as_string();
        if (!text) return std::unexpected(text.error());
        if (options.enforce_document_limits && text->size() > options.max_string_bytes) {
            return std::unexpected(std::string("JSON string exceeds configured size limit"));
        }
        return {};
    }
    if (type == ValueType::array) {
        if (options.enforce_document_limits && value.size() > options.max_array_items) {
            return std::unexpected(std::string("JSON array item count exceeds configured limit"));
        }
        return value.for_each_element([&](const Json& child) {
            return enforce_limits(child, options, depth + 1, nodes);
        });
    }
    if (type != ValueType::object) return {};

    const auto member_count = value.size();
    if (options.enforce_document_limits && member_count > options.max_object_members) {
        return std::unexpected(std::string("JSON object member count exceeds configured limit"));
    }

    DuplicateKeyScan keys{options, member_count};
    const auto walked = value.for_each_member([&](std::string_view key, const Json& child) -> result<void> {
        bool repeated = false;
        if (auto observed = keys.observe(key, repeated); !observed) return observed;
        return enforce_limits(child, options, depth + 1, nodes);
    });
    if (!walked) return walked;
    return keys.finish();
}

inline result<void> enforce_limits(const Json& value, const ParseOptions& options,
                                   std::size_t depth, std::size_t& nodes) {
    if (options.enforce_document_limits) {
        if (depth >= options.max_depth) {
            return std::unexpected(std::string("JSON parse error: nesting depth exceeds configured limit"));
        }
        if (nodes++ >= options.max_nodes) {
            return std::unexpected(std::string("JSON parse error: node count exceeds configured limit"));
        }
    }
    return enforce_contents(value, options, depth, nodes);
}

inline Parser& thread_parser() {
    thread_local Parser parser;
    return parser;
}

}  // namespace json::detail

namespace json {

struct Json::State {
    simdjson::dom::parser parser;
    std::string padded_input;
    std::uint64_t generation = 0;
};

inline bool Json::valid() const noexcept {
    return state_ != nullptr && generation_ == state_->generation;
}

inline Json::Json(std::shared_ptr<State> owner, simdjson::dom::element element)
    : owner_(std::move(owner)), state_(owner_.get()),
      generation_(state_ ? state_->generation : 0), element_(element) {}

inline result<Json> Json::parse_with_state(const std::shared_ptr<State>& state,
                                    std::string_view text,
                                    const ParseOptions& options) {
    if (text.size() > options.max_input_bytes) {
        return std::unexpected(std::string("JSON input exceeds configured size limit"));
    }

    ++state->generation;
    const auto depth = std::max<std::size_t>(options.max_depth, 1);
    const auto needed = std::max<std::size_t>(text.size(), 32);
    if (state->parser.capacity() < needed || state->parser.max_depth() < depth) {
        if (auto allocation = state->parser.allocate(needed, depth)) {
            return std::unexpected(detail::parse_error(allocation));
        }
    }

    const auto padded_size = text.size() + simdjson::SIMDJSON_PADDING;
    state->padded_input.resize(padded_size);
    std::memcpy(state->padded_input.data(), text.data(), text.size());
    auto parsed = state->parser.parse(state->padded_input.data(), text.size(), false);
    if (parsed.error()) {
        return std::unexpected(detail::parse_error(parsed.error()));
    }
    Json value{state, parsed.value_unsafe()};
    if (detail::needs_document_walk(options)) {
        std::size_t nodes = 0;
        if (auto limits = detail::enforce_limits(value, options, 0, nodes); !limits) {
            return std::unexpected(limits.error());
        }
    }
    return value;
}

inline result<Json> Json::parse(std::string_view text, const ParseOptions& options) {
    return parse_with_state(std::make_shared<State>(), text, options);
}

inline result<Json> Parser::parse(std::string_view text, const ParseOptions& options) {
    if (!state_) {
        state_ = std::make_shared<Json::State>();
    }
    return Json::parse_with_state(state_, text, options);
}

inline void Parser::reset() {
    if (!state_) {
        return;
    }
    ++state_->generation;
    state_.reset();
}

inline void reset_thread_parser() {
    detail::thread_parser().reset();
}

template <class T>
result<std::string> serialize(const T& value, const SerializeOptions& options = {}) {
    if (options.indent_width > 64) {
        return std::unexpected(std::string("JSON indent width exceeds configured limit"));
    }
    detail::EncodeContext context{options};
    auto encoded = detail::encodeValue(value, context, 0);
    if (!encoded) return std::unexpected(encoded.error());
    detail::Writer output{{}, options.max_output_bytes};
    std::string failure;
    if (!detail::appendValue(*encoded, output, options, 0, failure)) {
        if (!failure.empty()) return std::unexpected(std::move(failure));
        return std::unexpected(std::string("JSON output exceeds configured size limit"));
    }
    return std::move(output.output);
}

template <class T>
result<T> decode(const Json& value, const ParseOptions& options = {}) {
    return detail::decodeValue<T>(value, {}, options);
}

template <class T>
result<T> deserialize(std::string_view text, const ParseOptions& options = {}) {
    // 解析阶段不做第二次整篇遍历。深度交给 simdjson；输入放得进各项
    // 字节/节点上限时，只扫描对象键以拒绝重复键。json::parse 仍走完整检查。
    ParseOptions parsed_options = options;
    const bool structural = detail::needs_document_walk(options);
    if (structural) {
        parsed_options.enforce_document_limits = false;
        parsed_options.duplicate_keys = DuplicateKeyPolicy::first_wins;
    }
    auto parsed = detail::thread_parser().parse(text, parsed_options);
    if (!parsed) return std::unexpected(parsed.error());
    if (structural) {
        const bool limits_implied =
            detail::numeric_limits_implied_by_input(options, text.size());
        const bool check_depth = options.enforce_document_limits && text.size() > options.max_depth;
        if (!limits_implied) {
            std::size_t nodes = 0;
            if (auto limited = detail::enforce_limits(*parsed, options, 0, nodes); !limited) {
                return std::unexpected(limited.error());
            }
        } else if (check_depth || options.duplicate_keys == DuplicateKeyPolicy::reject) {
            if (auto scanned = detail::scan_structure(
                    *parsed, options, 0, check_depth,
                    options.duplicate_keys == DuplicateKeyPolicy::reject);
                !scanned) {
                return std::unexpected(scanned.error());
            }
        }
    }
    return decode<T>(*parsed, options);
}

}  // namespace json

#endif  // SERDE_JSON_HPP
