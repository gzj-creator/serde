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

benchmark::result measure_supplementary(std::string_view input,
                                        const benchmark::options& options,
                                        const document& checked) {
    json::detail::parser_context context;
    auto parsed = json::detail::parseDocumentReusable(
        input, {}, context, options.structural_index);
    if (!parsed) throw std::runtime_error(parsed.error());
    auto decoded = json::detail::decodeValue<document>(**parsed, {}, {});
    if (!decoded) throw std::runtime_error(decoded.error());
    if (*decoded != checked) throw std::runtime_error("JSON profiling track differs from public decode");

    std::string name = options.structural_index ? "serde-json-structural" : "serde-json-bytewise";
    name += options.reuse_context ? "-reuse" : "-fresh";
    if (options.selected_phase == benchmark::phase::decode_only) {
        return benchmark::measure(name + "-decode-only", input, options, [&]() {
            auto value = json::detail::decodeValue<document>(context.root, {}, {});
            if (!value) throw std::runtime_error(value.error());
            return checksum(*value);
        });
    }
    if (options.selected_phase == benchmark::phase::index_only) {
        auto index = [&](auto& indexes) {
            json::detail::buildStructuralIndexes(input, indexes);
            return indexes.size();
        };
        if (options.reuse_context) {
            return benchmark::measure(name + "-index-only", input, options, [&]() {
                return index(context.structural_indexes);
            });
        }
        return benchmark::measure(name + "-index-only", input, options, [&]() {
            std::vector<std::size_t> indexes;
            return index(indexes);
        });
    }
    auto observe = [&](const json::detail::node& value) {
        if (options.selected_phase == benchmark::phase::parse_only ||
            options.selected_phase == benchmark::phase::stage2_only) {
            return benchmark::node_checksum(value);
        }
        auto result = json::detail::decodeValue<document>(value, {}, {});
        if (!result) throw std::runtime_error(result.error());
        return checksum(*result);
    };
    if (options.selected_phase == benchmark::phase::stage2_only) {
        return benchmark::measure(name + "-stage2-only", input, options, [&]() {
            const json::ParseOptions limits;
            json::detail::BasicParser<true> parser{
                input, limits, &context, context.structural_indexes};
            if (options.reuse_context) {
                auto value = parser.parse_reusable();
                if (!value) throw std::runtime_error(value.error());
                return observe(**value);
            }
            auto value = parser.parse();
            if (!value) throw std::runtime_error(value.error());
            return observe(*value);
        });
    }
    if (options.selected_phase == benchmark::phase::parse_only) name += "-parse-only";
    if (options.reuse_context) {
        return benchmark::measure(name, input, options, [&]() {
            auto value = json::detail::parseDocumentReusable(
                input, {}, context, options.structural_index);
            if (!value) throw std::runtime_error(value.error());
            return observe(**value);
        });
    }
    return benchmark::measure(name, input, options, [&]() {
        json::detail::parser_context fresh;
        auto value = json::detail::parseDocumentIndexed(input, {}, fresh);
        if (!value) throw std::runtime_error(value.error());
        return observe(*value);
    });
}

}  // namespace

int main(int argc, char** argv) {
    try {
        auto options = benchmark::parse_options(argc, argv, "benchmark/data/config.json", true);
        const auto input = benchmark::read_file(options.input_path);
        const auto checked = json::deserialize<document>(input);
        if (!checked) {
            std::println(stderr, "serde JSON fixture rejected: {}", checked.error());
            return 1;
        }
        const auto measured = [&]() {
            if (options.reuse_context || options.structural_index) {
                return measure_supplementary(input, options, *checked);
            }
            if (options.selected_phase == benchmark::phase::end_to_end) {
                return benchmark::measure("serde-json", input, options, [&]() {
                    auto parsed = json::deserialize<document>(input);
                    if (!parsed) throw std::runtime_error(parsed.error());
                    return checksum(*parsed);
                });
            }

            auto document_node = json::detail::parseDocument(input, {});
            if (!document_node) throw std::runtime_error(document_node.error());
            if (options.selected_phase == benchmark::phase::parse_only) {
                return benchmark::measure("serde-json-parse-only", input, options, [&]() {
                    auto parsed = json::detail::parseDocument(input, {});
                    if (!parsed) throw std::runtime_error(parsed.error());
                    return benchmark::node_checksum(*parsed);
                });
            }
            return benchmark::measure("serde-json-decode-only", input, options, [&]() {
                auto parsed = json::detail::decodeValue<document>(*document_node, {}, {});
                if (!parsed) throw std::runtime_error(parsed.error());
                return checksum(*parsed);
            });
        }();
        benchmark::print_result(measured, options.csv);
        return 0;
    } catch (const std::exception& error) {
        std::println(stderr, "{}", error.what());
        return 1;
    }
}
