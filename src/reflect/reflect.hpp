#ifndef SERDE_REFLECT_HPP
#define SERDE_REFLECT_HPP
// serde 编译期反射实现头。经典 TU 直接 include；模块消费者经
// src/reflect/reflect.cppm 门面 import。

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

/**
 * @brief 编译器无关的反射词汇，供序列化器共享。
 *
 * 该模块有意不暴露编译器元对象句柄。因此序列化器可以由 Clang/LLVM 编译，
 * 而单独的 GCC 构建仅使用 C++26 反射来生成本地元数据。
 */
namespace reflect {

enum class enum_encoding { underlying, string };

template <class T>
struct is_optional : std::false_type {};

template <class T>
struct is_optional<std::optional<T>> : std::true_type {
    using value_type = T;
};

template <class T>
inline constexpr bool is_optional_v = is_optional<std::remove_cvref_t<T>>::value;

namespace detail {

template <class T>
using bare_t = std::remove_cv_t<std::remove_reference_t<T>>;

template <class T>
struct optional_value {
    using type = bare_t<T>;
};

template <class T>
struct optional_value<std::optional<T>> : optional_value<T> {};

template <class T>
using optional_value_t = typename optional_value<bare_t<T>>::type;

template <class T>
struct is_string : std::false_type {};

template <>
struct is_string<std::string> : std::true_type {};

template <>
struct is_string<std::string_view> : std::true_type {};

template <class T>
struct is_item_container : std::false_type {};

template <class T, class Allocator>
struct is_item_container<std::vector<T, Allocator>> : std::true_type {};

template <class T, std::size_t N>
struct is_item_container<std::array<T, N>> : std::true_type {};

template <class K, class V, class Compare, class Allocator>
struct is_item_container<std::map<K, V, Compare, Allocator>> : std::true_type {};

template <class K, class V, class Hash, class Equal, class Allocator>
struct is_item_container<std::unordered_map<K, V, Hash, Equal, Allocator>> : std::true_type {};

template <class T, bool = std::is_enum_v<T>>
struct bound_type {
    using type = std::conditional_t<std::is_arithmetic_v<T> && !std::same_as<T, bool>, T, double>;
};

template <class T>
struct bound_type<T, true> {
    using type = std::underlying_type_t<T>;
};

struct utf8_measurement {
    bool valid = false;
    std::size_t scalars = 0;
};

// Reject overlong sequences, surrogate code points and values above U+10FFFF
// while counting Unicode scalars, rather than bytes or grapheme clusters.
constexpr utf8_measurement measure_utf8(std::string_view text) noexcept {
    std::size_t count = 0;
    std::size_t index = 0;
    while (index < text.size()) {
        const auto first = static_cast<unsigned char>(text[index]);
        std::uint32_t code_point = 0;
        std::size_t width = 0;
        if (first <= 0x7F) {
            code_point = first;
            width = 1;
        } else if (first >= 0xC2 && first <= 0xDF) {
            code_point = first & 0x1F;
            width = 2;
        } else if (first >= 0xE0 && first <= 0xEF) {
            code_point = first & 0x0F;
            width = 3;
        } else if (first >= 0xF0 && first <= 0xF4) {
            code_point = first & 0x07;
            width = 4;
        } else {
            return {};
        }
        if (width > text.size() - index) return {};
        for (std::size_t offset = 1; offset < width; ++offset) {
            const auto continuation = static_cast<unsigned char>(text[index + offset]);
            if ((continuation & 0xC0) != 0x80) return {};
            code_point = (code_point << 6) | (continuation & 0x3F);
        }
        if ((width == 2 && code_point < 0x80) ||
            (width == 3 && code_point < 0x800) ||
            (width == 4 && code_point < 0x10000) ||
            code_point > 0x10FFFF || (code_point >= 0xD800 && code_point <= 0xDFFF)) {
            return {};
        }
        index += width;
        ++count;
    }
    return {true, count};
}

} // namespace detail

template <class Member>
struct field_options {
    using value_type = detail::optional_value_t<Member>;
    using bound_type = typename detail::bound_type<value_type>::type;

    std::string_view description{};
    std::optional<bound_type> minimum{};
    std::optional<bound_type> maximum{};
    std::optional<std::size_t> min_length{};
    std::optional<std::size_t> max_length{};
    std::optional<std::size_t> min_items{};
    std::optional<std::size_t> max_items{};

