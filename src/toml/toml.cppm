export module toml;

export import std;

export namespace toml {

/**
 * @brief 表示一个可能成功或失败的 TOML 操作结果。
 * @tparam T 成功时保存的值类型。
 */
template <class T>
using result = std::expected<T, std::string>;

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
struct inline_table {
    T value;

    /**
     * @brief 比较两个内联表包装器是否相同。
     * @param other 待比较的内联表。
     * @return 包装值相同时返回 `true`。
     */
    bool operator==(const inline_table& other) const = default;
};

/**
 * @brief 保存反射字段名称及其成员指针的描述符。
 *
 * 描述符保持轻量且可在编译期构造，使生成的反射信息无需动态分配。
 * @tparam Owner 拥有该成员的结构类型。
 * @tparam Member 成员类型。
 */
template <class Owner, class Member>
struct field {
    std::string_view name;
    Member Owner::*pointer;

    /**
     * @brief 通过成员指针取得对象中的字段。
     * @tparam Object 对象类型，可以是左值或右值。
     * @param object 待读取字段的对象。
     * @return 对应字段的引用，并保持对象的值类别。
     */
    template <class Object>
    constexpr decltype(auto) get(Object&& object) const noexcept {
        return std::forward<Object>(object).*pointer;
    }
};

/**
 * @brief 创建一个反射字段描述符。
 * @tparam Owner 拥有该成员的结构类型。
 * @tparam Member 成员类型。
 * @param name TOML 字段名称。
 * @param pointer 指向成员的指针。
 * @return 新建的字段描述符。
 */
template <class Owner, class Member>
constexpr auto make_field(std::string_view name, Member Owner::*pointer) {
    return field<Owner, Member>{name, pointer};
}

/**
 * @brief 判断类型是否提供可通过参数依赖查找找到的反射函数。
 * @tparam T 待检测的类型。
 */
template <class T>
concept reflectable = requires(const std::remove_cvref_t<T>& value) {
    toml_reflect(value);
};

/**
 * @brief 遍历对象的全部反射字段。
 * @tparam T 对象类型。
 * @tparam Function 接收字段描述符和对象引用的可调用类型。
 * @param value 待读取或修改的对象。
 * @param function 每个字段调用一次的回调函数。
 */
template <class T, class Function>
    requires reflectable<T>
constexpr void for_each_field(T& value, Function&& function) {
    auto descriptors = toml_reflect(value);
    std::apply(
        [&](const auto&... descriptor) {
            (std::invoke(function, descriptor, value), ...);
        },
        descriptors);
}

namespace detail {

template <class T>
using bare_t = std::remove_cvref_t<T>;

template <class T>
struct optional_traits {
    static constexpr bool value = false;
};

template <class T>
struct optional_traits<std::optional<T>> {
    static constexpr bool value = true;
    using value_type = T;
};

template <class T>
struct vector_traits {
    static constexpr bool value = false;
};

template <class T, class Allocator>
struct vector_traits<std::vector<T, Allocator>> {
    static constexpr bool value = true;
    using value_type = T;
};

template <class T>
struct array_traits {
    static constexpr bool value = false;
};

template <class T>
struct inline_table_traits {
    static constexpr bool value = false;
};

template <class T>
struct inline_table_traits<inline_table<T>> {
    static constexpr bool value = true;
    using value_type = T;
};

template <class T, std::size_t Size>
struct array_traits<std::array<T, Size>> {
    static constexpr bool value = true;
    using value_type = T;
    static constexpr std::size_t size = Size;
};

template <class T>
struct map_traits {
    static constexpr bool value = false;
};

template <class Value, class Compare, class Allocator>
struct map_traits<std::map<std::string, Value, Compare, Allocator>> {
    static constexpr bool value = true;
    using mapped_type = Value;
};

template <class Value, class Hash, class Equal, class Allocator>
struct map_traits<std::unordered_map<std::string, Value, Hash, Equal,
                                      Allocator>> {
    static constexpr bool value = true;
    using mapped_type = Value;
};

struct node {
    using array = std::vector<node>;
    using table = std::map<std::string, node>;
    using storage = std::variant<std::monostate, bool, std::int64_t, double,
                                 std::string, date, time, local_date_time,
                                 offset_date_time, array, table>;

    storage value{};
    bool inline_table = false;

    /** @brief 构造一个空节点。 */
    node() = default;

    /**
     * @brief 复制构造节点。
     * @param other 待复制的节点。
     */
    node(const node& other) = default;

    /**
     * @brief 移动构造节点。
     * @param other 待移动的节点。
     */
    node(node&& other) noexcept = default;

    /**
     * @brief 复制赋值节点。
     * @param other 待复制的节点。
     * @return 当前节点的引用。
     */
    node& operator=(const node& other) = default;

