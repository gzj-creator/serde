#ifndef SERDE_TEST_JSON_STREAM_CASES_HPP
#define SERDE_TEST_JSON_STREAM_CASES_HPP

#include <serde/reflect/reflect_macros.hpp>

namespace stream_tests {

using json::stream::StreamWriter;

inline bool expect(bool condition, const char* message) {
    if (!condition) std::fprintf(stderr, "json_stream: %s\n", message);
    return condition;
}

template <class T>
bool error(const json::result<T>& result, std::string_view fragment) {
    return !result && result.error().find(fragment) != std::string::npos;
}

inline StreamWriter::Sink collect(std::string& output) {
    return [&](std::string_view part) -> json::result<void> {
        output.append(part);
        return {};
    };
}

enum class Mode : unsigned { ready = 7 };
enum class Switch : bool { off = false, on = true };

struct Payload {
    std::string text;
    std::vector<int> values;
    std::optional<bool> flag;
    Mode mode;
    bool operator==(const Payload&) const = default;
};

#define STREAM_PAYLOAD_FIELDS(X) X(text) X(values) X(flag) X(mode)
REFLECT_FIELDS(Payload, STREAM_PAYLOAD_FIELDS)
#undef STREAM_PAYLOAD_FIELDS

inline bool messages() {
    std::string output;
    StreamWriter writer(collect(output));
    bool ok = expect(writer.start_object().has_value() && output == "{",
                     "the first token reaches the sink immediately");
    ok &= expect(writer.key("jsonrpc") && writer.string("2.0") &&
                 writer.key("id") && writer.number(42) && writer.key("result") &&
                 writer.start_object() && writer.key("content") && writer.start_array() &&
                 writer.start_object() && writer.key("type") && writer.string("text") &&
                 writer.key("text") && writer.string("你好\n\"ok\"") &&
                 writer.end_object() && writer.end_array() && writer.key("structuredContent") &&
                 writer.raw(R"({"done":true,"count":2})") && writer.end_object() &&
                 writer.end_object() && writer.finish(), "build MCP JSON-RPC response");
    ok &= expect(output == R"({"jsonrpc":"2.0","id":42,"result":{"content":[{"type":"text","text":"你好\n\"ok\""}],"structuredContent":{"done":true,"count":2}}})",
                 "JSON-RPC output preserves event order and escapes strings");
    const auto parsed = json::parse(output);
    ok &= expect(parsed && parsed->at("result").at("content").size() == 1 &&
                 parsed->at("id").as_int64() == 42, "MCP message parses and retains its values");
    ok &= expect(writer.bytes_written() == output.size() && writer.finish(),
                 "bytes include all committed tokens and finish is idempotent");
    writer.reset();
    output.clear();
    ok &= expect(writer.value("next") && writer.finish() && output == "\"next\"" &&
                 writer.bytes_written() == 6, "reset permits another independent document");
    return ok;
}

