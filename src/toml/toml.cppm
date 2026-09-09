module;
#if defined(__SSE2__)
#  include <emmintrin.h>
#endif

export module toml;

export import std;
export import reflect;

export namespace toml {

/**
 * @brief 表示一个可能成功或失败的 TOML 操作结果。
 * @tparam T 成功时保存的值类型。
 */
template <class T>
using result = std::expected<T, std::string>;

enum class UnknownFieldPolicy { ignore, reject };

struct ParseOptions {
    std::size_t max_input_bytes = 64ULL * 1024ULL * 1024ULL;
    std::size_t max_nodes = 1000000;
    std::size_t max_depth = 128;
    std::size_t max_string_bytes = 16ULL * 1024ULL * 1024ULL;
    std::size_t max_key_bytes = 1ULL * 1024ULL * 1024ULL;
    std::size_t max_array_items = 1000000;
    UnknownFieldPolicy unknown_fields = UnknownFieldPolicy::ignore;
};

struct SerializeOptions {
    std::size_t max_output_bytes = 64ULL * 1024ULL * 1024ULL;
};

/**
 * @brief 表示 TOML 的本地日期值。
 *
 * 年、月、日字段分别对应 ISO 8601 日期的三个组成部分。该类型只保存
 * 结构化值，合法性会在解析或序列化时校验。
 */
struct date {
    int year{};
    unsigned month{};
    unsigned day{};

    /**
     * @brief 比较两个日期是否完全相同。
     * @param other 待比较的日期。
     * @return 两个日期的年、月、日均相同时返回 `true`。
     */
    constexpr bool operator==(const date& other) const = default;
};

/**
 * @brief 表示 TOML 的本地时间值。
 */
struct time {
    unsigned hour{};
    unsigned minute{};
    unsigned second{};
    std::string fractional_second;

    /**
     * @brief 比较两个时间是否完全相同。
     * @param other 待比较的时间。
     * @return 时、分、秒及小数秒均相同时返回 `true`。
     */
    bool operator==(const time& other) const = default;
};

/**
 * @brief 表示不带时区偏移的本地日期时间。
 */
struct local_date_time {
    date date_part;
    time time_part;

    /**
     * @brief 比较两个本地日期时间是否完全相同。
     * @param other 待比较的本地日期时间。
     * @return 日期和时间均相同时返回 `true`。
     */
    bool operator==(const local_date_time& other) const = default;
};

/**
 * @brief 表示带 UTC 偏移量的日期时间。
 */
struct offset_date_time {
    local_date_time local;
    int offset_minutes{};

    /**
     * @brief 比较两个带偏移日期时间是否完全相同。
     * @param other 待比较的日期时间。
     * @return 本地时间和偏移分钟数均相同时返回 `true`。
     */
    bool operator==(const offset_date_time& other) const = default;
};

/**
 * @brief 强制将一个反射结构序列化为 TOML 内联表。
 * @tparam T 被包装的反射结构类型。
 */
template <class T>
struct InlineTable {
    T value;

    /**
     * @brief 比较两个内联表包装器是否相同。
     * @param other 待比较的内联表。
     * @return 包装值相同时返回 `true`。
     */
    bool operator==(const InlineTable& other) const = default;
};

template <class T>
using inline_table = InlineTable<T>;

/**
 * @brief 保存反射字段名称及其成员指针的描述符。
 *
 * 描述符保持轻量且可在编译期构造，使生成的反射信息无需动态分配。
 * @tparam Owner 拥有该成员的结构类型。
 * @tparam Member 成员类型。
 */
template <class Owner, class Member>
using field = reflect::field<Owner, Member>;

using reflect::make_field;

/**
 * @brief 判断类型是否提供通用反射字段描述。
 * @tparam T 待检测的类型。
 */
template <class T>
concept Reflectable = reflect::Reflectable<T>;

using parse_options = ParseOptions;
using serialize_options = SerializeOptions;
using unknown_field_policy = UnknownFieldPolicy;

/**
 * @brief 遍历对象的全部反射字段。
 * @tparam T 对象类型。
 * @tparam Function 接收字段描述符和对象引用的可调用类型。
 * @param value 待读取或修改的对象。
 * @param function 每个字段调用一次的回调函数。
 */
template <class T, class Function>
    requires Reflectable<T>
constexpr void for_each_field(T& value, Function&& function) {
    reflect::for_each_field(value, std::forward<Function>(function));
}

namespace detail {

template <class T>
using BareT = std::remove_cvref_t<T>;

template <class T>
using bare_t = BareT<T>;

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

template <class T>
struct InlineTableTraits {
    static constexpr bool value = false;
};

template <class T>
struct InlineTableTraits<InlineTable<T>> {
    static constexpr bool value = true;
    using value_type = T;
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
struct MapTraits<std::unordered_map<std::string, Value, Hash, Equal,
                                      Allocator>> {
    static constexpr bool value = true;
    using mapped_type = Value;
};

struct Node {
    using array = std::vector<Node>;
    using table = std::map<std::string, Node>;
    using storage = std::variant<std::monostate, bool, std::int64_t, double,
                                 std::string, date, time, local_date_time,
                                 offset_date_time, array, table>;

    storage value{};
    bool inline_table = false;
    bool array_table = false;

    enum class TableDefinition {
        implicit,
        dotted_key,
        explicit_table,
        array_element,
        inline_table,
    } definition = TableDefinition::implicit;

    /** @brief 构造一个空节点。 */
    Node() = default;

    /**
     * @brief 复制构造节点。
     * @param other 待复制的节点。
     */
    Node(const Node& other) = default;

    /**
     * @brief 移动构造节点。
     * @param other 待移动的节点。
     */
    Node(Node&& other) noexcept = default;

    /**
     * @brief 复制赋值节点。
     * @param other 待复制的节点。
     * @return 当前节点的引用。
     */
    Node& operator=(const Node& other) = default;

    /**
     * @brief 移动赋值节点。
     * @param other 待移动的节点。
     * @return 当前节点的引用。
     */
    Node& operator=(Node&& other) noexcept = default;

    /**
     * @brief 使用任意受支持的值构造节点。
     * @tparam T 输入值类型。
     * @param input 要保存的值。
     */
    template <class T>
        requires(!std::same_as<bare_t<T>, Node>)
    Node(T&& input) : value(std::forward<T>(input)) {}
};

using node = Node;

/** @brief 检查字节字符串是否为合法的 UTF-8。 */
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

inline std::size_t skip_toml_space(std::string_view text, std::size_t position,
                                    bool allow_newlines) noexcept {
#if defined(__SSE2__)
    const auto* data = text.data();
    const auto space = _mm_set1_epi8(0x20);
    const auto tab = _mm_set1_epi8(0x09);
    const auto newline = _mm_set1_epi8(0x0a);
    const auto carriage = _mm_set1_epi8(0x0d);
    while (position + 16 <= text.size()) {
        const auto chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + position));
        auto matches = _mm_or_si128(_mm_cmpeq_epi8(chunk, space), _mm_cmpeq_epi8(chunk, tab));
        if (allow_newlines) {
            matches = _mm_or_si128(matches, _mm_cmpeq_epi8(chunk, newline));
            matches = _mm_or_si128(matches, _mm_cmpeq_epi8(chunk, carriage));
        }
        const auto mask = static_cast<unsigned>(_mm_movemask_epi8(matches));
        if (mask == 0xffffU) {
            position += 16;
            continue;
        }
        position += static_cast<std::size_t>(std::countr_zero(mask ^ 0xffffU));
        return position;
    }
#endif
    while (position < text.size() &&
           (text[position] == 0x20 || text[position] == 0x09 ||
            (allow_newlines && (text[position] == 0x0a || text[position] == 0x0d)))) {
        ++position;
    }
    return position;
}

inline std::size_t find_toml_string_special(std::string_view text,
                                             std::size_t position, char quote,
                                             bool include_escape) noexcept {
#if defined(__SSE2__)
    const auto* data = text.data();
    const auto delimiter = _mm_set1_epi8(quote);
    const auto slash = _mm_set1_epi8(0x5c);
    const auto newline = _mm_set1_epi8(0x0a);
    const auto carriage = _mm_set1_epi8(0x0d);
    const auto deleted = _mm_set1_epi8(0x7f);
    const auto zero = _mm_setzero_si128();
    const auto high_bit = _mm_set1_epi8(static_cast<char>(0x80));
    const auto control_limit = _mm_set1_epi8(0x20);
    const auto tab = _mm_set1_epi8(0x09);
    while (position + 16 <= text.size()) {
        const auto chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + position));
        auto matches = _mm_cmpeq_epi8(chunk, delimiter);
        if (include_escape) matches = _mm_or_si128(matches, _mm_cmpeq_epi8(chunk, slash));
        matches = _mm_or_si128(matches, _mm_cmpeq_epi8(chunk, newline));
        matches = _mm_or_si128(matches, _mm_cmpeq_epi8(chunk, carriage));
        matches = _mm_or_si128(matches, _mm_cmpeq_epi8(chunk, deleted));
        const auto ascii = _mm_cmpeq_epi8(_mm_and_si128(chunk, high_bit), zero);
        auto controls = _mm_and_si128(ascii, _mm_cmpgt_epi8(control_limit, chunk));
        controls = _mm_andnot_si128(_mm_cmpeq_epi8(chunk, tab), controls);
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
        if (character == static_cast<unsigned char>(quote) ||
            (include_escape && character == 0x5c) || character == 0x0a ||
            character == 0x0d || character == 0x7f ||
            (character < 0x20 && character != 0x09)) {
            return position;
        }
        ++position;
    }
    return position;
}

/** @brief 标记所有可通过 TOML 字符串表示的控制字符的后代。 */
inline void markInlineTable(Node& value) {
    value.inline_table = true;
    value.definition = Node::TableDefinition::inline_table;
    if (auto* table = std::get_if<Node::table>(&value.value)) {
        for (auto& [key, child] : *table) {
            static_cast<void>(key);
            markInlineTable(child);
        }
    } else if (auto* array = std::get_if<Node::array>(&value.value)) {
        for (auto& child : *array) {
            markInlineTable(child);
        }
    }
}

/**
 * @brief 去除字符串首尾的 ASCII 空白字符。
 * @param text 待处理的文本视图。
 * @return 去除首尾空白后的视图；不会复制底层字符串。
 */
inline std::string_view trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) {
        text.remove_suffix(1);
    }
    return text;
}

/**
 * @brief 移除不在字符串字面量中的 TOML 行注释。
 * @param line 待处理的单行文本。
 * @return 去除注释后的字符串。
 */
inline result<void> append_utf8(std::string& output, std::uint32_t code_point);

/**
 * @brief 解析单个 TOML 键并返回其实际名称。
 *
 * 同时支持裸键、基本字符串键和字面字符串键。
 * @param raw_key 原始键文本。
 * @return 解析后的键名，或包含原因的错误结果。
 */
