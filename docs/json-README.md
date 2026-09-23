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

`json::Json` 持有一份 simdjson DOM 文档，并提供类型查询、`at`/`operator[]`、标量取值和数组/对象遍历。`at` / `for_each_*` 返回的子节点是视图：在父 `Json` 或产生它的 `Parser` 仍然指向同一代文档时有效。`json::Parser` 只移不拷，复用同一份 simdjson parser 容量；下一次 `parse`（成功或失败）会使此前返回的 `Json` 的 `valid()` 变为 false。`Parser::reset` 释放该 parser 占用的容量并同样使旧文档失效。`deserialize` 使用线程局部 `Parser`，容量会涨到该线程见过的最大文档；长寿命线程可调用 `reset_thread_parser()` 回收。独立的 `json::parse` / `Json::parse` 每次分配自己的文档，互不影响。内部实现放在未导出的 `json::detail` 中。

语法、类型、范围和配置的资源限制错误返回 `std::expected`，不抛异常。本库以 `-fno-exceptions` 构建，内存耗尽会终止进程，而不是变成 `expected`。

解析器接受 RFC 8259 JSON：空白、对象、数组、字符串、true、false、null 和有限十进制数。拒绝注释、尾随逗号、前导零、无效转义、不配对的 UTF-16 代理对、无效 UTF-8 和非有限数值。默认拒绝重复对象键；`DuplicateKeyPolicy::first_wins` 按首次出现取值，此时 `size()` / `for_each_member` 仍会看到重复项，`at` / `contains` 只看到第一项。适合 int64_t 的数字保留为有符号整数；更大的非负整数（最大 uint64_t）保留为无符号整数。小数和指数解析为有限 double 值。

默认 `enforce_document_limits = true`，在解析后校验字节、深度、节点、字符串、键、数组和对象成员上限。信任的输入可设为 false，以跳过整树校验、贴近 simdjson 的 parse 成本。重复键策略与资源限制相互独立：只改 `max_depth` 不会改成 first-wins。

递归支持反射结构体、字符串键映射、向量、固定数组、optional、枚举、布尔值、字符串、有符号/无符号整数、浮点值和 JSON null。空的 std::optional 输出为 null；JSON null 解码为空 optional。缺少可选对象成员被接受，缺少必需反射成员则为错误。未知成员默认忽略，可通过 `ParseOptions::unknown_fields = json::UnknownFieldPolicy::reject` 拒绝。

现有的 TOML 日期/时间类型通过 `json::date`、`json::time`、`json::local_date_time` 和 `json::offset_date_time` 别名可用。由于 JSON 没有时间原语，它们以经过验证的 ISO/RFC-3339 类 JSON 字符串表示。

序列化是确定性的：对象键按字典序排列。默认输出为紧凑格式；设置 `SerializeOptions::pretty` 可获得缩进输出。非有限 C++ 浮点值默认被拒绝。显式的 `NonFinitePolicy::null_value` 选项可在应用层策略中将其映射为 JSON null。

## 流式输出：`json::stream::StreamWriter`

`json::stream` 提供独立的流式行为。头文件消费者可包含
`<serde/json/stream.hpp>` 或 `<serde/json/json.hpp>`；模块消费者使用 `import json`。

`StreamWriter` 接受一个同步 sink：`json::result<void>(std::string_view)`。
每次写入会立即把 JSON 片段交给 sink，不累计完整输出字符串。sink 成功返回时
必须已消费整个片段；`string_view` 仅在回调期间有效，需要异步发送时应先复制到
应用自己的发送队列。回调不得重入同一个 writer，也不能修改当前正在序列化的值。

下面的 MCP JSON-RPC 响应示例展示手动写入、typed 值和原始 JSON 的组合：