    constexpr bool empty() const noexcept {
        return description.empty() && !minimum && !maximum &&
               !min_length && !max_length && !min_items && !max_items;
    }
};

template <class E>
struct enum_entry {
    E value;
    std::string_view name;
};

template <class E, std::size_t N>
struct enum_descriptor {
    enum_encoding encoding;
    std::array<enum_entry<E>, N> values;
};

template <class Owner, class Member, bool HasOptions = false>
struct field {
    std::string_view name;
    Member Owner::*pointer;
    static constexpr field_options<Member> options{};

    template <class Object>
    constexpr decltype(auto) get(Object&& object) const noexcept {
        return std::forward<Object>(object).*pointer;
    }
};

template <class Owner, class Member>
struct field<Owner, Member, true> {
    std::string_view name;
    Member Owner::*pointer;
    field_options<Member> options{};

    template <class Object>
    constexpr decltype(auto) get(Object&& object) const noexcept {
        return std::forward<Object>(object).*pointer;
    }
};

template <class Owner, class Member>
constexpr auto make_field(std::string_view name, Member Owner::*pointer) noexcept {
    return field<Owner, Member>{name, pointer};
}

template <class Owner, class Member>
constexpr auto make_field(std::string_view name, Member Owner::*pointer,
                          field_options<Member> options) noexcept {
    return field<Owner, Member, true>{name, pointer, options};
}

namespace detail {

template <class T>
struct enum_descriptor_traits;

template <class E, std::size_t N>
struct enum_descriptor_traits<enum_descriptor<E, N>> {
    using enum_type = E;
};

template <class E, std::size_t N>
constexpr std::expected<void, std::string> validate_descriptor(
    const enum_descriptor<E, N>& descriptor) {
    if (descriptor.encoding != enum_encoding::underlying &&
        descriptor.encoding != enum_encoding::string) {
        return std::unexpected(std::string("invalid enum encoding"));
    }
    if (descriptor.values.empty()) {
        return std::unexpected(std::string("enum descriptor has no values"));
    }
    for (std::size_t index = 0; index < descriptor.values.size(); ++index) {
        const auto& entry = descriptor.values[index];
        if (entry.name.empty() || !measure_utf8(entry.name).valid) {
            return std::unexpected(std::string("enum name must be nonempty valid UTF-8"));
        }
        for (std::size_t previous = 0; previous < index; ++previous) {
            if (descriptor.values[previous].value == entry.value) {
                return std::unexpected(std::string("duplicate enum value"));
            }
            if (descriptor.values[previous].name == entry.name) {
                return std::unexpected(std::string("duplicate enum name"));
            }
        }
    }
    return {};
}

template <class T>
concept HasEnumDescriptor = requires {
    typename enum_descriptor_traits<bare_t<decltype(
        reflect_enum(std::type_identity<bare_t<T>>{}))>>::enum_type;
    requires std::same_as<bare_t<T>, typename enum_descriptor_traits<bare_t<decltype(
        reflect_enum(std::type_identity<bare_t<T>>{}))>>::enum_type>;
};

} // namespace detail

template <class E>
concept EnumReflectable = std::is_enum_v<detail::bare_t<E>> && detail::HasEnumDescriptor<E>;

template <class E>
    requires EnumReflectable<E>
constexpr auto enum_descriptor_for() {
    return reflect_enum(std::type_identity<detail::bare_t<E>>{});
}

template <class E>
    requires std::is_enum_v<detail::bare_t<E>>
constexpr std::expected<void, std::string> validate_enum_descriptor() {
    if constexpr (EnumReflectable<E>) {
        const auto descriptor = enum_descriptor_for<E>();
        return detail::validate_descriptor(descriptor);
    }
    return {};
}

template <class E>
    requires std::is_enum_v<detail::bare_t<E>>
constexpr std::expected<void, std::string> validate_enum(E value) {
    if constexpr (EnumReflectable<E>) {
        const auto descriptor = enum_descriptor_for<E>();
        if (auto checked = detail::validate_descriptor(descriptor); !checked) return checked;
        for (const auto& entry : descriptor.values) {
            if (entry.value == value) return {};
        }
        return std::unexpected(std::string("unknown enum value"));
    }
    return {};
}

template <class E>
    requires std::is_enum_v<detail::bare_t<E>>
constexpr std::expected<std::string_view, std::string> enum_to_string(E value) {
    if constexpr (EnumReflectable<E>) {
        const auto descriptor = enum_descriptor_for<E>();
        if (auto checked = detail::validate_descriptor(descriptor); !checked) {
            return std::unexpected(std::move(checked.error()));
        }
        for (const auto& entry : descriptor.values) {
            if (entry.value == value) return entry.name;
        }
        return std::unexpected(std::string("unknown enum value"));
    }
    return std::unexpected(std::string("enum has no registered names"));
}

template <class E>
    requires std::is_enum_v<detail::bare_t<E>>
constexpr std::expected<E, std::string> enum_from_string(std::string_view name) {
    if constexpr (EnumReflectable<E>) {
        const auto descriptor = enum_descriptor_for<E>();
        if (auto checked = detail::validate_descriptor(descriptor); !checked) {
            return std::unexpected(std::move(checked.error()));
        }
        for (const auto& entry : descriptor.values) {
            if (entry.name == name) return entry.value;
        }
        return std::unexpected(std::string("unknown enum name"));
    }
    return std::unexpected(std::string("enum has no registered names"));
}

namespace detail {

template <class Member>
constexpr std::expected<void, std::string> validate_options(const field_options<Member>& options) {
    using Value = optional_value_t<Member>;
    using Bound = typename field_options<Member>::bound_type;
    if (!options.description.empty() && !measure_utf8(options.description).valid) {
        return std::unexpected(std::string("field description is not valid UTF-8"));
    }
    if (options.minimum || options.maximum) {
        if constexpr ((std::is_arithmetic_v<Value> && !std::same_as<Value, bool>) ||
                      std::is_enum_v<Value>) {
            if constexpr (std::same_as<Bound, bool>) {
                return std::unexpected(std::string("numeric bounds do not apply to boolean enums"));
            }
            if constexpr (EnumReflectable<Value>) {
                if (enum_descriptor_for<Value>().encoding == enum_encoding::string) {
                    return std::unexpected(std::string("numeric bounds do not apply to string enums"));
                }
            }
            if constexpr (std::floating_point<Bound>) {
                if ((options.minimum && !std::isfinite(*options.minimum)) ||
                    (options.maximum && !std::isfinite(*options.maximum))) {
                    return std::unexpected(std::string("numeric bounds must be finite"));
                }
            }
            if (options.minimum && options.maximum && *options.minimum > *options.maximum) {
                return std::unexpected(std::string("minimum exceeds maximum"));
            }
        } else {
            return std::unexpected(std::string("numeric bounds do not apply to this field type"));
        }
    }
    if (options.min_length || options.max_length) {
        if constexpr (!is_string<Value>::value) {
            return std::unexpected(std::string("string length bounds do not apply to this field type"));
        }
        if (options.min_length && options.max_length && *options.min_length > *options.max_length) {
            return std::unexpected(std::string("min_length exceeds max_length"));
        }
    }
    if (options.min_items || options.max_items) {
        if constexpr (!is_item_container<Value>::value) {
            return std::unexpected(std::string("item count bounds do not apply to this field type"));
        }
        if (options.min_items && options.max_items && *options.min_items > *options.max_items) {
            return std::unexpected(std::string("min_items exceeds max_items"));
        }
    }
    if constexpr (std::is_enum_v<Value>) return validate_enum_descriptor<Value>();
    return {};
}

template <class Value, class Member>
constexpr std::expected<void, std::string> validate_field_value(
    const Value& value, const field_options<Member>& options) {
    using Bare = bare_t<Value>;
    if constexpr (is_optional_v<Value>) {
        if (!value) return {};
        return validate_field_value(*value, options);
    } else {
        if constexpr (std::is_enum_v<Bare>) {
            if (auto checked = validate_enum(value); !checked) return checked;
        }
        if constexpr ((std::is_arithmetic_v<Bare> && !std::same_as<Bare, bool>) ||
                      std::is_enum_v<Bare>) {
            if (options.minimum || options.maximum) {
                const auto numeric_value = [&] {
                    if constexpr (std::is_enum_v<Bare>) return static_cast<std::underlying_type_t<Bare>>(value);
                    else return value;
                }();
                if constexpr (std::floating_point<decltype(numeric_value)>) {
                    if (!std::isfinite(numeric_value)) {
                        return std::unexpected(std::string("constrained numeric value must be finite"));
                    }
                }
                if (options.minimum && numeric_value < *options.minimum) {
                    return std::unexpected(std::string("field value is below minimum"));
                }
                if (options.maximum && numeric_value > *options.maximum) {
                    return std::unexpected(std::string("field value exceeds maximum"));
                }
            }
        }
        if constexpr (is_string<Bare>::value) {
            if (options.min_length || options.max_length) {
                const auto measured = measure_utf8(std::string_view(value));
                if (!measured.valid) return std::unexpected(std::string("field value is not valid UTF-8"));
                if (options.min_length && measured.scalars < *options.min_length) {
                    return std::unexpected(std::string("field value is shorter than min_length"));
                }
                if (options.max_length && measured.scalars > *options.max_length) {
                    return std::unexpected(std::string("field value exceeds max_length"));
                }
            }
        }
        if constexpr (is_item_container<Bare>::value) {
            if (options.min_items && value.size() < *options.min_items) {
                return std::unexpected(std::string("field value has fewer than min_items"));
            }
            if (options.max_items && value.size() > *options.max_items) {
                return std::unexpected(std::string("field value exceeds max_items"));
            }
        }
    }
    return {};
}

} // namespace detail

template <class Owner, class Member, bool HasOptions>
constexpr std::expected<void, std::string> validate_field(
    const field<Owner, Member, HasOptions>& descriptor, const std::type_identity_t<Member>& value) {
    if constexpr (!EnumReflectable<detail::optional_value_t<Member>>) {
        if (descriptor.options.empty()) return {};
    }
    if (auto checked = detail::validate_options(descriptor.options); !checked) return checked;
    return detail::validate_field_value(value, descriptor.options);
}

template <class Descriptor, class Member>
    requires (!requires(const Descriptor& descriptor) { descriptor.options; })
constexpr std::expected<void, std::string> validate_field(
    const Descriptor& descriptor, const Member& value) {
    if constexpr (is_optional_v<Member>) {
        if (value) return validate_field(descriptor, *value);
        using Value = detail::optional_value_t<Member>;
        if constexpr (std::is_enum_v<Value>) return validate_enum_descriptor<Value>();
    } else if constexpr (std::is_enum_v<Member>) {
        return validate_enum(value);
    }
    return {};
}

namespace detail {

template <class T>
concept hasReflectFields = requires(const std::remove_cvref_t<T>& value) {
    reflect_fields(value);
};

template <class T>
concept hasAnyFields = hasReflectFields<T> || requires {
    reflect_fields(std::type_identity<std::remove_cvref_t<T>>{});
};

template <class T>
    requires hasAnyFields<T>
constexpr decltype(auto) getFields(const T& value) {
    if constexpr (hasReflectFields<T>) return reflect_fields(value);
    else return reflect_fields(std::type_identity<std::remove_cvref_t<T>>{});
}

template <class T>
constexpr std::size_t staticFieldNamesChecksum() {
    auto descriptors = reflect_fields(std::type_identity<std::remove_cvref_t<T>>{});
    std::size_t checksum = 0;
    std::apply([&](const auto&... descriptor) {
        ([&] {
            for (const unsigned char character : descriptor.name) checksum += character;
        }(), ...);
    }, descriptors);
    return checksum;
}

}  // namespace detail

template <class T>
concept Reflectable = detail::hasAnyFields<T>;

template <class T>
concept StaticReflectable = Reflectable<T> && requires {
    reflect_fields(std::type_identity<std::remove_cvref_t<T>>{});
    typename std::integral_constant<std::size_t, detail::staticFieldNamesChecksum<T>()>;
};

template <class T>
    requires StaticReflectable<T>
constexpr auto static_fields() {
    return reflect_fields(std::type_identity<std::remove_cvref_t<T>>{});
}

template <class T>
    requires Reflectable<T>
constexpr decltype(auto) fields(const T& value) {
    return detail::getFields(value);
}

template <class T, class Function>
    requires Reflectable<T>
constexpr void for_each_field(T& value, Function&& function) {
    const auto& descriptors = fields(value);
    std::apply(
        [&](const auto&... descriptor) {
            (std::invoke(function, descriptor, value), ...);
        },
        descriptors);
}

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
inline constexpr bool nativeReflectionAvailable = true;
#else
inline constexpr bool nativeReflectionAvailable = false;
#endif

}  // namespace reflect

#endif  // SERDE_REFLECT_HPP
