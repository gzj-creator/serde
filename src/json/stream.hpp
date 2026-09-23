#ifndef SERDE_JSON_STREAM_HPP
#define SERDE_JSON_STREAM_HPP

#include "json.hpp"

namespace json::stream {

/// Synchronous, incremental JSON output. A sink consumes the entire view before
/// returning success; views must not be retained. Failures are sticky and bytes
/// already delivered cannot be rolled back. The sink must not reenter the writer
/// or mutate values being serialized. Object keys retain their input order;
/// callers are responsible for key uniqueness.
class StreamWriter {
public:
    using Sink = std::function<result<void>(std::string_view)>;

    explicit StreamWriter(Sink sink, const SerializeOptions& options = {})
        : sink_(std::move(sink)), options_(options) {
        reset();
    }

    StreamWriter(const StreamWriter&) = delete;
    StreamWriter& operator=(const StreamWriter&) = delete;
    StreamWriter(StreamWriter&& other) noexcept
        : sink_(std::exchange(other.sink_, {})), options_(other.options_),
          stack_(std::move(other.stack_)), failure_(std::move(other.failure_)),
          bytes_(other.bytes_), nodes_(other.nodes_),
          root_written_(other.root_written_), finished_(other.finished_) {
        other.stack_.clear();
        other.failure_.reset();
        other.bytes_ = other.nodes_ = 0;
        other.root_written_ = other.finished_ = false;
    }
    StreamWriter& operator=(StreamWriter&& other) noexcept {
        if (this != &other) {
            sink_ = std::exchange(other.sink_, {});
            options_ = other.options_;
            stack_ = std::move(other.stack_);
            failure_ = std::move(other.failure_);
            bytes_ = other.bytes_;
            nodes_ = other.nodes_;
            root_written_ = other.root_written_;
            finished_ = other.finished_;
            other.stack_.clear();
            other.failure_.reset();
            other.bytes_ = other.nodes_ = 0;
            other.root_written_ = other.finished_ = false;
        }
        return *this;
    }

    [[nodiscard]] result<void> start_object() { return start_container(true); }
    [[nodiscard]] result<void> end_object() { return end_container(true); }
    [[nodiscard]] result<void> start_array() { return start_container(false); }
    [[nodiscard]] result<void> end_array() { return end_container(false); }

    [[nodiscard]] result<void> key(std::string_view name) {
        if (auto state = active(); !state) return state;
        if (stack_.empty() || !stack_.back().object || stack_.back().expect_value) {
            return fail("JSON object key is not expected here");
        }
        auto& context = stack_.back();
        if (context.count >= options_.max_object_members) {
            return fail("JSON object member count exceeds configured limit");
        }
        if (name.size() > options_.max_key_bytes || !json::detail::valid_utf8(name)) {
            return fail("invalid or oversized UTF-8 JSON object key");
        }
        if (auto prefix = item_prefix(context.count); !prefix) return prefix;
        if (auto written = quoted(name); !written) return written;
        if (auto colon = emit(options_.pretty ? ": " : ":"); !colon) return colon;
        ++context.count;
        context.expect_value = true;
        return {};
    }

    [[nodiscard]] result<void> string(std::string_view text) {
        if (auto state = active(); !state) return state;
        if (text.size() > options_.max_string_bytes || !json::detail::valid_utf8(text)) {
            return fail("invalid or oversized UTF-8 JSON string");
        }
        if (auto prefix = before_value(); !prefix) return prefix;
        return quoted(text);
    }