inline bool scalars_and_values() {
    std::string output;
    StreamWriter writer(collect(output));
    std::string text;
    for (int index = 0; index != 32; ++index) text.push_back(static_cast<char>(index));
    text += "\\\"中文😀";
    bool ok = expect(writer.string(text) && writer.finish(), "all control bytes serialize");
    auto parsed = json::parse(output);
    ok &= expect(parsed && parsed->as_string() == text, "control characters and UTF-8 round trip");

    output.clear();
    writer.reset();
    ok &= expect(writer.start_array() && writer.number(std::numeric_limits<std::int64_t>::min()) &&
                 writer.number(std::numeric_limits<std::uint64_t>::max()) &&
                 writer.number(-0.0) && writer.number(std::numeric_limits<double>::denorm_min()) &&
                 writer.number(std::numeric_limits<double>::max()) && writer.number(1.5f) &&
                 writer.number(2.5L) && writer.boolean(false) && writer.null_value() &&
                 writer.end_array() && writer.finish(), "integer and floating point boundaries");
    parsed = json::parse(output);
    ok &= expect(parsed && parsed->at(0).as_int64() == std::numeric_limits<std::int64_t>::min() &&
                 parsed->at(1).as_uint64() == std::numeric_limits<std::uint64_t>::max() &&
                 parsed->at(3).as_double() == std::numeric_limits<double>::denorm_min() &&
                 parsed->at(4).as_double() == std::numeric_limits<double>::max() &&
                 parsed->at(8).is_null(), "streamed numeric values parse exactly");

    Payload original{"mcp", {1, 2}, true, Mode::ready};
    output.clear();
    ok &= expect(json::stream::serialize(original, collect(output)).has_value(),
                 "typed sink serialization");
    ok &= expect(output == R"({"text":"mcp","values":[1,2],"flag":true,"mode":7})",
                 "streaming reflection follows declaration order");
    auto decoded = json::deserialize<Payload>(output);
    ok &= expect(decoded && *decoded == original, "typed reflected values round trip");

    output.clear();
    writer.reset();
    const std::map<std::string, std::array<std::optional<int>, 2>> nested{
        {"b", {2, std::nullopt}}, {"a", {0, 1}}};
    ok &= expect(writer.start_array() && json::stream::serialize(writer, nested) &&
                 writer.value(std::vector<bool>{true, false}) &&
                 writer.value(std::array<bool, 2>{false, true}) &&
                 writer.value(json::InlineTable<Payload>{original}) &&
                 writer.end_array() && writer.finish(), "nested typed serialize uses current position");
    parsed = json::parse(output);
    ok &= expect(parsed && parsed->size() == 4 && parsed->at(0).at("b").at(1).is_null() &&
                 parsed->at(1).at(0).as_bool() == true &&
                 parsed->at(2).at(0).as_bool() == false, "containers, optional and vector<bool>");

    output.clear();
    writer.reset();
    ok &= expect(writer.start_array() && writer.value(json::date{2026, 9, 23}) &&
                 writer.value(json::time{12, 30, 1, "5"}) &&
                 writer.value(json::local_date_time{{2026, 9, 23}, {12, 30, 1, ""}}) &&
                 writer.value(json::offset_date_time{{{2026, 9, 23}, {12, 30, 1, ""}}, 480}) &&
                 writer.end_array() && writer.finish(), "all temporal types stream");
    ok &= expect(output == R"(["2026-09-23","12:30:01.5","2026-09-23T12:30:01","2026-09-23T12:30:01+08:00"])",
                 "temporal output uses validated text");

    output.clear();
    writer.reset();
    auto document = json::parse(R"({"z":[null,true,-1,18446744073709551615,1.25,"ok"],"a":{}})");
    ok &= expect(document && writer.value(*document) && writer.finish() &&
                 output == R"({"z":[null,true,-1,18446744073709551615,1.25,"ok"],"a":{}})",
                 "DOM traversal includes all types and preserves member order");
    output.clear();
    writer.reset();
    ok &= expect(document && writer.value(document->at("z")) && writer.finish() &&
                 output == R"([null,true,-1,18446744073709551615,1.25,"ok"])",
                 "DOM child views can be streamed");
    return ok;
}

inline bool pretty_and_raw() {
    std::string output;
    json::SerializeOptions options;
    options.pretty = true;
    StreamWriter writer(collect(output), options);
    bool ok = expect(writer.start_object() && writer.key("a") && writer.start_array() &&
                     writer.number(1) && writer.start_object() && writer.end_object() &&
                     writer.end_array() && writer.key("b") && writer.start_array() &&
                     writer.end_array() && writer.end_object() && writer.finish(), "nested pretty output");
    ok &= expect(output == "{\n  \"a\": [\n    1,\n    {}\n  ],\n  \"b\": []\n}",
                 "indentation and empty containers match existing JSON formatting");
    options.indent_width = 0;
    output.clear();
    ok &= expect(json::stream::serialize(std::vector<int>{1}, collect(output), options) &&
                 output == "[\n1\n]", "zero indentation still emits newlines");

    writer.reset();
    output.clear();
    ok &= expect(writer.start_array() && writer.raw(" {\"a\": 1} ") && writer.number(2) &&
                 writer.end_array() && writer.finish() &&
                 output == "[\n   {\"a\": 1} ,\n  2\n]", "raw preserves its own whitespace");
    for (const auto invalid : {"", "[1,]", "{}{}", "1 2", "{", "NaN", "01", "\"\\uD800\""}) {
        writer.reset();
        output.clear();
        ok &= expect(!writer.raw(invalid) && output.empty() && !writer.finish(),
                     "invalid raw fragments fail before output");
    }
    return ok;
}

