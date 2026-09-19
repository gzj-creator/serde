module;

#include "../module_prelude.hpp"

export module toml;

export import reflect;
export import serde_common;

#define SERDE_TOML_MODULE_MODE 1
export extern "C++" {
#include "toml.hpp"
}