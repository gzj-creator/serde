export module reflect;

export import std;

/**
 * @brief Compiler-independent reflection vocabulary shared by serializers.
 *
 * The module intentionally exposes no compiler metaobject handles. A
 * serializer can therefore be compiled by Clang/LLVM while a separate GCC
 * build uses C++26 reflection only to generate local metadata.
 */
export namespace reflect {

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
concept has_reflect_fields = requires(const std::remove_cvref_t<T>& value) {
    reflect_fields(value);
};

template <class T>
concept has_any_fields = has_reflect_fields<T>;

template <class T>
    requires has_reflect_fields<T>
constexpr decltype(auto) get_fields(const T& value) {
    return reflect_fields(value);
}

}  // namespace detail

template <class T>
concept reflectable = detail::has_any_fields<T>;

template <class T>
    requires reflectable<T>
constexpr decltype(auto) fields(const T& value) {
    return detail::get_fields(value);
}

template <class T, class Function>
    requires reflectable<T>
constexpr void for_each_field(T& value, Function&& function) {
    auto descriptors = fields(value);
    std::apply(
        [&](const auto&... descriptor) {
            (std::invoke(function, descriptor, value), ...);
        },
        descriptors);
}

#if defined(__cpp_impl_reflection) && __cpp_impl_reflection >= 202506L
inline constexpr bool native_reflection_available = true;
#else
inline constexpr bool native_reflection_available = false;
#endif

}  // namespace reflect