inline result<std::string> parseKey(std::string_view raw_key) {
    const auto key = trim(raw_key);
    if (key.empty()) {
        return std::unexpected(std::string("empty TOML key"));
    }

    if (key.front() == '"' || key.front() == '\'') {
        const char quote = key.front();
        if (key.size() < 2 || key.back() != quote) {
            return std::unexpected(std::string("unterminated TOML key"));
        }
        if (quote == '\'') {
            for (std::size_t index = 1; index + 1 < key.size(); ++index) {
                if (key[index] == '\'' || key[index] == '\n' || key[index] == '\r' ||
                    key[index] == 0x7f ||
                    (static_cast<unsigned char>(key[index]) < 0x20 && key[index] != '\t')) {
                    return std::unexpected(std::string("invalid literal TOML key"));
                }
            }
            const auto value = key.substr(1, key.size() - 2);
            if (!valid_utf8(value)) {
                return std::unexpected(std::string("invalid UTF-8 in TOML key"));
            }
            return std::string(value);
        }

        std::string decoded;
        bool escaped = false;
        for (std::size_t index = 1; index + 1 < key.size(); ++index) {
            const char character = key[index];
            if (escaped) {
                switch (character) {
                    case 'b': decoded.push_back('\b'); break;
                    case 'f': decoded.push_back('\f'); break;
                    case 'n': decoded.push_back('\n'); break;
                    case 'r': decoded.push_back('\r'); break;
                    case 't': decoded.push_back('\t'); break;
                    case '"': decoded.push_back('"'); break;
                    case '\\': decoded.push_back('\\'); break;
                    case 'u':
                    case 'U': {
                        const std::size_t width = character == 'u' ? 4 : 8;
                        if (index + width >= key.size()) {
                            return std::unexpected(std::string("truncated Unicode TOML key escape"));
                        }
                        std::uint32_t code_point = 0;
                        for (std::size_t digit_index = 0; digit_index < width; ++digit_index) {
                            const char digit = key[++index];
                            code_point <<= 4;
                            if (digit >= '0' && digit <= '9') {
                                code_point |= static_cast<std::uint32_t>(digit - '0');
                            } else if (digit >= 'a' && digit <= 'f') {
                                code_point |= static_cast<std::uint32_t>(digit - 'a' + 10);
                            } else if (digit >= 'A' && digit <= 'F') {
                                code_point |= static_cast<std::uint32_t>(digit - 'A' + 10);
                            } else {
                                return std::unexpected(std::string("invalid Unicode TOML key escape"));
                            }
                        }
                        auto encoded = append_utf8(decoded, code_point);
                        if (!encoded) {
                            return std::unexpected(encoded.error());
                        }
                        break;
                    }
                    default:
                        return std::unexpected(std::string("unsupported key escape"));
                }
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                return std::unexpected(std::string("unescaped quote in TOML key"));
            } else if (character == '\n' || character == '\r') {
                return std::unexpected(std::string("invalid TOML key"));
            } else if ((static_cast<unsigned char>(character) < 0x20 &&
                        character != '\t') ||
                       static_cast<unsigned char>(character) == 0x7f) {
                return std::unexpected(std::string("control character in TOML key"));
            } else {
                decoded.push_back(character);
            }
        }
        if (escaped) {
            return std::unexpected(std::string("unterminated key escape"));
        }
        if (!valid_utf8(decoded)) {
            return std::unexpected(std::string("invalid UTF-8 in TOML key"));
        }
        return decoded;
    }

    for (const char character : key) {
        if (!((character >= 'A' && character <= 'Z') ||
              (character >= 'a' && character <= 'z') ||
              (character >= '0' && character <= '9') ||
              character == '_' || character == '-')) {
            return std::unexpected(std::string("invalid bare TOML key: ") +
                                   std::string(key));
        }
    }
    return std::string(key);
}

/**
 * @brief 解析由点分隔的 TOML 键路径。
 * @param raw 原始键路径文本。
 * @return 按层级拆分后的键名列表，或包含原因的错误结果。
 */
inline result<std::vector<std::string>> parseKeyPath(std::string_view raw,
                                                        std::size_t max_key_bytes = std::numeric_limits<std::size_t>::max(),
                                                        std::size_t max_components = std::numeric_limits<std::size_t>::max()) {
    if (raw.size() > max_key_bytes) {
        return std::unexpected(std::string("TOML key exceeds configured size limit"));
    }
    std::vector<std::string> parts;
    std::size_t start = 0;
    bool double_quoted = false;
    bool single_quoted = false;
    bool escaped = false;

    for (std::size_t index = 0; index <= raw.size(); ++index) {
        const bool at_end = index == raw.size();
        const char character = at_end ? '\0' : raw[index];
        if (!at_end) {
            if (double_quoted) {
                if (escaped) {
                    escaped = false;
                } else if (character == '\\') {
                    escaped = true;
                } else if (character == '"') {
                    double_quoted = false;
                }
            } else if (single_quoted) {
                if (character == '\'') {
                    single_quoted = false;
                }
            } else if (character == '"') {
                double_quoted = true;
            } else if (character == '\'') {
                single_quoted = true;
            }
        }

        if ((character == '.' && !double_quoted && !single_quoted) || at_end) {
            auto part = parseKey(raw.substr(start, index - start));
            if (!part) {
                return std::unexpected(part.error());
            }
            if (part->size() > max_key_bytes || parts.size() >= max_components) {
                return std::unexpected(std::string("TOML key exceeds configured size limit"));
            }
            parts.push_back(std::move(*part));
            start = index + 1;
        }
    }

    return parts;
}

/**
 * @brief 判断字符是否为 ASCII 十进制数字。
 * @param value 待判断的字符。
 * @return 字符位于 `'0'` 到 `'9'` 范围内时返回 `true`。
 */
inline bool ascii_digit(char value) {
    return value >= '0' && value <= '9';
}

inline bool number_digit(char value, int base) {
    if (ascii_digit(value)) {
        return value - '0' < base;
    }
    if (base == 16 && value >= 'a' && value <= 'f') {
        return true;
    }
    if (base == 16 && value >= 'A' && value <= 'F') {
        return true;
    }
    return false;
}

/**
 * @brief 验证 TOML 数字语法并移除合法的数字分隔符。
 *
 * `from_chars` 有意接受比 TOML 更宽的语法（例如前导零和小数点后缺少数字），
 * 因此词法检查放在这里，而不是依赖库转换例程。
 */
inline result<std::string> normalize_number_lexeme(std::string_view text,
                                                   bool& floating, int& base) {
    floating = false;
    base = 10;
    if (text.empty()) {
        return std::unexpected(std::string("empty TOML number"));
    }

    std::size_t start = 0;
    if (text.front() == '+' || text.front() == '-') {
        start = 1;
        if (start == text.size()) {
            return std::unexpected(std::string("invalid TOML number"));
        }
    }
    const auto unsigned_text = text.substr(start);
    const bool prefixed = unsigned_text.size() >= 2 && unsigned_text[0] == '0' &&
                          (unsigned_text[1] == 'x' || unsigned_text[1] == 'X' ||
                           unsigned_text[1] == 'o' || unsigned_text[1] == 'O' ||
                           unsigned_text[1] == 'b' || unsigned_text[1] == 'B');
    if (prefixed) {
        switch (unsigned_text[1]) {
            case 'x': case 'X': base = 16; break;
            case 'o': case 'O': base = 8; break;
            case 'b': case 'B': base = 2; break;
            default: break;
        }
        const auto digits = unsigned_text.substr(2);
        if (digits.empty()) {
            return std::unexpected(std::string("prefixed TOML integer has no digits"));
        }
        for (std::size_t index = 0; index < digits.size(); ++index) {
            if (digits[index] == '_') {
                if (index == 0 || index + 1 == digits.size() ||
                    !number_digit(digits[index - 1], base) ||
                    !number_digit(digits[index + 1], base)) {
                    return std::unexpected(std::string("invalid underscore in TOML number"));
                }
            } else if (!number_digit(digits[index], base)) {
                return std::unexpected(std::string("invalid TOML integer"));
            }
        }
    } else {
        for (std::size_t index = 0; index < unsigned_text.size(); ++index) {
            const char character = unsigned_text[index];
            if (character == '_') {
                if (index == 0 || index + 1 == unsigned_text.size() ||
                    !ascii_digit(unsigned_text[index - 1]) ||
                    !ascii_digit(unsigned_text[index + 1])) {
                    return std::unexpected(std::string("invalid underscore in TOML number"));
                }
            } else if (!ascii_digit(character) && character != '.' && character != 'e' &&
                       character != 'E' && character != '+' && character != '-') {
                return std::unexpected(std::string("invalid TOML number"));
            }
        }
    }

    std::string normalized;
    normalized.reserve(text.size());
    for (const char character : text) {
        if (character != '_') {
            normalized.push_back(character);
        }
    }
    if (prefixed) {
        return normalized;
    }

    std::string_view value = normalized;
    if (!value.empty() && (value.front() == '+' || value.front() == '-')) {
        value.remove_prefix(1);
    }
    const auto exponent = value.find_first_of("eE");
    const auto dot = value.find('.');
    if (dot != std::string_view::npos && exponent != std::string_view::npos && dot > exponent) {
        return std::unexpected(std::string("invalid TOML floating-point value"));
    }
    const auto integer_end = std::min(dot, exponent);
    const auto integer_part = value.substr(0, integer_end);
    if (integer_part.empty()) {
        return std::unexpected(std::string("invalid TOML floating-point value"));
    }
    for (const char character : integer_part) {
        if (!ascii_digit(character)) {
            return std::unexpected(std::string("invalid TOML floating-point value"));
        }
    }
    if (integer_part.size() > 1 && integer_part.front() == '0') {
        return std::unexpected(std::string("leading zero in TOML number"));
    }
    if (dot != std::string_view::npos) {
        const auto fraction_end = exponent == std::string_view::npos ? value.size() : exponent;
        const auto fraction = value.substr(dot + 1, fraction_end - dot - 1);
        if (fraction.empty()) {
            return std::unexpected(std::string("TOML floating-point value has no fraction"));
        }
        for (const char character : fraction) {
            if (!ascii_digit(character)) {
                return std::unexpected(std::string("invalid TOML floating-point value"));
            }
        }
        floating = true;
    }
    if (exponent != std::string_view::npos) {
        auto exponent_part = value.substr(exponent + 1);
        if (!exponent_part.empty() &&
            (exponent_part.front() == '+' || exponent_part.front() == '-')) {
            exponent_part.remove_prefix(1);
        }
        if (exponent_part.empty()) {
            return std::unexpected(std::string("TOML exponent has no digits"));
        }
        for (const char character : exponent_part) {
            if (!ascii_digit(character)) {
                return std::unexpected(std::string("invalid TOML exponent"));
            }
        }
        floating = true;
    }
    if (!floating && integer_part.size() > 1 && integer_part.front() == '0') {
        return std::unexpected(std::string("leading zero in TOML number"));
    }
    return normalized;
}

/**
 * @brief 从固定位置读取指定宽度的十进制数字。
 * @param text 待读取的文本。
 * @param offset 起始偏移量。
 * @param width 数字的字符宽度。
 * @param output 成功时写入解析结果。
 * @return 文本包含合法数字时返回 `true`，否则返回 `false`。
 */