inline bool failure_states() {
    std::string output;
    StreamWriter writer(collect(output));
    bool ok = expect(error(writer.key("x"), "key") && output.empty(), "key at root is rejected");
    const auto first_error = writer.status();
    ok &= expect(!writer.null_value() && writer.finish().error() == first_error.error() && output.empty(),
                 "first error is sticky");
    writer.reset();
    ok &= expect(error(writer.finish(), "incomplete"), "empty document cannot finish");
    writer.reset();
    ok &= expect(error(writer.end_object(), "mismatched"), "unopened object cannot end");
    writer.reset();
    ok &= expect(writer.start_array() && error(writer.end_object(), "mismatched"),
                 "container types must match");
    writer.reset();
    ok &= expect(writer.start_object() && error(writer.number(1), "key"), "object requires keys");
    writer.reset();
    ok &= expect(writer.start_object() && writer.key("x") && error(writer.key("y"), "key"),
                 "key cannot replace a pending value");
    writer.reset();
    ok &= expect(writer.start_object() && writer.key("x") && error(writer.end_object(), "missing"),
                 "object cannot end with a missing value");
    writer.reset();
    ok &= expect(writer.start_array() && error(writer.finish(), "incomplete"),
                 "unclosed container cannot finish");
    writer.reset();
    ok &= expect(writer.number(1) && error(writer.number(2), "root"), "multiple roots rejected");
    writer.reset();
    ok &= expect(writer.null_value() && writer.finish() && error(writer.start_array(), "finished"),
                 "finished document rejects further values");
    writer.reset();
    ok &= expect(error(writer.string(std::string("\xc0\xaf", 2)), "UTF-8"), "invalid UTF-8 string");
    writer.reset();
    ok &= expect(writer.start_object() && error(writer.key(std::string("\xff", 1)), "UTF-8"),
                 "invalid UTF-8 key");
    writer.reset();
    ok &= expect(error(writer.value(static_cast<const char*>(nullptr)), "null"), "null string pointer");
    writer.reset();
    ok &= expect(error(writer.value(json::date{2026, 2, 30}), "date"), "invalid date");
    writer.reset();
    ok &= expect(error(writer.value(json::time{24, 0, 0, ""}), "time"), "invalid time");
    writer.reset();
    ok &= expect(error(writer.value(json::local_date_time{{2026, 2, 30}, {0, 0, 0, ""}}), "date_time"),
                 "invalid local date-time");
    writer.reset();
    ok &= expect(error(writer.value(json::offset_date_time{{{2026, 1, 1}, {0, 0, 0, ""}}, 1440}), "date_time"),
                 "invalid offset date-time");
    writer.reset();
    struct Unsupported {};
    ok &= expect(error(writer.value(Unsupported{}), "unsupported"), "unsupported values return errors");
    writer.reset();
    ok &= expect(error(writer.value(json::Json{}), "invalid"), "invalid DOM rejected");
    json::Parser parser;
    auto stale = parser.parse("1");
    auto fresh = parser.parse("2");
    writer.reset();
    ok &= expect(stale && fresh && error(writer.value(*stale), "invalid"), "stale DOM rejected");

    std::size_t calls = 0;
    StreamWriter failing([&](std::string_view) -> json::result<void> {
        if (++calls == 2) return std::unexpected(std::string("transport closed"));
        return {};
    });
    ok &= expect(failing.start_array() && error(failing.number(123), "transport closed") &&
                 error(failing.end_array(), "transport closed") &&
                 error(failing.finish(), "transport closed") && calls == 2 &&
                 failing.bytes_written() == 1, "sink error halts output and preserves reason");
    failing.reset();
    ok &= expect(failing.boolean(true) && failing.finish() && calls == 3, "reset recovers from sink failure");
    StreamWriter empty_failure([](std::string_view) -> json::result<void> {
        return std::unexpected(std::string{});
    });
    ok &= expect(!empty_failure.boolean(true) && !empty_failure.finish(), "empty sink errors stay errors");
    StreamWriter missing(StreamWriter::Sink{});
    ok &= expect(error(missing.null_value(), "sink"), "empty sink is rejected");
    return ok;
}

