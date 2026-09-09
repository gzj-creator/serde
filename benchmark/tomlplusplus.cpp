import std;

#include "common.hpp"
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
    try {
        auto options = benchmark::parse_options(argc, argv, "benchmark/data/config.toml");
        const auto input = benchmark::read_file(options.input_path);
        auto checked = toml::parse(input);
        static_cast<void>(checked);
        const auto measured = benchmark::measure("toml++", input, options, [&]() {
            const auto parsed = toml::parse(input);
            return checksum(parsed);
        });
        benchmark::print_result(measured, options.csv);
        return 0;
    } catch (const std::exception& error) {
        std::println(stderr, "{}", error.what());
        return 1;
    }
}
