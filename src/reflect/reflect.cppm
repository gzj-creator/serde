module;

#include "../module_prelude.hpp"
#include <third_party/boost_pfr/include/boost/pfr/core.hpp>
#include <third_party/boost_pfr/include/boost/pfr/core_name.hpp>

export module reflect;

export extern "C++" {
#include "reflect.hpp"
#include "detail/object_fields.hpp"
}
