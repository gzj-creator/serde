#include <cassert>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <serde/reflect/reflect.hpp>
#include <serde/reflect/reflect_macros.hpp>
#include <serde/toml/toml.hpp>
#include <serde/json/json.hpp>

namespace {

struct server {
    std::string host;
    int port{};
    std::vector<std::string> zones;
    bool operator==(const server&) const = default;
};

#define SERVER_FIELDS(X) X(host) X(port) X(zones)
REFLECT_FIELDS(server, SERVER_FIELDS)
#undef SERVER_FIELDS

struct document {
    std::string application;
    bool enabled{};
    double ratio{};
    std::optional<std::string> note;
    server primary;
    bool operator==(const document&) const = default;
};

#define DOCUMENT_FIELDS(X) X(application) X(enabled) X(ratio) X(note) X(primary)
REFLECT_FIELDS(document, DOCUMENT_FIELDS)
#undef DOCUMENT_FIELDS

}  // namespace

int main() {
    document value{"demo", true, 0.5, std::nullopt, {"example.org", 8080, {"a", "b"}}};

    const auto toml_text = toml::serialize(value);
    assert(toml_text);
    const auto toml_back = toml::deserialize<document>(*toml_text);
    assert(toml_back);
    assert(*toml_back == value);

    const auto json_text = json::serialize(value);
    assert(json_text);
    const auto json_back = json::deserialize<document>(*json_text);
    assert(json_back);
    assert(*json_back == value);

    const auto parsed = json::parse(R"({"name": "demo", "tags": [1, 2, 3]})");
    assert(parsed);
    assert(parsed->is_object());
    assert(parsed->at("tags").is_array());
    assert(parsed->at("tags").size() == 3);
    return 0;
}