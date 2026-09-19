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

template <class Owner, class Member>
struct field {
    std::string_view name;
    Member Owner::*pointer;

    template <class Object>
    constexpr decltype(auto) get(Object&& object) const noexcept {
        return std::forward<Object>(object).*pointer;
    }
};

template <class Owner, class Member>
constexpr auto make_field(std::string_view name, Member Owner::*pointer) noexcept {
    return field<Owner, Member>{name, pointer};
}

namespace detail {

template <class T>
concept hasReflectFields = requires(const std::remove_cvref_t<T>& value) {
    reflect_fields(value);
};

template <class T>
concept hasAnyFields = hasReflectFields<T>;

template <class T>
    requires hasReflectFields<T>
constexpr decltype(auto) getFields(const T& value) {
    return reflect_fields(value);
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
    auto descriptors = fields(value);
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