    template <std::integral T>
        requires(!std::same_as<T, bool>)
    [[nodiscard]] result<void> number(T input) {
        if (auto state = active(); !state) return state;
        using Integer = std::conditional_t<std::is_signed_v<T>, std::int64_t, std::uint64_t>;
        if constexpr (std::numeric_limits<T>::digits > std::numeric_limits<Integer>::digits) {
            if (input < static_cast<T>(std::numeric_limits<Integer>::min()) ||
                input > static_cast<T>(std::numeric_limits<Integer>::max())) {
                return fail("integer is outside JSON 64-bit range");
            }
        }
        char buffer[32];
        const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer),
                                               static_cast<Integer>(input));
        if (error != std::errc{}) return fail("failed to format JSON integer");
        return scalar(std::string_view(buffer, static_cast<std::size_t>(end - buffer)));
    }

    template <std::floating_point T>
    [[nodiscard]] result<void> number(T input) {
        if (auto state = active(); !state) return state;
        if (!std::isfinite(input) || input > std::numeric_limits<double>::max() ||
            input < -std::numeric_limits<double>::max()) {
            if (options_.non_finite == NonFinitePolicy::null_value) return null_value();
            return fail("JSON does not permit non-finite numbers");
        }
        char buffer[64];
        const auto [end, error] = std::to_chars(
            buffer, buffer + sizeof(buffer), static_cast<double>(input),
            std::chars_format::general, std::numeric_limits<double>::max_digits10);
        if (error != std::errc{}) return fail("failed to format JSON floating-point value");
        return scalar(std::string_view(buffer, static_cast<std::size_t>(end - buffer)));
    }

    [[nodiscard]] result<void> boolean(bool input) { return scalar(input ? "true" : "false"); }
    [[nodiscard]] result<void> null_value() { return scalar("null"); }

    /// Validate a complete JSON fragment, then emit its original bytes. Validation
    /// temporarily builds a DOM for this fragment and applies the remaining
    /// document limits. Whitespace and member order in the fragment are retained.
    [[nodiscard]] result<void> raw(std::string_view text) {
        if (auto state = value_position(); !state) return state;
        ParseOptions limits;
        limits.max_input_bytes = options_.max_output_bytes - bytes_;
        limits.max_nodes = options_.max_nodes;
        limits.max_depth = options_.max_depth - stack_.size();
        limits.max_string_bytes = options_.max_string_bytes;
        limits.max_key_bytes = options_.max_key_bytes;
        limits.max_array_items = options_.max_array_items;
        limits.max_object_members = options_.max_object_members;
        limits.duplicate_keys = DuplicateKeyPolicy::first_wins;
        limits.enforce_document_limits = false;
        auto parsed = json::parse(text, limits);
        if (!parsed) return fail(parsed.error());
        limits.enforce_document_limits = true;
        limits.max_depth = options_.max_depth;
        auto total_nodes = nodes_;
        if (auto checked = json::detail::enforce_limits(
                *parsed, limits, stack_.size(), total_nodes); !checked) {
            return fail(checked.error());
        }
        if (auto prefix = before_value(); !prefix) return prefix;
        nodes_ = total_nodes;
        return emit(text);
    }

    /// Traverse an existing DOM directly, including child views.
    [[nodiscard]] result<void> value(const Json& input) {
        if (auto state = active(); !state) return state;
        switch (input.type()) {
            case ValueType::null_value: return null_value();
            case ValueType::boolean: return boolean(*input.as_bool());
            case ValueType::signed_integer: return number(*input.as_int64());
            case ValueType::unsigned_integer: return number(*input.as_uint64());
            case ValueType::number: return number(*input.as_double());
            case ValueType::string: return string(*input.as_string());
            case ValueType::array: {
                if (auto begin = start_array(); !begin) return begin;
                auto children = input.for_each_element([&](const Json& child) { return value(child); });
                if (!children) return fail(children.error());
                return end_array();
            }
            case ValueType::object: {
                if (auto begin = start_object(); !begin) return begin;
                auto members = input.for_each_member([&](std::string_view name, const Json& child) -> result<void> {
                    if (auto written = key(name); !written) return written;
                    return value(child);
                });
                if (!members) return fail(members.error());
                return end_object();
            }
            default: return fail("invalid JSON value");
        }
    }

    /// Emit supported C++ values without a temporary JSON tree. Map iteration
    /// order and reflected declaration order are preserved; wrappers such as
    /// optional do not count as additional JSON nodes or nesting levels.
    template <class T>
    [[nodiscard]] result<void> value(const T& input) {
        if (auto state = active(); !state) return state;
        using U = json::detail::BareT<T>;
        if constexpr (json::detail::OptionalTraits<U>::value) {
            return input ? value(*input) : null_value();
        } else if constexpr (json::detail::InlineTableTraits<U>::value) {
            return value(input.value);
        } else if constexpr (std::same_as<U, date>) {
            if (!json::detail::valid_date_value(input)) return fail("invalid json::date value");
            return string(json::detail::temporal_text(input));
        } else if constexpr (std::same_as<U, time>) {
            if (!json::detail::valid_time_value(input)) return fail("invalid json::time value");
            return string(json::detail::temporal_text(input));
        } else if constexpr (std::same_as<U, local_date_time>) {
            if (!json::detail::valid_date_value(input.date_part) ||
                !json::detail::valid_time_value(input.time_part)) {
                return fail("invalid json::local_date_time value");
            }
            return string(json::detail::temporal_text(input));
        } else if constexpr (std::same_as<U, offset_date_time>) {
            if (!json::detail::valid_date_value(input.local.date_part) ||
                !json::detail::valid_time_value(input.local.time_part) ||
                input.offset_minutes < -1439 || input.offset_minutes > 1439) {
                return fail("invalid json::offset_date_time value");
            }
            return string(json::detail::temporal_text(input));
        } else if constexpr (std::same_as<U, std::nullptr_t>) {
            return null_value();
        } else if constexpr (std::same_as<U, bool>) {
            return boolean(input);
        } else if constexpr (std::is_arithmetic_v<U>) {
            return number(input);
        } else if constexpr (std::is_enum_v<U>) {
            return value(static_cast<std::underlying_type_t<U>>(input));
        } else if constexpr (std::is_convertible_v<const T&, std::string_view>) {
            if constexpr (std::is_pointer_v<U>) {
                if (input == nullptr) return fail("null JSON string pointer");
            }
            return string(std::string_view(input));
        } else if constexpr (json::detail::VectorTraits<U>::value || json::detail::ArrayTraits<U>::value) {
            if (input.size() > options_.max_array_items) {
                return fail("JSON array item count exceeds configured limit");
            }
            if (auto begin = start_array(); !begin) return begin;
            for (const auto& child : input) {
                const auto written = [&]() -> result<void> {
                    // vector<bool> exposes proxy objects, not references to bool.
                    if constexpr (json::detail::VectorTraits<U>::value &&
                                  std::same_as<typename U::value_type, bool>) {
                        return boolean(static_cast<bool>(child));
                    } else {
                        return value(child);
                    }
                }();
                if (!written) return written;
            }
            return end_array();
        } else if constexpr (json::detail::MapTraits<U>::value) {
            if (input.size() > options_.max_object_members) {
                return fail("JSON object member count exceeds configured limit");
            }
            if (auto begin = start_object(); !begin) return begin;
            for (const auto& [name, child] : input) {
                if (auto written = key(name); !written) return written;
                if (auto written = value(child); !written) return written;
            }
            return end_object();
        } else if constexpr (Reflectable<U>) {
            if (auto begin = start_object(); !begin) return begin;
            result<void> written;
            json::for_each_field(input, [&](const auto& descriptor, const auto& object) {
                if (!written) return;
                written = key(descriptor.name);
                if (written) written = value(descriptor.get(object));
            });
            if (!written) return written;
            return end_object();
        } else {
            return fail("unsupported type in JSON stream serializer");
        }
    }

    /// Seal one complete document. Does not flush or close the external sink.
    [[nodiscard]] result<void> finish() {
        if (auto state = status(); !state) return state;
        if (!root_written_ || !stack_.empty()) return fail("incomplete JSON document");
        finished_ = true;
        return {};
    }

    [[nodiscard]] result<void> status() const {
        if (failure_) return std::unexpected(*failure_);
        if (!sink_) return std::unexpected(std::string("JSON stream sink is empty"));
        return {};
    }

    std::size_t bytes_written() const noexcept { return bytes_; }

    /// Start a new document with the same sink and options; emitted bytes remain
    /// the caller's responsibility (including any NDJSON or transport framing).
    void reset() {
        stack_.clear();
        failure_.reset();
        bytes_ = nodes_ = 0;
        root_written_ = finished_ = false;
        if (!sink_) failure_ = "JSON stream sink is empty";
        else if (options_.indent_width > 64) failure_ = "JSON indent width exceeds configured limit";
    }

