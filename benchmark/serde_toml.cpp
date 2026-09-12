import std;
import toml;

#include "common.hpp"
#include "../src/reflect/reflect_macros.hpp"

namespace {

struct owner {
    std::string name;
    std::string team;
};
#define BENCHMARK_OWNER_FIELDS(X) X(name) X(team)
REFLECT_FIELDS(owner, BENCHMARK_OWNER_FIELDS)
#undef BENCHMARK_OWNER_FIELDS

struct server {
    std::string host;
    int port{};
    std::vector<std::string> zones;
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

}  // namespace

int main(int argc, char** argv) {
    auto options_result = benchmark::parseOptions(argc, argv, "benchmark/data/config.toml");
    if (!options_result) { std::println(stderr, "{}", options_result.error()); return 2; }
    auto options = std::move(*options_result);
    auto input_result = benchmark::readFile(options.input_path);
    if (!input_result) { std::println(stderr, "{}", input_result.error()); return 2; }
    const auto& input = *input_result;
    const auto checked = toml::deserialize<document>(input);
    if (!checked) {
        std::println(stderr, "serde TOML fixture rejected: {}", checked.error());
        return 1;
    }
    const auto measured = [&]() -> std::expected<benchmark::result, std::string> {
        if (options.selected_phase == benchmark::phase::end_to_end) {
            return benchmark::measure("serde-toml", input, options, [&]() -> std::expected<std::uint64_t, std::string> {
                auto parsed = toml::deserialize<document>(input);
                if (!parsed) return std::unexpected(parsed.error());
                return checksum(*parsed);
            });
        }

        auto document_node = toml::detail::parseDocument(input, {});
        if (!document_node) return std::unexpected(document_node.error());
        if (options.selected_phase == benchmark::phase::parse_only) {
            return benchmark::measure("serde-toml-parse-only", input, options, [&]() -> std::expected<std::uint64_t, std::string> {
                auto parsed = toml::detail::parseDocument(input, {});
                if (!parsed) return std::unexpected(parsed.error());
                return benchmark::nodeChecksum(*parsed);
            });
        }
        return benchmark::measure("serde-toml-decode-only", input, options, [&]() -> std::expected<std::uint64_t, std::string> {
            auto parsed = toml::detail::decodeValue<document>(*document_node, {}, {});
            if (!parsed) return std::unexpected(parsed.error());
            return checksum(*parsed);
        });
    }();
    if (!measured) { std::println(stderr, "{}", measured.error()); return 1; }
    benchmark::printResult(*measured, options.csv);
    return 0;
}
