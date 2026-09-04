import std;
import toml;

#include "../src/toml/toml_reflect.hpp"

namespace {

struct Required {
    std::string name;
    int age{};
};

TOML_REFLECT_MEMBERS(Required, name, age)

struct Pair {
    std::array<int, 2> values{};
};

TOML_REFLECT_MEMBERS(Pair, values)

struct DateRecord {
    toml::date day;
};

TOML_REFLECT_MEMBERS(DateRecord, day)

struct ConstRecord {
    const int value = 1;
};

TOML_REFLECT_MEMBERS(ConstRecord, value)

struct WideUnsigned {
    std::uint64_t id{};
};

TOML_REFLECT_MEMBERS(WideUnsigned, id)

struct OptionalArray {
    std::vector<std::optional<int>> values;
};

TOML_REFLECT_MEMBERS(OptionalArray, values)

/**
 * @brief 检查结果是否失败并包含预期错误片段。
 * @tparam T 结果中的成功值类型。
 * @param result 待检查的 TOML 操作结果。
 * @param expected 预期出现在错误信息中的文本。
 * @param context 当前测试场景描述。
 * @return 结果失败且错误信息包含 `expected` 时返回 `true`。
 */
template <class T>
bool expect_error(const toml::result<T>& result, std::string_view expected,
                  std::string_view context) {
    if (!result && result.error().find(expected) != std::string::npos) {
        return true;
    }
    if (result) {
        std::println("test_errors: {} unexpectedly succeeded", context);
    } else {
        std::println("test_errors: {} returned unexpected error: {}", context, result.error());
    }
    return false;
}

}  // 匿名命名空间

/**
 * @brief 执行错误处理和边界条件测试。
 * @return 全部错误断言通过时返回 0，否则返回 1。
 */
int main() {
    bool passed = true;

    passed &= expect_error(toml::deserialize<Required>("name = \"Ada\"\n"), "age",
                           "missing required field");
    passed &= expect_error(
        toml::deserialize<Required>("name = \"Ada\"\nage = \"old\"\n"),
        "integer", "scalar type mismatch");
    passed &= expect_error(toml::deserialize<Pair>("values = [1]\n"), "wrong TOML array size",
                           "short fixed-size array");
    passed &= expect_error(toml::deserialize<Pair>("values = [1, \"two\"]\n"), "integer",
                           "array element type mismatch");
    passed &= expect_error(
        toml::deserialize<Required>("name = \"Ada\"\nage = 1\nage = 2\n"),
        "duplicate", "duplicate key");
    passed &= expect_error(
        toml::deserialize<Required>("name = \"Ada\"\nage = 1\n[age]\n"),
        "conflicts", "scalar/table conflict");
    passed &= expect_error(
        toml::deserialize<Required>("name = { value = 1, }\nage = 1\n"),
        "trailing comma", "invalid inline table");
    passed &= expect_error(
        toml::deserialize<Required>("name = \"\\uD800\"\nage = 1\n"),
        "Unicode", "invalid Unicode escape");
    passed &= expect_error(toml::deserialize<DateRecord>("day = 2024-02-30\n"),
                           "calendar", "invalid calendar date");
    passed &= expect_error(toml::deserialize<DateRecord>("day = \"2024-02-29\"\n"),
                           "local date", "temporal type mismatch");
    passed &= expect_error(toml::deserialize<Required>("items = 1\n[[items]]\n"),
                           "array-table conflicts", "array-table/scalar conflict");
    passed &= expect_error(toml::deserialize<Required>("name = \"Ada\"\nage = [1\n"),
                           "unterminated", "unterminated array");
    passed &= expect_error(
        toml::deserialize<Required>("name = \"Ada\"\nage = 9223372036854775808\n"),
        "signed 64-bit", "integer outside TOML range");
    passed &= expect_error(toml::deserialize<ConstRecord>("value = 2\n"), "not assignable",
                           "const reflected member");

    passed &= expect_error(toml::serialize(42), "reflected struct/table",
                           "non-reflected serialization root");
    passed &= expect_error(toml::serialize(WideUnsigned{std::numeric_limits<std::uint64_t>::max()}),
                           "signed 64-bit", "wide unsigned integer serialization");
    passed &= expect_error(toml::serialize(OptionalArray{{1, std::nullopt, 3}}), "cannot contain null",
                           "optional null inside TOML array");
    passed &= expect_error(toml::serialize(DateRecord{{2024, 2, 30}}), "invalid toml::date",
                           "invalid temporal serialization value");

    return passed ? 0 : 1;
}
