import std;

#include "common.hpp"
#define TOML_EXCEPTIONS 0
#include "../../tomlplusplus-3.4.0/toml.hpp"

namespace {

std::uint64_t checksum(const toml::table& root) {
    std::uint64_t result = root["application"].value_or<std::string_view>("").size();
    result += root["message"].value_or<std::string_view>("").size();
    result += root["retries"].value_or(0);
    result += root["tags"].as_array()->size();
    result += root["limits"].as_table()->size();
    result += root["servers"].as_array()->size();
    const auto* owner = root["owner"].as_table();
    result += owner->get("name")->value_or<std::string_view>("").size();
    result += owner->get("team")->value_or<std::string_view>("").size();
    for (const auto& item : *root["servers"].as_array()) {
        const auto* server = item.as_table();
        result += server->get("host")->value_or<std::string_view>("").size();
        result += server->get("port")->value_or(0);
    }
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
    auto checked = toml::parse(input);
    if (!checked) { std::println(stderr, "{}", checked.error().description()); return 1; }
    const auto measured = benchmark::measure("toml++", input, options, [&]() -> std::expected<std::uint64_t, std::string> {
        const auto parsed = toml::parse(input);
        if (!parsed) return std::unexpected(std::string(parsed.error().description()));
        return checksum(parsed.table());
    });
    if (!measured) { std::println(stderr, "{}", measured.error()); return 1; }
    benchmark::printResult(*measured, options.csv);
    return 0;
}