inline bool limits() {
    std::string output;
    auto fits = [&](auto input, json::SerializeOptions options, std::string_view expected) {
        output.clear();
        auto result = json::stream::serialize(input, collect(output), options);
        return expect(result && output == expected, "exact configured limit succeeds");
    };
    auto rejects = [&](auto input, json::SerializeOptions options, std::string_view fragment) {
        output.clear();
        return expect(error(json::stream::serialize(input, collect(output), options), fragment),
                      "exceeding configured limit is rejected");
    };
    bool ok = true;
    json::SerializeOptions options;
    options.max_output_bytes = 4;
    ok &= fits(nullptr, options, "null");
    options.max_output_bytes = 3;
    ok &= rejects(nullptr, options, "output");
    options.max_output_bytes = 0;
    ok &= rejects(0, options, "output");
    options = {};
    options.max_output_bytes = 8;
    ok &= fits(std::string(1, '\0'), options, R"("\u0000")");
    options.max_output_bytes = 7;
    ok &= rejects(std::string(1, '\0'), options, "output");
    options = {};
    options.max_nodes = 3;
    ok &= fits(std::vector<int>{1, 2}, options, "[1,2]");
    options.max_nodes = 2;
    ok &= rejects(std::vector<int>{1, 2}, options, "node");
    options.max_nodes = 0;
    ok &= rejects(nullptr, options, "node");
    options = {};
    options.max_depth = 2;
    ok &= fits(std::vector<int>{1}, options, "[1]");
    ok &= rejects(std::vector<std::vector<int>>{{1}}, options, "depth");
    options.max_depth = 1;
    options.max_nodes = 1;
    ok &= fits(std::optional<int>{3}, options, "3");
    ok &= fits(Mode::ready, options, "7");
    ok &= fits(Switch::on, options, "true");
    ok &= fits(std::vector<int>{}, options, "[]");
    options.max_depth = 0;
    ok &= rejects(nullptr, options, "depth");
    options = {};
    options.max_string_bytes = 2;
    ok &= fits(std::string("hi"), options, "\"hi\"");
    ok &= rejects(std::string("bye"), options, "string");
    options = {};
    options.max_key_bytes = 1;
    ok &= fits(std::map<std::string, int>{{"a", 1}}, options, R"({"a":1})");
    ok &= rejects(std::map<std::string, int>{{"aa", 1}}, options, "key");
    options = {};
    options.max_array_items = 1;
    ok &= fits(std::array{1}, options, "[1]");
    ok &= rejects(std::array{1, 2}, options, "array");
    options.max_array_items = 0;
    ok &= fits(std::vector<int>{}, options, "[]");
    options = {};
    options.max_object_members = 1;
    ok &= fits(std::map<std::string, int>{{"a", 1}}, options, R"({"a":1})");
    ok &= rejects(std::map<std::string, int>{{"a", 1}, {"b", 2}}, options, "member");
    ok &= rejects(Payload{}, options, "member");
    options = {};
    options.indent_width = 65;
    ok &= rejects(nullptr, options, "indent");
    options = {};
    ok &= rejects(std::numeric_limits<double>::infinity(), options, "non-finite");
    ok &= rejects(std::numeric_limits<double>::quiet_NaN(), options, "non-finite");
    options.non_finite = json::NonFinitePolicy::null_value;
    ok &= fits(std::numeric_limits<double>::infinity(), options, "null");
    ok &= fits(-std::numeric_limits<double>::infinity(), options, "null");
    ok &= fits(std::numeric_limits<double>::quiet_NaN(), options, "null");

    options = {};
    options.max_nodes = 4;
    output.clear();
    StreamWriter raw(collect(output), options);
    ok &= expect(raw.start_array() && raw.raw("[1]") && raw.raw("2") &&
                 error(raw.null_value(), "node"), "raw nodes count toward whole-document limit");
    options.max_nodes = 3;
    StreamWriter too_many(collect(output), options);
    ok &= expect(too_many.start_array() && too_many.number(0) &&
                 error(too_many.raw("[1]"), "node"), "raw checks remaining node budget");
    options = {};
    options.max_depth = 2;
    StreamWriter deep(collect(output), options);
    ok &= expect(deep.start_array() && error(deep.raw("[1]"), "depth"), "raw depth includes parent containers");
    for (std::size_t depth = 1; depth <= 3; ++depth) {
        options.max_depth = depth;
        const std::string fragment = std::string(depth, '[') + std::string(depth, ']');
        output.clear();
        StreamWriter exact_depth(collect(output), options);
        ok &= expect(exact_depth.raw(fragment) && exact_depth.finish() && output == fragment,
                     "raw empty containers accept exactly the configured depth");
        output.clear();
        StreamWriter overflow_depth(collect(output), options);
        ok &= expect(error(overflow_depth.raw("[" + fragment + "]"), "depth") && output.empty(),
                     "raw empty containers reject one level beyond configured depth");
    }
    options = {};
    options.max_string_bytes = 1;
    StreamWriter long_raw(collect(output), options);
    ok &= expect(error(long_raw.raw(R"({"v":"xx"})"), "string"), "raw string limits enforced");
    options = {};
    options.max_output_bytes = 3;
    output.clear();
    StreamWriter prefix(collect(output), options);
    ok &= expect(prefix.start_array() && prefix.number(0) &&
                 error(prefix.raw("1"), "output") && output.size() <= 3,
                 "raw output limit includes generated separator bytes");
    return ok;
}

