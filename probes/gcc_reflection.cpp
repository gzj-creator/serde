// This probe is compiled only by the GCC 16.1 C++26 job. It deliberately
// keeps std::meta confined to this translation unit; the public reflect module
// has no compiler-specific types in its API.
#include <meta>

struct native_settings {
    int port{};
};

consteval bool has_expected_member() {
    const auto members = std::meta::nonstatic_data_members_of(
        ^^native_settings, std::meta::access_context::unchecked());
    return !members.empty() &&
           std::meta::identifier_of(members.front()) == "port";
}

static_assert(has_expected_member());

int main() { return 0; }
