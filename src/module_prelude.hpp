// serde 模块接口共享的全局模块片段预置头。
//
// 不变量：一切非本模块头（标准库/系统/第三方）必须在全局模块片段内展开；
// purview 只允许本模块头（reflect.hpp / toml.hpp / json.hpp 及它们之间
// 的相对 include）。本文件由各 .cppm 门面在 `module;` 后先于一切其它
// include 引入，条目为安全超集（__has_include 守卫，平台缺失自动跳过），
// 因此超集是安全的 no-op —— 宁多勿漏。

#pragma once

#if __has_include(<algorithm>)
#include <algorithm>
#endif
#if __has_include(<array>)
#include <array>
#endif
#if __has_include(<bit>)
#include <bit>
#endif
#if __has_include(<charconv>)
#include <charconv>
#endif
#if __has_include(<cmath>)
#include <cmath>
#endif
#if __has_include(<concepts>)
#include <concepts>
#endif
#if __has_include(<cstddef>)
#include <cstddef>
#endif
#if __has_include(<cstdint>)
#include <cstdint>
#endif
#if __has_include(<cstring>)
#include <cstring>
#endif
// GCC 的 x86 intrinsic 头从 <emmintrin.h> 引入 <mm_malloc.h>，其 TU-local
// 辅助函数一旦在 purview 内展开，GCC 报 "exporting declaration ... with
// internal linkage"。
#if (defined(__x86_64__) || defined(__i386__)) && __has_include(<emmintrin.h>)
#include <emmintrin.h>
#endif
#if __has_include(<expected>)
#include <expected>
#endif
#if __has_include(<functional>)
#include <functional>
#endif
#if __has_include(<limits>)
#include <limits>
#endif
#if __has_include(<map>)
#include <map>
#endif
#if __has_include(<memory>)
#include <memory>
#endif
#if __has_include(<optional>)
#include <optional>
#endif
#if __has_include(<string>)
#include <string>
#endif
#if __has_include(<string_view>)
#include <string_view>
#endif
#if __has_include(<tuple>)
#include <tuple>
#endif
#if __has_include(<type_traits>)
#include <type_traits>
#endif
#if __has_include(<unordered_map>)
#include <unordered_map>
#endif
#if __has_include(<utility>)
#include <utility>
#endif
#if __has_include(<variant>)
#include <variant>
#endif
#if __has_include(<vector>)
#include <vector>
#endif