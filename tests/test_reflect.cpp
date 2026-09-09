import std;
import reflect;

#include "../src/reflect/reflect_macros.hpp"

namespace sample {

struct settings {
    std::string host;
    int port{};
};

#define SERDE_FIELDS_12(X) \
    X(host) \
    X(port)
REFLECT_FIELDS(settings, SERDE_FIELDS_12)
#undef SERDE_FIELDS_12

struct renamed {
    int retry_count{};
};

#define RENAMED_FIELDS(X) X(retry_count, "retry-count")
REFLECT_FIELDS(renamed, RENAMED_FIELDS)
#undef RENAMED_FIELDS

}  // namespace sample

int main() {
    static_assert(reflect::Reflectable<sample::settings>);
    sample::settings value{"localhost", 8080};
    std::vector<std::string> names;
    reflect::for_each_field(value, [&](const auto& descriptor, auto&) {
        names.emplace_back(descriptor.name);
    });
    if (names != std::vector<std::string>{"host", "port"}) {
        std::println("test_reflect: field names are not stable");
        return 1;
    }

    reflect::for_each_field(value, [&](const auto& descriptor, auto& object) {
        if (descriptor.name == "port") {
            descriptor.get(object) = 9090;
        }
    });
    if (value.port != 9090) {
        std::println("test_reflect: member pointer access failed");
        return 1;
    }

    sample::renamed renamed{3};
    const auto fields = reflect::fields(renamed);
    if (std::get<0>(fields).name != "retry-count") {
        std::println("test_reflect: named field mapping failed");
        return 1;
    }
    return 0;
}
