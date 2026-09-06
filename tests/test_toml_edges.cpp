import std;
import toml;

#include "../src/reflect/reflect_macros.hpp"

namespace {

struct Variety {
    std::string name;

    bool operator==(const Variety&) const = default;
};

#define SERDE_FIELDS_27(X) \
    X(name)
REFLECT_FIELDS(Variety, SERDE_FIELDS_27)
#undef SERDE_FIELDS_27

struct Fruit {
    std::string name;
    std::vector<Variety> varieties;

    bool operator==(const Fruit&) const = default;
};

#define SERDE_FIELDS_28(X) \
    X(name) \
    X(varieties)
REFLECT_FIELDS(Fruit, SERDE_FIELDS_28)
#undef SERDE_FIELDS_28

struct Document {
    std::vector<Fruit> fruits;

    bool operator==(const Document&) const = default;
};

#define SERDE_FIELDS_29(X) \
    X(fruits)
REFLECT_FIELDS(Document, SERDE_FIELDS_29)
#undef SERDE_FIELDS_29

struct Mixed {
    std::vector<double> values;
};

#define SERDE_FIELDS_30(X) \
    X(values)
REFLECT_FIELDS(Mixed, SERDE_FIELDS_30)
#undef SERDE_FIELDS_30

struct InlineArray {
    struct nested {
        std::vector<int> values;

        bool operator==(const nested&) const = default;
    };

    nested values;
};

#define SERDE_FIELDS_31(X) \
    X(values)
REFLECT_FIELDS(InlineArray::nested, SERDE_FIELDS_31)
#undef SERDE_FIELDS_31
#define SERDE_FIELDS_32(X) \
    X(values)
REFLECT_FIELDS(InlineArray, SERDE_FIELDS_32)
#undef SERDE_FIELDS_32

struct MultiQuote {
    std::string value;
};

#define SERDE_FIELDS_33(X) \
    X(value)
REFLECT_FIELDS(MultiQuote, SERDE_FIELDS_33)
#undef SERDE_FIELDS_33

struct NestedGrandchild {
    std::string name;

    bool operator==(const NestedGrandchild&) const = default;
};

#define SERDE_FIELDS_34(X) \
    X(name)
REFLECT_FIELDS(NestedGrandchild, SERDE_FIELDS_34)
#undef SERDE_FIELDS_34

struct NestedChild {
    std::vector<NestedGrandchild> grandchildren;

    bool operator==(const NestedChild&) const = default;
};

#define SERDE_FIELDS_35(X) \
    X(grandchildren)
REFLECT_FIELDS(NestedChild, SERDE_FIELDS_35)
#undef SERDE_FIELDS_35

struct NestedRoot {
    NestedChild child;

    bool operator==(const NestedRoot&) const = default;
};

#define SERDE_FIELDS_36(X) \
    X(child)
REFLECT_FIELDS(NestedRoot, SERDE_FIELDS_36)
#undef SERDE_FIELDS_36

bool expect(bool condition, std::string_view message) {
    if (!condition) {
        std::println("test_toml_edges: {}", message);
    }
    return condition;
}

}  // namespace

int main() {
    const auto parsed = toml::deserialize<Document>(
        "[[fruits]]\n"
        "name = \"apple\"\n"
        "[[fruits.varieties]]\n"
        "name = \"red delicious\"\n"
        "[[fruits.varieties]]\n"
        "name = \"granny smith\"\n"
        "[[fruits]]\n"
        "name = \"banana\"\n"
        "[[fruits.varieties]]\n"
        "name = \"cavendish\"\n");

    const Document expected{
        {{"apple", {{"red delicious"}, {"granny smith"}}},
         {"banana", {{"cavendish"}}}}};
    bool passed = expect(parsed && *parsed == expected,
                         "nested array-of-tables preserve parent element context");
    if (parsed) {
        const auto serialized = toml::serialize(*parsed);
        passed &= expect(static_cast<bool>(serialized), "nested array-of-tables serialize");
        if (serialized) {
            const auto reparsed = toml::deserialize<Document>(*serialized);
            passed &= expect(reparsed && *reparsed == expected,
                             "nested array-of-tables round-trip");
        }
    }

    passed &= expect(toml::deserialize<Document>(
                         "[[fruits]]\nname = \"apple\"\n[fruits]\nname = \"bad\"\n")
                         .has_value() == false,
                     "array-table cannot be reopened as a regular table");
    const auto mixed = toml::deserialize<Mixed>("values = [1, 2.0]\n");
    passed &= expect(mixed && mixed->values == std::vector<double>{1.0, 2.0},
                     "mixed TOML arrays remain valid and decode numerically");
    const auto inline_array = toml::deserialize<InlineArray>(
        "values = { values = [\n1,\n2,\n] }\n");
    passed &= expect(inline_array && inline_array->values.values == std::vector<int>{1, 2},
                     "inline tables allow newlines inside nested arrays");
    const auto multi_quote = toml::deserialize<MultiQuote>(
        "value = \"\"\"two \"\" quotes\"\"\"\n");
    passed &= expect(multi_quote && multi_quote->value == "two \"\" quotes",
                     "multiline basic strings preserve four quote runs");

    const auto nested = toml::deserialize<NestedRoot>(
        "[[child.grandchildren]]\nname = \"first\"\n"
        "[[child.grandchildren]]\nname = \"second\"\n");
    passed &= expect(nested && nested->child.grandchildren ==
                         std::vector<NestedGrandchild>{{"first"}, {"second"}},
                     "repeated nested array tables remain appendable");
    return passed ? 0 : 1;
}
