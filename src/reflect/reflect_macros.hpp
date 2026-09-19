#ifndef SERDE_REFLECT_MACROS_HPP
#define SERDE_REFLECT_MACROS_HPP

#include <string_view>
#include <tuple>
#include <type_traits>

// C++23 没有可移植的成员枚举。字段列表是一个 X-macro，每个成员调用一次，
// 例如 `#define SERVER_FIELDS(X) X(host) X(port)`。列表本身控制重复，
// 因此该接口没有预处理器字段数量限制。公共描述符和遍历位于 `reflect` 模块中，
// 与任何序列化器无关。
//
// 列表条目可以是 `X(member)` 或 `X(member, "external-name")`。将两种拼写
// 放在一个回调后面，使命名和未命名结构使用相同的规范声明宏。
#define REFLECT_DETAIL_FIELD_1(member) \
    ::reflect::make_field(#member, &reflect_type::member),

#define REFLECT_DETAIL_FIELD_2(member, name) \
    ::reflect::make_field(name, &reflect_type::member),

#define REFLECT_DETAIL_PICK_FIELD(_1, _2, selected, ...) selected
#define REFLECT_DETAIL_FIELD(...) \
    REFLECT_DETAIL_PICK_FIELD(__VA_ARGS__, REFLECT_DETAIL_FIELD_2, \
                              REFLECT_DETAIL_FIELD_1)(__VA_ARGS__)

#define REFLECT_FIELDS(TYPE, FIELDS) \
    constexpr auto reflect_fields(::std::type_identity<TYPE>) { \
        using reflect_type = TYPE; \
        return std::tuple{FIELDS(REFLECT_DETAIL_FIELD)}; \
    } \
    constexpr auto reflect_fields(const TYPE&) { \
        return reflect_fields(::std::type_identity<TYPE>{}); \
    }

// `REFLECT_MEMBERS` 保留为源代码兼容的拼写别名，但其第二个参数现在与
// `REFLECT_FIELDS` 使用相同的无限制 X-macro 字段列表；它不再接受
// 逗号分隔的成员列表。
#define REFLECT_MEMBERS(TYPE, FIELDS) REFLECT_FIELDS(TYPE, FIELDS)

// 保留为显式空描述符的简洁兼容拼写。
#define REFLECT_EMPTY(TYPE) \
    constexpr auto reflect_fields(::std::type_identity<TYPE>) { return std::tuple{}; } \
    constexpr auto reflect_fields(const TYPE&) { return std::tuple{}; }

#endif