    /**
     * @brief 移动赋值节点。
     * @param other 待移动的节点。
     * @return 当前节点的引用。
     */
    node& operator=(node&& other) noexcept = default;

    /**
     * @brief 使用任意受支持的值构造节点。
     * @tparam T 输入值类型。
     * @param input 要保存的值。
     */
    template <class T>
        requires(!std::same_as<bare_t<T>, node>)
    node(T&& input) : value(std::forward<T>(input)) {}
};

/**
 * @brief 去除字符串首尾的 ASCII 空白字符。
 * @param text 待处理的文本视图。
 * @return 去除首尾空白后的视图；不会复制底层字符串。
 */
inline std::string_view trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    return text;
}

/**
 * @brief 移除不在字符串字面量中的 TOML 行注释。
 * @param line 待处理的单行文本。
 * @return 去除注释后的字符串。
 */
inline std::string strip_comment(std::string_view line) {
    bool double_quoted = false;
    bool single_quoted = false;
    bool escaped = false;

    for (std::size_t index = 0; index < line.size(); ++index) {
        const char character = line[index];
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
        } else if (character == '#') {
            return std::string(line.substr(0, index));
        }
    }

    return std::string(line);
}

/**
 * @brief 查找不在引号中的目标字符。
 * @param text 待搜索的文本。
 * @param needle 要查找的字符。
 * @return 目标字符的位置；找不到时返回 `std::string_view::npos`。
 */
inline std::size_t find_unquoted(std::string_view text, char needle) {
    bool double_quoted = false;
    bool single_quoted = false;
    bool escaped = false;

    for (std::size_t index = 0; index < text.size(); ++index) {
        const char character = text[index];
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
        } else if (character == needle) {
            return index;
        }
    }

    return std::string_view::npos;
}

/**
 * @brief 解析单个 TOML 键并返回其实际名称。
 *
 * 同时支持裸键、基本字符串键和字面字符串键。
 * @param raw_key 原始键文本。
 * @return 解析后的键名，或包含原因的错误结果。
 */
