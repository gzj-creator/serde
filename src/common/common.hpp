#ifndef SERDE_COMMON_HPP
#define SERDE_COMMON_HPP
// serde 公共词汇实现头。TOML 与 JSON 共享的日期时间值类型与内联表包装，
// 二者经别名（toml::date、json::date 等）对外暴露，避免跨模块依赖。
// 经典 TU 直接 include；模块消费者经 src/common/common.cppm 门面 import。

#include <string>

/**
 * @brief 表示 ISO 8601 的本地日期值。
 *
 * 年、月、日字段分别对应 ISO 8601 日期的三个组成部分。该类型只保存
 * 结构化值，合法性会在解析或序列化时校验。
 */
namespace common {

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
 * @brief 表示 ISO 8601 的本地时间值。
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
 * @brief 强制将一个反射结构序列化为内联表（TOML 内联表 / JSON 对象成员）。
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

}  // namespace common

#endif  // SERDE_COMMON_HPP