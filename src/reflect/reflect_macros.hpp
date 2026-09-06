#ifndef SERDE_REFLECT_MACROS_HPP
#define SERDE_REFLECT_MACROS_HPP

#include <string_view>
#include <tuple>

// C++23 has no portable member enumeration. A field list is an X-macro with
// one invocation per member, for example `#define SERVER_FIELDS(X) X(host)
// X(port)`. The list itself controls repetition, so this interface has no
// preprocessor field-count limit. The public descriptor and traversal live in
// the `reflect` module and are independent of any serializer.
//
// A list entry may be `X(member)` or `X(member, "external-name")`. Keeping
// both spellings behind one callback lets named and unnamed structures use the
// same canonical declaration macro.
#define REFLECT_DETAIL_FIELD_1(member) \
    ::reflect::make_field(#member, &reflect_type::member),

#define REFLECT_DETAIL_FIELD_2(member, name) \
    ::reflect::make_field(name, &reflect_type::member),

#define REFLECT_DETAIL_PICK_FIELD(_1, _2, selected, ...) selected
#define REFLECT_DETAIL_FIELD(...) \
    REFLECT_DETAIL_PICK_FIELD(__VA_ARGS__, REFLECT_DETAIL_FIELD_2, \
                              REFLECT_DETAIL_FIELD_1)(__VA_ARGS__)

#define REFLECT_FIELDS(TYPE, FIELDS) \
    constexpr auto reflect_fields(const TYPE&) { \
        using reflect_type = TYPE; \
        return std::tuple{FIELDS(REFLECT_DETAIL_FIELD)}; \
    }

// `REFLECT_MEMBERS` is retained as a spelling alias for source compatibility,
// but its second argument is now the same unlimited X-macro field list used by
// `REFLECT_FIELDS`; it no longer accepts a comma-separated member list.
#define REFLECT_MEMBERS(TYPE, FIELDS) REFLECT_FIELDS(TYPE, FIELDS)

// Kept as a concise compatibility spelling for an explicit empty descriptor.
#define REFLECT_EMPTY(TYPE) \
    constexpr auto reflect_fields(const TYPE&) { return std::tuple{}; }

#endif