inline bool streaming_and_moves() {
    std::size_t calls = 0;
    std::size_t bytes = 0;
    StreamWriter counter([&](std::string_view part) -> json::result<void> {
        ++calls;
        bytes += part.size();
        return {};
    });
    bool ok = expect(counter.start_array().has_value(), "start large streamed array");
    for (std::size_t index = 0; index < 10000; ++index) {
        if (!counter.number(0)) return expect(false, "large streamed array failed");
    }
    ok &= expect(bytes == 20000 && calls > 10000 && counter.end_array() &&
                 counter.finish() && bytes == 20001, "many values reach the sink before finish");

    std::string text(1024 * 1024, 'x');
    bool borrowed = false;
    StreamWriter spans([&](std::string_view part) -> json::result<void> {
        if (part.data() == text.data() && part.size() == text.size()) borrowed = true;
        return {};
    });
    ok &= expect(spans.string(text) && spans.finish() && borrowed,
                 "unescaped strings are forwarded directly without copying");

    std::string output;
    StreamWriter original(collect(output));
    ok &= expect(original.start_object() && original.key("value"), "start before moving writer");
    StreamWriter moved(std::move(original));
    ok &= expect(moved.number(1) && moved.end_object() && moved.finish() && output == R"({"value":1})",
                 "move constructor preserves pending key and sink");
    ok &= expect(error(original.null_value(), "sink"), "moved-from writer cannot emit without sink");
    StreamWriter failed(collect(output));
    ok &= expect(error(failed.key("x"), "key"), "create a failed writer before moving");
    const auto failure = failed.status().error();
    StreamWriter moved_failure(std::move(failed));
    ok &= expect(moved_failure.status().error() == failure &&
                 error(failed.null_value(), "sink"),
                 "moving a failed writer keeps its error and clears the source");
    StreamWriter assigned_failure(collect(output));
    assigned_failure = std::move(moved_failure);
    ok &= expect(assigned_failure.status().error() == failure &&
                 error(moved_failure.null_value(), "sink"),
                 "move assignment of a failed writer keeps its error");
    moved.reset();
    output.clear();
    ok &= expect(moved.start_array().has_value(), "start before move assignment");
    original = std::move(moved);
    ok &= expect(original.boolean(true) && original.end_array() && original.finish() && output == "[true]",
                 "move assignment transfers open context");
    return ok;
}

inline int run() {
    bool ok = messages();
    ok &= scalars_and_values();
    ok &= pretty_and_raw();
    ok &= failure_states();
    ok &= limits();
    ok &= streaming_and_moves();
    return ok ? 0 : 1;
}

}  // namespace stream_tests

#endif
