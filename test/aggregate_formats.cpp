#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include <serde/json/json.hpp>
#include <serde/toml/toml.hpp>

namespace aggregate_formats {

struct User {
    int id{};
    std::string name;
    bool operator==(const User&) const = default;
};

struct Empty {};

struct OptionalDefault {
    std::optional<int> retries = 3;
};

struct Profile {
    User profile;
};

struct Large {
    std::uint64_t number = std::numeric_limits<std::uint64_t>::max();
};

struct Config {
    User profile;
    std::optional<int> retries = 3;
    std::optional<User> owner;
    std::vector<User> users;
    std::array<User, 1> fixed;
    std::vector<User> empty;
    std::map<std::string, User> by_name;
    toml::InlineTable<User> compact;
    toml::date created;
};

struct Wide {
    int field00, field01, field02, field03, field04, field05, field06, field07;
    int field08, field09, field10, field11, field12, field13, field14, field15;
    bool operator==(const Wide&) const = default;
};

struct Response {
    int id{};
    std::string internal_note = "private";
};

constexpr auto reflect_fields(std::type_identity<Response>) {
    return std::tuple{reflect::make_field("userId", &Response::id,
                                         reflect::field_options<int>{.minimum = 1})};
}

struct Envelope {
    Response response;
};

struct ProtocolItems {
    std::vector<std::optional<User>> items;
};

constexpr auto reflect_fields(std::type_identity<ProtocolItems>) {
    return std::tuple{reflect::make_field("items", &ProtocolItems::items)};
}

struct WireMessage {
    User user;
};

std::array<User, 1> to_wire(const WireMessage& message) {
    return {message.user};
}

constexpr std::type_identity<std::array<User, 1>> wire_type(std::type_identity<WireMessage>) {
    return {};
}

WireMessage from_wire(std::type_identity<WireMessage>, std::array<User, 1> wire) {
    return {std::move(wire.front())};
}

static_assert(!reflect::Reflectable<User>);
static_assert(!reflect::StaticReflectable<User>);
static_assert(reflect::StaticReflectable<Response>);

void check_basic_roundtrip() {
    const User user{42, "Alice"};
    auto json_text = json::serialize(user);
    auto toml_text = toml::serialize(user);
    assert(json_text && *json_text == R"({"id":42,"name":"Alice"})");
    assert(toml_text && *toml_text == "id = 42\nname = \"Alice\"\n");
    assert(json::deserialize<User>(*json_text) == user);
    assert(toml::deserialize<User>(*toml_text) == user);
    assert(json::deserialize<User>(R"({"name":"Alice","id":42})") == user);
    assert(toml::deserialize<User>("name = 'Alice'\nid = 42\n") == user);

    auto document = json::parse(*json_text);
    assert(document && json::decode<User>(*document) == user);
    assert(json::serialize(Empty{}) == "{}");
    assert(toml::serialize(Empty{}) == "");
    assert(json::deserialize<Empty>("{}"));
    assert(toml::deserialize<Empty>(""));
}

void check_nested_roundtrip() {
    const User user{7, "demo"};
    const Config config{
        .profile = user,
        .retries = std::nullopt,
        .owner = user,
        .users = {user},
        .fixed = {user},
        .empty = {},
        .by_name = {{"demo", user}},
        .compact = {user},
        .created = {2026, 11, 18},
    };
    auto json_text = json::serialize(config);
    auto toml_text = toml::serialize(config);
    assert(json_text && json_text->contains(R"("retries":null)"));
    assert(toml_text && !toml_text->contains("retries"));
    assert(toml_text->contains("[profile]\n"));
    assert(toml_text->contains("[[users]]\n"));
    assert(toml_text->contains("[[fixed]]\n"));
    assert(toml_text->contains("compact = {"));
    assert(toml_text->contains("empty = []\n"));
    assert(toml_text->contains("created = 2026-11-18\n"));

    auto json_back = json::deserialize<Config>(*json_text, {.unknown_fields = json::UnknownFieldPolicy::reject});
    auto toml_back = toml::deserialize<Config>(*toml_text, {.unknown_fields = toml::UnknownFieldPolicy::reject});
    assert(json_back && !json_back->retries);
    assert(toml_back && toml_back->retries == 3);
    for (const auto* decoded : {&*json_back, &*toml_back}) {
        assert(decoded->profile == user && decoded->owner == user);
        assert(decoded->users == config.users && decoded->fixed == config.fixed);
        assert(decoded->empty.empty() && decoded->by_name == config.by_name);
        assert(decoded->compact.value == user && decoded->created == config.created);
    }

    auto json_default = json::deserialize<OptionalDefault>("{}");
    assert(json_default && json_default->retries == 3);

    const std::map<std::string, std::vector<std::optional<User>>> optional_items{{"items", {std::nullopt}}};
    assert(json::serialize(optional_items) == R"({"items":[null]})");
    assert(!toml::serialize(optional_items));
}

void check_errors_and_index() {
    const json::ParseOptions strict_json{.unknown_fields = json::UnknownFieldPolicy::reject};
    const toml::ParseOptions strict_toml{.unknown_fields = toml::UnknownFieldPolicy::reject};
    assert(!json::deserialize<User>(R"({"name":"demo"})"));
    assert(!toml::deserialize<User>("name = 'demo'\n"));
    assert(!json::deserialize<User>(R"({"id":1,"name":"demo","extra":true})", strict_json));
    assert(!toml::deserialize<User>("id = 1\nname = 'demo'\nextra = true\n", strict_toml));
    assert(!json::deserialize<User>(R"({"id":1,"id":2,"name":"demo"})"));
    assert(!toml::deserialize<User>("id = 1\nid = 2\nname = 'demo'\n"));
    assert(!json::deserialize<Empty>("[]"));
    assert(!json::deserialize<User>(R"({"id":1,"name":"demo"})", {.max_object_members = 1}));
    assert(!toml::deserialize<User>("id = 1\nname = 'demo'\n", {.max_input_bytes = 1}));

    auto json_error = json::deserialize<Profile>(R"({"profile":{"id":"wrong","name":"demo"}})");
    auto toml_error = toml::deserialize<Profile>("[profile]\nid = 'wrong'\nname = 'demo'\n");
    assert(!json_error && json_error.error().contains("value.profile.id"));
    assert(!toml_error && toml_error.error().contains("value.profile.id"));

    assert(json::serialize(Large{}));
    assert(!toml::serialize(Large{}));
    assert(!toml::deserialize<Large>("number = 9223372036854775808\n"));

    constexpr Wide wide{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    auto wide_text = json::serialize(wide);
    assert(wide_text && json::deserialize<Wide>(*wide_text, strict_json) == wide);
    wide_text->insert(1, "\"extra\":true,");
    auto unknown = json::deserialize<Wide>(*wide_text, strict_json);
    assert(!unknown && unknown.error().contains("unknown field 'extra'"));
}

void check_explicit_contracts() {
    const Response response{42, "do not serialize"};
    assert(json::serialize(response) == R"({"userId":42})");
    assert(toml::serialize(response) == "userId = 42\n");
    auto json_back = json::deserialize<Response>(R"({"userId":42,"internal_note":"input"})");
    auto toml_back = toml::deserialize<Response>("userId = 42\ninternal_note = 'input'\n");
    assert(json_back && json_back->id == 42 && json_back->internal_note == "private");
    assert(toml_back && toml_back->id == 42 && toml_back->internal_note == "private");
    assert(json::serialize(Envelope{response}) == R"({"response":{"userId":42}})");
    auto nested_toml = toml::serialize(Envelope{response});
    assert(nested_toml && nested_toml->contains("[response]\nuserId = 42\n"));
    assert(!nested_toml->contains("internal_note"));
    assert(!json::serialize(Envelope{Response{0}}));
    assert(!toml::serialize(Envelope{Response{0}}));
    assert(!json::deserialize<Envelope>(R"({"response":{"userId":0}})"));
    assert(!toml::deserialize<Envelope>("[response]\nuserId = 0\n"));

    const User user{42, "Alice"};
    ProtocolItems protocol{{user}};
    assert(!json::serialize(protocol));
    assert(!toml::serialize(protocol));
    constexpr auto json_items = R"({"items":[{"id":42,"name":"Alice"}]})";
    constexpr auto toml_items = "[[items]]\nid = 42\nname = 'Alice'\n";
    auto json_protocol = json::deserialize<ProtocolItems>(json_items);
    auto toml_protocol = toml::deserialize<ProtocolItems>(toml_items);
    assert(!json_protocol && json_protocol.error().contains("unsupported"));
    assert(!toml_protocol && toml_protocol.error().contains("unsupported"));

    auto document = json::parse(json_items);
    assert(document);
    assert(!json::decode_field(std::get<0>(reflect::static_fields<ProtocolItems>()), document->at("items")));
    assert(!json::decode_fields_into(*document, protocol, [](const auto&) { return true; }));

    assert(!json::serialize(WireMessage{user}));
    auto wire_message = json::deserialize<WireMessage>(R"([{"id":42,"name":"Alice"}])");
    assert(!wire_message && wire_message.error().contains("unsupported"));
    json::stream::StreamWriter writer([](std::string_view) -> json::result<void> { return {}; });
    auto streamed = writer.value(user);
    assert(!streamed && streamed.error().contains("unsupported"));
}

}

int main() {
    aggregate_formats::check_basic_roundtrip();
    aggregate_formats::check_nested_roundtrip();
    aggregate_formats::check_errors_and_index();
    aggregate_formats::check_explicit_contracts();
}
