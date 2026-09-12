import std;
import json;

#include "common.hpp"
#include "../src/reflect/reflect_macros.hpp"

namespace {

struct owner {
    std::string name;
    std::string team;
    bool operator==(const owner&) const = default;
};
#define BENCHMARK_OWNER_FIELDS(X) X(name) X(team)
REFLECT_FIELDS(owner, BENCHMARK_OWNER_FIELDS)
#undef BENCHMARK_OWNER_FIELDS

struct server {
    std::string host;
    int port{};
    std::vector<std::string> zones;
    bool operator==(const server&) const = default;
};
#define BENCHMARK_SERVER_FIELDS(X) X(host) X(port) X(zones)
REFLECT_FIELDS(server, BENCHMARK_SERVER_FIELDS)
#undef BENCHMARK_SERVER_FIELDS

struct document {
    std::string application;
    bool enabled{};
    int retries{};
    double ratio{};
    std::vector<std::string> tags;
    std::map<std::string, int> limits;
    owner owner_info;
    std::vector<server> servers;
    std::string message;
    bool operator==(const document&) const = default;
};
#define BENCHMARK_DOCUMENT_FIELDS(X) \
    X(application) X(enabled) X(retries) X(ratio) X(tags) X(limits) \
    X(owner_info, "owner") X(servers) X(message)
REFLECT_FIELDS(document, BENCHMARK_DOCUMENT_FIELDS)
#undef BENCHMARK_DOCUMENT_FIELDS

std::uint64_t checksum(const document& value) {
    std::uint64_t result = value.application.size() + value.message.size();
    result += value.retries + value.tags.size() + value.limits.size() + value.servers.size();
    result += value.owner_info.name.size() + value.owner_info.team.size();
    for (const auto& item : value.servers) result += item.host.size() + item.port;
    return result;
}

std::expected<benchmark::result, std::string> measureSupplementary(
    std::string_view input, const benchmark::options& options, const document& checked) {
    json::Parser parser;
    auto parsed = parser.parse(input);
    if (!parsed) return std::unexpected(parsed.error());
    auto decoded = json::decode<document>(*parsed);
    if (!decoded) return std::unexpected(decoded.error());
    if (*decoded != checked) return std::unexpected("JSON profiling track differs from public decode");

    std::string name = "serde-json";
    name += options.reuse_context ? "-reuse" : "-fresh";
    if (options.selected_phase == benchmark::phase::decode_only) {
        return benchmark::measure(name + "-decode-only", input, options, [&]() -> std::expected<std::uint64_t, std::string> {
            auto value = json::decode<document>(*parsed);
            if (!value) return std::unexpected(value.error());
            return checksum(*value);
        });
    }
    auto observe = [&](const json::Json& value) -> std::expected<std::uint64_t, std::string> {
        if (options.selected_phase == benchmark::phase::parse_only) {
            return value.is_object() ? std::uint64_t{1} : std::uint64_t{0};
        }
        auto result = json::decode<document>(value);
        if (!result) return std::unexpected(result.error());
        return checksum(*result);
    };
    if (options.selected_phase == benchmark::phase::parse_only) name += "-parse-only";
    if (options.reuse_context) {
        return benchmark::measure(name, input, options, [&]() -> std::expected<std::uint64_t, std::string> {
            auto value = parser.parse(input);
            if (!value) return std::unexpected(value.error());
            return observe(*value);
        });
    }
    return benchmark::measure(name, input, options, [&]() -> std::expected<std::uint64_t, std::string> {
        auto value = json::parse(input);
        if (!value) return std::unexpected(value.error());
        return observe(*value);
    });
}

}  // namespace

int main(int argc, char** argv) {
    auto options_result = benchmark::parseOptions(argc, argv, "benchmark/data/config.json", true);
    if (!options_result) { std::println(stderr, "{}", options_result.error()); return 2; }
    auto options = std::move(*options_result);
    auto input_result = benchmark::readFile(options.input_path);
    if (!input_result) { std::println(stderr, "{}", input_result.error()); return 2; }
    const auto& input = *input_result;
    const auto checked = json::deserialize<document>(input);
    if (!checked) {
        std::println(stderr, "serde JSON fixture rejected: {}", checked.error());
        return 1;
    }
    const auto measured = [&]() -> std::expected<benchmark::result, std::string> {
        if (options.reuse_context) {
            return measureSupplementary(input, options, *checked);
        }
        if (options.selected_phase == benchmark::phase::end_to_end) {
            return benchmark::measure("serde-json", input, options, [&]() -> std::expected<std::uint64_t, std::string> {
                auto parsed = json::deserialize<document>(input);
                if (!parsed) return std::unexpected(parsed.error());
                return checksum(*parsed);
            });
        }

        if (options.selected_phase == benchmark::phase::parse_only) {
            return benchmark::measure("serde-json-parse-only", input, options, [&]() -> std::expected<std::uint64_t, std::string> {
                auto parsed = json::parse(input);
                if (!parsed) return std::unexpected(parsed.error());
                return parsed->is_object() ? std::uint64_t{1} : std::uint64_t{0};
            });
        }
        auto parsed_document = json::parse(input);
        if (!parsed_document) return std::unexpected(parsed_document.error());
        return benchmark::measure("serde-json-decode-only", input, options, [&]() -> std::expected<std::uint64_t, std::string> {
            auto parsed = json::decode<document>(*parsed_document);
            if (!parsed) return std::unexpected(parsed.error());
            return checksum(*parsed);
        });
    }();
    if (!measured) { std::println(stderr, "{}", measured.error()); return 1; }
    benchmark::printResult(*measured, options.csv);
    return 0;
}
