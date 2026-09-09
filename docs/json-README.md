# json 模块

本目录包含无依赖的 JSON 编码器和解码器。使用项目内与编译器无关的 reflect 模块，无需 simdjson、nlohmann/json 或任何第三方包。

标准 API：

    json::result<std::string> json::serialize(const T&, const json::SerializeOptions& = {});
    json::result<T> json::deserialize<T>(std::string_view, const json::ParseOptions& = {});

两个操作均返回 std::expected，对于格式错误的输入、不支持的值、分配失败或配置的资源限制违规均不抛出异常。`try_serialize`、`deserializee` 和 `deSerialize` 是与 TOML 模块匹配的兼容别名。

解析器仅接受 RFC 8259 JSON：空白、对象、数组、字符串、true、false、null 和有限十进制数。拒绝注释、尾随逗号、重复对象名、前导零、无效转义、不配对的 UTF-16 代理对、无效 UTF-8 和非有限数值。适合 int64_t 的数字保留为有符号整数；更大的非负整数（最大 uint64_t）保留为无符号整数。小数和指数解析为有限 double 值。

递归支持反射结构体、字符串键映射、向量、固定数组、optional、枚举、布尔值、字符串、有符号/无符号整数、浮点值和 JSON null。空的 std::optional 输出为 null；JSON null 解码为空 optional。缺少可选对象成员被接受，缺少必需反射成员则为错误。未知成员默认忽略，可通过 `ParseOptions::unknown_fields = json::UnknownFieldPolicy::reject` 拒绝。

现有的 TOML 日期/时间类型通过 `json::date`、`json::time`、`json::local_date_time` 和 `json::offset_date_time` 别名可用。由于 JSON 没有时间原语，它们以经过验证的 ISO/RFC-3339 类 JSON 字符串表示。

序列化是确定性的：对象键按字典序排列。默认输出为紧凑格式；设置 `SerializeOptions::pretty` 可获得缩进输出。非有限 C++ 浮点值默认被拒绝。显式的 `NonFinitePolicy::null_value` 选项可在应用层策略中将其映射为 JSON null。

两个方向均暴露字节、深度、节点、字符串、键、数组和对象成员限制。在信任边界处保持默认值，在资源预算更紧的协议端点处降低它们。

simdjson 被有意不包含。未来的可选高吞吐量后端可能在相同转换策略后使用它，但可移植模块及其测试保持自包含和基于标准。
