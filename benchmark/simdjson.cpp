import std;

#include "common.hpp"
#include "../../galay/thirdparty/simdjson/simdjson.h"
#include "../../galay/thirdparty/simdjson/simdjson.cpp"

namespace {

std::uint64_t checksum(simdjson::dom::element root) {
    std::uint64_t result = std::string_view(root["application"]).size();
    result += std::string_view(root["message"]).size();
    result += std::int64_t(root["retries"]);
    result += simdjson::dom::array(root["tags"]).size();
    result += simdjson::dom::object(root["limits"]).size();
    result += simdjson::dom::array(root["servers"]).size();
    result += std::string_view(root["owner"]["name"]).size();
    result += std::string_view(root["owner"]["team"]).size();
    for (const auto item : simdjson::dom::array(root["servers"])) {
        result += std::string_view(item["host"]).size() + std::int64_t(item["port"]);
    }
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        auto options = benchmark::parse_options(argc, argv, "benchmark/data/config.json");
        auto input = benchmark::read_file(options.input_path);
        simdjson::dom::parser parser;
        const auto original_bytes = input.size();
        const auto padded = simdjson::pad(input);
        auto checked = parser.parse(padded);
        if (checked.error()) {
            std::println(stderr, "simdjson fixture rejected: {}", simdjson::error_message(checked.error()));
            return 1;
        }
        const auto measured = benchmark::measure("simdjson-dom", std::string_view(input.data(), original_bytes), options, [&]() {
            auto parsed = parser.parse(padded);
            if (parsed.error()) throw std::runtime_error(simdjson::error_message(parsed.error()));
            return checksum(parsed.value());
        });
        benchmark::print_result(measured, options.csv);
        return 0;
    } catch (const std::exception& error) {
        std::println(stderr, "{}", error.what());
        return 1;
    }
}
