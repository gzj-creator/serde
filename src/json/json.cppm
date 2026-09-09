module;
#if defined(__SSE2__)
#  include <emmintrin.h>
#endif

export module json;

export import std;
export import reflect;
export import toml;

export namespace json {

template <class T>
using result = std::expected<T, std::string>;

enum class UnknownFieldPolicy { ignore, reject };

enum class NonFinitePolicy { reject, null_value };

struct ParseOptions {
    std::size_t max_input_bytes = 64ULL * 1024ULL * 1024ULL;
    std::size_t max_nodes = 1000000;
    std::size_t max_depth = 128;
    std::size_t max_string_bytes = 16ULL * 1024ULL * 1024ULL;
    std::size_t max_key_bytes = 1ULL * 1024ULL * 1024ULL;
    std::size_t max_array_items = 1000000;
    std::size_t max_object_members = 1000000;
    UnknownFieldPolicy unknown_fields = UnknownFieldPolicy::ignore;
};

struct SerializeOptions {
    std::size_t max_output_bytes = 64ULL * 1024ULL * 1024ULL;
    std::size_t max_nodes = 1000000;
    std::size_t max_depth = 128;
    std::size_t max_string_bytes = 16ULL * 1024ULL * 1024ULL;
    std::size_t max_key_bytes = 1ULL * 1024ULL * 1024ULL;
    std::size_t max_array_items = 1000000;
    std::size_t max_object_members = 1000000;
    bool pretty = false;
    unsigned indent_width = 2;
    NonFinitePolicy non_finite = NonFinitePolicy::reject;
};

// JSON 没有原生时间值。别名使项目中已验证的 TOML 时间词汇
// 可作为 RFC 3339 JSON 字符串使用。
using date = toml::date;
using time = toml::time;
using local_date_time = toml::local_date_time;
using offset_date_time = toml::offset_date_time;
template <class T>
using inline_table = toml::inline_table<T>;

using parse_options = ParseOptions;
using serialize_options = SerializeOptions;
using unknown_field_policy = UnknownFieldPolicy;
using non_finite_policy = NonFinitePolicy;

template <class Owner, class Member>
using field = reflect::field<Owner, Member>;
using reflect::make_field;

template <class T>
concept Reflectable = reflect::Reflectable<T>;

template <class T, class Function>
    requires Reflectable<T>
constexpr void for_each_field(T& value, Function&& function) {
    reflect::for_each_field(value, std::forward<Function>(function));
}

namespace detail {

template <class T>
using BareT = std::remove_cvref_t<T>;

template <class T>
struct OptionalTraits {
    static constexpr bool value = false;
};

template <class T>
struct OptionalTraits<std::optional<T>> {
    static constexpr bool value = true;
    using value_type = T;
};

template <class T>
struct VectorTraits {
    static constexpr bool value = false;
};

template <class T, class Allocator>
struct VectorTraits<std::vector<T, Allocator>> {
    static constexpr bool value = true;
    using value_type = T;
};

template <class T>
struct ArrayTraits {
    static constexpr bool value = false;
};

template <class T, std::size_t Size>
struct ArrayTraits<std::array<T, Size>> {
    static constexpr bool value = true;
    using value_type = T;
    static constexpr std::size_t size = Size;
};

template <class T>
struct MapTraits {
    static constexpr bool value = false;
};

template <class Value, class Compare, class Allocator>
struct MapTraits<std::map<std::string, Value, Compare, Allocator>> {
    static constexpr bool value = true;
    using mapped_type = Value;
};

template <class Value, class Hash, class Equal, class Allocator>
struct MapTraits<std::unordered_map<std::string, Value, Hash, Equal, Allocator>> {
    static constexpr bool value = true;
    using mapped_type = Value;
};

template <class T>
struct InlineTableTraits {
    static constexpr bool value = false;
};

template <class T>
struct InlineTableTraits<inline_table<T>> {
    static constexpr bool value = true;
    using value_type = T;
};

/** 检查字节字符串是否为合法的 UTF-8。 */
inline bool valid_utf8(std::string_view text) {
    for (std::size_t index = 0; index < text.size();) {
#if defined(__SSE2__)
        while (index + 16 <= text.size()) {
            const auto chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(text.data() + index));
            if (_mm_movemask_epi8(chunk) != 0) break;
            index += 16;
        }
        if (index == text.size()) {
            break;
        }
#endif
        const auto byte = static_cast<unsigned char>(text[index]);
        if (byte <= 0x7f) {
            ++index;
            continue;
        }
        std::size_t width = 0;
        std::uint32_t code_point = 0;
        std::uint32_t minimum = 0;
        if (byte >= 0xc2 && byte <= 0xdf) {
            width = 2;
            code_point = byte & 0x1f;
            minimum = 0x80;
        } else if (byte >= 0xe0 && byte <= 0xef) {
            width = 3;
            code_point = byte & 0x0f;
            minimum = 0x800;
        } else if (byte >= 0xf0 && byte <= 0xf4) {
            width = 4;
            code_point = byte & 0x07;
            minimum = 0x10000;
        } else {
            return false;
        }
        if (index + width > text.size()) {
            return false;
        }
        for (std::size_t continuation = 1; continuation < width; ++continuation) {
            const auto next = static_cast<unsigned char>(text[index + continuation]);
            if ((next & 0xc0) != 0x80) {
                return false;
            }
            code_point = (code_point << 6) | (next & 0x3f);
        }
        if (code_point < minimum || code_point > 0x10ffff ||
            (code_point >= 0xd800 && code_point <= 0xdfff)) {
            return false;
        }
        index += width;
    }
    return true;
}

inline result<void> append_utf8(std::string& output, std::uint32_t code_point) {
    if (code_point > 0x10ffff ||
        (code_point >= 0xd800 && code_point <= 0xdfff)) {
        return std::unexpected(std::string("invalid Unicode code point in JSON string"));
    }
    if (code_point <= 0x7f) {
        output.push_back(static_cast<char>(code_point));
    } else if (code_point <= 0x7ff) {
        output.push_back(static_cast<char>(0xc0 | (code_point >> 6)));
        output.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
    } else if (code_point <= 0xffff) {
        output.push_back(static_cast<char>(0xe0 | (code_point >> 12)));
        output.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
    } else {
        output.push_back(static_cast<char>(0xf0 | (code_point >> 18)));
        output.push_back(static_cast<char>(0x80 | ((code_point >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((code_point >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (code_point & 0x3f)));
    }
    return {};
}

inline bool ascii_digit(char value) {
    return value >= '0' && value <= '9';
}

inline bool hex_digit(char value, unsigned& output) {
    if (value >= '0' && value <= '9') {
        output = static_cast<unsigned>(value - '0');
        return true;
    }
    if (value >= 'a' && value <= 'f') {
        output = static_cast<unsigned>(value - 'a' + 10);
        return true;
    }
    if (value >= 'A' && value <= 'F') {
        output = static_cast<unsigned>(value - 'A' + 10);
        return true;
    }
    return false;
}

inline std::size_t skip_json_space(std::string_view text, std::size_t position) noexcept {
#if defined(__SSE2__)
    const auto* data = text.data();
    const auto space = _mm_set1_epi8(0x20);
    const auto tab = _mm_set1_epi8(0x09);
    const auto newline = _mm_set1_epi8(0x0a);
    const auto carriage = _mm_set1_epi8(0x0d);
    while (position + 16 <= text.size()) {
        const auto chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + position));
        auto matches = _mm_cmpeq_epi8(chunk, space);
        matches = _mm_or_si128(matches, _mm_cmpeq_epi8(chunk, tab));
        matches = _mm_or_si128(matches, _mm_cmpeq_epi8(chunk, newline));
        matches = _mm_or_si128(matches, _mm_cmpeq_epi8(chunk, carriage));
        const auto mask = static_cast<unsigned>(_mm_movemask_epi8(matches));
        if (mask == 0xffffU) {
            position += 16;
            continue;
        }
        position += static_cast<std::size_t>(std::countr_zero(mask ^ 0xffffU));
        return position;
    }
#endif
    while (position < text.size() && (text[position] == 0x20 || text[position] == 0x09 ||
                                      text[position] == 0x0a || text[position] == 0x0d)) {
        ++position;
    }
    return position;
}

inline std::size_t find_json_string_special(std::string_view text,
                                            std::size_t position) noexcept {
#if defined(__SSE2__)
    const auto* data = text.data();
    const auto quote = _mm_set1_epi8(0x22);
    const auto slash = _mm_set1_epi8(0x5c);
    const auto zero = _mm_setzero_si128();
    const auto high_bit = _mm_set1_epi8(static_cast<char>(0x80));
    const auto control_limit = _mm_set1_epi8(0x20);
    while (position + 16 <= text.size()) {
        const auto chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + position));
        auto matches = _mm_or_si128(_mm_cmpeq_epi8(chunk, quote),
                                    _mm_cmpeq_epi8(chunk, slash));
        const auto ascii = _mm_cmpeq_epi8(_mm_and_si128(chunk, high_bit), zero);
        const auto controls = _mm_and_si128(ascii, _mm_cmpgt_epi8(control_limit, chunk));
        matches = _mm_or_si128(matches, controls);
        const auto mask = static_cast<unsigned>(_mm_movemask_epi8(matches));
        if (mask == 0) {
            position += 16;
            continue;
        }
        position += static_cast<std::size_t>(std::countr_zero(mask));
        return position;
    }
#endif
    while (position < text.size()) {
        const auto character = static_cast<unsigned char>(text[position]);
        if (character == 0x22 || character == 0x5c || character < 0x20) {
            return position;
        }
        ++position;
    }
    return position;
}

inline std::string parse_error(std::size_t position, std::string_view message) {
    return "JSON parse error at byte " + std::to_string(position) + ": " +
           std::string(message);
}

// 索引语法、词素起始和字符串转义/控制字符边界。验证留在第二阶段，
// 使格式错误的输入保持逐字节解析器的错误顺序。
inline void buildStructuralIndexes(std::string_view text,
                                     std::vector<std::size_t>& indexes) {
    indexes.clear();
    bool in_string = false;
    bool escaped = false;
    bool token_start = true;
    for (std::size_t index = 0; index < text.size(); ++index) {
        const char character = text[index];
        if (in_string) {
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                indexes.push_back(index);
                escaped = true;
            } else if (character == '"') {
                indexes.push_back(index);
                in_string = false;
                token_start = false;
            } else if (static_cast<unsigned char>(character) < 0x20) {
                indexes.push_back(index);
            }
            continue;
        }
        if (character == '"') {
            indexes.push_back(index);
            in_string = true;
            token_start = false;
        } else if (character == '[' || character == ']' || character == '{' ||
                   character == '}' || character == ',' || character == ':') {
            indexes.push_back(index);
            token_start = true;
        } else if (character == ' ' || character == '\t' || character == '\n' ||
                   character == '\r') {
            token_start = true;
        } else if (token_start) {
            indexes.push_back(index);
            token_start = false;
        }
    }
}

