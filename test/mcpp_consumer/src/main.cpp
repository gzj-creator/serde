#include <expected>
#include <string>
#include <string_view>
#include <third_party/simdjson/simdjson.h>

import json;
import toml;

#include <serde/reflect/reflect_macros.hpp>

struct Server {
    std::string host;
    int port{};
    bool operator==(const Server&) const = default;
};
#define SERVER_FIELDS(X) X(host) X(port)
REFLECT_FIELDS(Server, SERVER_FIELDS)
#undef SERVER_FIELDS

struct Config {
    Server server;
};

int main() {
    const Server original{"localhost", 8080};
    const auto config_json = json::serialize(Config{original});
    const auto config_toml = toml::serialize(Config{original});
    if (!config_json || !config_toml) return 1;
    const auto json_config = json::deserialize<Config>(*config_json);
    const auto toml_config = toml::deserialize<Config>(*config_toml);
    if (!json_config || !toml_config || json_config->server != original ||
        toml_config->server != original) return 1;

    const auto json_text = json::serialize(original);
    const auto toml_text = toml::serialize(original);
    if (!json_text || !toml_text) return 1;
    const auto from_json = json::deserialize<Server>(*json_text);
    const auto from_toml = toml::deserialize<Server>(*toml_text);
    if (!from_json || !from_toml || *from_json != original || *from_toml != original) return 1;
    if (json::deserialize<Server>(R"({"host":"localhost"})")) return 1;
    std::string streamed;
    json::stream::StreamWriter writer([&](std::string_view part) -> json::result<void> {
        streamed.append(part);
        return {};
    });
    if (!json::stream::serialize(writer, original) || !writer.finish()) return 1;
    const auto from_stream = json::deserialize<Server>(streamed);
    if (!from_stream || *from_stream != original) return 1;
    simdjson::dom::parser parser;
    simdjson::dom::element document;
    return parser.parse(*json_text).get(document) == simdjson::SUCCESS ? 0 : 1;
}
