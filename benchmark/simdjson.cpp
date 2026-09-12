import std;

#include "common.hpp"
#define SIMDJSON_EXCEPTIONS 0
#define nssv_CONFIG_NO_EXCEPTIONS 1
#include "../third_party/simdjson/simdjson.h"

namespace {

struct owner {
    std::string name;
    std::string team;
};

struct server {
    std::string host;
    int port{};
    std::vector<std::string> zones;
};

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

simdjson::dom::element object_field(simdjson::dom::element object, std::string_view key) {
    return object[key].value_unsafe();
}

std::string_view string_value(simdjson::dom::element value) {
    return value.get_string().value_unsafe();
}

std::int64_t integer_value(simdjson::dom::element value) {
    return value.get_int64().value_unsafe();
}

bool boolean_value(simdjson::dom::element value) {
    return value.get_bool().value_unsafe();
}

double floating_value(simdjson::dom::element value) {
    return value.get_double().value_unsafe();
}

simdjson::dom::array array_value(simdjson::dom::element value) {
    return value.get_array().value_unsafe();
}

simdjson::dom::object object_value(simdjson::dom::element value) {
    return value.get_object().value_unsafe();
}

std::uint64_t checksum(simdjson::dom::element root) {
    std::uint64_t result = string_value(object_field(root, "application")).size();
    result += string_value(object_field(root, "message")).size();
    result += integer_value(object_field(root, "retries"));
    result += array_value(object_field(root, "tags")).size();
    result += object_value(object_field(root, "limits")).size();
    result += array_value(object_field(root, "servers")).size();
    const auto owner_value = object_field(root, "owner");
    result += string_value(object_field(owner_value, "name")).size();
    result += string_value(object_field(owner_value, "team")).size();
    for (const auto item : array_value(object_field(root, "servers"))) {
        result += string_value(object_field(item, "host")).size() + integer_value(object_field(item, "port"));
    }
    return result;
}

std::uint64_t checksum(const document& value) {
    std::uint64_t result = value.application.size() + value.message.size();
    result += value.retries + value.tags.size() + value.limits.size() + value.servers.size();
    result += value.owner_info.name.size() + value.owner_info.team.size();
    for (const auto& item : value.servers) result += item.host.size() + item.port;
    return result;
}

document domToDocument(simdjson::dom::element root) {
    document output;
    output.application = std::string(string_value(object_field(root, "application")));
    output.enabled = boolean_value(object_field(root, "enabled"));
    output.retries = static_cast<int>(integer_value(object_field(root, "retries")));
    output.ratio = floating_value(object_field(root, "ratio"));

    const auto tags = array_value(object_field(root, "tags"));
    output.tags.reserve(tags.size());
    for (const auto value : tags) output.tags.emplace_back(std::string(string_value(value)));

    for (const auto field : object_value(object_field(root, "limits"))) {
        output.limits.emplace(std::string(field.key), static_cast<int>(integer_value(field.value)));
    }

    const auto owner_value = object_field(root, "owner");
    output.owner_info.name = std::string(string_value(object_field(owner_value, "name")));
    output.owner_info.team = std::string(string_value(object_field(owner_value, "team")));

    const auto servers = array_value(object_field(root, "servers"));
    output.servers.reserve(servers.size());
    for (const auto value : servers) {
        server item;
        item.host = std::string(string_value(object_field(value, "host")));
        item.port = static_cast<int>(integer_value(object_field(value, "port")));
        const auto zones = array_value(object_field(value, "zones"));
        item.zones.reserve(zones.size());
        for (const auto zone : zones) {
            item.zones.emplace_back(std::string(string_value(zone)));
        }
        output.servers.emplace_back(std::move(item));
    }

    output.message = std::string(string_value(object_field(root, "message")));
    return output;
}

}  // namespace

int main(int argc, char** argv) {
    auto options_result = benchmark::parseOptions(argc, argv, "benchmark/data/config.json", false, true);
    if (!options_result) { std::println(stderr, "{}", options_result.error()); return 2; }
    auto options = std::move(*options_result);
    auto input_result = benchmark::readFile(options.input_path);
    if (!input_result) { std::println(stderr, "{}", input_result.error()); return 2; }
    auto input = std::move(*input_result);
    simdjson::dom::parser parser;
    const auto original_bytes = input.size();
    const auto padded = simdjson::pad(input);
    auto checked = parser.parse(padded);
    if (checked.error()) {
        std::println(stderr, "simdjson fixture rejected: {}", simdjson::error_message(checked.error()));
        return 1;
    }
    const auto checked_root = checked.value_unsafe();
    const auto measured = [&]() {
        const auto measured_input = std::string_view(input.data(), original_bytes);
        if (options.selected_phase == benchmark::phase::parse_only) {
            return benchmark::measure("simdjson-parse-only", measured_input, options, [&]() -> std::expected<std::uint64_t, std::string> {
                auto parsed = parser.parse(padded);
                if (parsed.error()) return std::unexpected(std::string(simdjson::error_message(parsed.error())));
                return parsed.value_unsafe().is_object() ? std::uint64_t{1} : std::uint64_t{0};
            });
        }
        if (options.selected_phase == benchmark::phase::dom_walk_only) {
            return benchmark::measure("simdjson-dom-walk", measured_input, options, [&]() {
                return checksum(checked_root);
            });
        }
        if (options.selected_phase == benchmark::phase::dom_to_document ||
            options.selected_phase == benchmark::phase::decode_only) {
            return benchmark::measure("simdjson-dom-to-document", measured_input, options, [&]() {
                return checksum(domToDocument(checked_root));
            });
        }
        return benchmark::measure("simdjson-dom", measured_input, options, [&]() -> std::expected<std::uint64_t, std::string> {
            auto parsed = parser.parse(padded);
            if (parsed.error()) return std::unexpected(std::string(simdjson::error_message(parsed.error())));
            return checksum(parsed.value_unsafe());
        });
    }();
    if (!measured) { std::println(stderr, "{}", measured.error()); return 1; }
    benchmark::printResult(*measured, options.csv);
    return 0;
}
