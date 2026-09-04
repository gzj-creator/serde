#ifndef DEMO_TOML_REFLECT_HPP
#define DEMO_TOML_REFLECT_HPP

// 字段列表由使用者显式提供。C++23 预处理器无法检查已经声明的类，
// 但可以将列表展开为可在编译期构造、并可通过 ADL 找到的反射函数。
#define TOML_DETAIL_REFLECT_FIELD(member) \
    ::toml::make_field(#member, &toml_reflect_type::member),

#define TOML_DETAIL_REFLECT_NAMED_FIELD(member, name) \
    ::toml::make_field(name, &toml_reflect_type::member),

// 直接的可变参数形式适合小型记录：
// TOML_REFLECT_MEMBERS(A, name, age)。当多个生成函数需要共享字段列表时，
// 下面的 X-macro 形式更加方便。
#define TOML_DETAIL_REFLECT_FOR_EACH_1(M, a) M(a)
#define TOML_DETAIL_REFLECT_FOR_EACH_2(M, a, ...) \
    M(a) TOML_DETAIL_REFLECT_FOR_EACH_1(M, __VA_ARGS__)
#define TOML_DETAIL_REFLECT_FOR_EACH_3(M, a, ...) \
    M(a) TOML_DETAIL_REFLECT_FOR_EACH_2(M, __VA_ARGS__)
#define TOML_DETAIL_REFLECT_FOR_EACH_4(M, a, ...) \
    M(a) TOML_DETAIL_REFLECT_FOR_EACH_3(M, __VA_ARGS__)
#define TOML_DETAIL_REFLECT_FOR_EACH_5(M, a, ...) \
    M(a) TOML_DETAIL_REFLECT_FOR_EACH_4(M, __VA_ARGS__)
#define TOML_DETAIL_REFLECT_FOR_EACH_6(M, a, ...) \
    M(a) TOML_DETAIL_REFLECT_FOR_EACH_5(M, __VA_ARGS__)
#define TOML_DETAIL_REFLECT_FOR_EACH_7(M, a, ...) \
    M(a) TOML_DETAIL_REFLECT_FOR_EACH_6(M, __VA_ARGS__)
#define TOML_DETAIL_REFLECT_FOR_EACH_8(M, a, ...) \
    M(a) TOML_DETAIL_REFLECT_FOR_EACH_7(M, __VA_ARGS__)
#define TOML_DETAIL_REFLECT_FOR_EACH_9(M, a, ...) \
    M(a) TOML_DETAIL_REFLECT_FOR_EACH_8(M, __VA_ARGS__)
#define TOML_DETAIL_REFLECT_FOR_EACH_10(M, a, ...) \
    M(a) TOML_DETAIL_REFLECT_FOR_EACH_9(M, __VA_ARGS__)
#define TOML_DETAIL_REFLECT_FOR_EACH_11(M, a, ...) \
    M(a) TOML_DETAIL_REFLECT_FOR_EACH_10(M, __VA_ARGS__)
#define TOML_DETAIL_REFLECT_FOR_EACH_12(M, a, ...) \
    M(a) TOML_DETAIL_REFLECT_FOR_EACH_11(M, __VA_ARGS__)
#define TOML_DETAIL_REFLECT_FOR_EACH_13(M, a, ...) \
    M(a) TOML_DETAIL_REFLECT_FOR_EACH_12(M, __VA_ARGS__)
#define TOML_DETAIL_REFLECT_FOR_EACH_14(M, a, ...) \
    M(a) TOML_DETAIL_REFLECT_FOR_EACH_13(M, __VA_ARGS__)
#define TOML_DETAIL_REFLECT_FOR_EACH_15(M, a, ...) \
    M(a) TOML_DETAIL_REFLECT_FOR_EACH_14(M, __VA_ARGS__)
#define TOML_DETAIL_REFLECT_FOR_EACH_16(M, a, ...) \
    M(a) TOML_DETAIL_REFLECT_FOR_EACH_15(M, __VA_ARGS__)

#define TOML_DETAIL_REFLECT_NARG_I( \
    _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, N, ...) N
#define TOML_DETAIL_REFLECT_NARG(...) \
    TOML_DETAIL_REFLECT_NARG_I(__VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, \
                               8, 7, 6, 5, 4, 3, 2, 1, 0)
#define TOML_DETAIL_REFLECT_CAT_I(a, b) a##b
#define TOML_DETAIL_REFLECT_CAT(a, b) TOML_DETAIL_REFLECT_CAT_I(a, b)
#define TOML_DETAIL_REFLECT_FOR_EACH_I(N, M, ...) \
    TOML_DETAIL_REFLECT_CAT(TOML_DETAIL_REFLECT_FOR_EACH_, N)(M, __VA_ARGS__)
#define TOML_DETAIL_REFLECT_FOR_EACH(M, ...) \
    TOML_DETAIL_REFLECT_FOR_EACH_I(TOML_DETAIL_REFLECT_NARG(__VA_ARGS__), M, __VA_ARGS__)

/**
 * @brief 为类型生成基于成员名称的反射函数。
 *
 * 生成的函数必须与 `TYPE` 位于同一关联命名空间，以便参数依赖查找发现。
 * @param TYPE 要反射的结构类型。
 * @param members 按 TOML 输出顺序排列的 C++ 成员名称列表，最多 16 个。
 */
#define TOML_REFLECT_MEMBERS(TYPE, ...) \
    constexpr auto toml_reflect(const TYPE&) { \
        using toml_reflect_type = TYPE; \
        return std::tuple{TOML_DETAIL_REFLECT_FOR_EACH( \
            TOML_DETAIL_REFLECT_FIELD, __VA_ARGS__)}; \
    }

/**
 * @brief 使用 X-macro 字段列表为类型生成反射函数。
 * @param TYPE 要反射的结构类型。
 * @param FIELDS 接受字段宏参数的 X-macro 列表。
 */
#define TOML_REFLECT(TYPE, FIELDS) \
    constexpr auto toml_reflect(const TYPE&) { \
        using toml_reflect_type = TYPE; \
        return std::tuple{FIELDS(TOML_DETAIL_REFLECT_FIELD)}; \
    }

// 命名 X-macro 形式可以将 C++ 成员与 TOML 键解耦：
// #define FIELDS(X) X(cpp_member, "toml-key")
/**
 * @brief 使用命名 X-macro 字段列表生成反射函数。
 * @param TYPE 要反射的结构类型。
 * @param FIELDS 接受 `(成员, TOML 键)` 参数的 X-macro 列表。
 */
#define TOML_REFLECT_NAMED(TYPE, FIELDS) \
    constexpr auto toml_reflect(const TYPE&) { \
        using toml_reflect_type = TYPE; \
        return std::tuple{FIELDS(TOML_DETAIL_REFLECT_NAMED_FIELD)}; \
    }

#endif