struct Node {
    using array = std::vector<Node>;
    using object = std::map<std::string, Node>;
    using storage = std::variant<std::monostate, bool, std::int64_t, std::uint64_t,
                                 double, std::string, array, object>;

    storage value{};

    Node() = default;
    Node(const Node&) = default;
    Node(Node&&) noexcept = default;
    Node& operator=(const Node&) = default;
    Node& operator=(Node&&) noexcept = default;

    template <class T>
        requires(!std::same_as<BareT<T>, Node>)
    Node(T&& input) : value(std::forward<T>(input)) {}
};

using node = Node;

// 补充可复用解析器基准测试的内部状态。根节点在迭代间拥有已解析的文档，
// 以便 vector、string 和 map 存储可以回收而不改变公共类型化 API。
struct ParserContext {
    Node root{};
    std::vector<std::size_t> structural_indexes;
    std::string string_scratch;

    void reset() {
        structural_indexes.clear();
        string_scratch.clear();
    }
};

using parser_context = ParserContext;

struct ParseContext {
    const ParseOptions& options;
    std::size_t nodes = 0;
};

using parse_context = ParseContext;

template <bool Indexed = false>
class BasicParser {
public:
    BasicParser(std::string_view text, const ParseOptions& options,
                 ParserContext* reusable_context = nullptr,
                 std::span<const std::size_t> indexes = {})
        : text_(text), options_(options), context_{options},
          reusable_context_(reusable_context), indexes_(indexes) {}

    result<Node> parse() {
        position_ = 0;
        context_.nodes = 0;
        index_cursor_ = 0;
        position_ = skip_space();
        if (position_ == text_.size()) {
            return std::unexpected(parse_error(position_, "empty JSON input"));
        }
        auto value = parse_value(0);
        if (!value) {
            return std::unexpected(value.error());
        }
        position_ = skip_space();
        if (position_ != text_.size()) {
            return std::unexpected(parse_error(position_, "trailing characters"));
        }
        return value;
    }

    result<const Node*> parse_reusable() {
        if (reusable_context_ == nullptr) {
            return std::unexpected(std::string("JSON reusable parser context is missing"));
        }
        position_ = 0;
        context_.nodes = 0;
        index_cursor_ = 0;
        position_ = skip_space();
        if (position_ == text_.size()) {
            return std::unexpected(parse_error(position_, "empty JSON input"));
        }
        auto value = parse_value_into(reusable_context_->root, 0);
        if (!value) {
            return std::unexpected(value.error());
        }
        position_ = skip_space();
        if (position_ != text_.size()) {
            return std::unexpected(parse_error(position_, "trailing characters"));
        }
        return std::addressof(reusable_context_->root);
    }

private:

    std::size_t next_index() {
        while (index_cursor_ < indexes_.size() && indexes_[index_cursor_] < position_) {
            ++index_cursor_;
        }
        return index_cursor_ < indexes_.size() ? indexes_[index_cursor_] : text_.size();
    }

    std::size_t skip_space() {
        if constexpr (!Indexed) {
            return skip_json_space(text_, position_);
        } else {
            if (position_ == text_.size()) return position_;
            const char character = text_[position_];
            // Do not jump over invalid suffixes such as the 'x' in "1x".
            if (character != ' ' && character != '\t' && character != '\n' &&
                character != '\r') return position_;
            return next_index();
        }
    }

    std::size_t string_special() {
        if constexpr (Indexed) return next_index();
        else return find_json_string_special(text_, position_);
    }

    bool starts_with(std::string_view token) const {
        return text_.substr(position_).starts_with(token);
    }

    result<Node> parse_value(std::size_t depth) {
        position_ = skip_space();
        if (position_ >= text_.size()) {
            return std::unexpected(parse_error(position_, "missing value"));
        }
        if (depth >= options_.max_depth) {
            return std::unexpected(parse_error(position_, "nesting depth exceeds configured limit"));
        }
        const char character = text_[position_];
        result<Node> parsed;
        if (character == 'n') {
            if (!starts_with("null")) {
                return std::unexpected(parse_error(position_, "invalid literal"));
            }
            position_ += 4;
            parsed = Node{};
        } else if (character == 't') {
            if (!starts_with("true")) {
                return std::unexpected(parse_error(position_, "invalid literal"));
            }
            position_ += 4;
            parsed = Node{true};
        } else if (character == 'f') {
            if (!starts_with("false")) {
                return std::unexpected(parse_error(position_, "invalid literal"));
            }
            position_ += 5;
            parsed = Node{false};
        } else if (character == '"') {
            auto string = parse_string();
            if (!string) {
                return std::unexpected(string.error());
            }
            parsed = Node{std::move(*string)};
        } else if (character == '[') {
            parsed = parse_array(depth);
        } else if (character == '{') {
            parsed = parse_object(depth);
        } else if (character == '-' || ascii_digit(character)) {
            parsed = parse_number();
        } else {
            return std::unexpected(parse_error(position_, "unexpected character"));
        }
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        if (context_.nodes >= options_.max_nodes) {
            return std::unexpected(parse_error(position_, "node count exceeds configured limit"));
        }
        ++context_.nodes;
        return parsed;
    }

    result<void> parse_value_into(Node& output, std::size_t depth) {
        position_ = skip_space();
        if (position_ >= text_.size()) {
            return std::unexpected(parse_error(position_, "missing value"));
        }
        if (depth >= options_.max_depth) {
            return std::unexpected(parse_error(position_, "nesting depth exceeds configured limit"));
        }
        const char character = text_[position_];
        if (character == 'n') {
            if (!starts_with("null")) {
                return std::unexpected(parse_error(position_, "invalid literal"));
            }
            position_ += 4;
            output.value = std::monostate{};
        } else if (character == 't') {
            if (!starts_with("true")) {
                return std::unexpected(parse_error(position_, "invalid literal"));
            }
            position_ += 4;
            output.value = true;
        } else if (character == 'f') {
            if (!starts_with("false")) {
                return std::unexpected(parse_error(position_, "invalid literal"));
            }
            position_ += 5;
            output.value = false;
        } else if (character == '"') {
            auto* string = std::get_if<std::string>(&output.value);
            if (string == nullptr) {
                output.value = std::string{};
                string = std::get_if<std::string>(&output.value);
            }
            auto parsed = parse_string_into(*string);
            if (!parsed) return std::unexpected(parsed.error());
        } else if (character == '[') {
            auto parsed = parse_array_reusable(output, depth);
            if (!parsed) return std::unexpected(parsed.error());
        } else if (character == '{') {
            auto parsed = parse_object_reusable(output, depth);
            if (!parsed) return std::unexpected(parsed.error());
        } else if (character == '-' || ascii_digit(character)) {
            auto parsed = parse_number();
            if (!parsed) return std::unexpected(parsed.error());
            output = std::move(*parsed);
        } else {
            return std::unexpected(parse_error(position_, "unexpected character"));
        }
        if (context_.nodes >= options_.max_nodes) {
            return std::unexpected(parse_error(position_, "node count exceeds configured limit"));
        }
        ++context_.nodes;
        return {};
    }

