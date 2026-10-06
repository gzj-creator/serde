# 字段契约和枚举编码

serde 的字段描述符既用于取得外部字段名和成员值，也可保存字段说明及实际执行的约束。字段列表登记一次，JSON、TOML 和流式 JSON 共用这些规则。反射层不包含 HTTP 参数来源、路由、状态码或认证信息。

## 字段选项

```cpp
#include <serde/json/json.hpp>
#include <serde/reflect/reflect_macros.hpp>

struct User {
    std::string name;
    int age{};
};

constexpr reflect::field_options<std::string> name_options{
    .description = "Display name", .min_length = 1, .max_length = 40};
constexpr reflect::field_options<int> age_options{.minimum = 18, .maximum = 150};

#define USER_FIELDS(X) \
    X(name, "display-name", (name_options)) \
    X(age, "age", (age_options))
REFLECT_FIELDS(User, USER_FIELDS)
#undef USER_FIELDS
```

三参数字段条目最后一项加括号，以保护含逗号的初始化表达式。`X(member)` 和 `X(member, "external-name")` 表示默认选项。也可直接使用 `reflect::make_field(name, pointer, options)`。

两参数 `make_field` 返回轻量的 `field<Owner, Member>`，实例只保存 name 和 pointer，options 是静态空值；三参数版本返回 `field<Owner, Member, true>`，保存本字段的选项。使用 auto 或泛型参数保留描述符类型；带约束字段不能隐式转换为默认字段，防止约束被切片丢失。两种类型均可查询 options，并由同一组校验和编解码接口消费。

既有仅提供 name/get 的自定义描述符仍可用于三个 codec。无 options 时 `validate_field` 只执行枚举合法集合检查，空 optional 也会检查其枚举元数据；要登记通用字段约束请使用带选项的 make_field。

| 选项 | 行为 |
|---|---|
| `description` | 公开的字段描述，不改变编码 |
| `minimum` / `maximum` | 闭区间边界，保存实际整数或浮点类型，不经 double 转换整数 |
| `min_length` / `max_length` | 字符串 Unicode 标量数量，不是 UTF-8 字节数量或用户感知字形数量 |
| `min_items` / `max_items` | 序列或 map 的元素数量 |

不适用的选项、反向范围、非有限边界、无效 UTF-8 或越界值返回 `std::expected` 错误，不会被忽略。JSON DOM 和 TOML 的编解码、JSON StreamWriter 的编码都会执行约束。流式写入失败时可能已产生部分输出，调用方必须检查并向上传播错误，同时丢弃该次已经写出的不完整消息。`StreamWriter` 保留首次失败原因，后续写入、`status()` 和 `finish()` 都返回该错误。

optional 的选项类型使用成员本身，例如 `field_options<std::optional<int>>`。有值时校验其内部值，空值不执行数值或长度限制；非法选项仍会报错。缺失 optional 字段保留对象的默认值，该默认值也必须通过校验。

JSON 空 optional 输出 `null`，显式 `null` 解码为空值；TOML 无 null 值，因此省略空 optional 字段。普通字段仍然必填。本轮不增加 required 开关或字段省略策略。

`reflect::static_fields<T>()` 可以读取 `descriptor.options`，不构造 T；`reflect::validate_field(descriptor, member)` 为外部绑定层提供同一份校验规则。字段说明及校验元数据的 string_view 必须指向使用期间有效的存储，常量元数据优先使用字符串字面量。

## 枚举编码

在枚举的关联命名空间提供 `reflect_enum`：

```cpp
enum class Mode { active, disabled };

constexpr auto reflect_enum(std::type_identity<Mode>) {
    return reflect::enum_descriptor<Mode, 2>{
        reflect::enum_encoding::string,
        {{{Mode::active, "active"}, {Mode::disabled, "disabled"}}}
    };
}
```

`EnumReflectable<Mode>` 判断是否登记；`enum_descriptor_for<Mode>()` 可取得 encoding 与 values。字符串编码实际输出登记名称，只接受这些名称；不会同时接受整数。改用 `enum_encoding::underlying` 则使用底层编码，但仍只接受登记值。底层为 bool 时编码为 boolean。

空 descriptor、空名称、重复枚举值、重复名称、非法 UTF-8 名称、无效 encoding 及未登记值都返回错误。未提供描述符的 enum 继续使用原有底层值编码，不自动获得合法值集合限制。TOML 整数仍须处于其有符号 64 位范围。数值边界只适用于数值编码的枚举，不适用于字符串编码或 bool 底层的枚举。

编解码和公共 helper 也接受非 constexpr 的 ADL 描述符。供后续文档生成复用时，建议使用稳定的 constexpr 描述符；`EnumReflectable` 判断登记接口的类型，不代表其名称、合法值或 encoding 已通过校验。映射中的 string_view 同样需要有效的存储生命周期。

公共 `validate_enum`、`validate_enum_descriptor<E>()`、`enum_to_string` 和 `enum_from_string<E>` 用于读取和检查相同枚举映射，避免外部参数绑定或文档再登记一份名称列表。

每个公共枚举 helper 只取得一次描述符，并在同一次快照上校验和查找。即使登记函数是 constexpr，也会检查当前快照，因为它可能借用可变名称或在常量求值时返回不同内容；不使用会隐藏元数据损坏的全局缓存。成功的字段校验和枚举映射不分配堆内存。

## 字段级和选择解码

`json::decode_field(descriptor, Json, options)` 解码一个字段并执行父字段选项。invalid Json 表示缺失：普通字段失败，optional 返回空值。该接口没有所属对象，因此不会读取结构体的默认成员值。

`json::decode_fields_into(body, output, selector, options)` 则向已有对象写入选中字段。缺失 optional 保留 output 原来的值，并对它进行校验；未选中的字段不改变。selector 接收描述符并返回 bool，应为无副作用谓词。

```cpp
User user{"Ada", 25};
auto parsed = json::parse(R"({"display-name":"Bob"})");
if (!parsed) return std::unexpected(parsed.error());
auto updated = json::decode_fields_into(
    *parsed, user, [](const auto& field) { return field.name == "display-name"; });
if (!updated) return std::unexpected(updated.error());
```

这里 user.age 保留为 25，不要求它在 body 中出现。`unknown_fields=reject` 只接受此次选择集合里的字段，未选择字段若出现在 JSON 中也会拒绝。解码失败时 output 可能已部分赋值，调用方应丢弃本次对象，接口不提供回滚。

现有 `json::decode<T>` 和 deserialize 继续解码完整对象，无需用户调用新接口。公开字段接口用于未来 HTTP path/query 与 JSON body 的组合绑定；不要求添加新的源码扫描或反射依赖。
