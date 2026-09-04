export module demo.common:classA;

import std;
import toml;

#include "../toml/toml_reflect.hpp"

export namespace common {

struct A {
    std::string name;
    int age{};
};

TOML_REFLECT_MEMBERS(A, name, age)

}  // 命名空间 common
