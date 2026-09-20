extern "C" bool serde_shared_parse(const char* text);

int main() {
    if (!serde_shared_parse("[1,2,3]")) return 1;
    if (serde_shared_parse("[1,2,")) return 2;
    if (serde_shared_parse("{}")) return 3;
    return 0;
}