inline result<std::string> parse_key(std::string_view raw_key) {
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
                if (key[index] == '\'' || key[index] == '\n' || key[index] == '\r') {
                    return std::unexpected(std::string("invalid literal TOML key"));
                }
            }
            return std::string(key.substr(1, key.size() - 2));
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
            } else {
                decoded.push_back(character);
            }
        }
        if (escaped) {
            return std::unexpected(std::string("unterminated key escape"));
        }
        return decoded;
    }

    for (const char character : key) {
        if (!(std::isalnum(static_cast<unsigned char>(character)) ||
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
inline result<std::vector<std::string>> parse_key_path(std::string_view raw) {
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
            auto part = parse_key(raw.substr(start, index - start));
            if (!part) {
                return std::unexpected(part.error());
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
inline result<node> parse_temporal_value(std::string_view text) {
    const bool date_like = text.size() >= 10 && ascii_digit(text[0]) &&
                           ascii_digit(text[1]) && ascii_digit(text[2]) &&
                           ascii_digit(text[3]) && text[4] == '-' && text[7] == '-';
    if (date_like) {
        auto parsed_date = parse_date_value(text.substr(0, 10));
        if (!parsed_date) {
            return std::unexpected(parsed_date.error());
        }
        if (text.size() == 10) {
            return node{std::move(*parsed_date)};
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
            return node{local};
        }
        if (text.substr(offset) == "Z" || text.substr(offset) == "z") {
            return node{offset_date_time{local, 0}};
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
        return node{offset_date_time{local, offset_minutes}};
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
        return node{std::move(parsed_time->first)};
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
inline bool insert_value_at(node::table& root,
                            const std::vector<std::string>& path,
                            node value) {
    if (path.empty()) {
        return false;
    }
    node::table* table = &root;
    for (std::size_t index = 0; index + 1 < path.size(); ++index) {
        auto [iterator, inserted] = table->try_emplace(path[index], node{node::table{}});
        if (!inserted && !std::holds_alternative<node::table>(iterator->second.value)) {
            return false;
        }
        table = &std::get<node::table>(iterator->second.value);
    }
    return table->emplace(path.back(), std::move(value)).second;
}

/**
 * @brief 解析单个 TOML 值及其嵌套结构。
 *
 * 解析器维护输入视图和当前偏移量，供文档解析器在赋值语句中复用。
 */
class value_parser {
public:
    /**
     * @brief 构造值解析器。
     * @param text 待解析的完整文本。
     * @param position 初始读取位置，默认为文本开头。
     */
    explicit value_parser(std::string_view text, std::size_t position = 0)
        : text_(text), position_(position) {}

    /**
     * @brief 解析一个值并确保后面没有多余文本。
     * @return 解析出的节点，或语法错误。
     */
    result<node> parse() {
        auto parsed = parse_value();
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
    result<node> parse_one() {
        return parse_value();
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
        while (position_ < text_.size() &&
               std::isspace(static_cast<unsigned char>(text_[position_]))) {
            ++position_;
        }
    }

    /** @brief 跳过内联表允许的空格和制表符。 */
    void skip_inline_space() {
        while (position_ < text_.size() &&
               (text_[position_] == ' ' || text_[position_] == '\t')) {
            ++position_;
        }
    }

    /** @brief 跳过空白和以 `#` 开头的注释。 */
    void skip_trivia() {
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
    result<node> parse_value() {
        skip_space();
        if (position_ >= text_.size()) {
            return std::unexpected(std::string("missing TOML value"));
        }

        switch (text_[position_]) {
            case '"':
                return starts_with("\"\"\"") ? parse_multiline_basic_string()
                                                   : parse_basic_string();
            case '\'':
                return starts_with("'''") ? parse_multiline_literal_string()
                                             : parse_literal_string();
            case '[':
                return parse_array();
            case '{':
                return parse_inline_table();
            default:
                return parse_atom();
        }
    }

    /**
     * @brief 解析单行基本字符串。
     * @return 字符串节点，或字符串语法错误。
     */
    result<node> parse_basic_string() {
        ++position_;
        std::string value;
        while (position_ < text_.size()) {
            const char character = text_[position_++];
            if (character == '"') {
                return node{std::move(value)};
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
            } else {
                value.push_back(character);
            }
        }
        return std::unexpected(std::string("unterminated basic TOML string"));
    }

    /**
     * @brief 解析单行字面字符串。
     * @return 字符串节点，或字符串语法错误。
     */
    result<node> parse_literal_string() {
        ++position_;
        std::string value;
        while (position_ < text_.size()) {
            const char character = text_[position_++];
            if (character == '\'') {
                return node{std::move(value)};
            }
            if (character == '\n' || character == '\r') {
                return std::unexpected(std::string("newline in literal TOML string"));
            }
            value.push_back(character);
        }
        return std::unexpected(std::string("unterminated literal TOML string"));
    }

    /**
     * @brief 解析三引号包围的多行基本字符串。
     * @return 字符串节点，或字符串语法错误。
     */
    result<node> parse_multiline_basic_string() {
        position_ += 3;
        if (position_ < text_.size() &&
            (text_[position_] == '\n' || text_[position_] == '\r')) {
            consume_line_break();
        }

        std::string value;
        while (position_ < text_.size()) {
            if (starts_with("\"\"\"")) {
                position_ += 3;
                return node{std::move(value)};
            }
            const char character = text_[position_++];
            if (character == '\\') {
                if (position_ < text_.size() &&
                    (text_[position_] == '\n' || text_[position_] == '\r')) {
                    consume_line_break();
                    while (position_ < text_.size() &&
                           std::isspace(static_cast<unsigned char>(text_[position_]))) {
                        ++position_;
                    }
                    continue;
                }
                auto escaped = append_basic_escape(value);
                if (!escaped) {
                    return std::unexpected(escaped.error());
                }
            } else {
                value.push_back(character);
            }
        }
        return std::unexpected(std::string("unterminated multiline basic TOML string"));
    }

    /**
     * @brief 解析三单引号包围的多行字面字符串。
     * @return 字符串节点，或字符串语法错误。
     */
    result<node> parse_multiline_literal_string() {
        position_ += 3;
        if (position_ < text_.size() &&
            (text_[position_] == '\n' || text_[position_] == '\r')) {
            consume_line_break();
        }

        std::string value;
        while (position_ < text_.size()) {
            if (starts_with("'''") ) {
                position_ += 3;
                return node{std::move(value)};
            }
            value.push_back(text_[position_++]);
        }
        return std::unexpected(std::string("unterminated multiline literal TOML string"));
    }

    /**
     * @brief 解析 TOML 数组及其元素。
     * @return 数组节点，或数组语法错误。
     */
    result<node> parse_array() {
        ++position_;
        node::array values;
        skip_trivia();
        if (position_ < text_.size() && text_[position_] == ']') {
            ++position_;
            return node{std::move(values)};
        }

        while (position_ < text_.size()) {
            auto value = parse_value();
            if (!value) {
                return std::unexpected(value.error());
            }
            values.push_back(std::move(*value));
            skip_trivia();
            if (position_ >= text_.size()) {
                break;
            }
            if (text_[position_] == ']') {
                ++position_;
                return node{std::move(values)};
            }
            if (text_[position_] != ',') {
                return std::unexpected(std::string("expected ',' or ']' in TOML array"));
            }
            ++position_;
            skip_trivia();
            if (position_ < text_.size() && text_[position_] == ']') {
                ++position_;
                return node{std::move(values)};
            }
        }

        return std::unexpected(std::string("unterminated TOML array"));
    }

    /**
     * @brief 解析 TOML 内联表。
     * @return 内联表节点，或键值语法错误。
     */
    result<node> parse_inline_table() {
        ++position_;
        node::table values;
        skip_inline_space();
        if (position_ < text_.size() && text_[position_] == '}') {
            ++position_;
            return node{std::move(values)};
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
            auto path = parse_key_path(text_.substr(key_start, position_ - key_start));
            if (!path || path->empty()) {
                return std::unexpected(path ? std::string("empty inline TOML key")
                                             : path.error());
            }
            ++position_;
            auto value = parse_value();
            if (!value) {
                return std::unexpected(value.error());
            }
            if (!insert_value_at(values, *path, std::move(*value))) {
                return std::unexpected(std::string("duplicate key in inline TOML table"));
            }
            skip_inline_space();
            if (position_ >= text_.size()) {
                break;
            }
            if (text_[position_] == '}') {
                ++position_;
                return node{std::move(values)};
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
    result<node> parse_atom() {
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
            return node{true};
        }
        if (atom == "false") {
            return node{false};
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
        if (atom.front() == '_' || atom.back() == '_' || atom.find("__") != std::string_view::npos) {
            return std::unexpected(std::string("invalid underscore in TOML number"));
        }

        std::string normalized;
        normalized.reserve(atom.size());
        for (const char character : atom) {
            if (character != '_') {
                normalized.push_back(character);
            }
        }

        if (normalized == "inf" || normalized == "+inf") {
            return node{std::numeric_limits<double>::infinity()};
        }
        if (normalized == "-inf") {
            return node{-std::numeric_limits<double>::infinity()};
        }
        if (normalized == "nan" || normalized == "+nan" || normalized == "-nan") {
            return node{std::numeric_limits<double>::quiet_NaN()};
        }

        std::string_view numeric_text = normalized;
        if (!numeric_text.empty() &&
            (numeric_text.front() == '+' || numeric_text.front() == '-')) {
            numeric_text.remove_prefix(1);
        }
        const bool prefixed_integer =
            numeric_text.size() >= 2 && numeric_text.front() == '0' &&
            (numeric_text[1] == 'x' || numeric_text[1] == 'X' ||
             numeric_text[1] == 'o' || numeric_text[1] == 'O' ||
             numeric_text[1] == 'b' || numeric_text[1] == 'B');
        const bool floating = !prefixed_integer &&
                              normalized.find_first_of(".eE") != std::string::npos;
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
            return node{value};
        }

        bool negative = false;
        std::string_view digits = normalized;
        if (!digits.empty() && (digits.front() == '+' || digits.front() == '-')) {
            negative = digits.front() == '-';
            digits.remove_prefix(1);
        }
        int base = 10;
        if (digits.size() >= 2 && digits[0] == '0' &&
            (digits[1] == 'x' || digits[1] == 'X' || digits[1] == 'o' ||
             digits[1] == 'O' || digits[1] == 'b' || digits[1] == 'B')) {
            switch (digits[1]) {
                case 'x': case 'X': base = 16; break;
                case 'o': case 'O': base = 8; break;
                case 'b': case 'B': base = 2; break;
                default: break;
            }
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
                return node{std::numeric_limits<std::int64_t>::min()};
            }
            return node{-static_cast<std::int64_t>(magnitude)};
        }
        if (magnitude > positive_limit) {
            return std::unexpected(std::string("TOML integer is outside signed 64-bit range"));
        }
        return node{static_cast<std::int64_t>(magnitude)};
    }

    std::string_view text_;
    std::size_t position_ = 0;
};

/**
 * @brief 解析完整 TOML 文档并生成内部表树。
 */
class document_parser {
public:
    /**
     * @brief 构造文档解析器。
     * @param text 待解析的 TOML 文档文本。
     */
    explicit document_parser(std::string_view text) : text_(text) {}

    /**
     * @brief 解析完整文档。
     * @return 文档根表，或包含位置上下文的语法错误。
     */
    result<node::table> parse() {
        node::table root;
        node::table* current_table = &root;
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
            while (position_ < text_.size() &&
                   std::isspace(static_cast<unsigned char>(text_[position_]))) {
                ++position_;
            }
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
        while (position_ < text_.size() &&
               (text_[position_] == ' ' || text_[position_] == '\t' ||
                text_[position_] == '\r')) {
            ++position_;
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
    result<node::table*> parse_header(node::table& root) {
        const bool array_table = text_.substr(position_).starts_with("[[");
        const std::size_t content_start = position_ + (array_table ? 2 : 1);
        const std::size_t content_end = find_header_end(content_start, array_table);
        if (content_end == std::string_view::npos) {
            return std::unexpected(std::string("unterminated TOML table header"));
        }
        auto path = parse_key_path(text_.substr(content_start, content_end - content_start));
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
    result<void> parse_assignment(node::table& current_table) {
        const std::size_t equals = find_assignment();
        if (equals == std::string_view::npos) {
            return std::unexpected(std::string("expected '=' in TOML assignment"));
        }
        auto path = parse_key_path(text_.substr(position_, equals - position_));
        if (!path || path->empty()) {
            return std::unexpected(path ? std::string("empty TOML key")
                                         : path.error());
        }
        position_ = equals + 1;
        value_parser parser(text_, position_);
        auto value = parser.parse_one();
        if (!value) {
            return std::unexpected(value.error());
        }
        position_ = parser.position();
        auto finished = finish_line();
        if (!finished) {
            return std::unexpected(finished.error());
        }
        if (!insert_value_at(current_table, *path, std::move(*value))) {
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
    static result<node::table*> open_table(node::table& root,
                                            const std::vector<std::string>& path) {
        node::table* table = &root;
        for (const auto& part : path) {
            auto [iterator, inserted] = table->try_emplace(part, node{node::table{}});
            if (std::holds_alternative<node::table>(iterator->second.value)) {
                table = &std::get<node::table>(iterator->second.value);
                continue;
            }
            if (auto* array = std::get_if<node::array>(&iterator->second.value);
                array != nullptr && !array->empty() &&
                std::holds_alternative<node::table>(array->back().value)) {
                table = &std::get<node::table>(array->back().value);
                continue;
            }
            return std::unexpected(std::string("TOML table conflicts with a scalar value"));
        }
        return table;
    }

    /**
     * @brief 按路径追加一个数组表元素并返回其表指针。
     * @param root 文档根表。
     * @param path 数组表的键路径。
     * @return 新元素的表指针，或路径冲突错误。
     */
    static result<node::table*> open_array_table(
        node::table& root, const std::vector<std::string>& path) {
        if (path.empty()) {
            return std::unexpected(std::string("empty TOML array-table header"));
        }
        std::vector<std::string> parent_path(path.begin(), path.end() - 1);
        auto parent = open_table(root, parent_path);
        if (!parent) {
            return std::unexpected(parent.error());
        }
        auto [iterator, inserted] = (*parent)->try_emplace(path.back(), node{node::array{}});
        if (!std::holds_alternative<node::array>(iterator->second.value)) {
            return std::unexpected(std::string("TOML array-table conflicts with another value"));
        }
        auto& values = std::get<node::array>(iterator->second.value);
        values.emplace_back(node::table{});
        return &std::get<node::table>(values.back().value);
    }

    std::string_view text_;
    std::size_t position_ = 0;
};

/**
 * @brief 将 C++ 值递归编码为内部 TOML 节点。
 * @tparam T 输入值类型。
 * @param input 待编码的 C++ 值。
 * @return 内部节点，或类型不受支持/值超出范围的错误。
 */
template <class T>
result<node> encode_value(const T& input);

/**
 * @brief 将内部 TOML 节点递归解码为 C++ 值。
 * @tparam T 目标 C++ 类型。
 * @param input 待读取的内部节点。
 * @param path 当前值在文档中的路径，用于生成错误信息。
 * @return 解码后的 C++ 值，或类型/范围不匹配错误。
 */
template <class T>
result<T> decode_value(const node& input, std::string_view path);

/**
 * @brief 将 C++ 值递归编码为内部 TOML 节点。
 * @tparam T 输入值类型。
 * @param input 待编码的 C++ 值。
 * @return 内部节点，或类型不受支持/值超出范围的错误。
 */
template <class T>
result<node> encode_value(const T& input) {
    using U = bare_t<T>;

    if constexpr (optional_traits<U>::value) {
        if (!input) {
            return node{};
        }
        return encode_value(*input);
    } else if constexpr (inline_table_traits<U>::value) {
        auto encoded = encode_value(input.value);
        if (!encoded) {
            return std::unexpected(encoded.error());
        }
        if (!std::holds_alternative<node::table>(encoded->value)) {
            return std::unexpected(std::string("toml::inline_table requires a reflected struct"));
        }
        encoded->inline_table = true;
        return encoded;
    } else if constexpr (std::same_as<U, date>) {
        if (!valid_date_value(input)) {
            return std::unexpected(std::string("invalid toml::date value"));
        }
        return node{input};
    } else if constexpr (std::same_as<U, time>) {
        if (!valid_time_value(input)) {
            return std::unexpected(std::string("invalid toml::time value"));
        }
        return node{input};
    } else if constexpr (std::same_as<U, local_date_time>) {
        if (!valid_date_value(input.date_part) || !valid_time_value(input.time_part)) {
            return std::unexpected(std::string("invalid toml::local_date_time value"));
        }
        return node{input};
    } else if constexpr (std::same_as<U, offset_date_time>) {
        if (!valid_date_value(input.local.date_part) ||
            !valid_time_value(input.local.time_part) ||
            input.offset_minutes < -1439 || input.offset_minutes > 1439) {
            return std::unexpected(std::string("invalid toml::offset_date_time value"));
        }
        return node{input};
    } else if constexpr (std::same_as<U, std::string>) {
        return node{input};
    } else if constexpr (std::same_as<U, std::string_view>) {
        return node{std::string(input)};
    } else if constexpr (std::same_as<U, bool>) {
        return node{input};
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
        return node{static_cast<std::int64_t>(input)};
    } else if constexpr (std::is_floating_point_v<U>) {
        return node{static_cast<double>(input)};
    } else if constexpr (std::is_enum_v<U>) {
        using Underlying = std::underlying_type_t<U>;
        return encode_value(static_cast<Underlying>(input));
    } else if constexpr (vector_traits<U>::value) {
        node::array values;
        for (const auto& element : input) {
            using Element = typename vector_traits<U>::value_type;
            // `vector<bool>` 暴露的是代理引用；这里先物化声明的元素类型，
            // 同时保留普通 vector 对不可复制元素类型的支持。
            auto encoded = [&]() {
                if constexpr (std::constructible_from<Element, decltype(element)>) {
                    return encode_value(static_cast<Element>(element));
                } else {
                    return encode_value(element);
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
        return node{std::move(values)};
    } else if constexpr (array_traits<U>::value) {
        node::array values;
        for (const auto& element : input) {
            auto encoded = encode_value(element);
            if (!encoded) {
                return std::unexpected(encoded.error());
            }
            if (std::holds_alternative<std::monostate>(encoded->value)) {
                return std::unexpected(std::string("TOML arrays cannot contain null optional values"));
            }
            values.push_back(std::move(*encoded));
        }
        return node{std::move(values)};
    } else if constexpr (map_traits<U>::value) {
        node::table values;
        for (const auto& [key, element] : input) {
            auto encoded = encode_value(element);
            if (!encoded) {
                return std::unexpected(encoded.error());
            }
            if (std::holds_alternative<std::monostate>(encoded->value)) {
                continue;
            }
            values.emplace(key, std::move(*encoded));
        }
        return node{std::move(values)};
    } else if constexpr (reflectable<U>) {
        node::table values;
        std::string failure;
        bool failed = false;
        for_each_field(input, [&](const auto& descriptor, const auto& object) {
            if (failed) {
                return;
            }
            const auto& member = descriptor.get(object);
            using Member = bare_t<decltype(member)>;
            if constexpr (optional_traits<Member>::value) {
                if (!member) {
                    return;
                }
            }
            auto encoded = encode_value(member);
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
        return node{std::move(values)};
    } else {
        return std::unexpected(std::string("unsupported type in TOML serializer"));
    }
}

/**
 * @brief 判断键是否可以按 TOML 裸键形式输出。
 * @param key 待判断的键名。
 * @return 键只含字母、数字、下划线或连字符时返回 `true`。
 */
inline bool bare_key(std::string_view key) {
    if (key.empty()) {
        return false;
    }
    for (const char character : key) {
        if (!(std::isalnum(static_cast<unsigned char>(character)) ||
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
inline std::string format_key(std::string_view key) {
    if (bare_key(key)) {
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
            default: result.push_back(character); break;
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
inline void append_padded_decimal(std::string& output, unsigned value,
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
inline void append_date_text(const date& value, std::string& output) {
    append_padded_decimal(output, static_cast<unsigned>(value.year), 4);
    output.push_back('-');
    append_padded_decimal(output, value.month, 2);
    output.push_back('-');
    append_padded_decimal(output, value.day, 2);
}

/**
 * @brief 将本地时间按 TOML 标准格式追加到字符串。
 * @param value 待格式化的时间。
 * @param output 目标字符串。
 */
inline void append_time_text(const time& value, std::string& output) {
    append_padded_decimal(output, value.hour, 2);
    output.push_back(':');
    append_padded_decimal(output, value.minute, 2);
    output.push_back(':');
    append_padded_decimal(output, value.second, 2);
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
inline void append_local_date_time_text(const local_date_time& value,
                                        std::string& output) {
    append_date_text(value.date_part, output);
    output.push_back('T');
    append_time_text(value.time_part, output);
}

/**
 * @brief 将带偏移日期时间按 TOML 标准格式追加到字符串。
 * @param value 待格式化的带偏移日期时间。
 * @param output 目标字符串。
 */
inline void append_offset_date_time_text(const offset_date_time& value,
                                         std::string& output) {
    append_local_date_time_text(value.local, output);
    if (value.offset_minutes == 0) {
        output.push_back('Z');
        return;
    }
    const int magnitude = std::abs(value.offset_minutes);
    output.push_back(value.offset_minutes < 0 ? '-' : '+');
    append_padded_decimal(output, static_cast<unsigned>(magnitude / 60), 2);
    output.push_back(':');
    append_padded_decimal(output, static_cast<unsigned>(magnitude % 60), 2);
}

/**
 * @brief 将节点作为 TOML 内联值追加到输出。
 * @param value 待输出的节点。
 * @param output 目标字符串。
 * @param failure 失败时写入错误描述。
 * @return 输出成功时返回 `true`，否则返回 `false`。
 */
inline bool append_inline(const node& value, std::string& output,
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
                append_date_text(item, output);
                return true;
            } else if constexpr (std::same_as<Item, time>) {
                append_time_text(item, output);
                return true;
            } else if constexpr (std::same_as<Item, local_date_time>) {
                append_local_date_time_text(item, output);
                return true;
            } else if constexpr (std::same_as<Item, offset_date_time>) {
                append_offset_date_time_text(item, output);
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
                        default: output.push_back(character); break;
                    }
                }
                output.push_back('"');
                return true;
            } else if constexpr (std::same_as<Item, node::array>) {
                output.push_back('[');
                for (std::size_t index = 0; index < item.size(); ++index) {
                    if (index != 0) {
                        output += ", ";
                    }
                    if (!append_inline(item[index], output, failure)) {
                        return false;
                    }
                }
                output.push_back(']');
                return true;
            } else if constexpr (std::same_as<Item, node::table>) {
                output.push_back('{');
                std::size_t index = 0;
                for (const auto& [key, child] : item) {
                    if (index++ != 0) {
                        output += ", ";
                    }
                    output += format_key(key);
                    output += " = ";
                    if (!append_inline(child, output, failure)) {
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
inline void append_key_path(std::string_view prefix, std::string_view key,
                            std::string& output) {
    if (prefix.empty()) {
        output += format_key(key);
    } else {
        output += prefix;
        output.push_back('.');
        output += format_key(key);
    }
}

/**
 * @brief 判断节点是否为由普通表组成的数组表。
 * @param value 待检查的节点。
 * @return 数组表底层数组指针；不是数组表或为空时返回 `nullptr`。
 */
inline const node::array* table_array(const node& value) {
    const auto* values = std::get_if<node::array>(&value.value);
    if (values == nullptr || values->empty()) {
        return nullptr;
    }
    for (const auto& element : *values) {
        if (!std::holds_alternative<node::table>(element.value) || element.inline_table) {
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
inline bool append_table(const node::table& table, std::string_view prefix,
                         std::string& output, std::string& failure) {
    for (const auto& [key, value] : table) {
        if ((std::holds_alternative<node::table>(value.value) && !value.inline_table) ||
            table_array(value) != nullptr) {
            continue;
        }
        output += format_key(key);
        output += " = ";
        if (!append_inline(value, output, failure)) {
            return false;
        }
        output.push_back('\n');
    }

    for (const auto& [key, value] : table) {
        const auto* child = std::get_if<node::table>(&value.value);
        if (child == nullptr || value.inline_table) {
            continue;
        }
        if (!output.empty() && output.back() != '\n') {
            output.push_back('\n');
        }
        output.push_back('\n');
        std::string section;
        append_key_path(prefix, key, section);
        output += "[" + section + "]\n";
        if (!append_table(*child, section, output, failure)) {
            return false;
        }
    }

    for (const auto& [key, value] : table) {
        const auto* values = table_array(value);
        if (values == nullptr) {
            continue;
        }
        std::string section;
        append_key_path(prefix, key, section);
        for (const auto& element : *values) {
            const auto& child = std::get<node::table>(element.value);
            if (!output.empty() && output.back() != '\n') {
                output.push_back('\n');
            }
            output.push_back('\n');
            output += "[[" + section + "]]\n";
            if (!append_table(child, section, output, failure)) {
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
result<T> decode_value(const node& input, std::string_view path) {
    using U = bare_t<T>;
    const auto where = path.empty() ? std::string("value") : std::string(path);

    if constexpr (optional_traits<U>::value) {
        if (std::holds_alternative<std::monostate>(input.value)) {
            return U{};
        }
        auto decoded = decode_value<typename optional_traits<U>::value_type>(input, path);
        if (!decoded) {
            return std::unexpected(decoded.error());
        }
        return U{std::move(*decoded)};
    } else if constexpr (inline_table_traits<U>::value) {
        auto decoded = decode_value<typename inline_table_traits<U>::value_type>(input, path);
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
        auto decoded = decode_value<Underlying>(input, path);
        if (!decoded) {
            return std::unexpected(decoded.error());
        }
        return static_cast<U>(*decoded);
    } else if constexpr (vector_traits<U>::value) {
        const auto* values = std::get_if<node::array>(&input.value);
        if (values == nullptr) {
            return std::unexpected(where + " must be a TOML array");
        }
        U output;
        if constexpr (requires { output.reserve(values->size()); }) {
            output.reserve(values->size());
        }
        for (std::size_t index = 0; index < values->size(); ++index) {
            auto decoded = decode_value<typename vector_traits<U>::value_type>(
                (*values)[index], where + "[" + std::to_string(index) + "]");
            if (!decoded) {
                return std::unexpected(decoded.error());
            }
            output.push_back(std::move(*decoded));
        }
        return output;
    } else if constexpr (array_traits<U>::value) {
        const auto* values = std::get_if<node::array>(&input.value);
        if (values == nullptr || values->size() != array_traits<U>::size) {
            return std::unexpected(where + " has the wrong TOML array size");
        }
        U output{};
        for (std::size_t index = 0; index < values->size(); ++index) {
            auto decoded = decode_value<typename array_traits<U>::value_type>(
                (*values)[index], where + "[" + std::to_string(index) + "]");
            if (!decoded) {
                return std::unexpected(decoded.error());
            }
            output[index] = std::move(*decoded);
        }
        return output;
    } else if constexpr (map_traits<U>::value) {
        const auto* values = std::get_if<node::table>(&input.value);
        if (values == nullptr) {
            return std::unexpected(where + " must be a TOML table");
        }
        U output;
        for (const auto& [key, value] : *values) {
            auto decoded = decode_value<typename map_traits<U>::mapped_type>(
                value, where + "." + key);
            if (!decoded) {
                return std::unexpected(decoded.error());
            }
            output.emplace(key, std::move(*decoded));
        }
        return output;
    } else if constexpr (reflectable<U>) {
        const auto* values = std::get_if<node::table>(&input.value);
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
                        if constexpr (optional_traits<Member>::value) {
                            return;
                        } else {
                            failed = true;
                            failure = where + " is missing field '" +
                                      std::string(descriptor.name) + "'";
                        }
                        return;
                    }
                    auto decoded = decode_value<Member>(
                        iterator->second,
                        where + "." + std::string(descriptor.name));
                    if (!decoded) {
                        failed = true;
                        failure = decoded.error();
                        return;
                    }
                    descriptor.get(object) = std::move(*decoded);
                }
            });
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
result<std::string> serialize(const T& value) {
    auto encoded = detail::encode_value(value);
    if (!encoded) {
        return std::unexpected(encoded.error());
    }
    const auto* root = std::get_if<typename detail::node::table>(&encoded->value);
    if (root == nullptr) {
        return std::unexpected(std::string("TOML serialization requires a reflected struct/table at the root"));
    }
    std::string output;
    std::string failure;
    if (!detail::append_table(*root, {}, output, failure)) {
        return std::unexpected(std::move(failure));
    }
    return output;
}

/**
 * @brief `serialize` 的兼容别名。
 * @tparam T 待序列化的对象类型。
 * @param value 待序列化的对象。
 * @return 与 `serialize` 相同的 TOML 文本或错误结果。
 */
template <class T>
result<std::string> try_serialize(const T& value) {
    return serialize(value);
}

/**
 * @brief 将 TOML 文本解析并转换为指定的 C++ 类型。
 * @tparam T 目标 C++ 类型。
 * @param text 待解析的 TOML 文本。
 * @return 解码后的对象，或语法、类型及范围错误。
 */
template <class T>
result<T> deserialize(std::string_view text) {
    detail::document_parser parser(text);
    auto parsed = parser.parse();
    if (!parsed) {
        return std::unexpected(parsed.error());
    }
    detail::node root{std::move(*parsed)};
    return detail::decode_value<T>(root, {});
}

/**
 * @brief `deserialize` 的历史拼写兼容别名。
 * @tparam T 目标 C++ 类型。
 * @param text 待解析的 TOML 文本。
 * @return 与 `deserialize<T>` 相同的结果。
 */
template <class T>
result<T> deserializee(std::string_view text) {
    return deserialize<T>(text);
}

/**
 * @brief `deserialize` 的大小写兼容别名。
 * @tparam T 目标 C++ 类型。
 * @param text 待解析的 TOML 文本。
 * @return 与 `deserialize<T>` 相同的结果。
 */
template <class T>
result<T> deSerialize(std::string_view text) {
    return deserialize<T>(text);
}

}  // 命名空间 toml
