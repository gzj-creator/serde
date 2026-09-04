import std;
import demo.common;
import toml;

/**
 * @brief 运行示例程序，演示对象的 TOML 往返转换。
 * @param argc 命令行参数数量（示例程序未使用）。
 * @param argv 命令行参数数组（示例程序未使用）。
 * @return 序列化或反序列化失败时返回 1，成功时返回 0。
 */
int main(int argc, const char* argv[])
{
    common::A a{"Ada", 37};
    const auto text = toml::serialize(a);
    if (!text) {
        std::println("serialize failed: {}", text.error());
        return 1;
    }
    std::print("{}", *text);

    const auto decoded = toml::deserializee<common::A>(*text);
    if (!decoded) {
        std::println("deserialize failed: {}", decoded.error());
        return 1;
    }
    std::println("round-trip: {} ({})", decoded->name, decoded->age);
    return 0;
}
