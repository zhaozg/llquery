# llquery API 文档

本文档详细描述了 llquery 库的所有公开 API。

## 目录

- [数据结构](#数据结构)
- [枚举类型](#枚举类型)
- [初始化与清理](#初始化与清理)
- [解析函数](#解析函数)
- [查询函数](#查询函数)
- [操作函数](#操作函数)
- [实用工具](#实用工具)
- [回调函数类型](#回调函数类型)
- [错误处理](#错误处理)

---

## 数据结构

### `struct llquery_kv`

表示单个键值对。

```c
struct llquery_kv {
    const char *key;         // 键的起始指针
    size_t key_len;          // 键的长度
    const char *value;       // 值的起始指针
    size_t value_len;        // 值的长度
    bool is_encoded;         // 是否包含URL编码字符
};
```

**字段说明:**
- `key`: 指向键字符串的指针，可能指向原始字符串或解码缓冲区
- `key_len`: 键的字节长度（不包括终止符）
- `value`: 指向值字符串的指针
- `value_len`: 值的字节长度（不包括终止符）
- `is_encoded`: 标识此键值对是否经过解码处理

### `struct llquery`

主解析器结构体。

```c
struct llquery {
    uint32_t field_set;               // 位掩码，表示设置了哪些字段
    uint16_t kv_count;                // 键值对数量
    uint16_t max_kv_count;            // 最大支持的键值对数量
    uint16_t flags;                   // 解析时使用的选项标志
    char *decode_buffer;              // 解码缓冲区指针（如果需要）
    size_t decode_buffer_size;        // 解码缓冲区大小
    struct llquery_kv *kv_pairs;      // 键值对数组指针
    void *_reserved;                  // 保留字段，供内部使用
};
```

**使用注意:**
- 不要直接修改此结构体的字段
- 使用 API 函数进行所有操作
- `_reserved` 字段为内部使用，用户代码不应访问

---

## 枚举类型

### `enum llquery_option_flags`

解析器配置选项。

```c
enum llquery_option_flags {
    LQF_NONE             = 0,      // 默认选项
    LQF_AUTO_DECODE      = 1 << 0, // 自动URL解码（处理%XX和+）
    LQF_MERGE_DUPLICATES = 1 << 1, // 合并重复键为数组
    LQF_KEEP_EMPTY       = 1 << 2, // 保留空键值对
    LQF_STRICT           = 1 << 3, // 严格模式，遇到错误时返回错误
    LQF_SORT_KEYS        = 1 << 4, // 按键名排序结果
    LQF_LOWERCASE_KEYS   = 1 << 5, // 键名转换为小写
    LQF_TRIM_VALUES      = 1 << 6, // 去除值的前后空白字符
    LQF_DEFAULT          = LQF_AUTO_DECODE // 默认配置
};
```

**选项说明:**
- `LQF_AUTO_DECODE`: 自动解码 `%XX` 和 `+` 字符
- `LQF_KEEP_EMPTY`: 默认情况下会忽略空值，设置此标志保留它们
- `LQF_STRICT`: 在遇到格式错误时立即返回错误而不是尽力解析
- `LQF_LOWERCASE_KEYS`: 自动将所有键转换为小写，便于不区分大小写的查询
- `LQF_TRIM_VALUES`: 自动去除值两端的空白字符

**组合使用:**
```c
uint16_t flags = LQF_AUTO_DECODE | LQF_SORT_KEYS | LQF_LOWERCASE_KEYS;
```

### `enum llquery_error`

错误代码。

```c
enum llquery_error {
    LQE_OK = 0,                       // 成功
    LQE_NULL_INPUT,                   // 输入字符串为空指针
    LQE_EMPTY_STRING,                 // 输入字符串为空
    LQE_INVALID_HEX,                  // 无效的十六进制编码
    LQE_BUFFER_TOO_SMALL,             // 缓冲区太小
    LQE_MEMORY_ERROR,                 // 内存分配错误
    LQE_TOO_MANY_PAIRS,               // 键值对数量超过限制
    LQE_INVALID_FORMAT,               // 格式无效
    LQE_INTERNAL_ERROR                // 内部错误
};
```

---

## 初始化与清理

### `llquery_init()`

初始化查询解析器结构体。

```c
enum llquery_error llquery_init(struct llquery *q,
                                uint16_t max_pairs,
                                uint16_t flags);
```

**参数:**
- `q`: 指向 `llquery` 结构体的指针
- `max_pairs`: 最大键值对数量，0 表示使用默认值 (128)
- `flags`: 解析选项标志（参见 `enum llquery_option_flags`）

**返回值:**
- `LQE_OK`: 成功
- `LQE_NULL_INPUT`: `q` 为 NULL
- `LQE_MEMORY_ERROR`: 内存分配失败

**示例:**
```c
struct llquery query;
enum llquery_error err = llquery_init(&query, 0, LQF_DEFAULT);
if (err != LQE_OK) {
    fprintf(stderr, "Init failed: %s\n", llquery_strerror(err));
    return 1;
}
```

**线程安全性:** 线程安全（每个 `llquery` 实例独立）

### `llquery_init_ex()`

使用自定义内存分配器初始化查询解析器。

```c
enum llquery_error llquery_init_ex(struct llquery *q,
                                   uint16_t max_pairs,
                                   uint16_t flags,
                                   llquery_alloc_fn alloc_fn,
                                   llquery_free_fn free_fn,
                                   void *alloc_data);
```

**参数:**
- `q`: 指向 `llquery` 结构体的指针
- `max_pairs`: 最大键值对数量
- `flags`: 解析选项标志
- `alloc_fn`: 内存分配函数
- `free_fn`: 内存释放函数
- `alloc_data`: 传递给分配函数的用户数据

**返回值:** 同 `llquery_init()`

**示例:**
```c
void* my_alloc(size_t size, void *user_data) {
    return custom_malloc(size);
}

void my_free(void *ptr, void *user_data) {
    custom_free(ptr);
}

struct llquery query;
llquery_init_ex(&query, 128, LQF_DEFAULT, my_alloc, my_free, NULL);
```

**适用场景:** 嵌入式系统、内存池管理、调试追踪

### `llquery_free()`

释放查询解析器占用的资源。

```c
void llquery_free(struct llquery *q);
```

**参数:**
- `q`: 指向 `llquery` 结构体的指针

**注意:**
- 必须在解析完成后调用，否则会导致内存泄漏
- 调用后，`q` 指向的结构体被清零
- 可以安全地对已释放的结构体多次调用（会被忽略）

**示例:**
```c
llquery_free(&query);
```

---

## 解析函数

### `llquery_parse()`

解析查询字符串。

```c
enum llquery_error llquery_parse(const char *query,
                                 size_t query_len,
                                 struct llquery *q);
```

**参数:**
- `query`: 要解析的查询字符串（可包含前导 `?`）
- `query_len`: 查询字符串长度，0 表示自动计算（使用 `strlen`）
- `q`: 已初始化的 `llquery` 结构体指针

**返回值:**
- `LQE_OK`: 成功
- `LQE_NULL_INPUT`: `query` 或 `q` 为 NULL
- `LQE_EMPTY_STRING`: 查询字符串为空
- `LQE_TOO_MANY_PAIRS`: 键值对数量超过限制（仅在设置 `LQF_STRICT` 时）
- `LQE_MEMORY_ERROR`: 内存分配失败

**性能特点:**
- 零拷贝：尽可能直接引用原始字符串
- 单次遍历：同时完成解析和解码
- 内存高效：仅在需要时分配解码缓冲区

**示例:**
```c
const char *query_str = "name=John&age=30";
enum llquery_error err = llquery_parse(query_str, 0, &query);
if (err != LQE_OK) {
    fprintf(stderr, "Parse failed: %s\n", llquery_strerror(err));
}
```

**注意:**
- 可以重复调用解析不同的字符串（自动重置）
- 前导 `?` 会被自动跳过
- 原始字符串在解析期间必须保持有效（如果未启用解码）

### `llquery_parse_ex()`

解析查询字符串的扩展版本。

```c
enum llquery_error llquery_parse_ex(const char *query,
                                    size_t query_len,
                                    struct llquery *q,
                                    char *decode_buf,
                                    size_t decode_buf_size);
```

**参数:**
- `query`: 要解析的查询字符串
- `query_len`: 查询字符串长度
- `q`: 已初始化的 `llquery` 结构体指针
- `decode_buf`: 外部提供的解码缓冲区（可为 NULL）
- `decode_buf_size`: 解码缓冲区大小

**返回值:** 同 `llquery_parse()`

**使用场景:**
- 避免动态内存分配（使用栈缓冲区）
- 控制解码缓冲区的位置和大小

**示例:**
```c
char decode_buffer[1024];
llquery_parse_ex(query_str, 0, &query, decode_buffer, sizeof(decode_buffer));
```

### `llquery_parse_fast()`

快速解析查询字符串（简化接口）。

```c
uint16_t llquery_parse_fast(const char *query,
                            size_t query_len,
                            struct llquery_kv *kv_pairs,
                            uint16_t max_pairs,
                            uint16_t flags);
```

**参数:**
- `query`: 查询字符串
- `query_len`: 查询字符串长度
- `kv_pairs`: 输出键值对数组（由调用者分配）
- `max_pairs`: 最大键值对数量
- `flags`: 解析选项

**返回值:** 实际解析的键值对数量

**特点:**
- 栈分配，无动态内存分配
- 零拷贝设计，指针直接指向原始字符串
- 高性能，适用于性能关键场景
- **重要限制**：查询字符串长度不能超过 2048 字节（启用 `LQF_AUTO_DECODE` 时）

**适用场景:**
- ✅ **高频调用场景**：需要频繁解析短查询字符串
- ✅ **嵌入式系统**：内存受限，避免动态分配
- ✅ **性能敏感路径**：如热点函数、实时处理
- ✅ **简单查询**：键值对数量已知且较少（<50）
- ✅ **临时解析**：不需要长期保存结果

**不适用场景:**
- ❌ **长查询字符串**：超过 2048 字节且需要解码
- ❌ **复杂处理**：需要排序、过滤、克隆等操作
- ❌ **持久化需求**：结果需要长期保存或跨函数传递
- ❌ **大量键值对**：超过栈数组大小
- ❌ **需要内存管理**：需要自定义内存分配器

**使用注意事项:**
1. **生命周期**：返回的键值对指针指向原始查询字符串，必须确保原始字符串在使用期间有效
2. **栈空间**：注意栈数组大小，避免栈溢出（建议 max_pairs < 100）
3. **解码限制**：如果查询字符串超过 2048 字节且启用解码，函数将返回 0
4. **零拷贝权衡**：高性能但结果与原始字符串耦合

**性能对比:**
- `llquery_parse_fast()`: ~41.5M ops/sec
- `llquery_parse()`: ~3.95M ops/sec（简单查询）
- 性能提升：~10倍（对于小规模查询）

**示例:**
```c
// 适合的使用场景：HTTP 请求处理
void handle_request(const char *query_string) {
    struct llquery_kv pairs[20];  // 栈上分配
    uint16_t count = llquery_parse_fast(query_string, 0, pairs, 20, LQF_AUTO_DECODE);
    
    // 在当前函数内使用结果
    for (uint16_t i = 0; i < count; i++) {
        process_param(pairs[i].key, pairs[i].key_len,
                     pairs[i].value, pairs[i].value_len);
    }
    // 函数返回后，pairs 自动释放
}

// 不适合的使用场景：需要持久化
struct llquery_kv *parse_and_store(const char *query) {
    struct llquery_kv pairs[10];
    llquery_parse_fast(query, 0, pairs, 10, LQF_NONE);
    return pairs;  // ❌ 错误！返回栈上地址
}

// 正确的持久化方式：使用 llquery_parse()
struct llquery *parse_and_store_correct(const char *query) {
    struct llquery *q = malloc(sizeof(struct llquery));
    llquery_init(q, 0, LQF_DEFAULT);
    llquery_parse(query, 0, q);
    return q;  // ✅ 正确
}
```

---

## 查询函数

### `llquery_count()`

获取键值对数量。

```c
uint16_t llquery_count(const struct llquery *q);
```

**参数:**
- `q`: 指向 `llquery` 结构体的指针

**返回值:** 键值对数量，如果 `q` 为 NULL 则返回 0

**示例:**
```c
printf("Found %u parameters\n", llquery_count(&query));
```

### `llquery_get_kv()`

根据索引获取键值对。

```c
const struct llquery_kv *llquery_get_kv(const struct llquery *q,
                                        uint16_t index);
```

**参数:**
- `q`: 指向 `llquery` 结构体的指针
- `index`: 键值对索引（从 0 开始）

**返回值:** 指向键值对的指针，如果索引无效则返回 NULL

**示例:**
```c
const struct llquery_kv *kv = llquery_get_kv(&query, 0);
if (kv) {
    printf("First pair: %.*s = %.*s\n",
           (int)kv->key_len, kv->key,
           (int)kv->value_len, kv->value);
}
```

### `llquery_get_value()`

根据键名查找值。

```c
const char *llquery_get_value(const struct llquery *q,
                              const char *key,
                              size_t key_len);
```

**参数:**
- `q`: 指向 `llquery` 结构体的指针
- `key`: 要查找的键名
- `key_len`: 键名长度，0 表示自动计算

**返回值:** 指向值的指针（以 null 结尾的字符串），如果未找到则返回 NULL

**注意:**
- 返回第一个匹配的键对应的值
- 如果键存在但值为空，返回空字符串 `""`
- 返回的指针指向内部缓冲区，在 `llquery_free()` 或下次 `llquery_parse()` 后失效

**示例:**
```c
const char *name = llquery_get_value(&query, "name", 4);
if (name) {
    printf("Name: %s\n", name);
}
```

### `llquery_get_all_values()`

根据键名查找所有值（用于重复键）。

```c
uint16_t llquery_get_all_values(const struct llquery *q,
                                const char *key,
                                size_t key_len,
                                const char **values,
                                uint16_t max_values);
```

**参数:**
- `q`: 指向 `llquery` 结构体的指针
- `key`: 要查找的键名
- `key_len`: 键名长度
- `values`: 输出值数组（由调用者分配）
- `max_values`: 最大返回值数量

**返回值:** 实际找到的值数量

**示例:**
```c
// 对于查询字符串 "tag=red&tag=blue&tag=green"
const char *values[10];
uint16_t count = llquery_get_all_values(&query, "tag", 3, values, 10);
for (uint16_t i = 0; i < count; i++) {
    printf("Tag: %s\n", values[i]);
}
```

### `llquery_has_key()`

检查是否包含指定键。

```c
bool llquery_has_key(const struct llquery *q,
                     const char *key,
                     size_t key_len);
```

**参数:**
- `q`: 指向 `llquery` 结构体的指针
- `key`: 要检查的键名
- `key_len`: 键名长度

**返回值:** 如果包含则返回 `true`，否则返回 `false`

**示例:**
```c
if (llquery_has_key(&query, "debug", 5)) {
    printf("Debug mode enabled\n");
}
```

---

## 操作函数

### `llquery_iterate()`

遍历所有键值对。

```c
uint16_t llquery_iterate(const struct llquery *q,
                         llquery_iter_cb callback,
                         void *user_data);
```

**参数:**
- `q`: 指向 `llquery` 结构体的指针
- `callback`: 回调函数
- `user_data`: 传递给回调函数的用户数据

**返回值:** 遍历的键值对数量

**回调函数签名:**
```c
typedef int (*llquery_iter_cb)(const struct llquery_kv *kv, void *user_data);
```
回调返回非 0 值可以提前终止遍历。

**示例:**
```c
int print_callback(const struct llquery_kv *kv, void *user_data) {
    printf("%.*s = %.*s\n",
           (int)kv->key_len, kv->key,
           (int)kv->value_len, kv->value);
    return 0;  // 继续遍历
}

llquery_iterate(&query, print_callback, NULL);
```

### `llquery_sort()`

按键名排序键值对。

```c
enum llquery_error llquery_sort(struct llquery *q,
                                llquery_compare_cb compare_fn);
```

**参数:**
- `q`: 指向 `llquery` 结构体的指针
- `compare_fn`: 比较函数，NULL 表示使用默认字典序比较

**返回值:** `LQE_OK` 或错误码

**比较函数签名:**
```c
typedef int (*llquery_compare_cb)(const struct llquery_kv *a,
                                  const struct llquery_kv *b);
```
返回值：< 0 表示 a < b，0 表示相等，> 0 表示 a > b

**示例:**
```c
// 使用默认排序
llquery_sort(&query, NULL);

// 自定义排序（按值排序）
int compare_by_value(const struct llquery_kv *a, const struct llquery_kv *b) {
    size_t min_len = a->value_len < b->value_len ? a->value_len : b->value_len;
    int cmp = memcmp(a->value, b->value, min_len);
    if (cmp != 0) return cmp;
    return (int)a->value_len - (int)b->value_len;
}
llquery_sort(&query, compare_by_value);
```

### `llquery_filter()`

过滤键值对。

```c
uint16_t llquery_filter(struct llquery *q,
                        bool (*filter_fn)(const struct llquery_kv *kv, void *user_data),
                        void *user_data);
```

**参数:**
- `q`: 指向 `llquery` 结构体的指针
- `filter_fn`: 过滤函数，返回 `true` 表示保留
- `user_data`: 传递给过滤函数的用户数据

**返回值:** 过滤后的键值对数量

**示例:**
```c
// 只保留键长度大于 3 的键值对
bool filter_long_keys(const struct llquery_kv *kv, void *user_data) {
    return kv->key_len > 3;
}
llquery_filter(&query, filter_long_keys, NULL);
```

### `llquery_stringify()`

将解析结果格式化为查询字符串。

```c
size_t llquery_stringify(const struct llquery *q,
                         char *buffer,
                         size_t buffer_size,
                         bool encode);
```

**参数:**
- `q`: 指向 `llquery` 结构体的指针
- `buffer`: 输出缓冲区
- `buffer_size`: 输出缓冲区大小
- `encode`: 是否进行 URL 编码

**返回值:** 格式化后的字符串长度（不包括终止符），如果缓冲区太小则返回需要的长度

**示例:**
```c
char buffer[256];
size_t len = llquery_stringify(&query, buffer, sizeof(buffer), false);
printf("Query string: %s\n", buffer);
```

### `llquery_clone()`

复制查询解析器。

```c
enum llquery_error llquery_clone(struct llquery *dst,
                                 const struct llquery *src);
```

**参数:**
- `dst`: 目标查询解析器（未初始化）
- `src`: 源查询解析器

**返回值:** `LQE_OK` 或错误码

**注意:**
- `dst` 会被自动初始化
- 创建完整的深拷贝
- 使用后需要对 `dst` 调用 `llquery_free()`

**示例:**
```c
struct llquery src, dst;
llquery_init(&src, 0, LQF_DEFAULT);
llquery_parse("a=1&b=2", 0, &src);

llquery_clone(&dst, &src);
// dst 现在是 src 的独立副本

llquery_free(&src);
llquery_free(&dst);
```

### `llquery_reset()`

重置查询解析器。

```c
void llquery_reset(struct llquery *q);
```

**参数:**
- `q`: 指向 `llquery` 结构体的指针

**说明:**
- 重置计数器，但保留已分配的内存
- 可用于重复解析不同的查询字符串，提高效率
- 调用后可以立即调用 `llquery_parse()` 而无需重新初始化

**示例:**
```c
llquery_parse("query1=val1", 0, &query);
// 使用查询结果...

llquery_reset(&query);
llquery_parse("query2=val2", 0, &query);
// 使用新的查询结果...
```

### `llquery_set_allocator()`

设置内存分配器。

```c
void llquery_set_allocator(struct llquery *q,
                           llquery_alloc_fn alloc_fn,
                           llquery_free_fn free_fn,
                           void *alloc_data);
```

**参数:**
- `q`: 指向 `llquery` 结构体的指针
- `alloc_fn`: 内存分配函数
- `free_fn`: 内存释放函数
- `alloc_data`: 传递给分配函数的用户数据

**说明:** 运行时更改内存分配器

---

## 实用工具

### `llquery_url_encode()`

对字符串进行 URL 编码。

```c
size_t llquery_url_encode(const char *input,
                          size_t input_len,
                          char *output,
                          size_t output_size);
```

**参数:**
- `input`: 输入字符串
- `input_len`: 输入字符串长度，0 表示自动计算
- `output`: 输出缓冲区
- `output_size`: 输出缓冲区大小

**返回值:** 编码后的字符串长度，如果缓冲区太小则返回需要的长度

**编码规则:**
- 字母数字和 `-_.~` 保持不变
- 空格编码为 `+`
- 其他字符编码为 `%XX`（十六进制）

**示例:**
```c
char encoded[256];
llquery_url_encode("hello world!", 0, encoded, sizeof(encoded));
// 结果: "hello+world%21"
```

### `llquery_url_decode()`

对字符串进行 URL 解码。

```c
size_t llquery_url_decode(const char *input,
                          size_t input_len,
                          char *output,
                          size_t output_size);
```

**参数:**
- `input`: 输入字符串
- `input_len`: 输入字符串长度
- `output`: 输出缓冲区
- `output_size`: 输出缓冲区大小

**返回值:** 解码后的字符串长度

**解码规则:**
- `+` 解码为空格
- `%XX` 解码为对应字节
- 其他字符保持不变

**示例:**
```c
char decoded[256];
llquery_url_decode("hello+world%21", 0, decoded, sizeof(decoded));
// 结果: "hello world!"
```

### `llquery_is_valid()`

检查字符串是否为有效的查询字符串。

```c
bool llquery_is_valid(const char *str, size_t len);
```

**参数:**
- `str`: 要检查的字符串
- `len`: 字符串长度，0 表示自动计算

**返回值:** 如果是有效的查询字符串则返回 `true`

**验证规则:** 只允许字母数字、`-_.~%+=&` 字符

**示例:**
```c
if (llquery_is_valid(user_input, 0)) {
    llquery_parse(user_input, 0, &query);
}
```

### `llquery_count_pairs()`

快速统计查询字符串中的键值对数量。

```c
uint16_t llquery_count_pairs(const char *query, size_t query_len);
```

**参数:**
- `query`: 查询字符串
- `query_len`: 查询字符串长度

**返回值:** 键值对数量

**特点:** 快速计数，不进行完整解析

**示例:**
```c
uint16_t count = llquery_count_pairs("a=1&b=2&c=3", 0);
printf("Expected %u pairs\n", count);
```

---

## 回调函数类型

### `llquery_iter_cb`

遍历回调函数类型。

```c
typedef int (*llquery_iter_cb)(const struct llquery_kv *kv, void *user_data);
```

**参数:**
- `kv`: 当前键值对
- `user_data`: 用户数据

**返回值:** 0 继续遍历，非 0 停止遍历

### `llquery_compare_cb`

比较回调函数类型（用于排序）。

```c
typedef int (*llquery_compare_cb)(const struct llquery_kv *a,
                                  const struct llquery_kv *b);
```

**返回值:** < 0, 0, > 0 表示 a < b, a == b, a > b

### `llquery_alloc_fn` / `llquery_free_fn`

内存分配器函数类型。

```c
typedef void* (*llquery_alloc_fn)(size_t size, void *user_data);
typedef void  (*llquery_free_fn)(void *ptr, void *user_data);
```

---

## 错误处理

### `llquery_strerror()`

获取错误描述。

```c
const char *llquery_strerror(enum llquery_error error);
```

**参数:**
- `error`: 错误码

**返回值:** 错误描述字符串

**示例:**
```c
enum llquery_error err = llquery_parse(query_str, 0, &query);
if (err != LQE_OK) {
    fprintf(stderr, "Error: %s\n", llquery_strerror(err));
}
```

---

## 最佳实践

### 基本使用流程

```c
struct llquery query;

// 1. 初始化
llquery_init(&query, 0, LQF_DEFAULT);

// 2. 解析
llquery_parse(query_string, 0, &query);

// 3. 查询和操作
const char *value = llquery_get_value(&query, "key", 3);

// 4. 清理
llquery_free(&query);
```

### 性能优化建议

1. **重用解析器**: 使用 `llquery_reset()` 而不是 `free` + `init`
2. **预分配大小**: 如果知道大致的键值对数量，在 `init` 时指定
3. **栈分配**: 对于简单场景使用 `llquery_parse_fast()`
4. **避免排序**: 只在必要时调用 `llquery_sort()`

### 内存管理

- 所有通过 `llquery_get_value()` 等函数返回的指针都指向内部缓冲区
- 这些指针在调用 `llquery_free()` 或 `llquery_parse()` 后失效
- 如需长期保存，请复制字符串内容

### 线程安全

- 每个 `llquery` 实例是独立的，可以在不同线程中使用不同实例
- 不要在多个线程中同时操作同一个 `llquery` 实例
- 所有函数都是可重入的（无全局状态）

---

## 示例代码

### 完整示例

参见 `example.c` 文件。

### 高级用法

```c
// 自定义过滤和排序
llquery_sort(&query, NULL);
llquery_filter(&query, my_filter, NULL);

// 字符串化输出
char output[1024];
llquery_stringify(&query, output, sizeof(output), true);

// 克隆和修改
struct llquery copy;
llquery_clone(&copy, &query);
llquery_filter(&copy, some_filter, NULL);
```

---

## 参考

- RFC 3986: Uniform Resource Identifier (URI): Generic Syntax
- [llhttp](https://github.com/nodejs/llhttp) - 参考实现
- [llurl](https://github.com/zhaozg/llurl) - 相关项目