    result<void> parse_string_into(std::string& output) {
        if (position_ >= text_.size() || text_[position_] != '"') {
            return std::unexpected(parse_error(position_, "expected string"));
        }
        ++position_;
        output.clear();
        while (position_ < text_.size()) {
            const auto special = string_special();
            if (special > position_) {
                const auto count = special - position_;
                if (count > options_.max_string_bytes ||
                    output.size() > options_.max_string_bytes - count) {
                    return std::unexpected(parse_error(position_, "string exceeds configured size limit"));
                }
                if (output.empty()) output.reserve(count);
                output.append(text_.data() + position_, count);
                position_ = special;
            }
            if (position_ >= text_.size()) break;
            const char character = text_[position_++];
            if (character == '"') return {};
            if (character == '\\') {
                if (position_ >= text_.size()) {
                    return std::unexpected(parse_error(position_, "unterminated escape"));
                }
                const char escaped = text_[position_++];
                switch (escaped) {
                    case '"': output.push_back('"'); break;
                    case '\\': output.push_back('\\'); break;
                    case '/': output.push_back('/'); break;
                    case 'b': output.push_back('\b'); break;
                    case 'f': output.push_back('\f'); break;
                    case 'n': output.push_back('\n'); break;
                    case 'r': output.push_back('\r'); break;
                    case 't': output.push_back('\t'); break;
                    case 'u': {
                        std::uint32_t code_point = 0;
                        for (unsigned index = 0; index < 4; ++index) {
                            if (position_ >= text_.size()) {
                                return std::unexpected(parse_error(position_, "truncated Unicode escape"));
                            }
                            unsigned digit = 0;
                            if (!hex_digit(text_[position_++], digit)) {
                                return std::unexpected(parse_error(position_ - 1, "invalid Unicode escape"));
                            }
                            code_point = (code_point << 4) | digit;
                        }
                        if (code_point >= 0xd800 && code_point <= 0xdbff) {
                            if (position_ + 6 > text_.size() || text_[position_] != '\\' ||
                                text_[position_ + 1] != 'u') {
                                return std::unexpected(parse_error(position_, "unpaired high surrogate"));
                            }
                            position_ += 2;
                            std::uint32_t low = 0;
                            for (unsigned index = 0; index < 4; ++index) {
                                unsigned digit = 0;
                                if (position_ >= text_.size() || !hex_digit(text_[position_++], digit)) {
                                    return std::unexpected(parse_error(position_ - 1, "invalid low surrogate"));
                                }
                                low = (low << 4) | digit;
                            }
                            if (low < 0xdc00 || low > 0xdfff) {
                                return std::unexpected(parse_error(position_, "invalid low surrogate"));
                            }
                            code_point = 0x10000 + ((code_point - 0xd800) << 10) +
                                         (low - 0xdc00);
                        } else if (code_point >= 0xdc00 && code_point <= 0xdfff) {
                            return std::unexpected(parse_error(position_, "unpaired low surrogate"));
                        }
                        auto appended = append_utf8(output, code_point);
                        if (!appended) return std::unexpected(parse_error(position_, appended.error()));
                        break;
                    }
                    default:
                        return std::unexpected(parse_error(position_ - 1, "unsupported escape"));
                }
            } else if (static_cast<unsigned char>(character) < 0x20) {
                return std::unexpected(parse_error(position_ - 1, "control character in string"));
            } else {
                output.push_back(character);
            }
            if (output.size() > options_.max_string_bytes) {
                return std::unexpected(parse_error(position_, "string exceeds configured size limit"));
            }
        }
        return std::unexpected(parse_error(position_, "unterminated string"));
    }

    result<void> parse_array_reusable(Node& output, std::size_t depth) {
        auto* values = std::get_if<Node::array>(&output.value);
        if (values == nullptr) {
            output.value = Node::array{};
            values = std::get_if<Node::array>(&output.value);
        }
        ++position_;
        position_ = skip_space();
        if (position_ < text_.size() && text_[position_] == ']') {
            values->resize(0);
            ++position_;
            return {};
        }
        std::size_t index = 0;
        while (true) {
            if (index >= options_.max_array_items) {
                return std::unexpected(parse_error(position_, "array item count exceeds configured limit"));
            }
            if (index < values->size()) {
                auto parsed = parse_value_into((*values)[index], depth + 1);
                if (!parsed) return std::unexpected(parsed.error());
            } else {
                values->emplace_back();
                auto parsed = parse_value_into(values->back(), depth + 1);
                if (!parsed) return std::unexpected(parsed.error());
            }
            ++index;
            position_ = skip_space();
            if (position_ >= text_.size()) {
                return std::unexpected(parse_error(position_, "unterminated array"));
            }
            if (text_[position_] == ']') {
                values->resize(index);
                ++position_;
                return {};
            }
            if (text_[position_] != ',') {
                return std::unexpected(parse_error(position_, "expected ',' or ']'"));
            }
            ++position_;
            position_ = skip_space();
            if (position_ < text_.size() && text_[position_] == ']') {
                return std::unexpected(parse_error(position_, "trailing comma in array"));
            }
        }
    }

    result<void> parse_object_reusable(Node& output, std::size_t depth) {
        auto* values = std::get_if<Node::object>(&output.value);
        if (values == nullptr) {
            output.value = Node::object{};
            values = std::get_if<Node::object>(&output.value);
        }
        // Reinsert only keys seen in this document. Unused old Nodes are
        // destroyed on exit, and lookup remains logarithmic for wide objects.
        Node::object previous;
        previous.swap(*values);
        ++position_;
        position_ = skip_space();
        if (position_ < text_.size() && text_[position_] == '}') {
            ++position_;
            return {};
        }
        while (true) {
            if (values->size() >= options_.max_object_members) {
                return std::unexpected(parse_error(position_, "object member count exceeds configured limit"));
            }
            if (position_ >= text_.size() || text_[position_] != '"') {
                return std::unexpected(parse_error(position_, "object keys must be strings"));
            }
            reusable_context_->string_scratch.clear();
            auto parsed_key = parse_string_into(reusable_context_->string_scratch);
            if (!parsed_key) return std::unexpected(parsed_key.error());
            const auto& key = reusable_context_->string_scratch;
            if (key.size() > options_.max_key_bytes) {
                return std::unexpected(parse_error(position_, "object key exceeds configured size limit"));
            }
            position_ = skip_space();
            if (position_ >= text_.size() || text_[position_] != ':') {
                return std::unexpected(parse_error(position_, "expected ':' after object key"));
            }
            ++position_;
            if (values->find(key) != values->end()) {
                // Match the default parser's value validation and error offset
                // before rejecting a duplicate, including malformed values.
                auto child = parse_value(depth + 1);
                if (!child) return std::unexpected(child.error());
                return std::unexpected(parse_error(position_, "duplicate object key"));
            }
            Node* child = nullptr;
            auto handle = previous.extract(key);
            if (handle) {
                child = std::addressof(values->insert(std::move(handle)).position->second);
            } else {
                child = std::addressof(values->try_emplace(key).first->second);
            }
            // The map owns the key before recursion can overwrite scratch.
            auto parsed = parse_value_into(*child, depth + 1);
            if (!parsed) return std::unexpected(parsed.error());
            position_ = skip_space();
            if (position_ >= text_.size()) {
                return std::unexpected(parse_error(position_, "unterminated object"));
            }
            if (text_[position_] == '}') {
                ++position_;
                return {};
            }
            if (text_[position_] != ',') {
                return std::unexpected(parse_error(position_, "expected ',' or '}'"));
            }
            ++position_;
            position_ = skip_space();
            if (position_ < text_.size() && text_[position_] == '}') {
                return std::unexpected(parse_error(position_, "trailing comma in object"));
            }
        }
    }

