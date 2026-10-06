#ifdef SERDE_BENCHMARK_HEADERS
#include <atomic>
#include <serde/reflect/reflect.hpp>
#else
import std;
import reflect;
#endif

#include "common.hpp"
#include "../src/reflect/reflect_macros.hpp"

namespace {

enum class mode { active, disabled, pending };
constexpr auto reflect_enum(std::type_identity<mode>) {
    return reflect::enum_descriptor<mode, 3>{reflect::enum_encoding::string,
        {{{mode::active, "active"}, {mode::disabled, "disabled"}, {mode::pending, "pending"}}}};
}

struct document {
    std::uint64_t id = 9007199254740993ULL;
    double ratio = 0.5;
    std::string name = "benchmark-user";
    std::optional<int> retries = 3;
    std::vector<int> values{1, 2, 3};
    mode state = mode::active;
};
#define CONTRACT_FIELDS(X) \
    X(id, "id", (reflect::field_options<std::uint64_t>{.minimum = 9007199254740993ULL})) \
    X(ratio, "ratio", (reflect::field_options<double>{.minimum = 0.0, .maximum = 1.0})) \
    X(name, "name", (reflect::field_options<std::string>{.min_length = 1, .max_length = 40})) \
    X(retries, "retries", (reflect::field_options<std::optional<int>>{.minimum = 1, .maximum = 5})) \
    X(values, "values", (reflect::field_options<std::vector<int>>{.min_items = 1, .max_items = 8})) \
    X(state)
REFLECT_FIELDS(document, CONTRACT_FIELDS)
#undef CONTRACT_FIELDS

} // namespace

int main(int argc, char** argv) {
    const auto options = benchmark::parseOptions(argc, argv, "");
    if (!options) { std::println(stderr, "{}", options.error()); return 2; }
    document value;
    // Keep the data observable between iterations so validation cannot be hoisted.
    const auto validated = benchmark::measure("serde-contract-fields", value.name, *options,
        [&]() -> std::expected<std::uint64_t, std::string> {
            std::atomic_signal_fence(std::memory_order_acq_rel);
            std::expected<void, std::string> checked;
            reflect::for_each_field(value, [&](const auto& field, const auto& object) {
                if (checked) checked = reflect::validate_field(field, field.get(object));
            });
            if (!checked) return std::unexpected(std::move(checked.error()));
            return value.id + value.values.size();
        });
    if (!validated) { std::println(stderr, "{}", validated.error()); return 1; }
    benchmark::printResult(*validated, options->csv);

    std::size_t iteration = 0;
    const auto mapped = benchmark::measure("serde-contract-enum-roundtrip", "active/disabled/pending", *options,
        [&]() -> std::expected<std::uint64_t, std::string> {
            const auto input = static_cast<mode>(iteration++ % 3);
            auto name = reflect::enum_to_string(input);
            if (!name) return std::unexpected(std::move(name.error()));
            auto back = reflect::enum_from_string<mode>(*name);
            if (!back) return std::unexpected(std::move(back.error()));
            if (*back != input) return std::unexpected(std::string("enum roundtrip differs"));
            return name->size() + static_cast<std::uint64_t>(*back);
        });
    if (!mapped) { std::println(stderr, "{}", mapped.error()); return 1; }
    benchmark::printResult(*mapped, options->csv);
    return 0;
}
