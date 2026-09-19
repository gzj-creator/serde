module;

// simdjson 宏必须先于 simdjson.h 展开；与 serde::serde_simdjson 目标
// 传递的定义等值，重复定义为合法 C。
#define SIMDJSON_EXCEPTIONS 0
#define nssv_CONFIG_NO_EXCEPTIONS 1
#include "../../third_party/simdjson/simdjson.h"

#include "../module_prelude.hpp"

export module json;

export import reflect;
export import serde_common;

#define SERDE_JSON_MODULE_MODE 1
export extern "C++" {
#include "json.hpp"
}