#include <serde/json/json.hpp>

extern "C" bool serde_shared_parse(const char* text) {
    const auto value = json::parse(text);
    return value && value->is_array() && value->size() == 3;
}
