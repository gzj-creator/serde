import std;
import json;

#include "common.hpp"
#include "../src/reflect/reflect_macros.hpp"

namespace {

#define WIDE_FIELDS(X) \
    X(f00) X(f01) X(f02) X(f03) X(f04) X(f05) X(f06) X(f07) \
    X(f08) X(f09) X(f10) X(f11) X(f12) X(f13) X(f14) X(f15) \
    X(f16) X(f17) X(f18) X(f19) X(f20) X(f21) X(f22) X(f23) \
    X(f24) X(f25) X(f26) X(f27) X(f28) X(f29) X(f30) X(f31) \
    X(f32) X(f33) X(f34) X(f35) X(f36) X(f37) X(f38) X(f39) \
    X(f40) X(f41) X(f42) X(f43) X(f44) X(f45) X(f46) X(f47) \
    X(f48) X(f49) X(f50) X(f51) X(f52) X(f53) X(f54) X(f55) \
    X(f56) X(f57) X(f58) X(f59) X(f60) X(f61) X(f62) X(f63)
#define DECLARE_FIELD(name) int name{};
struct document { WIDE_FIELDS(DECLARE_FIELD) };
#undef DECLARE_FIELD
REFLECT_FIELDS(document, WIDE_FIELDS)
#undef WIDE_FIELDS

std::uint64_t checksum(const document& value) {
    std::uint64_t sum = 0;
    reflect::for_each_field(value, [&](const auto& field, const auto& object) {
        sum += field.get(object);
    });
    return sum;
}

}  // namespace

int main(int argc, char** argv) {
    auto parsed_options = benchmark::parseOptions(argc, argv, "");
    if (!parsed_options) { std::println(stderr, "{}", parsed_options.error()); return 2; }
    const auto& options = *parsed_options;
    std::string input = "{";
    for (int index = 63; index >= 0; --index) {
        if (index != 63) input += ',';
        input += std::format("\"f{:02}\":{}", index, index);
    }
    input += '}';
    if (!options.input_path.empty()) {
        auto file = benchmark::readFile(options.input_path);
        if (!file) { std::println(stderr, "{}", file.error()); return 2; }
        input = std::move(*file);
    }
    auto parsed = json::parse(input);
    if (!parsed) { std::println(stderr, "{}", parsed.error()); return 1; }
    auto checked = json::decode<document>(*parsed);
    if (!checked) { std::println(stderr, "{}", checked.error()); return 1; }
    const auto measured = [&]() {
        if (options.selected_phase == benchmark::phase::parse_only) {
            return benchmark::measure("serde-json-wide-parse-only", input, options,
                [&]() -> json::result<std::uint64_t> {
                    auto value = json::parse(input);
                    if (!value) return std::unexpected(value.error());
                    return value->size();
                });
        }
        if (options.selected_phase == benchmark::phase::decode_only) {
            return benchmark::measure("serde-json-wide-decode-only", input, options,
                [&]() -> json::result<std::uint64_t> {
                    auto value = json::decode<document>(*parsed);
                    if (!value) return std::unexpected(value.error());
                    return checksum(*value);
                });
        }
        return benchmark::measure("serde-json-wide", input, options,
            [&]() -> json::result<std::uint64_t> {
                auto value = json::deserialize<document>(input);
                if (!value) return std::unexpected(value.error());
                return checksum(*value);
            });
    }();
    if (!measured) { std::println(stderr, "{}", measured.error()); return 1; }
    benchmark::printResult(*measured, options.csv);
}
