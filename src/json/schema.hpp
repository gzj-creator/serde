#ifndef SERDE_JSON_SCHEMA_HPP
#define SERDE_JSON_SCHEMA_HPP

#include "json.hpp"

namespace json {
namespace schema_detail {
struct Property {
    std::string type;
    std::optional<std::string> description;
    std::optional<std::map<std::string, std::string>> items;
    std::optional<std::vector<std::string>> enumeration;
};
constexpr auto reflect_fields(std::type_identity<Property>) {
    return std::make_tuple(make_field("type", &Property::type),
        make_field("description", &Property::description, FieldPolicy{.omit_empty = true}),
        make_field("items", &Property::items, FieldPolicy{.omit_empty = true}),
        make_field("enum", &Property::enumeration, FieldPolicy{.omit_empty = true}));
}
struct Schema {
    std::string type{"object"};
    Object properties;
    std::optional<std::vector<std::string>> required;
};
constexpr auto reflect_fields(std::type_identity<Schema>) {
    return std::make_tuple(make_field("type", &Schema::type), make_field("properties", &Schema::properties),
        make_field("required", &Schema::required, FieldPolicy{.omit_empty = true}));
}
}

class SchemaBuilder {
public:
    SchemaBuilder() = default;
    SchemaBuilder(SchemaBuilder&&) noexcept = default;
    SchemaBuilder& operator=(SchemaBuilder&&) noexcept = default;
    SchemaBuilder clone() const {
        SchemaBuilder copy;
        copy.properties_ = properties_;
        return copy;
    }
    SchemaBuilder& add_string(const std::string& name, const std::string& description, bool required = false) {
        return add_property(name, description, "string", required);
    }
    SchemaBuilder& add_number(const std::string& name, const std::string& description, bool required = false) {
        return add_property(name, description, "number", required);
    }
    SchemaBuilder& add_integer(const std::string& name, const std::string& description, bool required = false) {
        return add_property(name, description, "integer", required);
    }
    SchemaBuilder& add_boolean(const std::string& name, const std::string& description, bool required = false) {
        return add_property(name, description, "boolean", required);
    }
    SchemaBuilder& add_array(const std::string& name, const std::string& description,
                            const std::string& item_type = "string", bool required = false) {
        add_property(name, description, "array", required);
        properties_.back().value.items = {{"type", item_type.empty() ? "string" : item_type}};
        return *this;
    }
    SchemaBuilder& add_enum(const std::string& name, const std::string& description,
                           const std::vector<std::string>& values, bool required = false) {
        add_property(name, description, "string", required);
        properties_.back().value.enumeration = values;
        return *this;
    }
    SchemaBuilder& add_object(const std::string& name, const std::string& description,
                             const std::string& schema, bool required = false) {
        add_property(name, description, "object", required);
        properties_.back().nested = schema;
        return *this;
    }
    SchemaBuilder& add_object(const std::string& name, const std::string& description,
                             const SchemaBuilder& schema, bool required = false) {
        return add_object(name, description, schema.build(), required);
    }
    result<std::string> encode() const {
        schema_detail::Schema schema;
        std::vector<std::string> required;
        for (const auto& property : properties_) {
            auto encoded = [&]() -> result<std::string> {
                if (!property.nested) return serialize(property.value);
                if (!property.value.description) {
                    auto object = deserialize<Object>(*property.nested);
                    if (!object) return std::unexpected(object.error());
                    return serialize(*object);
                }
                auto description = serialize(*property.value.description);
                if (!description) return std::unexpected(description.error());
                return merge_objects(*property.nested, {{"description", {std::move(*description)}}});
            }();
            if (!encoded) return std::unexpected(encoded.error());
            schema.properties.insert_or_assign(property.name, RawValue{std::move(*encoded)});
            if (property.required) required.push_back(property.name);
        }
        if (!required.empty()) schema.required = std::move(required);
        return serialize(schema);
    }
    std::string build() const {
        auto value = encode();
        if (!value) return {};
        return std::move(*value);
    }

private:
    SchemaBuilder(const SchemaBuilder&) = delete;
    SchemaBuilder& operator=(const SchemaBuilder&) = delete;
    struct Property {
        std::string name;
        schema_detail::Property value;
        bool required = false;
        std::optional<std::string> nested;
    };
    SchemaBuilder& add_property(const std::string& name, const std::string& description,
                               std::string type, bool required) {
        schema_detail::Property value;
        value.type = std::move(type);
        if (!description.empty()) value.description = description;
        properties_.push_back({name, std::move(value), required, {}});
        return *this;
    }
    std::vector<Property> properties_;
};
} // namespace json
#endif