    result<std::string> parse_string() {
        if (position_ >= text_.size() || text_[position_] != '"') {
            return std::unexpected(parse_error(position_, "expected string"));
        }
        ++position_;
        std::string output;
        while (position_ < text_.size()) {
            const auto special = string_special();
            if (special > position_) {
                const auto count = special - position_;
                if (count > options_.max_string_bytes ||
                    output.size() > options_.max_string_bytes - count) {
                    return std::unexpected(parse_error(position_, "string exceeds configured size limit"));
                }
                if (output.empty()) {
                    output.reserve(count);
                }
                output.append(text_.data() + position_, count);
                position_ = special;
            }
            if (position_ >= text_.size()) {
                break;
            }
            const char character = text_[position_++];
            if (character == '"') {
                // The complete JSON input is validated before parsing. Raw bytes
                // are therefore valid UTF-8; escapes are validated by append_utf8.
                return output;
            }
            if (character == '\\') {
                if (position_ >= text_.size()) {
                    return std::unexpected(parse_error(position_, "unterminated escape"));
                }
                const char escaped = text_[position_++];
                switch (escaped) {
                    case '"': output.push_back('"'); break;
                    case '\\': output.push_back('\\'); break;
                    case '/': output.push_back('/'); break;
                    case 'b': output.push_back('\b'); break;
                    case 'f': output.push_back('\f'); break;
                    case 'n': output.push_back('\n'); break;
                    case 'r': output.push_back('\r'); break;
                    case 't': output.push_back('\t'); break;
                    case 'u': {
                        std::uint32_t code_point = 0;
                        for (unsigned index = 0; index < 4; ++index) {
                            if (position_ >= text_.size()) {
                                return std::unexpected(parse_error(position_, "truncated Unicode escape"));
                            }
                            unsigned digit = 0;
                            if (!hex_digit(text_[position_++], digit)) {
                                return std::unexpected(parse_error(position_ - 1, "invalid Unicode escape"));
                            }
                            code_point = (code_point << 4) | digit;
                        }
                        if (code_point >= 0xd800 && code_point <= 0xdbff) {
                            if (position_ + 6 > text_.size() || text_[position_] != '\\' ||
                                text_[position_ + 1] != 'u') {
                                return std::unexpected(parse_error(position_, "unpaired high surrogate"));
                            }
                            position_ += 2;
                            std::uint32_t low = 0;
                            for (unsigned index = 0; index < 4; ++index) {
                                unsigned digit = 0;
                                if (position_ >= text_.size() || !hex_digit(text_[position_++], digit)) {
                                    return std::unexpected(parse_error(position_ - 1, "invalid low surrogate"));
                                }
                                low = (low << 4) | digit;
                            }
                            if (low < 0xdc00 || low > 0xdfff) {
                                return std::unexpected(parse_error(position_, "invalid low surrogate"));
                            }
                            code_point = 0x10000 + ((code_point - 0xd800) << 10) +
                                         (low - 0xdc00);
                        } else if (code_point >= 0xdc00 && code_point <= 0xdfff) {
                            return std::unexpected(parse_error(position_, "unpaired low surrogate"));
                        }
                        auto appended = append_utf8(output, code_point);
                        if (!appended) {
                            return std::unexpected(parse_error(position_, appended.error()));
                        }
                        break;
                    }
                    default:
                        return std::unexpected(parse_error(position_ - 1, "unsupported escape"));
                }
            } else if (static_cast<unsigned char>(character) < 0x20) {
                return std::unexpected(parse_error(position_ - 1, "control character in string"));
            } else {
                output.push_back(character);
            }
            if (output.size() > options_.max_string_bytes) {
                return std::unexpected(parse_error(position_, "string exceeds configured size limit"));
            }
        }
        return std::unexpected(parse_error(position_, "unterminated string"));
    }

    result<Node> parse_number() {
        const std::size_t start = position_;
        if (text_[position_] == '-') {
            ++position_;
            if (position_ >= text_.size()) {
                return std::unexpected(parse_error(position_, "incomplete number"));
            }
        }
        if (text_[position_] == '0') {
            ++position_;
            if (position_ < text_.size() && ascii_digit(text_[position_])) {
                return std::unexpected(parse_error(position_, "leading zero in number"));
            }
        } else if (text_[position_] >= '1' && text_[position_] <= '9') {
            while (position_ < text_.size() && ascii_digit(text_[position_])) {
                ++position_;
            }
        } else {
            return std::unexpected(parse_error(position_, "invalid number"));
        }

        bool floating = false;
        if (position_ < text_.size() && text_[position_] == '.') {
            floating = true;
            ++position_;
            const auto fraction_start = position_;
            while (position_ < text_.size() && ascii_digit(text_[position_])) {
                ++position_;
            }
            if (position_ == fraction_start) {
                return std::unexpected(parse_error(position_, "fraction has no digits"));
            }
        }
        if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
            floating = true;
            ++position_;
            if (position_ < text_.size() &&
                (text_[position_] == '+' || text_[position_] == '-')) {
                ++position_;
            }
            const auto exponent_start = position_;
            while (position_ < text_.size() && ascii_digit(text_[position_])) {
                ++position_;
            }
            if (position_ == exponent_start) {
                return std::unexpected(parse_error(position_, "exponent has no digits"));
            }
        }

        const auto lexeme = text_.substr(start, position_ - start);
        if (floating) {
            double value = 0.0;
            const auto [end, error] = std::from_chars(
                lexeme.data(), lexeme.data() + lexeme.size(), value,
                std::chars_format::general);
            if (error != std::errc{} || end != lexeme.data() + lexeme.size() ||
                !std::isfinite(value)) {
                return std::unexpected(parse_error(start, "number is outside the finite range"));
            }
            return Node{value};
        }

        const bool negative = !lexeme.empty() && lexeme.front() == '-';
        const auto digits = negative ? lexeme.substr(1) : lexeme;
        std::uint64_t magnitude = 0;
        const auto [end, error] = std::from_chars(
            digits.data(), digits.data() + digits.size(), magnitude, 10);
        if (error != std::errc{} || end != digits.data() + digits.size()) {
            return std::unexpected(parse_error(start, "integer is outside the 64-bit range"));
        }
        constexpr auto positive_limit =
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
        constexpr auto negative_limit = positive_limit + 1;
        if (negative) {
            if (magnitude > negative_limit) {
                return std::unexpected(parse_error(start, "integer is outside the 64-bit range"));
            }
            if (magnitude == negative_limit) {
                return Node{std::numeric_limits<std::int64_t>::min()};
            }
            return Node{-static_cast<std::int64_t>(magnitude)};
        }
        if (magnitude <= positive_limit) {
            return Node{static_cast<std::int64_t>(magnitude)};
        }
        return Node{magnitude};
    }

    result<Node> parse_array(std::size_t depth) {
        ++position_;
        Node::array values;
        position_ = skip_space();
        if (position_ < text_.size() && text_[position_] == ']') {
            ++position_;
            return Node{std::move(values)};
        }
        while (true) {
            if (values.size() >= options_.max_array_items) {
                return std::unexpected(parse_error(position_, "array item count exceeds configured limit"));
            }
            auto value = parse_value(depth + 1);
            if (!value) {
                return std::unexpected(value.error());
            }
            values.push_back(std::move(*value));
            position_ = skip_space();
            if (position_ >= text_.size()) {
                return std::unexpected(parse_error(position_, "unterminated array"));
            }
            if (text_[position_] == ']') {
                ++position_;
                return Node{std::move(values)};
            }
            if (text_[position_] != ',') {
                return std::unexpected(parse_error(position_, "expected ',' or ']'"));
            }
            ++position_;
            position_ = skip_space();
            if (position_ < text_.size() && text_[position_] == ']') {
                return std::unexpected(parse_error(position_, "trailing comma in array"));
            }
        }
    }

    result<Node> parse_object(std::size_t depth) {
        ++position_;
        Node::object values;
        position_ = skip_space();
        if (position_ < text_.size() && text_[position_] == '}') {
            ++position_;
            return Node{std::move(values)};
        }
        while (true) {
            if (values.size() >= options_.max_object_members) {
                return std::unexpected(parse_error(position_, "object member count exceeds configured limit"));
            }
            if (position_ >= text_.size() || text_[position_] != '"') {
                return std::unexpected(parse_error(position_, "object keys must be strings"));
            }
            auto key = parse_string();
            if (!key) {
                return std::unexpected(key.error());
            }
            if (key->size() > options_.max_key_bytes) {
                return std::unexpected(parse_error(position_, "object key exceeds configured size limit"));
            }
            position_ = skip_space();
            if (position_ >= text_.size() || text_[position_] != ':') {
                return std::unexpected(parse_error(position_, "expected ':' after object key"));
            }
            ++position_;
            auto value = parse_value(depth + 1);
            if (!value) {
                return std::unexpected(value.error());
            }
            if (!values.emplace(std::move(*key), std::move(*value)).second) {
                return std::unexpected(parse_error(position_, "duplicate object key"));
            }
            position_ = skip_space();
            if (position_ >= text_.size()) {
                return std::unexpected(parse_error(position_, "unterminated object"));
            }
            if (text_[position_] == '}') {
                ++position_;
                return Node{std::move(values)};
            }
            if (text_[position_] != ',') {
                return std::unexpected(parse_error(position_, "expected ',' or '}'"));
            }
            ++position_;
            position_ = skip_space();
            if (position_ < text_.size() && text_[position_] == '}') {
                return std::unexpected(parse_error(position_, "trailing comma in object"));
            }
        }
    }

    std::string_view text_;
    const ParseOptions& options_;
    ParseContext context_;
    ParserContext* reusable_context_ = nullptr;
    std::span<const std::size_t> indexes_;
    std::size_t index_cursor_ = 0;
    std::size_t position_ = 0;
};