private:
    struct Context {
        bool object;
        std::size_t count = 0;
        bool expect_value = false;
    };

    Sink sink_;
    SerializeOptions options_;
    std::vector<Context> stack_;
    std::optional<std::string> failure_;
    std::size_t bytes_ = 0;
    std::size_t nodes_ = 0;
    bool root_written_ = false;
    bool finished_ = false;

    result<void> fail(std::string message) {
        if (!failure_) failure_ = std::move(message);
        return std::unexpected(*failure_);
    }

    result<void> active() {
        if (auto state = status(); !state) return state;
        if (finished_) return fail("JSON document is already finished");
        return {};
    }

    result<void> emit(std::string_view text) {
        if (text.empty()) return {};
        if (text.size() > options_.max_output_bytes - bytes_) {
            return fail("JSON output exceeds configured size limit");
        }
        if (auto written = sink_(text); !written) return fail(std::move(written.error()));
        bytes_ += text.size();
        return {};
    }

    result<void> indent(std::size_t depth) {
        if (options_.indent_width && depth >
            (options_.max_output_bytes - bytes_) / options_.indent_width) {
            return fail("JSON output exceeds configured size limit");
        }
        std::size_t remaining = depth * options_.indent_width;
        constexpr std::string_view spaces = "                                                                ";
        while (remaining) {
            const auto count = std::min(remaining, spaces.size());
            if (auto written = emit(spaces.substr(0, count)); !written) return written;
            remaining -= count;
        }
        return {};
    }

    result<void> item_prefix(std::size_t count) {
        if (count) {
            if (auto comma = emit(","); !comma) return comma;
        }
        if (options_.pretty) {
            if (auto newline = emit("\n"); !newline) return newline;
            return indent(stack_.size());
        }
        return {};
    }

    result<void> value_position() {
        if (auto state = active(); !state) return state;
        if (stack_.empty()) {
            if (root_written_) return fail("JSON document already has a root value");
        } else if (stack_.back().object) {
            if (!stack_.back().expect_value) return fail("JSON object value requires a key");
        } else if (stack_.back().count >= options_.max_array_items) {
            return fail("JSON array item count exceeds configured limit");
        }
        if (stack_.size() >= options_.max_depth) {
            return fail("JSON nesting depth exceeds configured limit");
        }
        if (nodes_ >= options_.max_nodes) return fail("JSON node count exceeds configured limit");
        return {};
    }

    result<void> before_value() {
        if (auto state = value_position(); !state) return state;
        if (stack_.empty()) {
            root_written_ = true;
        } else {
            auto& context = stack_.back();
            if (context.object) {
                context.expect_value = false;
            } else {
                if (auto prefix = item_prefix(context.count); !prefix) return prefix;
                ++context.count;
            }
        }
        ++nodes_;
        return {};
    }

    result<void> scalar(std::string_view text) {
        if (auto prefix = before_value(); !prefix) return prefix;
        return emit(text);
    }

    result<void> start_container(bool object) {
        if (auto prefix = before_value(); !prefix) return prefix;
        if (auto begin = emit(object ? "{" : "["); !begin) return begin;
        stack_.push_back({object});
        return {};
    }

    result<void> end_container(bool object) {
        if (auto state = active(); !state) return state;
        if (stack_.empty() || stack_.back().object != object) {
            return fail("mismatched JSON container end");
        }
        if (stack_.back().expect_value) return fail("JSON object key is missing its value");
        if (options_.pretty && stack_.back().count) {
            if (auto newline = emit("\n"); !newline) return newline;
            if (auto spaces = indent(stack_.size() - 1); !spaces) return spaces;
        }
        if (auto end = emit(object ? "}" : "]"); !end) return end;
        stack_.pop_back();
        return {};
    }

    result<void> quoted(std::string_view text) {
        if (auto quote = emit("\""); !quote) return quote;
        std::size_t start = 0;
        for (std::size_t index = 0; index < text.size(); ++index) {
            const auto byte = static_cast<unsigned char>(text[index]);
            if (byte >= 0x20 && byte != '"' && byte != '\\') continue;
            if (auto span = emit(text.substr(start, index - start)); !span) return span;
            std::string_view escaped;
            char control[] = {'\\', 'u', '0', '0', '0', '0'};
            switch (byte) {
                case '"': escaped = "\\\""; break;
                case '\\': escaped = "\\\\"; break;
                case '\b': escaped = "\\b"; break;
                case '\f': escaped = "\\f"; break;
                case '\n': escaped = "\\n"; break;
                case '\r': escaped = "\\r"; break;
                case '\t': escaped = "\\t"; break;
                default:
                    constexpr char hex[] = "0123456789abcdef";
                    control[4] = hex[byte >> 4];
                    control[5] = hex[byte & 15];
                    escaped = std::string_view(control, sizeof(control));
            }
            if (auto written = emit(escaped); !written) return written;
            start = index + 1;
        }
        if (auto span = emit(text.substr(start)); !span) return span;
        return emit("\"");
    }
};

/// Emit one value at the writer's current position, including nested values.
/// Call finish() after completing the enclosing document.
template <class T>
[[nodiscard]] result<void> serialize(StreamWriter& writer, const T& value) {
    return writer.value(value);
}

/// Write and finish one document through a synchronous sink.
template <class T>
[[nodiscard]] result<void> serialize(const T& value, StreamWriter::Sink sink,
                                     const SerializeOptions& options = {}) {
    StreamWriter writer(std::move(sink), options);
    if (auto written = writer.value(value); !written) return written;
    return writer.finish();
}

}  // namespace json::stream

#endif  // SERDE_JSON_STREAM_HPP
