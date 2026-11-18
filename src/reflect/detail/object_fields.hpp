#ifndef SERDE_OBJECT_FIELDS_HPP
#define SERDE_OBJECT_FIELDS_HPP

#include <third_party/boost_pfr/include/boost/pfr/core.hpp>
#include <third_party/boost_pfr/include/boost/pfr/core_name.hpp>

#include "../reflect.hpp"

namespace reflect::detail {

enum class ObjectMapping { kExplicitOnly, kAllowAggregate };

template <class T>
concept AggregateObject = !Reflectable<T> && std::is_class_v<T> &&
    std::is_aggregate_v<T> && std::is_default_constructible_v<T> &&
    (!requires { std::tuple_size<T>::value; });

template <class T, ObjectMapping Mapping>
concept MappedObject = Reflectable<T> || (Mapping == ObjectMapping::kAllowAggregate && AggregateObject<T>);

template <class T>
concept StaticMappedObject = StaticReflectable<T> || AggregateObject<T>;

template <class T>
inline constexpr auto object_member_mapping = Reflectable<T>
    ? ObjectMapping::kExplicitOnly : ObjectMapping::kAllowAggregate;

template <std::size_t Index>
struct AggregateField {
    std::string_view name;

    template <class Object>
    constexpr decltype(auto) get(Object& object) const noexcept {
        return serde_boost_pfr::pfr::get<Index>(object);
    }
};

template <class T, std::size_t... Index>
constexpr auto make_aggregate_fields(std::index_sequence<Index...>) {
    return std::tuple{AggregateField<Index>{serde_boost_pfr::pfr::get_name<Index, T>()}...};
}

template <AggregateObject T>
inline constexpr auto aggregate_fields = make_aggregate_fields<T>(
    std::make_index_sequence<serde_boost_pfr::pfr::tuple_size_v<T>>{});

template <StaticMappedObject T>
constexpr decltype(auto) static_object_fields() {
    if constexpr (StaticReflectable<T>) return reflect::static_fields<T>();
    else return (aggregate_fields<T>);
}

template <class T>
constexpr decltype(auto) object_fields(const T& object) {
    if constexpr (Reflectable<T>) return reflect::fields(object);
    else return (aggregate_fields<T>);
}

template <class T, class Function>
constexpr void for_each_object_field(T& object, Function&& function) {
    const auto& descriptors = object_fields(object);
    std::apply([&](const auto&... descriptor) {
        (std::invoke(function, descriptor, object), ...);
    }, descriptors);
}

}

#endif