using Parser = BasicParser<>;

// 返回的节点属于 context，会在下一次解析时失效，包括失败的解析。
// 保留的容量是文档存储，而非缓存。
inline result<const Node*> parseDocumentReusable(std::string_view text,
                                                   const ParseOptions& options,
                                                   ParserContext& context,
                                                   bool indexed = false) {
    context.reset();
    if (text.size() > options.max_input_bytes) {
        return std::unexpected(std::string("JSON input exceeds configured size limit"));
    }
    if (!valid_utf8(text)) {
        return std::unexpected(std::string("JSON input is not valid UTF-8"));
    }
    if (indexed) {
        buildStructuralIndexes(text, context.structural_indexes);
        BasicParser<true> document_parser{text, options, std::addressof(context),
                                           context.structural_indexes};
        return document_parser.parse_reusable();
    } else {
        Parser document_parser{text, options, std::addressof(context)};
        return document_parser.parse_reusable();
    }
}

inline result<Node> parseDocumentIndexed(std::string_view text,
                                          const ParseOptions& options,
                                          ParserContext& context) {
    context.reset();
    if (text.size() > options.max_input_bytes) {
        return std::unexpected(std::string("JSON input exceeds configured size limit"));
    }
    if (!valid_utf8(text)) {
        return std::unexpected(std::string("JSON input is not valid UTF-8"));
    }
    buildStructuralIndexes(text, context.structural_indexes);
    BasicParser<true> document_parser{text, options, nullptr, context.structural_indexes};
    return document_parser.parse();
}

// 保持公共类型化 API 和基准测试的仅解析阶段在相同的验证和 DOM 构建路径上。
inline result<Node> parseDocument(std::string_view text, const ParseOptions& options) {
    if (text.size() > options.max_input_bytes) {
        return std::unexpected(std::string("JSON input exceeds configured size limit"));
    }
    if (!valid_utf8(text)) {
        return std::unexpected(std::string("JSON input is not valid UTF-8"));
    }
    Parser document_parser{text, options};
    return document_parser.parse();
}

