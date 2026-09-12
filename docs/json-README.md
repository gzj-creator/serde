# json 模块

本目录提供 JSON 编码器、解码器和只读 DOM。解析后端是项目内置的 simdjson 4.6.9；对外只暴露 `json::Json` 包装。

标准 API：

    json::result<json::Json> json::parse(std::string_view, const json::ParseOptions& = {});
    json::result<json::Json> json::Json::parse(std::string_view, const json::ParseOptions& = {});
    json::result<json::Json> json::Parser::parse(std::string_view, const json::ParseOptions& = {});
    void json::Parser::reset();
    void json::reset_thread_parser();
    json::result<T> json::decode<T>(const json::Json&, const json::ParseOptions& = {});
    json::result<std::string> json::serialize(const T&, const json::SerializeOptions& = {});
    json::result<T> json::deserialize<T>(std::string_view, const json::ParseOptions& = {});

`try_serialize`、`deserializee` 和 `deSerialize` 是与 TOML 模块匹配的兼容别名。

`json::Json` 持有一份 simdjson DOM 文档，并提供类型查询、`at`/`operator[]`、标量取值和数组/对象遍历。`at` / `for_each_*` 返回的子节点是视图：在父 `Json` 或产生它的 `Parser` 仍然指向同一代文档时有效。`json::Parser` 只移不拷，复用同一份 simdjson parser 容量；下一次 `parse`（成功或失败）会使此前返回的 `Json` 的 `valid()` 变为 false。`Parser::reset` 释放该 parser 占用的容量并同样使旧文档失效。`deserialize` 使用线程局部 `Parser`，容量会涨到该线程见过的最大文档；长寿命线程可调用 `reset_thread_parser()` 回收。独立的 `json::parse` / `Json::parse` 每次分配自己的文档，互不影响。内部实现放在未导出的 `json::detail` 中。

语法、类型、范围和配置的资源限制错误返回 `std::expected`，不抛异常。本库以 `-fno-exceptions` 构建，内存耗尽会终止进程，而不是变成 `expected`。

解析器接受 RFC 8259 JSON：空白、对象、数组、字符串、true、false、null 和有限十进制数。拒绝注释、尾随逗号、前导零、无效转义、不配对的 UTF-16 代理对、无效 UTF-8 和非有限数值。默认拒绝重复对象键；`DuplicateKeyPolicy::first_wins` 按首次出现取值，此时 `size()` / `for_each_member` 仍会看到重复项，`at` / `contains` 只看到第一项。适合 int64_t 的数字保留为有符号整数；更大的非负整数（最大 uint64_t）保留为无符号整数。小数和指数解析为有限 double 值。

默认 `enforce_document_limits = true`，在解析后校验字节、深度、节点、字符串、键、数组和对象成员上限。信任的输入可设为 false，以跳过整树校验、贴近 simdjson 的 parse 成本。重复键策略与资源限制相互独立：只改 `max_depth` 不会改成 first-wins。

递归支持反射结构体、字符串键映射、向量、固定数组、optional、枚举、布尔值、字符串、有符号/无符号整数、浮点值和 JSON null。空的 std::optional 输出为 null；JSON null 解码为空 optional。缺少可选对象成员被接受，缺少必需反射成员则为错误。未知成员默认忽略，可通过 `ParseOptions::unknown_fields = json::UnknownFieldPolicy::reject` 拒绝。

现有的 TOML 日期/时间类型通过 `json::date`、`json::time`、`json::local_date_time` 和 `json::offset_date_time` 别名可用。由于 JSON 没有时间原语，它们以经过验证的 ISO/RFC-3339 类 JSON 字符串表示。

序列化是确定性的：对象键按字典序排列。默认输出为紧凑格式；设置 `SerializeOptions::pretty` 可获得缩进输出。非有限 C++ 浮点值默认被拒绝。显式的 `NonFinitePolicy::null_value` 选项可在应用层策略中将其映射为 JSON null。