```cpp
#include <serde/json/stream.hpp>

json::result<void> write_response(json::stream::StreamWriter::Sink sink,
                                  std::int64_t id,
                                  std::string_view text,
                                  std::string_view structured_json) {
    json::stream::StreamWriter writer(std::move(sink));
    if (auto r = writer.start_object(); !r) return r;
    if (auto r = writer.key("jsonrpc"); !r) return r;
    if (auto r = writer.string("2.0"); !r) return r;
    if (auto r = writer.key("id"); !r) return r;
    if (auto r = writer.number(id); !r) return r;
    if (auto r = writer.key("result"); !r) return r;
    if (auto r = writer.start_object(); !r) return r;
    if (auto r = writer.key("content"); !r) return r;
    const std::vector<std::map<std::string, std::string>> content{
        {{"type", "text"}, {"text", std::string(text)}}};
    if (auto r = writer.value(content); !r) return r;
    if (auto r = writer.key("structuredContent"); !r) return r;
    if (auto r = writer.raw(structured_json); !r) return r;
    if (auto r = writer.end_object(); !r) return r;
    if (auto r = writer.end_object(); !r) return r;
    return writer.finish();
}
```

| 方法 | 行为 |
|------|------|
| `start_object()` / `end_object()` | 开始 / 结束对象 |
| `start_array()` / `end_array()` | 开始 / 结束数组 |
| `key(std::string_view)` | 写入对象键，之后必须有一个值 |
| `string(std::string_view)` | 验证 UTF-8 并转义，支持含 NUL 的显式视图 |
| `number(T)` | 写入整数或浮点数，遵循非有限数策略 |
| `boolean(bool)` / `null_value()` | 写入布尔值 / null |
| `value(const T&)` | 直接遍历反射结构体、map、vector、array、optional、枚举及日期时间等类型 |
| `value(const json::Json&)` | 直接遍历有效的 DOM 或子节点视图 |
| `raw(std::string_view)` | 验证一个完整 JSON 值后原样写入，保留片段内空白和键顺序 |
| `finish()` | 检查存在唯一根值且所有容器均已闭合，封存当前文档 |
| `status()` | 获取当前错误状态；不检查文档是否完整 |
| `bytes_written()` | sink 已成功接受的字节数 |
| `reset()` | 保留 sink 和配置，清除状态，允许写入下一篇文档 |

所有写入方法和 `finish()` 都返回 `json::result<void>`。首次发生错误后，后续
写入不再调用 sink，并返回同一个错误，包括 sink 返回的原始错误消息。
sink 若部分写入后失败，writer 无法计算失败片段中实际送出的字节，也无法回滚。
调用方应丢弃未完成消息或关闭对应传输；`reset()` 不会清除 sink 已收到的内容。
`finish()` 可重复调用，但之后继续写入会报错；它不会 flush 或关闭外部 sink。

`SerializeOptions` 在构造 writer 时指定，支持紧凑 / pretty 输出、缩进、非有限数
策略，以及输出字节、节点、嵌套深度、字符串、键和容器成员上限。上限按整篇
文档累计，`raw()` 中的节点和深度也计入当前文档。根值深度为 0；optional、
枚举等 C++ 包装类型不增加 JSON 节点或深度。`raw()` 为验证片段临时构建该片段
的 DOM；已有 DOM 时可用 `value()` 避免重新解析。普通 typed 输出直接遍历输入，
不构造中间 JSON 树；writer 只维护容器上下文栈。

对象按输入顺序输出：手动写入按调用顺序，反射结构体按字段声明顺序，map 按
自身迭代顺序。因此 unordered_map 的输出顺序不保证稳定。stream writer 不排序
或检查重复键；包括 `raw()` 在内，键的唯一性由调用方保证。需要键排序和现有
行为时使用 `json::serialize`。

可直接把 C++ 值写到 sink，也可以在手动构建的对象 / 数组中嵌入 typed 值：

```cpp
std::string output;
auto sink = [&](std::string_view part) -> json::result<void> {
    output.append(part);  // 也可以同步写入文件或应用缓冲区
    return {};
};

// 自动写入并 finish 一篇文档。
auto result = json::stream::serialize(std::vector<int>{1, 2, 3}, sink);
// result 为成功时，output 为 [1,2,3]。

// 将 typed 值写入已有 writer 的当前位置，不自动结束外层文档。
// json::stream::serialize(writer, value) 等价于 writer.value(value)。
```

JSON writer 不添加 NDJSON 换行、SSE 的 `data:` 前缀或 HTTP 分块边界；MCP 传输层
在 `finish()` 成功后完成自身消息封装。sink 的片段边界不保证对应完整 JSON token
或传输帧。