inline bool valid_date_value(const date& value) {
    if (value.year < 0 || value.year > 9999 || value.month == 0 || value.month > 12 ||
        value.day == 0) {
        return false;
    }
    constexpr std::array<unsigned, 12> days_per_month{
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const auto year = static_cast<unsigned>(value.year);
    const bool leap_year = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return value.day <= days_per_month[value.month - 1] +
                            (value.month == 2 && leap_year ? 1 : 0);
}

inline bool valid_time_value(const time& value) {
    if (value.hour > 23 || value.minute > 59 || value.second > 59) {
        return false;
    }
    for (const char character : value.fractional_second) {
        if (!ascii_digit(character)) {
            return false;
        }
    }
    return true;
}

inline bool parse_fixed_decimal(std::string_view text, std::size_t offset,
                                std::size_t width, unsigned& output) {
    if (offset + width > text.size()) {
        return false;
    }
    unsigned value = 0;
    for (std::size_t index = 0; index < width; ++index) {
        if (!ascii_digit(text[offset + index])) {
            return false;
        }
        value = value * 10 + static_cast<unsigned>(text[offset + index] - '0');
    }
    output = value;
    return true;
}

inline result<date> parse_date_text(std::string_view text) {
    unsigned year = 0;
    unsigned month = 0;
    unsigned day = 0;
    if (text.size() != 10 || text[4] != '-' || text[7] != '-' ||
        !parse_fixed_decimal(text, 0, 4, year) ||
        !parse_fixed_decimal(text, 5, 2, month) ||
        !parse_fixed_decimal(text, 8, 2, day)) {
        return std::unexpected(std::string("invalid JSON date"));
    }
    date value{static_cast<int>(year), month, day};
    if (!valid_date_value(value)) {
        return std::unexpected(std::string("JSON date is outside the calendar range"));
    }
    return value;
}

inline result<std::pair<time, std::size_t>> parse_time_prefix(std::string_view text) {
    unsigned hour = 0;
    unsigned minute = 0;
    unsigned second = 0;
    if (text.size() < 8 || text[2] != ':' || text[5] != ':' ||
        !parse_fixed_decimal(text, 0, 2, hour) ||
        !parse_fixed_decimal(text, 3, 2, minute) ||
        !parse_fixed_decimal(text, 6, 2, second) || hour > 23 || minute > 59 ||
        second > 59) {
        return std::unexpected(std::string("invalid JSON time"));
    }
    std::size_t position = 8;
    std::string fraction;
    if (position < text.size() && text[position] == '.') {
        const auto start = ++position;
        while (position < text.size() && ascii_digit(text[position])) {
            ++position;
        }
        if (position == start) {
            return std::unexpected(std::string("JSON time has an empty fractional second"));
        }
        fraction = std::string(text.substr(start, position - start));
    }
    return std::pair{time{hour, minute, second, std::move(fraction)}, position};
}

inline result<local_date_time> parse_local_date_time_text(std::string_view text) {
    if (text.size() < 19 || (text[10] != 'T' && text[10] != 't' && text[10] != ' ')) {
        return std::unexpected(std::string("invalid JSON local date-time"));
    }
    auto parsed_date = parse_date_text(text.substr(0, 10));
    if (!parsed_date) {
        return std::unexpected(parsed_date.error());
    }
    auto parsed_time = parse_time_prefix(text.substr(11));
    if (!parsed_time || 11 + parsed_time->second != text.size()) {
        return std::unexpected(parsed_time ? std::string("invalid JSON local date-time")
                                           : parsed_time.error());
    }
    return local_date_time{std::move(*parsed_date), std::move(parsed_time->first)};
}

inline result<offset_date_time> parse_offset_date_time_text(std::string_view text) {
    if (text.size() < 20 || (text[10] != 'T' && text[10] != 't' && text[10] != ' ')) {
        return std::unexpected(std::string("invalid JSON offset date-time"));
    }
    auto parsed_date = parse_date_text(text.substr(0, 10));
    if (!parsed_date) {
        return std::unexpected(parsed_date.error());
    }
    auto parsed_time = parse_time_prefix(text.substr(11));
    if (!parsed_time) {
        return std::unexpected(parsed_time.error());
    }
    const auto offset = 11 + parsed_time->second;
    if (offset >= text.size()) {
        return std::unexpected(std::string("invalid JSON offset date-time"));
    }
    int offset_minutes = 0;
    const auto suffix = text.substr(offset);
    if (suffix == "Z" || suffix == "z") {
        offset_minutes = 0;
    } else {
        unsigned hour = 0;
        unsigned minute = 0;
        if (suffix.size() != 6 || (suffix[0] != '+' && suffix[0] != '-') ||
            suffix[3] != ':' || !parse_fixed_decimal(suffix, 1, 2, hour) ||
            !parse_fixed_decimal(suffix, 4, 2, minute) || hour > 23 || minute > 59) {
            return std::unexpected(std::string("invalid JSON offset date-time"));
        }
        offset_minutes = static_cast<int>(hour * 60 + minute);
        if (suffix[0] == '-') {
            offset_minutes = -offset_minutes;
        }
    }
    return offset_date_time{
        local_date_time{std::move(*parsed_date), std::move(parsed_time->first)}, offset_minutes};
}

inline void append_padded_decimal(std::string& output, unsigned value, unsigned width) {
    std::array<char, 16> digits{};
    for (unsigned index = 0; index < width; ++index) {
        digits[width - index - 1] = static_cast<char>('0' + value % 10);
        value /= 10;
    }
    output.append(digits.data(), width);
}

inline std::string temporal_text(const date& value) {
    std::string output;
    append_padded_decimal(output, static_cast<unsigned>(value.year), 4);
    output.push_back('-');
    append_padded_decimal(output, value.month, 2);
    output.push_back('-');
    append_padded_decimal(output, value.day, 2);
    return output;
}

inline std::string temporal_text(const time& value) {
    std::string output;
    append_padded_decimal(output, value.hour, 2);
    output.push_back(':');
    append_padded_decimal(output, value.minute, 2);
    output.push_back(':');
    append_padded_decimal(output, value.second, 2);
    if (!value.fractional_second.empty()) {
        output.push_back('.');
        output += value.fractional_second;
    }
    return output;
}

inline std::string temporal_text(const local_date_time& value) {
    std::string output = temporal_text(value.date_part);
    output.push_back('T');
    output += temporal_text(value.time_part);
    return output;
}

inline std::string temporal_text(const offset_date_time& value) {
    std::string output = temporal_text(value.local);
    if (value.offset_minutes == 0) {
        output.push_back('Z');
        return output;
    }
    const int magnitude = std::abs(value.offset_minutes);
    output.push_back(value.offset_minutes < 0 ? '-' : '+');
    append_padded_decimal(output, static_cast<unsigned>(magnitude / 60), 2);
    output.push_back(':');
    append_padded_decimal(output, static_cast<unsigned>(magnitude % 60), 2);
    return output;
}

struct EncodeContext {
    const SerializeOptions& options;
    std::size_t nodes = 0;
};

template <class T>
result<Node> encodeValue(const T& input, EncodeContext& context, std::size_t depth);

template <class T>
result<Node> encodeValue(const T& input, EncodeContext& context, std::size_t depth) {
    using U = BareT<T>;
    if (depth >= context.options.max_depth) {
        return std::unexpected(std::string("JSON nesting depth exceeds configured limit"));
    }
    if (context.nodes >= context.options.max_nodes) {
        return std::unexpected(std::string("JSON node count exceeds configured limit"));
    }
    ++context.nodes;

    if constexpr (OptionalTraits<U>::value) {
        if (!input) {
            return Node{};
        }
        return encodeValue(*input, context, depth + 1);
    } else if constexpr (InlineTableTraits<U>::value) {
        return encodeValue(input.value, context, depth + 1);
    } else if constexpr (std::same_as<U, date>) {
        if (!valid_date_value(input)) {
            return std::unexpected(std::string("invalid json::date value"));
        }
        return Node{temporal_text(input)};
    } else if constexpr (std::same_as<U, time>) {
        if (!valid_time_value(input)) {
            return std::unexpected(std::string("invalid json::time value"));
        }
        return Node{temporal_text(input)};
    } else if constexpr (std::same_as<U, local_date_time>) {
        if (!valid_date_value(input.date_part) || !valid_time_value(input.time_part)) {
            return std::unexpected(std::string("invalid json::local_date_time value"));
        }
        return Node{temporal_text(input)};
    } else if constexpr (std::same_as<U, offset_date_time>) {
        if (!valid_date_value(input.local.date_part) || !valid_time_value(input.local.time_part) ||
            input.offset_minutes < -1439 || input.offset_minutes > 1439) {
            return std::unexpected(std::string("invalid json::offset_date_time value"));
        }
        return Node{temporal_text(input)};
    } else if constexpr (std::same_as<U, std::string>) {
        if (input.size() > context.options.max_string_bytes || !valid_utf8(input)) {
            return std::unexpected(std::string("invalid or oversized UTF-8 JSON string"));
        }
        return Node{input};
    } else if constexpr (std::same_as<U, std::string_view>) {
        if (input.size() > context.options.max_string_bytes || !valid_utf8(input)) {
            return std::unexpected(std::string("invalid or oversized UTF-8 JSON string"));
        }
        return Node{std::string(input)};
    } else if constexpr (std::same_as<U, std::nullptr_t>) {
        return Node{};
    } else if constexpr (std::same_as<U, bool>) {
        return Node{input};
    } else if constexpr (std::is_integral_v<U>) {
        if constexpr (std::is_signed_v<U>) {
            if constexpr (std::numeric_limits<U>::digits >
                          std::numeric_limits<std::int64_t>::digits) {
                if (input < static_cast<U>(std::numeric_limits<std::int64_t>::min()) ||
                    input > static_cast<U>(std::numeric_limits<std::int64_t>::max())) {
                    return std::unexpected(std::string("signed integer is outside JSON int64 range"));
                }
            }
            return Node{static_cast<std::int64_t>(input)};
        } else {
            if constexpr (std::numeric_limits<U>::digits >
                          std::numeric_limits<std::uint64_t>::digits) {
                if (input > static_cast<U>(std::numeric_limits<std::uint64_t>::max())) {
                    return std::unexpected(std::string("unsigned integer is outside JSON uint64 range"));
                }
            }
            return Node{static_cast<std::uint64_t>(input)};
        }
    } else if constexpr (std::is_floating_point_v<U>) {
        const double value = static_cast<double>(input);
        if (!std::isfinite(value)) {
            if (context.options.non_finite == NonFinitePolicy::null_value) {
                return Node{};
            }
            return std::unexpected(std::string("JSON does not permit non-finite numbers"));
        }
        return Node{value};
    } else if constexpr (std::is_enum_v<U>) {
        return encodeValue(static_cast<std::underlying_type_t<U>>(input), context, depth + 1);
    } else if constexpr (VectorTraits<U>::value) {
        if (input.size() > context.options.max_array_items) {
            return std::unexpected(std::string("JSON array item count exceeds configured limit"));
        }
        Node::array values;
        values.reserve(input.size());
        for (const auto& element : input) {
            using Element = typename VectorTraits<U>::value_type;
            auto encoded = [&]() {
                if constexpr (std::constructible_from<Element, decltype(element)>) {
                    return encodeValue(static_cast<Element>(element), context, depth + 1);
                } else {
                    return encodeValue(element, context, depth + 1);
                }
            }();
            if (!encoded) {
                return std::unexpected(encoded.error());
            }
            values.push_back(std::move(*encoded));
        }
        return Node{std::move(values)};
    } else if constexpr (ArrayTraits<U>::value) {
        if (ArrayTraits<U>::size > context.options.max_array_items) {
            return std::unexpected(std::string("JSON array item count exceeds configured limit"));
        }
        Node::array values;
        values.reserve(ArrayTraits<U>::size);
        for (const auto& element : input) {
            auto encoded = encodeValue(element, context, depth + 1);
            if (!encoded) {
                return std::unexpected(encoded.error());
            }
            values.push_back(std::move(*encoded));
        }
        return Node{std::move(values)};
    } else if constexpr (MapTraits<U>::value) {
        if (input.size() > context.options.max_object_members) {
            return std::unexpected(std::string("JSON object member count exceeds configured limit"));
        }
        Node::object values;
        for (const auto& [key, element] : input) {
            if (key.size() > context.options.max_key_bytes || !valid_utf8(key)) {
                return std::unexpected(std::string("invalid or oversized UTF-8 JSON object key"));
            }
            auto encoded = encodeValue(element, context, depth + 1);
            if (!encoded) {
                return std::unexpected(encoded.error());
            }
            values.emplace(key, std::move(*encoded));
        }
        return Node{std::move(values)};
    } else if constexpr (Reflectable<U>) {
        Node::object values;
        std::string failure;
        bool failed = false;
        for_each_field(input, [&](const auto& descriptor, const auto& object) {
            if (failed) {
                return;
            }
            if (descriptor.name.size() > context.options.max_key_bytes ||
                !valid_utf8(descriptor.name)) {
                failed = true;
                failure = "invalid or oversized UTF-8 reflected JSON key";
                return;
            }
            auto encoded = encodeValue(descriptor.get(object), context, depth + 1);
            if (!encoded) {
                failed = true;
                failure = encoded.error();
                return;
            }
            if (!values.emplace(std::string(descriptor.name), std::move(*encoded)).second) {
                failed = true;
                failure = "duplicate reflected field: " + std::string(descriptor.name);
            }
            if (values.size() > context.options.max_object_members) {
                failed = true;
                failure = "JSON object member count exceeds configured limit";
            }
        });
        if (failed) {
            return std::unexpected(std::move(failure));
        }
        return Node{std::move(values)};
    } else {
        return std::unexpected(std::string("unsupported type in JSON serializer"));
    }
}

struct Writer {
    std::string output;
    std::size_t limit;

    bool append(std::string_view text) {
        if (text.size() > limit || output.size() > limit - text.size()) {
            return false;
        }
        output.append(text);
        return true;
    }

    bool push(char character) {
        if (output.size() >= limit) {
            return false;
        }
        output.push_back(character);
        return true;
    }
};

inline bool appendIndent(Writer& output, std::size_t depth, const SerializeOptions& options) {
    if (!options.pretty) {
        return true;
    }
    if (options.indent_width != 0 && depth >
        (std::numeric_limits<std::size_t>::max() / options.indent_width)) {
        return false;
    }
    const auto spaces = depth * options.indent_width;
    for (std::size_t index = 0; index < spaces; ++index) {
        if (!output.push(' ')) {
            return false;
        }
    }
    return true;
}

inline bool appendString(std::string_view value, Writer& output) {
    if (!output.push('"')) {
        return false;
    }
    constexpr char hex[] = "0123456789abcdef";
    for (const char character : value) {
        switch (character) {
            case '"': if (!output.append("\\\"")) return false; break;
            case '\\': if (!output.append("\\\\")) return false; break;
            case '\b': if (!output.append("\\b")) return false; break;
            case '\f': if (!output.append("\\f")) return false; break;
            case '\n': if (!output.append("\\n")) return false; break;
            case '\r': if (!output.append("\\r")) return false; break;
            case '\t': if (!output.append("\\t")) return false; break;
            default:
                if (static_cast<unsigned char>(character) < 0x20) {
                    char escaped[6] = {'\\', 'u', '0', '0', '0', '0'};
                    const auto byte = static_cast<unsigned char>(character);
                    escaped[4] = hex[byte >> 4];
                    escaped[5] = hex[byte & 0x0f];
                    if (!output.append(std::string_view(escaped, 6))) return false;
                } else if (!output.push(character)) {
                    return false;
                }
                break;
        }
    }
    return output.push('"');
}

inline bool appendValue(const Node& value, Writer& output,
                          const SerializeOptions& options, std::size_t depth,
                          std::string& failure) {
    return std::visit(
        [&](const auto& item) -> bool {
            using Item = BareT<decltype(item)>;
            if constexpr (std::same_as<Item, std::monostate>) {
                return output.append("null");
            } else if constexpr (std::same_as<Item, bool>) {
                return output.append(item ? "true" : "false");
            } else if constexpr (std::same_as<Item, std::int64_t> ||
                                 std::same_as<Item, std::uint64_t>) {
                return output.append(std::to_string(item));
            } else if constexpr (std::same_as<Item, double>) {
                if (!std::isfinite(item)) {
                    if (options.non_finite == NonFinitePolicy::null_value) {
                        return output.append("null");
                    }
                    failure = "JSON does not permit non-finite numbers";
                    return false;
                }
                char buffer[64]{};
                const auto [end, error] = std::to_chars(
                    buffer, buffer + sizeof(buffer), item,
                    std::chars_format::general, 17);
                if (error != std::errc{}) {
                    failure = "failed to format JSON floating-point value";
                    return false;
                }
                return output.append(std::string_view(buffer, static_cast<std::size_t>(end - buffer)));
            } else if constexpr (std::same_as<Item, std::string>) {
                return appendString(item, output);
            } else if constexpr (std::same_as<Item, Node::array>) {
                if (!output.push('[')) return false;
                if (!item.empty() && options.pretty && !output.push('\n')) return false;
                for (std::size_t index = 0; index < item.size(); ++index) {
                    if (options.pretty && !appendIndent(output, depth + 1, options)) return false;
                    if (!appendValue(item[index], output, options, depth + 1, failure)) return false;
                    if (index + 1 != item.size()) {
                        if (!output.push(',')) return false;
                        if (options.pretty && !output.push('\n')) return false;
                    }
                }
                if (!item.empty() && options.pretty) {
                    if (!output.push('\n') || !appendIndent(output, depth, options)) return false;
                }
                return output.push(']');
            } else if constexpr (std::same_as<Item, Node::object>) {
                if (!output.push('{')) return false;
                if (!item.empty() && options.pretty && !output.push('\n')) return false;
                std::size_t index = 0;
                for (const auto& [key, child] : item) {
                    if (options.pretty && !appendIndent(output, depth + 1, options)) return false;
                    if (!appendString(key, output)) return false;
                    if (!output.push(':')) return false;
                    if (options.pretty && !output.push(' ')) return false;
                    if (!appendValue(child, output, options, depth + 1, failure)) return false;
                    if (++index != item.size()) {
                        if (!output.push(',')) return false;
                        if (options.pretty && !output.push('\n')) return false;
                    }
                }
                if (!item.empty() && options.pretty) {
                    if (!output.push('\n') || !appendIndent(output, depth, options)) return false;
                }
                return output.push('}');
            }
        },
        value.value);
}

template <class T>
result<T> decodeValue(const Node& input, std::string_view path,
                        const ParseOptions& options);

inline std::string valuePath(std::string_view path) {
    return path.empty() ? std::string("value") : std::string(path);
}

// 递归解码仅在报告错误时需要完整路径。将成功的子调用保持在根路径
// 避免了常见情况下的临时字符串，同时保留了失败时的现有诊断信息。
inline std::string rebaseDecodeError(std::string error, std::string_view path) {
    constexpr std::string_view root = "value";
    if (!error.starts_with(root)) return error;
    std::string rebased;
    rebased.reserve(path.size() + error.size() - root.size());
    rebased.append(path);
    rebased.append(error.substr(root.size()));
    return rebased;
}

inline std::string indexedDecodePath(std::string_view path, std::size_t index) {
    std::array<char, 32> digits{};
    const auto converted = std::to_chars(digits.data(), digits.data() + digits.size(), index);
    std::string result;
    result.reserve(path.size() + 2 + static_cast<std::size_t>(converted.ptr - digits.data()));
    result.append(path);
    result.push_back('[');
    result.append(digits.data(), converted.ptr);
    result.push_back(']');
    return result;
}

inline std::string memberDecodePath(std::string_view path, std::string_view member) {
    std::string result;
    result.reserve(path.size() + 1 + member.size());
    result.append(path);
    result.push_back('.');
    result.append(member);
    return result;
}

template <class T>
result<T> decodeValue(const Node& input, std::string_view path,
                        const ParseOptions& options) {
    using U = BareT<T>;
    const auto where = valuePath(path);

    if constexpr (OptionalTraits<U>::value) {
        if (std::holds_alternative<std::monostate>(input.value)) {
            return U{};
        }
        auto decoded = decodeValue<typename OptionalTraits<U>::value_type>(input, path, options);
        if (!decoded) return std::unexpected(decoded.error());
        return U{std::move(*decoded)};
    } else if constexpr (InlineTableTraits<U>::value) {
        auto decoded = decodeValue<typename InlineTableTraits<U>::value_type>(input, path, options);
        if (!decoded) return std::unexpected(decoded.error());
        return U{std::move(*decoded)};
    } else if constexpr (std::same_as<U, std::nullptr_t>) {
        if (std::holds_alternative<std::monostate>(input.value)) return nullptr;
        return std::unexpected(where + " must be JSON null");
    } else if constexpr (std::same_as<U, date>) {
        const auto* value = std::get_if<std::string>(&input.value);
        if (value == nullptr) return std::unexpected(where + " must be a JSON date string");
        auto parsed = parse_date_text(*value);
        if (!parsed) return std::unexpected(where + ": " + parsed.error());
        return *parsed;
    } else if constexpr (std::same_as<U, time>) {
        const auto* value = std::get_if<std::string>(&input.value);
        if (value == nullptr) return std::unexpected(where + " must be a JSON time string");
        auto parsed = parse_time_prefix(*value);
        if (!parsed || parsed->second != value->size()) {
            return std::unexpected(where + ": invalid JSON time");
        }
        return parsed->first;
    } else if constexpr (std::same_as<U, local_date_time>) {
        const auto* value = std::get_if<std::string>(&input.value);
        if (value == nullptr) return std::unexpected(where + " must be a JSON local date-time string");
        auto parsed = parse_local_date_time_text(*value);
        if (!parsed) return std::unexpected(where + ": " + parsed.error());
        return *parsed;
    } else if constexpr (std::same_as<U, offset_date_time>) {
        const auto* value = std::get_if<std::string>(&input.value);
        if (value == nullptr) return std::unexpected(where + " must be a JSON offset date-time string");
        auto parsed = parse_offset_date_time_text(*value);
        if (!parsed) return std::unexpected(where + ": " + parsed.error());
        return *parsed;
    } else if constexpr (std::same_as<U, std::string>) {
        if (const auto* value = std::get_if<std::string>(&input.value)) return *value;
        return std::unexpected(where + " must be a JSON string");
    } else if constexpr (std::same_as<U, bool>) {
        if (const auto* value = std::get_if<bool>(&input.value)) return *value;
        return std::unexpected(where + " must be a JSON boolean");
    } else if constexpr (std::is_integral_v<U>) {
        std::int64_t signed_value = 0;
        std::uint64_t unsigned_value = 0;
        bool is_unsigned_value = false;
        if (const auto* value = std::get_if<std::int64_t>(&input.value)) {
            signed_value = *value;
        } else if (const auto* value = std::get_if<std::uint64_t>(&input.value)) {
            unsigned_value = *value;
            is_unsigned_value = true;
        } else {
            return std::unexpected(where + " must be a JSON integer");
        }
        if constexpr (std::is_signed_v<U>) {
            if (is_unsigned_value) {
                if (unsigned_value > static_cast<std::uint64_t>(std::numeric_limits<U>::max())) {
                    return std::unexpected(where + " is outside the destination integer range");
                }
                return static_cast<U>(unsigned_value);
            }
            if constexpr (std::numeric_limits<U>::digits < std::numeric_limits<std::int64_t>::digits) {
                if (signed_value < static_cast<std::int64_t>(std::numeric_limits<U>::min()) ||
                    signed_value > static_cast<std::int64_t>(std::numeric_limits<U>::max())) {
                    return std::unexpected(where + " is outside the destination integer range");
                }
            }
            return static_cast<U>(signed_value);
        } else {
            if (is_unsigned_value) {
                if constexpr (std::numeric_limits<U>::digits < std::numeric_limits<std::uint64_t>::digits) {
                    if (unsigned_value > static_cast<std::uint64_t>(std::numeric_limits<U>::max())) {
                        return std::unexpected(where + " is outside the destination integer range");
                    }
                }
                return static_cast<U>(unsigned_value);
            }
            if (signed_value < 0) {
                return std::unexpected(where + " is outside the destination integer range");
            }
            if constexpr (std::numeric_limits<U>::digits < std::numeric_limits<std::int64_t>::digits + 1) {
                if (static_cast<std::uint64_t>(signed_value) >
                    static_cast<std::uint64_t>(std::numeric_limits<U>::max())) {
                    return std::unexpected(where + " is outside the destination integer range");
                }
            }
            return static_cast<U>(signed_value);
        }
    } else if constexpr (std::is_floating_point_v<U>) {
        double value = 0.0;
        if (const auto* item = std::get_if<double>(&input.value)) value = *item;
        else if (const auto* item = std::get_if<std::int64_t>(&input.value)) value = static_cast<double>(*item);
        else if (const auto* item = std::get_if<std::uint64_t>(&input.value)) value = static_cast<double>(*item);
        else return std::unexpected(where + " must be a JSON number");
        const U converted = static_cast<U>(value);
        if (!std::isfinite(static_cast<double>(converted))) {
            return std::unexpected(where + " is outside the destination floating-point range");
        }
        return converted;
    } else if constexpr (std::is_enum_v<U>) {
        using Underlying = std::underlying_type_t<U>;
        auto decoded = decodeValue<Underlying>(input, path, options);
        if (!decoded) return std::unexpected(decoded.error());
        return static_cast<U>(*decoded);
    } else if constexpr (VectorTraits<U>::value) {
        const auto* values = std::get_if<Node::array>(&input.value);
        if (values == nullptr) return std::unexpected(where + " must be a JSON array");
        U output;
        if constexpr (requires { output.reserve(values->size()); }) output.reserve(values->size());
        for (std::size_t index = 0; index < values->size(); ++index) {
            auto decoded = decodeValue<typename VectorTraits<U>::value_type>(
                (*values)[index], {}, options);
            if (!decoded) {
                return std::unexpected(rebaseDecodeError(
                    decoded.error(), indexedDecodePath(where, index)));
            }
            output.push_back(std::move(*decoded));
        }
        return output;
    } else if constexpr (ArrayTraits<U>::value) {
        const auto* values = std::get_if<Node::array>(&input.value);
        if (values == nullptr || values->size() != ArrayTraits<U>::size) {
            return std::unexpected(where + " has the wrong JSON array size");
        }
        U output{};
        for (std::size_t index = 0; index < values->size(); ++index) {
            auto decoded = decodeValue<typename ArrayTraits<U>::value_type>(
                (*values)[index], {}, options);
            if (!decoded) {
                return std::unexpected(rebaseDecodeError(
                    decoded.error(), indexedDecodePath(where, index)));
            }
            output[index] = std::move(*decoded);
        }
        return output;
    } else if constexpr (MapTraits<U>::value) {
        const auto* values = std::get_if<Node::object>(&input.value);
        if (values == nullptr) return std::unexpected(where + " must be a JSON object");
        U output;
        for (const auto& [key, value] : *values) {
            auto decoded = decodeValue<typename MapTraits<U>::mapped_type>(
                value, {}, options);
            if (!decoded) {
                return std::unexpected(rebaseDecodeError(
                    decoded.error(), memberDecodePath(where, key)));
            }
            output.emplace(key, std::move(*decoded));
        }
        return output;
    } else if constexpr (Reflectable<U>) {
        const auto* values = std::get_if<Node::object>(&input.value);
        if (values == nullptr) return std::unexpected(where + " must be a JSON object");
        if constexpr (!std::is_default_constructible_v<U>) {
            return std::unexpected(where + " is not default constructible");
        } else {
            U output{};
            bool failed = false;
            std::string failure;
            for_each_field(output, [&](const auto& descriptor, auto& object) {
                if (failed) return;
                using Member = BareT<decltype(descriptor.get(object))>;
                if constexpr (!std::is_assignable_v<decltype(descriptor.get(object)), Member>) {
                    failed = true;
                    failure = where + " field '" + std::string(descriptor.name) + "' is not assignable";
                    return;
                }
                const auto iterator = values->find(std::string(descriptor.name));
                if (iterator == values->end()) {
                    if constexpr (OptionalTraits<Member>::value) return;
                    failed = true;
                    failure = where + " is missing field '" + std::string(descriptor.name) + "'";
                    return;
                }
                auto decoded = decodeValue<Member>(
                    iterator->second, {}, options);
                if (!decoded) {
                    failed = true;
                    failure = rebaseDecodeError(
                        decoded.error(), memberDecodePath(where, descriptor.name));
                    return;
                }
                descriptor.get(object) = std::move(*decoded);
            });
            if (!failed && options.unknown_fields == UnknownFieldPolicy::reject) {
                for (const auto& [key, ignored] : *values) {
                    bool known = false;
                    std::apply([&](const auto&... descriptor) {
                        known = ((key == descriptor.name) || ...);
                    }, reflect::fields(output));
                    static_cast<void>(ignored);
                    if (!known) {
                        failed = true;
                        failure = where + " contains unknown field '" + key + "'";
                        break;
                    }
                }
            }
            if (failed) return std::unexpected(std::move(failure));
            return output;
        }
    } else {
        return std::unexpected(where + " has an unsupported C++ type");
    }
}

}  // namespace detail