inline bool parse_fixed_decimal(std::string_view text, std::size_t offset,
                                std::size_t width, unsigned& output) {
    if (offset + width > text.size()) {
        return false;
    }
    unsigned value = 0;
    for (std::size_t index = 0; index < width; ++index) {
        const char character = text[offset + index];
        if (!ascii_digit(character)) {
            return false;
        }
        value = value * 10 + static_cast<unsigned>(character - '0');
    }
    output = value;
    return true;
}

/**
 * @brief 将 Unicode 码点编码为 UTF-8 并追加到字符串。
 * @param output 用于接收 UTF-8 字节的字符串。
 * @param code_point 待编码的 Unicode 码点。
 * @return 编码成功时返回空成功结果；码点无效时返回错误。
 */
inline result<void> append_utf8(std::string& output, std::uint32_t code_point) {
    if (code_point > 0x10ffff ||
        (code_point >= 0xd800 && code_point <= 0xdfff)) {
        return std::unexpected(std::string("invalid Unicode code point in TOML string"));
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

/**
 * @brief 解析并校验 `YYYY-MM-DD` 格式的 TOML 日期。
 * @param text 日期文本。
 * @return 结构化日期，或格式/日历范围错误。
 */
inline result<date> parse_date_value(std::string_view text) {
    unsigned year = 0;
    unsigned month = 0;
    unsigned day = 0;
    if (text.size() != 10 || text[4] != '-' || text[7] != '-' ||
        !parse_fixed_decimal(text, 0, 4, year) ||
        !parse_fixed_decimal(text, 5, 2, month) ||
        !parse_fixed_decimal(text, 8, 2, day)) {
        return std::unexpected(std::string("invalid TOML date"));
    }

    constexpr std::array<unsigned, 12> days_per_month{
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap_year = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    if (month == 0 || month > 12 || day == 0 ||
        day > days_per_month[month - 1] + (month == 2 && leap_year ? 1 : 0)) {
        return std::unexpected(std::string("TOML date is outside the calendar range"));
    }
    return date{static_cast<int>(year), month, day};
}

/**
 * @brief 校验日期是否落在 TOML 支持的日历范围内。
 * @param value 待校验的日期。
 * @return 日期合法时返回 `true`。
 */
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

/**
 * @brief 校验时间及小数秒字段。
 * @param value 待校验的时间。
 * @return 时间合法时返回 `true`。
 */
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

/**
 * @brief 解析文本开头的 `HH:MM:SS[.fraction]` 时间。
 * @param text 待读取的文本。
 * @return 解析出的时间及已消费的字符数，或格式错误。
 */
inline result<std::pair<time, std::size_t>> parse_time_prefix(std::string_view text) {
    unsigned hour = 0;
    unsigned minute = 0;
    unsigned second = 0;
    if (text.size() < 8 || text[2] != ':' || text[5] != ':' ||
        !parse_fixed_decimal(text, 0, 2, hour) ||
        !parse_fixed_decimal(text, 3, 2, minute) ||
        !parse_fixed_decimal(text, 6, 2, second) ||
        hour > 23 || minute > 59 || second > 59) {
        return std::unexpected(std::string("invalid TOML time"));
    }

    std::size_t position = 8;
    std::string fractional_second;
    if (position < text.size() && text[position] == '.') {
        const std::size_t start = ++position;
        while (position < text.size() && ascii_digit(text[position])) {
            ++position;
        }
        if (position == start) {
            return std::unexpected(std::string("TOML time has an empty fractional second"));
        }
        fractional_second = std::string(text.substr(start, position - start));
    }
    return std::pair{time{hour, minute, second, std::move(fractional_second)}, position};
}

/**
 * @brief 解析 TOML 日期、时间及日期时间值。
 * @param text 待解析的时间语法文本。
 * @return 对应的内部节点，或时间格式错误。
 */
inline result<Node> parse_temporal_value(std::string_view text) {
    const bool date_like = text.size() >= 10 && ascii_digit(text[0]) &&
                           ascii_digit(text[1]) && ascii_digit(text[2]) &&
                           ascii_digit(text[3]) && text[4] == '-' && text[7] == '-';
    if (date_like) {
        auto parsed_date = parse_date_value(text.substr(0, 10));
        if (!parsed_date) {
            return std::unexpected(parsed_date.error());
        }
        if (text.size() == 10) {
            return Node{std::move(*parsed_date)};
        }
        if (text[10] != 'T' && text[10] != 't' && text[10] != ' ') {
            return std::unexpected(std::string("invalid TOML date-time separator"));
        }
        auto parsed_time = parse_time_prefix(text.substr(11));
        if (!parsed_time) {
            return std::unexpected(parsed_time.error());
        }
        const local_date_time local{std::move(*parsed_date),
                                    std::move(parsed_time->first)};
        const auto offset = 11 + parsed_time->second;
        if (offset == text.size()) {
            return Node{local};
        }
        if (text.substr(offset) == "Z" || text.substr(offset) == "z") {
            return Node{offset_date_time{local, 0}};
        }
        if (text.size() != offset + 6 ||
            (text[offset] != '+' && text[offset] != '-') || text[offset + 3] != ':') {
            return std::unexpected(std::string("invalid TOML date-time offset"));
        }
        unsigned offset_hour = 0;
        unsigned offset_minute = 0;
        if (!parse_fixed_decimal(text, offset + 1, 2, offset_hour) ||
            !parse_fixed_decimal(text, offset + 4, 2, offset_minute) ||
            offset_hour > 23 || offset_minute > 59) {
            return std::unexpected(std::string("invalid TOML date-time offset"));
        }
        int offset_minutes = static_cast<int>(offset_hour * 60 + offset_minute);
        if (text[offset] == '-') {
            offset_minutes = -offset_minutes;
        }
        return Node{offset_date_time{local, offset_minutes}};
    }

    const bool time_like = text.size() >= 8 && ascii_digit(text[0]) &&
                           ascii_digit(text[1]) && text[2] == ':' && text[5] == ':';
    if (time_like) {
        auto parsed_time = parse_time_prefix(text);
        if (!parsed_time || parsed_time->second != text.size()) {
            return std::unexpected(parsed_time
                                       ? std::string("invalid TOML time")
                                       : parsed_time.error());
        }
        return Node{std::move(parsed_time->first)};
    }
    return std::unexpected(std::string("not a TOML temporal value"));
}

/**
 * @brief 按键路径向表中插入值。
 * @param root 目标根表。
 * @param path 从根到叶子的键路径。
 * @param value 要插入的节点值。
 * @return 插入成功时返回 `true`；路径冲突、为空或键重复时返回 `false`。
 */
inline bool insertValueAt(Node::table& root,
                            const std::vector<std::string>& path,
                            Node value) {
    if (path.empty()) {
        return false;
    }
    Node::table* table = &root;
    for (std::size_t index = 0; index + 1 < path.size(); ++index) {
        auto [iterator, inserted] = table->try_emplace(path[index], Node{Node::table{}});
        if (!inserted && (iterator->second.inline_table ||
                          !std::holds_alternative<Node::table>(iterator->second.value))) {
            return false;
        }
        if (inserted) {
            iterator->second.definition = Node::TableDefinition::dotted_key;
        }
        table = &std::get<Node::table>(iterator->second.value);
    }
    return table->emplace(path.back(), std::move(value)).second;
}

/**
 * @brief 解析单个 TOML 值及其嵌套结构。
 *
 * 解析器维护输入视图和当前偏移量，供文档解析器在赋值语句中复用。
 */
struct ParseContext {
    const ParseOptions& options;
    std::size_t depth = 0;
    std::size_t nodes = 0;
};

using parse_context = ParseContext;

class ValueParser {
public:
    /**
     * @brief 构造值解析器。
     * @param text 待解析的完整文本。
     * @param position 初始读取位置，默认为文本开头。
     */
    explicit ValueParser(std::string_view text, std::size_t position,
                          bool allow_newlines, ParseContext& context)
        : text_(text), position_(position), allow_newlines_(allow_newlines), context_(context) {}

    /**
     * @brief 解析一个值并确保后面没有多余文本。
     * @return 解析出的节点，或语法错误。
     */
    result<Node> parse() {
        auto parsed = parseValue();
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        skip_trivia();
        if (position_ != text_.size()) {
            return std::unexpected(std::string("unexpected characters after TOML value"));
        }
        return parsed;
    }

    /**
     * @brief 从当前位置解析一个值。
     * @return 解析出的节点；解析结束位置可通过 `position()` 获取。
     */
    result<Node> parse_one() {
        return parseValue();
    }

    /**
     * @brief 获取当前解析偏移量。
     * @return 相对于输入起点的字符偏移量。
     */
    std::size_t position() const noexcept {
        return position_;
    }

private:
    /** @brief 跳过所有空白字符。 */
    void skip_space() {
        position_ = skip_toml_space(text_, position_, allow_newlines_);
    }

    /** @brief 跳过内联表允许的空格和制表符。 */
    void skip_inline_space() {
        position_ = skip_toml_space(text_, position_, false);
    }

    /** @brief 跳过空白和以 `#` 开头的注释。 */
    void skip_trivia() {
        if (!allow_newlines_) {
            skip_inline_space();
            return;
        }
        while (true) {
            skip_space();
            if (position_ >= text_.size() || text_[position_] != '#') {
                return;
            }
            while (position_ < text_.size() && text_[position_] != '\n') {
                ++position_;
            }
        }
    }

    /**
     * @brief 判断当前位置是否以指定文本开头。
     * @param token 要匹配的文本。
     * @return 匹配成功时返回 `true`。
     */
    bool starts_with(std::string_view token) const {
        return text_.substr(position_).starts_with(token);
    }

    /** @brief 消费一个 LF、CRLF 或 CR 换行序列。 */
    void consume_line_break() {
        if (position_ < text_.size() && text_[position_] == '\r') {
            ++position_;
            if (position_ < text_.size() && text_[position_] == '\n') {
                ++position_;
            }
        } else if (position_ < text_.size() && text_[position_] == '\n') {
            ++position_;
        }
    }

    /**
     * @brief 解析当前位置的基本字符串转义并追加到结果。
     * @param value 用于接收解码字符的字符串。
     * @return 转义有效时返回成功结果，否则返回错误。
     */
    result<void> append_basic_escape(std::string& value) {
        if (position_ >= text_.size()) {
            return std::unexpected(std::string("unterminated TOML string escape"));
        }
        const char character = text_[position_++];
        switch (character) {
            case 'b': value.push_back('\b'); return {};
            case 'f': value.push_back('\f'); return {};
            case 'n': value.push_back('\n'); return {};
            case 'r': value.push_back('\r'); return {};
            case 't': value.push_back('\t'); return {};
            case '"': value.push_back('"'); return {};
            case '\\': value.push_back('\\'); return {};
            case 'u':
            case 'U': {
                const std::size_t width = character == 'u' ? 4 : 8;
                if (position_ + width > text_.size()) {
                    return std::unexpected(std::string("truncated Unicode TOML string escape"));
                }
                std::uint32_t code_point = 0;
                for (std::size_t index = 0; index < width; ++index) {
                    const char digit = text_[position_++];
                    code_point <<= 4;
                    if (digit >= '0' && digit <= '9') {
                        code_point |= static_cast<std::uint32_t>(digit - '0');
                    } else if (digit >= 'a' && digit <= 'f') {
                        code_point |= static_cast<std::uint32_t>(digit - 'a' + 10);
                    } else if (digit >= 'A' && digit <= 'F') {
                        code_point |= static_cast<std::uint32_t>(digit - 'A' + 10);
                    } else {
                        return std::unexpected(std::string("invalid Unicode TOML string escape"));
                    }
                }
                return append_utf8(value, code_point);
            }
            default:
                return std::unexpected(std::string("unsupported TOML string escape"));
        }
    }

    /**
     * @brief 根据首字符分派并解析一个 TOML 值。
     * @return 解析出的节点，或语法错误。
     */
    result<Node> parseValue() {
        skip_space();
        if (position_ >= text_.size()) {
            return std::unexpected(std::string("missing TOML value"));
        }
        if (context_.depth >= context_.options.max_depth) {
            return std::unexpected(std::string("TOML nesting depth exceeds configured limit"));
        }
        ++context_.depth;
        struct DepthGuard {
            std::size_t& depth;
            ~DepthGuard() { --depth; }
        } guard{context_.depth};
        auto parsed = parseValueImpl();
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        if (context_.nodes >= context_.options.max_nodes) {
            return std::unexpected(std::string("TOML node count exceeds configured limit"));
        }
        ++context_.nodes;
        if (const auto* string = std::get_if<std::string>(&parsed->value);
            string != nullptr && string->size() > context_.options.max_string_bytes) {
            return std::unexpected(std::string("TOML string exceeds configured size limit"));
        }
        return parsed;
    }

    result<Node> parseValueImpl() {
        skip_space();
        if (position_ >= text_.size()) {
            return std::unexpected(std::string("missing TOML value"));
        }

        switch (text_[position_]) {
            case '"':
                return starts_with("\"\"\"") ? parseMultilineBasicString()
                                                   : parseBasicString();
            case '\'':
                return starts_with("'''") ? parseMultilineLiteralString()
                                             : parseLiteralString();
            case '[':
                return parseArray();
            case '{':
                return parseInlineTable();
            default:
                return parseAtom();
        }
    }

    result<void> append_string_chunk(std::string& value, std::string_view chunk) {
        const auto limit = context_.options.max_string_bytes;
        if (value.size() > limit || chunk.size() > limit - value.size()) {
            return std::unexpected(std::string("TOML string exceeds configured size limit"));
        }
        if (value.empty() && !chunk.empty()) {
            value.reserve(chunk.size());
        }
        value.append(chunk);
        return {};
    }

    result<void> check_string_size(const std::string& value) const {
        if (value.size() > context_.options.max_string_bytes) {
            return std::unexpected(std::string("TOML string exceeds configured size limit"));
        }
        return {};
    }

    /**
     * @brief 解析单行基本字符串。
     * @return 字符串节点，或字符串语法错误。
     */
    result<Node> parseBasicString() {
        ++position_;
        std::string value;
        while (position_ < text_.size()) {
            const auto special = find_toml_string_special(text_, position_, '"', true);
            if (special > position_) {
                auto appended = append_string_chunk(
                    value, text_.substr(position_, special - position_));
                if (!appended) {
                    return std::unexpected(appended.error());
                }
                position_ = special;
            }
            if (position_ >= text_.size()) {
                break;
            }
            const char character = text_[position_++];
            if (character == '"') {
                return Node{std::move(value)};
            }
            if (character == '\\') {
                auto escaped = append_basic_escape(value);
                if (!escaped) {
                    return std::unexpected(escaped.error());
                }
            } else if (character == '\n' || character == '\r') {
                return std::unexpected(std::string("newline in basic TOML string"));
            } else if (static_cast<unsigned char>(character) < 0x20 && character != '\t') {
                return std::unexpected(std::string("control character in basic TOML string"));
            } else if (static_cast<unsigned char>(character) == 0x7f) {
                return std::unexpected(std::string("control character in basic TOML string"));
            } else {
                value.push_back(character);
            }
            auto checked = check_string_size(value);
            if (!checked) {
                return std::unexpected(checked.error());
            }
        }
        return std::unexpected(std::string("unterminated basic TOML string"));
    }

    /**
     * @brief 解析单行字面字符串。
     * @return 字符串节点，或字符串语法错误。
     */
    result<Node> parseLiteralString() {
        ++position_;
        std::string value;
        while (position_ < text_.size()) {
            const auto special = find_toml_string_special(text_, position_, '\'', false);
            if (special > position_) {
                auto appended = append_string_chunk(
                    value, text_.substr(position_, special - position_));
                if (!appended) {
                    return std::unexpected(appended.error());
                }
                position_ = special;
            }
            if (position_ >= text_.size()) {
                break;
            }
            const char character = text_[position_++];
            if (character == '\'') {
                return Node{std::move(value)};
            }
            if (character == '\n' || character == '\r') {
                return std::unexpected(std::string("newline in literal TOML string"));
            }
            if (static_cast<unsigned char>(character) < 0x20 && character != '\t') {
                return std::unexpected(std::string("control character in literal TOML string"));
            }
            if (static_cast<unsigned char>(character) == 0x7f) {
                return std::unexpected(std::string("control character in literal TOML string"));
            }
            value.push_back(character);
            auto checked = check_string_size(value);
            if (!checked) {
                return std::unexpected(checked.error());
            }
        }
        return std::unexpected(std::string("unterminated literal TOML string"));
    }

    /**
     * @brief 解析三引号包围的多行基本字符串。
     * @return 字符串节点，或字符串语法错误。
     */
    result<Node> parseMultilineBasicString() {
        position_ += 3;
        if (position_ < text_.size() &&
            (text_[position_] == '\n' || text_[position_] == '\r')) {
            consume_line_break();
        }

        std::string value;
        while (position_ < text_.size()) {
            const auto special = find_toml_string_special(text_, position_, '"', true);
            if (special > position_) {
                auto appended = append_string_chunk(
                    value, text_.substr(position_, special - position_));
                if (!appended) {
                    return std::unexpected(appended.error());
                }
                position_ = special;
            }
            if (position_ >= text_.size()) {
                break;
            }
            if (starts_with("\"\"\"")) {
                std::size_t run = 0;
                while (position_ + run < text_.size() && text_[position_ + run] == '"') {
                    ++run;
                }
                if (run == 3) {
                    position_ += 3;
                    return Node{std::move(value)};
                }
                if (run == 4 || run == 5) {
                    value.append(run - 3, '"');
                    auto checked = check_string_size(value);
                    if (!checked) {
                        return std::unexpected(checked.error());
                    }
                    position_ += run;
                    continue;
                }
                position_ += 3;
                return std::unexpected(std::string("invalid quote sequence in multiline TOML string"));
            }
            const char character = text_[position_++];
            if (character == '\\') {
                if (position_ < text_.size() &&
                    (text_[position_] == '\n' || text_[position_] == '\r')) {
                    consume_line_break();
                    position_ = skip_toml_space(text_, position_, true);
                    continue;
                }
                auto escaped = append_basic_escape(value);
                if (!escaped) {
                    return std::unexpected(escaped.error());
                }
            } else if (character == '\r') {
                consume_line_break();
                value.push_back('\n');
            } else if (character == '\n') {
                value.push_back('\n');
            } else if ((static_cast<unsigned char>(character) < 0x20 &&
                        character != '\t') ||
                       static_cast<unsigned char>(character) == 0x7f) {
                return std::unexpected(
                    std::string("control character in multiline basic TOML string"));
            } else {
                value.push_back(character);
            }
            auto checked = check_string_size(value);
            if (!checked) {
                return std::unexpected(checked.error());
            }
        }
        return std::unexpected(std::string("unterminated multiline basic TOML string"));
    }

    /**
     * @brief 解析三单引号包围的多行字面字符串。
     * @return 字符串节点，或字符串语法错误。
     */
    result<Node> parseMultilineLiteralString() {
        position_ += 3;
        if (position_ < text_.size() &&
            (text_[position_] == '\n' || text_[position_] == '\r')) {
            consume_line_break();
        }

        std::string value;
        while (position_ < text_.size()) {
            const auto special = find_toml_string_special(text_, position_, '\'', false);
            if (special > position_) {
                auto appended = append_string_chunk(
                    value, text_.substr(position_, special - position_));
                if (!appended) {
                    return std::unexpected(appended.error());
                }
                position_ = special;
            }
            if (position_ >= text_.size()) {
                break;
            }
            if (starts_with("'''")) {
                std::size_t run = 0;
                while (position_ + run < text_.size() && text_[position_ + run] == '\'') {
                    ++run;
                }
                if (run == 3) {
                    position_ += 3;
                    return Node{std::move(value)};
                }
                if (run == 4 || run == 5) {
                    value.append(run - 3, '\'');
                    auto checked = check_string_size(value);
                    if (!checked) {
                        return std::unexpected(checked.error());
                    }
                    position_ += run;
                    continue;
                }
                position_ += 3;
                return std::unexpected(std::string("invalid quote sequence in multiline TOML string"));
            }
            if (text_[position_] == '\r') {
                consume_line_break();
                value.push_back('\n');
            } else if (text_[position_] == '\n') {
                ++position_;
                value.push_back('\n');
            } else if (static_cast<unsigned char>(text_[position_]) < 0x20 &&
                       text_[position_] != '\t') {
                return std::unexpected(
                    std::string("control character in multiline literal TOML string"));
            } else if (static_cast<unsigned char>(text_[position_]) == 0x7f) {
                return std::unexpected(
                    std::string("control character in multiline literal TOML string"));
            } else {
                value.push_back(text_[position_++]);
            }
            auto checked = check_string_size(value);
            if (!checked) {
                return std::unexpected(checked.error());
            }
        }
        return std::unexpected(std::string("unterminated multiline literal TOML string"));
    }

    /**
     * @brief 解析 TOML 数组及其元素。
     * @return 数组节点，或数组语法错误。
     */
    result<Node> parseArray() {
        ++position_;
        struct NewlineGuard {
            bool& target;
            bool previous;
            ~NewlineGuard() { target = previous; }
        } guard{allow_newlines_, allow_newlines_};
        allow_newlines_ = true;
        Node::array values;
        skip_trivia();
        if (position_ < text_.size() && text_[position_] == ']') {
            ++position_;
            return Node{std::move(values)};
        }

        while (position_ < text_.size()) {
            auto value = parseValue();
            if (!value) {
                return std::unexpected(value.error());
            }
            if (values.size() >= context_.options.max_array_items) {
                return std::unexpected(std::string("TOML array item count exceeds configured limit"));
            }
            values.push_back(std::move(*value));
            skip_trivia();
            if (position_ >= text_.size()) {
                break;
            }
            if (text_[position_] == ']') {
                ++position_;
                return Node{std::move(values)};
            }
            if (text_[position_] != ',') {
                return std::unexpected(std::string("expected ',' or ']' in TOML array"));
            }
            ++position_;
            skip_trivia();
            if (position_ < text_.size() && text_[position_] == ']') {
                ++position_;
                return Node{std::move(values)};
            }
        }

        return std::unexpected(std::string("unterminated TOML array"));
    }

    /**
     * @brief 解析 TOML 内联表。
     * @return 内联表节点，或键值语法错误。
     */
    result<Node> parseInlineTable() {
        ++position_;
        Node::table values;
        skip_inline_space();
        if (position_ < text_.size() && text_[position_] == '}') {
            ++position_;
            Node result{std::move(values)};
            markInlineTable(result);
            return result;
        }

        while (position_ < text_.size()) {
            const std::size_t key_start = position_;
            bool double_quoted = false;
            bool single_quoted = false;
            bool escaped = false;
            while (position_ < text_.size()) {
                const char character = text_[position_];
                if (double_quoted) {
                    if (escaped) {
                        escaped = false;
                    } else if (character == '\\') {
                        escaped = true;
                    } else if (character == '"') {
                        double_quoted = false;
                    }
                } else if (single_quoted) {
                    if (character == '\'') {
                        single_quoted = false;
                    }
                } else if (character == '"') {
                    double_quoted = true;
                } else if (character == '\'') {
                    single_quoted = true;
                } else if (character == '=') {
                    break;
                } else if (character == ',' || character == '}' || character == '\n' ||
                           character == '\r') {
                    return std::unexpected(std::string("expected '=' in inline TOML table"));
                }
                ++position_;
            }
            if (position_ >= text_.size() || text_[position_] != '=') {
                return std::unexpected(std::string("expected '=' in inline TOML table"));
            }
            auto path = parseKeyPath(text_.substr(key_start, position_ - key_start), context_.options.max_key_bytes, context_.options.max_depth);
            if (!path || path->empty()) {
                return std::unexpected(path ? std::string("empty inline TOML key")
                                             : path.error());
            }
            ++position_;
            const bool previous_allow_newlines = allow_newlines_;
            allow_newlines_ = false;
            auto value = parseValue();
            allow_newlines_ = previous_allow_newlines;
            if (!value) {
                return std::unexpected(value.error());
            }
            if (!insertValueAt(values, *path, std::move(*value))) {
                return std::unexpected(std::string("duplicate key in inline TOML table"));
            }
            skip_inline_space();
            if (position_ >= text_.size()) {
                break;
            }
            if (text_[position_] == '}') {
                ++position_;
                Node result{std::move(values)};
                markInlineTable(result);
                return result;
            }
            if (text_[position_] != ',') {
                return std::unexpected(std::string("expected ',' or '}' in inline TOML table"));
            }
            ++position_;
            skip_inline_space();
            if (position_ < text_.size() && text_[position_] == '}') {
                return std::unexpected(std::string("trailing comma in inline TOML table"));
            }
        }

        return std::unexpected(std::string("unterminated inline TOML table"));
    }

    /**
     * @brief 解析布尔值、整数、浮点数和时间等非容器值。
     * @return 标量节点，或无法识别的值错误。
     */
    result<Node> parseAtom() {
        const std::size_t start = position_;
        while (position_ < text_.size()) {
            const char character = text_[position_];
            if (character == ',' || character == ']' || character == '}' ||
                character == '#' || character == '\n' || character == '\r') {
                break;
            }
            ++position_;
        }
        const auto atom = trim(text_.substr(start, position_ - start));
        if (atom == "true") {
            return Node{true};
        }
        if (atom == "false") {
            return Node{false};
        }
        if (atom.empty()) {
            return std::unexpected(std::string("empty TOML atom"));
        }
        const bool date_like = atom.size() >= 10 && ascii_digit(atom[0]) &&
                               ascii_digit(atom[1]) && ascii_digit(atom[2]) &&
                               ascii_digit(atom[3]) && atom[4] == '-' && atom[7] == '-';
        const bool time_like = atom.size() >= 8 && ascii_digit(atom[0]) &&
                               ascii_digit(atom[1]) && atom[2] == ':' && atom[5] == ':';
        if (date_like || time_like) {
            return parse_temporal_value(atom);
        }
        if (atom == "inf" || atom == "+inf") {
            return Node{std::numeric_limits<double>::infinity()};
        }
        if (atom == "-inf") {
            return Node{-std::numeric_limits<double>::infinity()};
        }
        if (atom == "nan" || atom == "+nan" || atom == "-nan") {
            return Node{std::numeric_limits<double>::quiet_NaN()};
        }
        bool lexical_floating = false;
        int lexical_base = 10;
        auto normalized_result = normalize_number_lexeme(atom, lexical_floating, lexical_base);
        if (!normalized_result) {
            return std::unexpected(normalized_result.error());
        }
        const std::string& normalized = *normalized_result;

        const bool floating = lexical_floating;
        if (floating) {
            std::string_view floating_text = normalized;
            // `from_chars` 有意支持前导负号，但不支持前导正号。
            if (!floating_text.empty() && floating_text.front() == '+') {
                floating_text.remove_prefix(1);
            }
            double value = 0.0;
            const auto [end, error] = std::from_chars(
                floating_text.data(), floating_text.data() + floating_text.size(), value,
                std::chars_format::general);
            if (error != std::errc{} || end != floating_text.data() + floating_text.size()) {
                return std::unexpected(std::string("invalid TOML floating-point value: ") +
                                       normalized);
            }
            return Node{value};
        }

        bool negative = false;
        std::string_view digits = normalized;
        if (!digits.empty() && (digits.front() == '+' || digits.front() == '-')) {
            negative = digits.front() == '-';
            digits.remove_prefix(1);
        }
        const int base = lexical_base;
        if (lexical_base != 10) {
            digits.remove_prefix(2);
        }
        if (digits.empty()) {
            return std::unexpected(std::string("unsupported TOML value: ") + normalized);
        }

        std::uint64_t magnitude = 0;
        const auto [end, error] = std::from_chars(
            digits.data(), digits.data() + digits.size(), magnitude, base);
        if (error != std::errc{} || end != digits.data() + digits.size()) {
            return std::unexpected(std::string("unsupported TOML value: ") + normalized);
        }

        constexpr auto positive_limit =
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
        constexpr auto negative_limit = positive_limit + 1;
        if (negative) {
            if (magnitude > negative_limit) {
                return std::unexpected(std::string("TOML integer is outside signed 64-bit range"));
            }
            if (magnitude == negative_limit) {
                return Node{std::numeric_limits<std::int64_t>::min()};
            }
            return Node{-static_cast<std::int64_t>(magnitude)};
        }
        if (magnitude > positive_limit) {
            return std::unexpected(std::string("TOML integer is outside signed 64-bit range"));
        }
        return Node{static_cast<std::int64_t>(magnitude)};
    }

    std::string_view text_;
    std::size_t position_ = 0;
    bool allow_newlines_ = true;
    ParseContext& context_;
};

/**
 * @brief 解析完整 TOML 文档并生成内部表树。
 */
class DocumentParser {
public:
    /**
     * @brief 构造文档解析器。
     * @param text 待解析的 TOML 文档文本。
     */
    explicit DocumentParser(std::string_view text, const ParseOptions& options)
        : text_(text), options_(options), context_{options} {}

    /**
     * @brief 解析完整文档。
     * @return 文档根表，或包含位置上下文的语法错误。
     */
    result<Node::table> parse() {
        if (!valid_utf8(text_)) {
            return std::unexpected(std::string("invalid UTF-8 in TOML document"));
        }
        Node::table root;
        Node::table* current_table = &root;
        while (true) {
            skip_trivia();
            if (position_ == text_.size()) {
                return root;
            }

            if (text_[position_] == '[') {
                auto opened = parse_header(root);
                if (!opened) {
                    return std::unexpected(opened.error());
                }
                current_table = *opened;
            } else {
                auto assigned = parse_assignment(*current_table);
                if (!assigned) {
                    return std::unexpected(assigned.error());
                }
            }
        }
    }

private:
    /** @brief 跳过文档中的空白和注释。 */
    void skip_trivia() {
        while (true) {
            position_ = skip_toml_space(text_, position_, true);
            if (position_ == text_.size() || text_[position_] != '#') {
                return;
            }
            while (position_ < text_.size() && text_[position_] != '\n') {
                ++position_;
            }
        }
    }

    /**
     * @brief 校验并消费当前赋值或表头的行尾。
     * @return 行尾合法时返回成功结果，否则返回多余字符错误。
     */
    result<void> finish_line() {
        position_ = skip_toml_space(text_, position_, false);
        while (position_ < text_.size() && text_[position_] == '\r') {
            ++position_;
            position_ = skip_toml_space(text_, position_, false);
        }
        if (position_ < text_.size() && text_[position_] == '#') {
            while (position_ < text_.size() && text_[position_] != '\n') {
                ++position_;
            }
        }
        if (position_ == text_.size()) {
            return {};
        }
        if (text_[position_] == '\n') {
            ++position_;
            return {};
        }
        return std::unexpected(std::string("unexpected characters after TOML assignment"));
    }

    /**
     * @brief 查找表头右方括号的位置，并忽略引号中的字符。
     * @param start 搜索起始位置。
     * @param array_table 是否正在解析数组表头。
     * @return 右方括号位置；找不到时返回 `std::string_view::npos`。
     */
    std::size_t find_header_end(std::size_t start, bool array_table) const {
        bool double_quoted = false;
        bool single_quoted = false;
        bool escaped = false;
        for (std::size_t index = start; index < text_.size(); ++index) {
            const char character = text_[index];
            if (double_quoted) {
                if (escaped) {
                    escaped = false;
                } else if (character == '\\') {
                    escaped = true;
                } else if (character == '"') {
                    double_quoted = false;
                }
            } else if (single_quoted) {
                if (character == '\'') {
                    single_quoted = false;
                }
            } else if (character == '"') {
                double_quoted = true;
            } else if (character == '\'') {
                single_quoted = true;
            } else if (character == '\n' || character == '\r') {
                return std::string_view::npos;
            } else if (character == ']' &&
                       (!array_table || (index + 1 < text_.size() &&
                                         text_[index + 1] == ']'))) {
                return index;
            }
        }
        return std::string_view::npos;
    }

    /**
     * @brief 查找当前行未加引号的赋值等号。
     * @return 等号位置；找不到时返回 `std::string_view::npos`。
     */
    std::size_t find_assignment() const {
        bool double_quoted = false;
        bool single_quoted = false;
        bool escaped = false;
        for (std::size_t index = position_; index < text_.size(); ++index) {
            const char character = text_[index];
            if (double_quoted) {
                if (escaped) {
                    escaped = false;
                } else if (character == '\\') {
                    escaped = true;
                } else if (character == '"') {
                    double_quoted = false;
                }
            } else if (single_quoted) {
                if (character == '\'') {
                    single_quoted = false;
                }
            } else if (character == '"') {
                double_quoted = true;
            } else if (character == '\'') {
                single_quoted = true;
            } else if (character == '=') {
                return index;
            } else if (character == '#' || character == '\n' || character == '\r') {
                return std::string_view::npos;
            }
        }
        return std::string_view::npos;
    }

    /**
     * @brief 解析普通表或数组表头，并切换当前表。
     * @param root 文档根表。
     * @return 新打开表的指针，或表头语法错误。
     */
    result<Node::table*> parse_header(Node::table& root) {
        const bool array_table = text_.substr(position_).starts_with("[[");
        const std::size_t content_start = position_ + (array_table ? 2 : 1);
        const std::size_t content_end = find_header_end(content_start, array_table);
        if (content_end == std::string_view::npos) {
            return std::unexpected(std::string("unterminated TOML table header"));
        }
        auto path = parseKeyPath(text_.substr(content_start, content_end - content_start), options_.max_key_bytes, options_.max_depth);
        if (!path || path->empty()) {
            return std::unexpected(path ? std::string("empty TOML table header")
                                         : path.error());
        }
        position_ = content_end + (array_table ? 2 : 1);
        auto finished = finish_line();
        if (!finished) {
            return std::unexpected(finished.error());
        }
        return array_table ? open_array_table(root, *path) : open_table(root, *path);
    }

    /**
     * @brief 解析当前表中的一条键值赋值。
     * @param current_table 当前活动表。
     * @return 赋值成功时返回成功结果，否则返回语法或键冲突错误。
     */
    result<void> parse_assignment(Node::table& current_table) {
        const std::size_t equals = find_assignment();
        if (equals == std::string_view::npos) {
            return std::unexpected(std::string("expected '=' in TOML assignment"));
        }
        auto path = parseKeyPath(text_.substr(position_, equals - position_), options_.max_key_bytes, options_.max_depth);
        if (!path || path->empty()) {
            return std::unexpected(path ? std::string("empty TOML key")
                                         : path.error());
        }
        position_ = equals + 1;
        position_ = skip_toml_space(text_, position_, false);
        if (position_ >= text_.size() || text_[position_] == '\n' ||
            text_[position_] == '\r') {
            return std::unexpected(std::string("missing TOML value"));
        }
        ValueParser parser(text_, position_, true, context_);
        auto value = parser.parse_one();
        if (!value) {
            return std::unexpected(value.error());
        }
        position_ = parser.position();
        auto finished = finish_line();
        if (!finished) {
            return std::unexpected(finished.error());
        }
        if (!insertValueAt(current_table, *path, std::move(*value))) {
            return std::unexpected(std::string("duplicate or conflicting TOML key"));
        }
        return {};
    }

    /**
     * @brief 按路径打开或创建普通表。
     * @param root 文档根表。
     * @param path 表的键路径。
     * @return 目标表指针，或与标量值冲突的错误。
     */
    static result<Node::table*> open_table(
        Node::table& root, const std::vector<std::string>& path,
        bool allow_terminal_array_table = false,
        bool define_terminal_table = true) {
        Node::table* table = &root;
        for (std::size_t index = 0; index < path.size(); ++index) {
            const auto& part = path[index];
            auto [iterator, inserted] = table->try_emplace(part, Node{Node::table{}});
            if (std::holds_alternative<Node::table>(iterator->second.value) &&
                !iterator->second.inline_table) {
                if (define_terminal_table && !inserted && index + 1 == path.size() &&
                    iterator->second.definition != Node::TableDefinition::implicit) {
                    return std::unexpected(std::string("duplicate TOML table definition"));
                }
                if (define_terminal_table && index + 1 == path.size()) {
                    iterator->second.definition = Node::TableDefinition::explicit_table;
                }
                table = &std::get<Node::table>(iterator->second.value);
                continue;
            }
            if (auto* array = std::get_if<Node::array>(&iterator->second.value);
                iterator->second.array_table && array != nullptr && !array->empty() &&
                (index + 1 < path.size() || allow_terminal_array_table) &&
                std::holds_alternative<Node::table>(array->back().value)) {
                table = &std::get<Node::table>(array->back().value);
                continue;
            }
            return std::unexpected(std::string("TOML table conflicts with an existing value"));
        }
        return table;
    }

    /**
     * @brief 按路径追加一个数组表元素并返回其表指针。
     * @param root 文档根表。
     * @param path 数组表的键路径。
     * @return 新元素的表指针，或路径冲突错误。
     */
    static result<Node::table*> open_array_table(
        Node::table& root, const std::vector<std::string>& path) {
        if (path.empty()) {
            return std::unexpected(std::string("empty TOML array-table header"));
        }
        std::vector<std::string> parent_path(path.begin(), path.end() - 1);
        auto parent = open_table(root, parent_path, true, false);
        if (!parent) {
            return std::unexpected(parent.error());
        }
        auto [iterator, inserted] = (*parent)->try_emplace(path.back(), Node{Node::array{}});
        if (!inserted && !iterator->second.array_table) {
            return std::unexpected(std::string("TOML array-table conflicts with another value"));
        }
        iterator->second.array_table = true;
        auto& values = std::get<Node::array>(iterator->second.value);
        values.emplace_back(Node::table{});
        values.back().definition = Node::TableDefinition::array_element;
        return &std::get<Node::table>(values.back().value);
    }

    std::string_view text_;
    const ParseOptions& options_;
    ParseContext context_;
    std::size_t position_ = 0;
};

// Keep the public typed API and the benchmark's parse-only phase on the same
// input checks and document construction path.
inline result<Node> parseDocument(std::string_view text, const ParseOptions& options) {
    if (text.size() > options.max_input_bytes) {
        return std::unexpected(std::string("TOML input exceeds configured size limit"));
    }
    DocumentParser parser{text, options};
    auto parsed = parser.parse();
    if (!parsed) {
        return std::unexpected(parsed.error());
    }
    return Node{std::move(*parsed)};
}

/**
 * @brief 将 C++ 值递归编码为内部 TOML 节点。
 * @tparam T 输入值类型。
 * @param input 待编码的 C++ 值。
 * @return 内部节点，或类型不受支持/值超出范围的错误。
 */
template <class T>
result<Node> encodeValue(const T& input);

/**
 * @brief 将内部 TOML 节点递归解码为 C++ 值。
 * @tparam T 目标 C++ 类型。
 * @param input 待读取的内部节点。
 * @param path 当前值在文档中的路径，用于生成错误信息。
 * @return 解码后的 C++ 值，或类型/范围不匹配错误。
 */
template <class T>
result<T> decodeValue(const Node& input, std::string_view path, const ParseOptions& options);

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

/**
 * @brief 将 C++ 值递归编码为内部 TOML 节点。
 * @tparam T 输入值类型。
 * @param input 待编码的 C++ 值。
 * @return 内部节点，或类型不受支持/值超出范围的错误。
 */
template <class T>
result<Node> encodeValue(const T& input) {
    using U = BareT<T>;

    if constexpr (OptionalTraits<U>::value) {
        if (!input) {
            return Node{};
        }
        return encodeValue(*input);
    } else if constexpr (InlineTableTraits<U>::value) {
        auto encoded = encodeValue(input.value);
        if (!encoded) {
            return std::unexpected(encoded.error());
        }
        if (!std::holds_alternative<Node::table>(encoded->value)) {
            return std::unexpected(std::string("toml::InlineTable requires a reflected struct"));
        }
        markInlineTable(*encoded);
        return encoded;
    } else if constexpr (std::same_as<U, date>) {
        if (!valid_date_value(input)) {
            return std::unexpected(std::string("invalid toml::date value"));
        }
        return Node{input};
    } else if constexpr (std::same_as<U, time>) {
        if (!valid_time_value(input)) {
            return std::unexpected(std::string("invalid toml::time value"));
        }
        return Node{input};
    } else if constexpr (std::same_as<U, local_date_time>) {
        if (!valid_date_value(input.date_part) || !valid_time_value(input.time_part)) {
            return std::unexpected(std::string("invalid toml::local_date_time value"));
        }
        return Node{input};
    } else if constexpr (std::same_as<U, offset_date_time>) {
        if (!valid_date_value(input.local.date_part) ||
            !valid_time_value(input.local.time_part) ||
            input.offset_minutes < -1439 || input.offset_minutes > 1439) {
            return std::unexpected(std::string("invalid toml::offset_date_time value"));
        }
        return Node{input};
    } else if constexpr (std::same_as<U, std::string>) {
        if (!valid_utf8(input)) {
            return std::unexpected(std::string("invalid UTF-8 in TOML string"));
        }
        return Node{input};
    } else if constexpr (std::same_as<U, std::string_view>) {
        if (!valid_utf8(input)) {
            return std::unexpected(std::string("invalid UTF-8 in TOML string"));
        }
        return Node{std::string(input)};
    } else if constexpr (std::same_as<U, bool>) {
        return Node{input};
    } else if constexpr (std::is_integral_v<U>) {
        if constexpr (std::is_signed_v<U>) {
            if constexpr (std::numeric_limits<U>::digits >
                          std::numeric_limits<std::int64_t>::digits) {
                if (input < static_cast<U>(std::numeric_limits<std::int64_t>::min()) ||
                    input > static_cast<U>(std::numeric_limits<std::int64_t>::max())) {
                    return std::unexpected(
                        std::string("integer value is outside TOML's signed 64-bit range"));
                }
            }
        } else if constexpr (std::numeric_limits<U>::digits >
                             std::numeric_limits<std::int64_t>::digits) {
            if (input > static_cast<U>(std::numeric_limits<std::int64_t>::max())) {
                return std::unexpected(
                    std::string("integer value is outside TOML's signed 64-bit range"));
            }
        }
        return Node{static_cast<std::int64_t>(input)};
    } else if constexpr (std::is_floating_point_v<U>) {
        return Node{static_cast<double>(input)};
    } else if constexpr (std::is_enum_v<U>) {
        using Underlying = std::underlying_type_t<U>;
        return encodeValue(static_cast<Underlying>(input));
    } else if constexpr (VectorTraits<U>::value) {
        Node::array values;
        for (const auto& element : input) {
            using Element = typename VectorTraits<U>::value_type;
            // `vector<bool>` 暴露的是代理引用；这里先物化声明的元素类型，
            // 同时保留普通 vector 对不可复制元素类型的支持。
            auto encoded = [&]() {
                if constexpr (std::constructible_from<Element, decltype(element)>) {
                    return encodeValue(static_cast<Element>(element));
                } else {
                    return encodeValue(element);
                }
            }();
            if (!encoded) {
                return std::unexpected(encoded.error());
            }
            if (std::holds_alternative<std::monostate>(encoded->value)) {
                return std::unexpected(std::string("TOML arrays cannot contain null optional values"));
            }
            values.push_back(std::move(*encoded));
        }
        Node output{std::move(values)};
        if constexpr (Reflectable<typename VectorTraits<U>::value_type>) {
            output.array_table = true;
        }
        return output;
    } else if constexpr (ArrayTraits<U>::value) {
        Node::array values;
        for (const auto& element : input) {
            auto encoded = encodeValue(element);
            if (!encoded) {
                return std::unexpected(encoded.error());
            }
            if (std::holds_alternative<std::monostate>(encoded->value)) {
                return std::unexpected(std::string("TOML arrays cannot contain null optional values"));
            }
            values.push_back(std::move(*encoded));
        }
        Node output{std::move(values)};
        if constexpr (Reflectable<typename ArrayTraits<U>::value_type>) {
            output.array_table = true;
        }
        return output;
    } else if constexpr (MapTraits<U>::value) {
        Node::table values;
        for (const auto& [key, element] : input) {
            if (!valid_utf8(key)) {
                return std::unexpected(std::string("invalid UTF-8 in TOML key"));
            }
            auto encoded = encodeValue(element);
            if (!encoded) {
                return std::unexpected(encoded.error());
            }
            if (std::holds_alternative<std::monostate>(encoded->value)) {
                continue;
            }
            values.emplace(key, std::move(*encoded));
        }
        return Node{std::move(values)};
    } else if constexpr (Reflectable<U>) {
        Node::table values;
        std::string failure;
        bool failed = false;
        for_each_field(input, [&](const auto& descriptor, const auto& object) {
            if (failed) {
                return;
            }
            const auto& member = descriptor.get(object);
            using Member = bare_t<decltype(member)>;
            if (!valid_utf8(descriptor.name)) {
                failed = true;
                failure = "invalid UTF-8 in reflected TOML key";
                return;
            }
            if constexpr (OptionalTraits<Member>::value) {
                if (!member) {
                    return;
                }
            }
            auto encoded = encodeValue(member);
            if (!encoded) {
                failed = true;
                failure = encoded.error();
                return;
            }
            if (std::holds_alternative<std::monostate>(encoded->value)) {
                return;
            }
            if (!values.emplace(std::string(descriptor.name), std::move(*encoded)).second) {
                failed = true;
                failure = "duplicate reflected field: " + std::string(descriptor.name);
            }
        });
        if (failed) {
            return std::unexpected(std::move(failure));
        }
        return Node{std::move(values)};
    } else {
        return std::unexpected(std::string("unsupported type in TOML serializer"));
    }
}

/**
 * @brief 判断键是否可以按 TOML 裸键形式输出。
 * @param key 待判断的键名。
 * @return 键只含字母、数字、下划线或连字符时返回 `true`。
 */
inline bool bareKey(std::string_view key) {
    if (key.empty()) {
        return false;
    }
    for (const char character : key) {
        if (!((character >= 'A' && character <= 'Z') ||
              (character >= 'a' && character <= 'z') ||
              (character >= '0' && character <= '9') ||
              character == '_' || character == '-')) {
            return false;
        }
    }
    return true;
}

/**
 * @brief 将键格式化为合法的 TOML 键文本。
 * @param key 原始键名。
 * @return 裸键或经过转义的基本字符串键。
 */
inline std::string formatKey(std::string_view key) {
    if (bareKey(key)) {
        return std::string(key);
    }
    std::string result = "\"";
    for (const char character : key) {
        switch (character) {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (static_cast<unsigned char>(character) < 0x20 ||
                    static_cast<unsigned char>(character) == 0x7f) {
                    constexpr char hex[] = "0123456789abcdef";
                    const auto byte = static_cast<unsigned char>(character);
                    result += "\\u00";
                    result.push_back(hex[byte >> 4]);
                    result.push_back(hex[byte & 0x0f]);
                } else {
                    result.push_back(character);
                }
                break;
        }
    }
    result.push_back('"');
    return result;
}

/**
 * @brief 将十进制数字按指定宽度补零后追加到字符串。
 * @param output 目标字符串。
 * @param value 待输出的数字。
 * @param width 最小输出宽度。
 */
inline void appendPaddedDecimal(std::string& output, unsigned value,
                                  unsigned width) {
    std::array<char, 16> digits{};
    for (unsigned index = 0; index < width; ++index) {
        digits[width - index - 1] = static_cast<char>('0' + value % 10);
        value /= 10;
    }
    output.append(digits.data(), width);
}

/**
 * @brief 将日期按 TOML 标准格式追加到字符串。
 * @param value 待格式化的日期。
 * @param output 目标字符串。
 */
inline void appendDateText(const date& value, std::string& output) {
    appendPaddedDecimal(output, static_cast<unsigned>(value.year), 4);
    output.push_back('-');
    appendPaddedDecimal(output, value.month, 2);
    output.push_back('-');
    appendPaddedDecimal(output, value.day, 2);
}

/**
 * @brief 将本地时间按 TOML 标准格式追加到字符串。
 * @param value 待格式化的时间。
 * @param output 目标字符串。
 */
inline void appendTimeText(const time& value, std::string& output) {
    appendPaddedDecimal(output, value.hour, 2);
    output.push_back(':');
    appendPaddedDecimal(output, value.minute, 2);
    output.push_back(':');
    appendPaddedDecimal(output, value.second, 2);
    if (!value.fractional_second.empty()) {
        output.push_back('.');
        output += value.fractional_second;
    }
}

/**
 * @brief 将本地日期时间按 TOML 标准格式追加到字符串。
 * @param value 待格式化的本地日期时间。
 * @param output 目标字符串。
 */
inline void appendLocalDateTimeText(const local_date_time& value,
                                        std::string& output) {
    appendDateText(value.date_part, output);
    output.push_back('T');
    appendTimeText(value.time_part, output);
}

/**
 * @brief 将带偏移日期时间按 TOML 标准格式追加到字符串。
 * @param value 待格式化的带偏移日期时间。
 * @param output 目标字符串。
 */
inline void appendOffsetDateTimeText(const offset_date_time& value,
                                         std::string& output) {
    appendLocalDateTimeText(value.local, output);
    if (value.offset_minutes == 0) {
        output.push_back('Z');
        return;
    }
    const int magnitude = std::abs(value.offset_minutes);
    output.push_back(value.offset_minutes < 0 ? '-' : '+');
    appendPaddedDecimal(output, static_cast<unsigned>(magnitude / 60), 2);
    output.push_back(':');
    appendPaddedDecimal(output, static_cast<unsigned>(magnitude % 60), 2);
}

/**
 * @brief 将节点作为 TOML 内联值追加到输出。
 * @param value 待输出的节点。
 * @param output 目标字符串。
 * @param failure 失败时写入错误描述。
 * @return 输出成功时返回 `true`，否则返回 `false`。
 */
inline bool appendInline(const Node& value, std::string& output,
                          std::string& failure) {
    return std::visit(
        [&](const auto& item) -> bool {
            using Item = bare_t<decltype(item)>;
            if constexpr (std::same_as<Item, std::monostate>) {
                failure = "TOML has no null value";
                return false;
            } else if constexpr (std::same_as<Item, bool>) {
                output += item ? "true" : "false";
                return true;
            } else if constexpr (std::same_as<Item, std::int64_t>) {
                output += std::to_string(item);
                return true;
            } else if constexpr (std::same_as<Item, double>) {
                if (std::isnan(item)) {
                    output += "nan";
                    return true;
                }
                if (std::isinf(item)) {
                    output += item < 0 ? "-inf" : "inf";
                    return true;
                }
                char buffer[64]{};
                const auto [end, error] = std::to_chars(
                    buffer, buffer + sizeof(buffer), item,
                    std::chars_format::general, 17);
                if (error != std::errc{}) {
                    failure = "failed to format TOML floating-point value";
                    return false;
                }
                output.append(buffer, end);
                return true;
            } else if constexpr (std::same_as<Item, date>) {
                appendDateText(item, output);
                return true;
            } else if constexpr (std::same_as<Item, time>) {
                appendTimeText(item, output);
                return true;
            } else if constexpr (std::same_as<Item, local_date_time>) {
                appendLocalDateTimeText(item, output);
                return true;
            } else if constexpr (std::same_as<Item, offset_date_time>) {
                appendOffsetDateTimeText(item, output);
                return true;
            } else if constexpr (std::same_as<Item, std::string>) {
                output.push_back('"');
                for (const char character : item) {
                    switch (character) {
                        case '\\': output += "\\\\"; break;
                        case '"': output += "\\\""; break;
                        case '\b': output += "\\b"; break;
                        case '\f': output += "\\f"; break;
                        case '\n': output += "\\n"; break;
                        case '\r': output += "\\r"; break;
                        case '\t': output += "\\t"; break;
                        default:
                            if (static_cast<unsigned char>(character) < 0x20 ||
                                static_cast<unsigned char>(character) == 0x7f) {
                                constexpr char hex[] = "0123456789abcdef";
                                const auto byte = static_cast<unsigned char>(character);
                                output += "\\u00";
                                output.push_back(hex[byte >> 4]);
                                output.push_back(hex[byte & 0x0f]);
                            } else {
                                output.push_back(character);
                            }
                            break;
                    }
                }
                output.push_back('"');
                return true;
            } else if constexpr (std::same_as<Item, Node::array>) {
                output.push_back('[');
                for (std::size_t index = 0; index < item.size(); ++index) {
                    if (index != 0) {
                        output += ", ";
                    }
                    if (!appendInline(item[index], output, failure)) {
                        return false;
                    }
                }
                output.push_back(']');
                return true;
            } else if constexpr (std::same_as<Item, Node::table>) {
                output.push_back('{');
                std::size_t index = 0;
                for (const auto& [key, child] : item) {
                    if (index++ != 0) {
                        output += ", ";
                    }
                    output += formatKey(key);
                    output += " = ";
                    if (!appendInline(child, output, failure)) {
                        return false;
                    }
                }
                output.push_back('}');
                return true;
            }
        },
        value.value);
}

/**
 * @brief 将键追加到已格式化的 TOML 路径后。
 * @param prefix 已有的路径前缀。
 * @param key 要追加的键名。
 * @param output 目标字符串。
 */
inline void appendKeyPath(std::string_view prefix, std::string_view key,
                            std::string& output) {
    if (prefix.empty()) {
        output += formatKey(key);
    } else {
        output += prefix;
        output.push_back('.');
        output += formatKey(key);
    }
}

/**
 * @brief 判断节点是否为由普通表组成的数组表。
 * @param value 待检查的节点。
 * @return 数组表底层数组指针；不是数组表或为空时返回 `nullptr`。
 */
inline const Node::array* tableArray(const Node& value) {
    if (!value.array_table) {
        return nullptr;
    }
    const auto* values = std::get_if<Node::array>(&value.value);
    if (values == nullptr || values->empty()) {
        return nullptr;
    }
    for (const auto& element : *values) {
        if (!std::holds_alternative<Node::table>(element.value) || element.inline_table) {
            return nullptr;
        }
    }
    return values;
}

/**
 * @brief 按规范顺序将表及其子表追加为 TOML 文本。
 * @param table 待输出的表。
 * @param prefix 当前表的路径前缀。
 * @param output 目标字符串。
 * @param failure 失败时写入错误描述。
 * @return 输出成功时返回 `true`，否则返回 `false`。
 */
inline bool appendTable(const Node::table& table, std::string_view prefix,
                         std::string& output, std::string& failure) {
    for (const auto& [key, value] : table) {
        if ((std::holds_alternative<Node::table>(value.value) && !value.inline_table) ||
            tableArray(value) != nullptr) {
            continue;
        }
        output += formatKey(key);
        output += " = ";
        if (!appendInline(value, output, failure)) {
            return false;
        }
        output.push_back('\n');
    }

    for (const auto& [key, value] : table) {
        const auto* child = std::get_if<Node::table>(&value.value);
        if (child == nullptr || value.inline_table) {
            continue;
        }
        if (!output.empty() && output.back() != '\n') {
            output.push_back('\n');
        }
        output.push_back('\n');
        std::string section;
        appendKeyPath(prefix, key, section);
    output += "[" + section + "]\n";
    if (!appendTable(*child, section, output, failure)) {
            return false;
        }
    }

    for (const auto& [key, value] : table) {
        const auto* values = tableArray(value);
        if (values == nullptr) {
            continue;
        }
        std::string section;
        appendKeyPath(prefix, key, section);
        for (const auto& element : *values) {
            const auto& child = std::get<Node::table>(element.value);
            if (!output.empty() && output.back() != '\n') {
                output.push_back('\n');
            }
            output.push_back('\n');
            output += "[[" + section + "]]\n";
            if (!appendTable(child, section, output, failure)) {
                return false;
            }
        }
    }
    return true;
}

/**
 * @brief 将内部 TOML 节点递归解码为 C++ 值。
 * @tparam T 目标 C++ 类型。
 * @param input 待读取的内部节点。
 * @param path 当前值在文档中的路径，用于生成错误信息。
 * @return 解码后的 C++ 值，或类型/范围不匹配错误。
 */
template <class T>
result<T> decodeValue(const Node& input, std::string_view path, const ParseOptions& options) {
    using U = BareT<T>;
    const auto where = path.empty() ? std::string("value") : std::string(path);

    if constexpr (OptionalTraits<U>::value) {
        if (std::holds_alternative<std::monostate>(input.value)) {
            return U{};
        }
        auto decoded = decodeValue<typename OptionalTraits<U>::value_type>(input, path, options);
        if (!decoded) {
            return std::unexpected(decoded.error());
        }
        return U{std::move(*decoded)};
    } else if constexpr (InlineTableTraits<U>::value) {
        auto decoded = decodeValue<typename InlineTableTraits<U>::value_type>(input, path, options);
        if (!decoded) {
            return std::unexpected(decoded.error());
        }
        return U{std::move(*decoded)};
    } else if constexpr (std::same_as<U, date>) {
        if (const auto* value = std::get_if<date>(&input.value)) {
            return *value;
        }
        return std::unexpected(where + " must be a TOML local date");
    } else if constexpr (std::same_as<U, time>) {
        if (const auto* value = std::get_if<time>(&input.value)) {
            return *value;
        }
        return std::unexpected(where + " must be a TOML local time");
    } else if constexpr (std::same_as<U, local_date_time>) {
        if (const auto* value = std::get_if<local_date_time>(&input.value)) {
            return *value;
        }
        return std::unexpected(where + " must be a TOML local date-time");
    } else if constexpr (std::same_as<U, offset_date_time>) {
        if (const auto* value = std::get_if<offset_date_time>(&input.value)) {
            return *value;
        }
        return std::unexpected(where + " must be a TOML offset date-time");
    } else if constexpr (std::same_as<U, std::string>) {
        if (const auto* value = std::get_if<std::string>(&input.value)) {
            return *value;
        }
        return std::unexpected(where + " must be a TOML string");
    } else if constexpr (std::same_as<U, bool>) {
        if (const auto* value = std::get_if<bool>(&input.value)) {
            return *value;
        }
        return std::unexpected(where + " must be a TOML boolean");
    } else if constexpr (std::is_integral_v<U>) {
        const auto* value = std::get_if<std::int64_t>(&input.value);
        if (value == nullptr) {
            return std::unexpected(where + " must be a TOML integer");
        }
        if constexpr (std::is_signed_v<U>) {
            if constexpr (std::numeric_limits<U>::digits <
                          std::numeric_limits<std::int64_t>::digits) {
                if (*value < static_cast<std::int64_t>(std::numeric_limits<U>::min()) ||
                    *value > static_cast<std::int64_t>(std::numeric_limits<U>::max())) {
                    return std::unexpected(where + " is outside the destination integer range");
                }
            }
        } else {
            if (*value < 0) {
                return std::unexpected(where + " is outside the destination integer range");
            }
            if constexpr (std::numeric_limits<U>::digits <
                          std::numeric_limits<std::int64_t>::digits + 1) {
                if (static_cast<std::uint64_t>(*value) >
                    static_cast<std::uint64_t>(std::numeric_limits<U>::max())) {
                    return std::unexpected(where + " is outside the destination integer range");
                }
            }
        }
        return static_cast<U>(*value);
    } else if constexpr (std::is_floating_point_v<U>) {
        if (const auto* value = std::get_if<double>(&input.value)) {
            return static_cast<U>(*value);
        }
        if (const auto* value = std::get_if<std::int64_t>(&input.value)) {
            return static_cast<U>(*value);
        }
        return std::unexpected(where + " must be a TOML number");
    } else if constexpr (std::is_enum_v<U>) {
        using Underlying = std::underlying_type_t<U>;
        auto decoded = decodeValue<Underlying>(input, path, options);
        if (!decoded) {
            return std::unexpected(decoded.error());
        }
        return static_cast<U>(*decoded);
    } else if constexpr (VectorTraits<U>::value) {
        const auto* values = std::get_if<Node::array>(&input.value);
        if (values == nullptr) {
            return std::unexpected(where + " must be a TOML array");
        }
        U output;
        if constexpr (requires { output.reserve(values->size()); }) {
            output.reserve(values->size());
        }
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
            return std::unexpected(where + " has the wrong TOML array size");
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
        const auto* values = std::get_if<Node::table>(&input.value);
        if (values == nullptr) {
            return std::unexpected(where + " must be a TOML table");
        }
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
        const auto* values = std::get_if<Node::table>(&input.value);
        if (values == nullptr) {
            return std::unexpected(where + " must be a TOML table");
        }
        if constexpr (!std::is_default_constructible_v<U>) {
            return std::unexpected(where + " is not default constructible");
        } else {
            U output{};
            bool failed = false;
            std::string failure;
            for_each_field(output, [&](const auto& descriptor, auto& object) {
                if (failed) {
                    return;
                }
                using Member = bare_t<decltype(descriptor.get(object))>;
                if constexpr (!std::is_assignable_v<decltype(descriptor.get(object)),
                                                    Member>) {
                    failed = true;
                    failure = where + " field '" + std::string(descriptor.name) +
                              "' is not assignable";
                } else {
                    const auto iterator = values->find(std::string(descriptor.name));
                    if (iterator == values->end()) {
                        if constexpr (OptionalTraits<Member>::value) {
                            return;
                        } else {
                            failed = true;
                            failure = where + " is missing field '" +
                                      std::string(descriptor.name) + "'";
                        }
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
                }
            });
            if (!failed && options.unknown_fields == UnknownFieldPolicy::reject) {
                for (const auto& [key, ignored] : *values) {
                    bool known = false;
                    std::apply([&](const auto&... descriptor) {
                        known = ((key == descriptor.name) || ...);
                    }, reflect::fields(output));
                    if (!known) {
                        failed = true;
                        failure = where + " contains unknown field '" + key + "'";
                        break;
                    }
                    static_cast<void>(ignored);
                }
            }
            if (failed) {
                return std::unexpected(std::move(failure));
            }
            return output;
        }
    } else {
        return std::unexpected(where + " has an unsupported C++ type");
    }
}

}  // 命名空间 detail

/**
 * @brief 将可反射的 C++ 对象序列化为规范 TOML 文本。
 * @tparam T 待序列化的对象类型。
 * @param value 待序列化的对象。
 * @return TOML 文本，或类型不受支持、值非法等错误。
 */
template <class T>
result<std::string> serialize(const T& value,
                              const SerializeOptions& options = {}) {
    try {
        auto encoded = detail::encodeValue(value);
        if (!encoded) {
            return std::unexpected(encoded.error());
        }
        const auto* root = std::get_if<typename detail::Node::table>(&encoded->value);
        if (root == nullptr) {
            return std::unexpected(std::string("TOML serialization requires a reflected struct/table at the root"));
        }
        std::string output;
        std::string failure;
        if (!detail::appendTable(*root, {}, output, failure)) {
            return std::unexpected(std::move(failure));
        }
        if (output.size() > options.max_output_bytes) {
            return std::unexpected(std::string("TOML output exceeds configured size limit"));
        }
        return output;
    } catch (const std::bad_alloc&) {
        return std::unexpected(std::string("TOML operation exhausted memory"));
    } catch (const std::exception& error) {
        return std::unexpected(std::string("TOML operation failed: ") + error.what());
    } catch (...) {
        return std::unexpected(std::string("TOML operation failed with an unknown exception"));
    }
}

/**
 * @brief `serialize` 的兼容别名。
 * @tparam T 待序列化的对象类型。
 * @param value 待序列化的对象。
 * @return 与 `serialize` 相同的 TOML 文本或错误结果。
 */
template <class T>
result<std::string> try_serialize(const T& value, const SerializeOptions& options = {}) {
    return serialize(value, options);
}

/**
 * @brief 将 TOML 文本解析并转换为指定的 C++ 类型。
 * @tparam T 目标 C++ 类型。
 * @param text 待解析的 TOML 文本。
 * @return 解码后的对象，或语法、类型及范围错误。
 */
template <class T>
result<T> deserialize(std::string_view text,
                      const ParseOptions& options = {}) {
    try {
        auto parsed = detail::parseDocument(text, options);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        return detail::decodeValue<T>(*parsed, {}, options);
    } catch (const std::bad_alloc&) {
        return std::unexpected(std::string("TOML operation exhausted memory"));
    } catch (const std::exception& error) {
        return std::unexpected(std::string("TOML operation failed: ") + error.what());
    } catch (...) {
        return std::unexpected(std::string("TOML operation failed with an unknown exception"));
    }
}

/**
 * @brief `deserialize` 的历史拼写兼容别名。
 * @tparam T 目标 C++ 类型。
 * @param text 待解析的 TOML 文本。
 * @return 与 `deserialize<T>` 相同的结果。
 */
template <class T>
result<T> deserializee(std::string_view text, const ParseOptions& options = {}) {
    return deserialize<T>(text, options);
}

/**
 * @brief `deserialize` 的大小写兼容别名。
 * @tparam T 目标 C++ 类型。
 * @param text 待解析的 TOML 文本。
 * @return 与 `deserialize<T>` 相同的结果。
 */
template <class T>
result<T> deSerialize(std::string_view text, const ParseOptions& options = {}) {
    return deserialize<T>(text, options);
}

}  // 命名空间 toml