template <class T>
result<std::string> serialize(const T& value, const SerializeOptions& options = {}) {
    try {
        if (options.indent_width > 64) {
            return std::unexpected(std::string("JSON indent width exceeds configured limit"));
        }
        detail::EncodeContext context{options};
        auto encoded = detail::encodeValue(value, context, 0);
        if (!encoded) return std::unexpected(encoded.error());
        detail::Writer output{{}, options.max_output_bytes};
        std::string failure;
        if (!detail::appendValue(*encoded, output, options, 0, failure)) {
            if (!failure.empty()) return std::unexpected(std::move(failure));
            return std::unexpected(std::string("JSON output exceeds configured size limit"));
        }
        return std::move(output.output);
    } catch (const std::bad_alloc&) {
        return std::unexpected(std::string("JSON operation exhausted memory"));
    } catch (const std::exception& error) {
        return std::unexpected(std::string("JSON operation failed: ") + error.what());
    } catch (...) {
        return std::unexpected(std::string("JSON operation failed with an unknown exception"));
    }
}

template <class T>
result<std::string> try_serialize(const T& value, const SerializeOptions& options = {}) {
    return serialize(value, options);
}

template <class T>
result<T> deserialize(std::string_view text, const ParseOptions& options = {}) {
    try {
        auto parsed = detail::parseDocument(text, options);
        if (!parsed) return std::unexpected(parsed.error());
        return detail::decodeValue<T>(*parsed, {}, options);
    } catch (const std::bad_alloc&) {
        return std::unexpected(std::string("JSON operation exhausted memory"));
    } catch (const std::exception& error) {
        return std::unexpected(std::string("JSON operation failed: ") + error.what());
    } catch (...) {
        return std::unexpected(std::string("JSON operation failed with an unknown exception"));
    }
}

template <class T>
result<T> deserializee(std::string_view text, const ParseOptions& options = {}) {
    return deserialize<T>(text, options);
}

template <class T>
result<T> deSerialize(std::string_view text, const ParseOptions& options = {}) {
    return deserialize<T>(text, options);
}

}  // namespace json
