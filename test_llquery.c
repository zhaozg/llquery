#include "llquery.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* 测试计数器 */
static int test_count = 0;
static int test_passed = 0;
static int test_failed = 0;

/* 测试宏 */
#define TEST_START(name) \
    do { \
        test_count++; \
        printf("Test #%d: %s ... ", test_count, name); \
    } while(0)

#define TEST_PASS() \
    do { \
        printf("PASS\n"); \
        test_passed++; \
    } while(0)

#define TEST_FAIL(msg) \
    do { \
        printf("FAIL: %s\n", msg); \
        test_failed++; \
    } while(0)

#define ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            TEST_FAIL(msg); \
            return; \
        } \
    } while(0)

#define ASSERT_EQ(a, b, msg) \
    do { \
        if ((a) != (b)) { \
            printf("FAIL: %s (expected %d, got %d)\n", msg, (int)(b), (int)(a)); \
            test_failed++; \
            return; \
        } \
    } while(0)

#define ASSERT_STR_EQ(a, b, msg) \
    do { \
        if (strcmp((a), (b)) != 0) { \
            printf("FAIL: %s (expected '%s', got '%s')\n", msg, b, a); \
            test_failed++; \
            return; \
        } \
    } while(0)

/* 测试基本解析 */
void test_basic_parse() {
    TEST_START("Basic parse");
    struct llquery query;
    
    enum llquery_error err = llquery_init(&query, 0, LQF_DEFAULT);
    ASSERT(err == LQE_OK, "Init failed");
    
    err = llquery_parse("key1=value1&key2=value2", 0, &query);
    ASSERT(err == LQE_OK, "Parse failed");
    
    ASSERT_EQ(llquery_count(&query), 2, "Wrong count");
    
    const char *val = llquery_get_value(&query, "key1", 4);
    ASSERT(val != NULL, "key1 not found");
    ASSERT_STR_EQ(val, "value1", "Wrong value for key1");
    
    val = llquery_get_value(&query, "key2", 4);
    ASSERT(val != NULL, "key2 not found");
    ASSERT_STR_EQ(val, "value2", "Wrong value for key2");
    
    llquery_free(&query);
    TEST_PASS();
}

/* 测试 URL 解码 */
void test_url_decode() {
    TEST_START("URL decode");
    struct llquery query;
    
    llquery_init(&query, 0, LQF_AUTO_DECODE);
    llquery_parse("name=John+Doe&city=New+York&lang=zh%2Fcn", 0, &query);
    
    const char *name = llquery_get_value(&query, "name", 4);
    ASSERT_STR_EQ(name, "John Doe", "Wrong decoded name");
    
    const char *city = llquery_get_value(&query, "city", 4);
    ASSERT_STR_EQ(city, "New York", "Wrong decoded city");
    
    const char *lang = llquery_get_value(&query, "lang", 4);
    ASSERT_STR_EQ(lang, "zh/cn", "Wrong decoded lang");
    
    llquery_free(&query);
    TEST_PASS();
}

/* 测试空值 */
void test_empty_values() {
    TEST_START("Empty values");
    struct llquery query;
    
    llquery_init(&query, 0, LQF_AUTO_DECODE | LQF_KEEP_EMPTY);
    llquery_parse("key1=&key2=value2&key3=", 0, &query);
    
    ASSERT_EQ(llquery_count(&query), 3, "Wrong count");
    
    const char *val = llquery_get_value(&query, "key1", 4);
    ASSERT(val != NULL, "key1 not found");
    ASSERT_EQ(strlen(val), 0, "key1 should be empty");
    
    llquery_free(&query);
    TEST_PASS();
}

/* 测试前导问号 */
void test_leading_question_mark() {
    TEST_START("Leading question mark");
    struct llquery query;
    
    llquery_init(&query, 0, LQF_DEFAULT);
    llquery_parse("?key1=value1&key2=value2", 0, &query);
    
    ASSERT_EQ(llquery_count(&query), 2, "Wrong count");
    
    const char *val = llquery_get_value(&query, "key1", 4);
    ASSERT_STR_EQ(val, "value1", "Wrong value");
    
    llquery_free(&query);
    TEST_PASS();
}

/* 测试重复键 */
void test_duplicate_keys() {
    TEST_START("Duplicate keys");
    struct llquery query;
    
    llquery_init(&query, 0, LQF_DEFAULT);
    llquery_parse("tag=red&tag=blue&tag=green", 0, &query);
    
    const char *values[10];
    uint16_t count = llquery_get_all_values(&query, "tag", 3, values, 10);
    
    ASSERT_EQ(count, 3, "Wrong duplicate count");
    ASSERT_STR_EQ(values[0], "red", "Wrong first value");
    ASSERT_STR_EQ(values[1], "blue", "Wrong second value");
    ASSERT_STR_EQ(values[2], "green", "Wrong third value");
    
    llquery_free(&query);
    TEST_PASS();
}

/* 测试排序 */
void test_sort() {
    TEST_START("Sort keys");
    struct llquery query;
    
    llquery_init(&query, 0, LQF_DEFAULT);
    llquery_parse("zebra=1&apple=2&banana=3", 0, &query);
    
    llquery_sort(&query, NULL);
    
    const struct llquery_kv *kv0 = llquery_get_kv(&query, 0);
    const struct llquery_kv *kv1 = llquery_get_kv(&query, 1);
    const struct llquery_kv *kv2 = llquery_get_kv(&query, 2);
    
    ASSERT(strncmp(kv0->key, "apple", 5) == 0, "Wrong first key after sort");
    ASSERT(strncmp(kv1->key, "banana", 6) == 0, "Wrong second key after sort");
    ASSERT(strncmp(kv2->key, "zebra", 5) == 0, "Wrong third key after sort");
    
    llquery_free(&query);
    TEST_PASS();
}

/* 测试迭代 */
int count_callback(const struct llquery_kv *kv, void *user_data) {
    (void)kv;  // Unused in this callback
    int *count = (int *)user_data;
    (*count)++;
    return 0;
}

void test_iterate() {
    TEST_START("Iterate");
    struct llquery query;
    
    llquery_init(&query, 0, LQF_DEFAULT);
    llquery_parse("a=1&b=2&c=3", 0, &query);
    
    int count = 0;
    llquery_iterate(&query, count_callback, &count);
    
    ASSERT_EQ(count, 3, "Wrong iteration count");
    
    llquery_free(&query);
    TEST_PASS();
}

/* 测试字符串化 */
void test_stringify() {
    TEST_START("Stringify");
    struct llquery query;
    
    llquery_init(&query, 0, LQF_DEFAULT);
    llquery_parse("key1=value1&key2=value2", 0, &query);
    
    char buffer[256];
    size_t len = llquery_stringify(&query, buffer, sizeof(buffer), false);
    
    ASSERT(len > 0, "Stringify failed");
    ASSERT(strstr(buffer, "key1=value1") != NULL, "Missing key1 in output");
    ASSERT(strstr(buffer, "key2=value2") != NULL, "Missing key2 in output");
    
    llquery_free(&query);
    TEST_PASS();
}

/* 测试快速解析 */
void test_fast_parse() {
    TEST_START("Fast parse");
    
    struct llquery_kv pairs[10];
    uint16_t count = llquery_parse_fast("a=1&b=2&c=3", 0, pairs, 10, LQF_NONE);
    
    ASSERT_EQ(count, 3, "Wrong fast parse count");
    
    TEST_PASS();
}

/* 测试有效性检查 */
void test_is_valid() {
    TEST_START("Validity check");
    
    ASSERT(llquery_is_valid("key=value", 9) == true, "Valid string rejected");
    ASSERT(llquery_is_valid("key=value&foo=bar", 17) == true, "Valid string rejected");
    ASSERT(llquery_is_valid("", 0) == false, "Empty string accepted");
    
    TEST_PASS();
}

/* 测试计数键值对 */
void test_count_pairs() {
    TEST_START("Count pairs");
    
    uint16_t count = llquery_count_pairs("a=1&b=2&c=3", 0);
    ASSERT_EQ(count, 3, "Wrong pair count");
    
    count = llquery_count_pairs("?a=1&b=2", 0);
    ASSERT_EQ(count, 2, "Wrong pair count with ?");
    
    count = llquery_count_pairs("single", 0);
    ASSERT_EQ(count, 1, "Wrong single pair count");
    
    TEST_PASS();
}

/* 测试 URL 编码/解码函数 */
void test_url_encode_decode() {
    TEST_START("URL encode/decode");
    
    char encoded[256];
    char decoded[256];
    
    // 测试编码
    size_t len = llquery_url_encode("hello world", 0, encoded, sizeof(encoded));
    ASSERT(len > 0, "Encode failed");
    ASSERT(strstr(encoded, "hello") != NULL, "Missing text in encoded");
    
    // 测试解码
    len = llquery_url_decode("hello+world", 0, decoded, sizeof(decoded));
    ASSERT_STR_EQ(decoded, "hello world", "Wrong decode result");
    
    len = llquery_url_decode("hello%20world", 0, decoded, sizeof(decoded));
    ASSERT_STR_EQ(decoded, "hello world", "Wrong decode %20 result");
    
    TEST_PASS();
}

/* 测试克隆 */
void test_clone() {
    TEST_START("Clone");
    struct llquery src, dst;
    
    llquery_init(&src, 0, LQF_DEFAULT);
    llquery_parse("key1=value1&key2=value2", 0, &src);
    
    enum llquery_error err = llquery_clone(&dst, &src);
    ASSERT(err == LQE_OK, "Clone failed");
    
    ASSERT_EQ(llquery_count(&dst), llquery_count(&src), "Clone count mismatch");
    
    const char *val = llquery_get_value(&dst, "key1", 4);
    ASSERT_STR_EQ(val, "value1", "Clone value mismatch");
    
    llquery_free(&src);
    llquery_free(&dst);
    TEST_PASS();
}

/* 测试重置 */
void test_reset() {
    TEST_START("Reset");
    struct llquery query;
    
    llquery_init(&query, 0, LQF_DEFAULT);
    llquery_parse("key1=value1", 0, &query);
    
    ASSERT_EQ(llquery_count(&query), 1, "Initial count wrong");
    
    llquery_reset(&query);
    ASSERT_EQ(llquery_count(&query), 0, "Reset failed");
    
    // 重新解析
    llquery_parse("key2=value2&key3=value3", 0, &query);
    ASSERT_EQ(llquery_count(&query), 2, "Reparse count wrong");
    
    llquery_free(&query);
    TEST_PASS();
}

/* 测试错误处理 */
void test_error_handling() {
    TEST_START("Error handling");
    struct llquery query;
    
    // 测试 NULL 输入
    enum llquery_error err = llquery_parse(NULL, 0, &query);
    ASSERT(err == LQE_NULL_INPUT, "NULL input not detected");
    
    // 测试空字符串
    llquery_init(&query, 0, LQF_DEFAULT);
    err = llquery_parse("", 0, &query);
    ASSERT(err == LQE_EMPTY_STRING, "Empty string not detected");
    
    llquery_free(&query);
    TEST_PASS();
}

/* 测试键名小写 */
void test_lowercase_keys() {
    TEST_START("Lowercase keys");
    struct llquery query;
    
    llquery_init(&query, 0, LQF_AUTO_DECODE | LQF_LOWERCASE_KEYS);
    llquery_parse("KEY1=value1&Key2=value2", 0, &query);
    
    const char *val = llquery_get_value(&query, "key1", 4);
    ASSERT(val != NULL, "Lowercase key1 not found");
    
    val = llquery_get_value(&query, "key2", 4);
    ASSERT(val != NULL, "Lowercase key2 not found");
    
    llquery_free(&query);
    TEST_PASS();
}

/* 测试 has_key */
void test_has_key() {
    TEST_START("Has key");
    struct llquery query;
    
    llquery_init(&query, 0, LQF_DEFAULT);
    llquery_parse("key1=value1&key2=value2", 0, &query);
    
    ASSERT(llquery_has_key(&query, "key1", 4) == true, "key1 should exist");
    ASSERT(llquery_has_key(&query, "key2", 4) == true, "key2 should exist");
    ASSERT(llquery_has_key(&query, "key3", 4) == false, "key3 should not exist");
    
    llquery_free(&query);
    TEST_PASS();
}

/* 测试边界情况 */
void test_edge_cases() {
    TEST_START("Edge cases");
    struct llquery query;
    
    llquery_init(&query, 0, LQF_DEFAULT | LQF_KEEP_EMPTY);
    
    // 测试单个键无值
    llquery_parse("key", 0, &query);
    ASSERT_EQ(llquery_count(&query), 1, "Single key count wrong");
    llquery_reset(&query);
    
    // 测试多个连续的 &
    llquery_parse("key1=value1&&&key2=value2", 0, &query);
    ASSERT_EQ(llquery_count(&query), 2, "Multiple & count wrong");
    llquery_reset(&query);
    
    // 测试只有 ?
    llquery_parse("?", 0, &query);
    ASSERT_EQ(llquery_count(&query), 0, "Only ? should be 0");
    
    llquery_free(&query);
    TEST_PASS();
}

/* 测试过滤 */
bool filter_callback(const struct llquery_kv *kv, void *user_data) {
    (void)user_data;  // Unused in this callback
    // 只保留键长度大于 3 的
    return kv->key_len > 3;
}

void test_filter() {
    TEST_START("Filter");
    struct llquery query;
    
    llquery_init(&query, 0, LQF_DEFAULT);
    llquery_parse("ab=1&abcd=2&xy=3&wxyz=4", 0, &query);
    
    ASSERT_EQ(llquery_count(&query), 4, "Initial count wrong");
    
    uint16_t filtered = llquery_filter(&query, filter_callback, NULL);
    ASSERT_EQ(filtered, 2, "Filter count wrong");
    
    llquery_free(&query);
    TEST_PASS();
}

/* 测试边界值 - 大量参数 */
void test_boundary_large_params() {
    TEST_START("Boundary: large number of parameters");
    struct llquery query;
    
    llquery_init(&query, 10, LQF_DEFAULT);
    
    // 超过最大限制
    const char *many = "p1=v1&p2=v2&p3=v3&p4=v4&p5=v5&p6=v6&p7=v7&p8=v8&p9=v9&p10=v10&p11=v11&p12=v12";
    llquery_parse(many, 0, &query);
    
    // 非严格模式应该解析前10个
    ASSERT_EQ(llquery_count(&query), 10, "Should parse up to max_pairs");
    
    llquery_free(&query);
    TEST_PASS();
}

/* 测试边界值 - 超长键值 */
void test_boundary_long_values() {
    TEST_START("Boundary: very long key and value");
    struct llquery query;
    
    llquery_init(&query, 0, LQF_DEFAULT);
    
    // 构造一个超长的键值对
    char long_query[2048];
    memset(long_query, 'a', 500);
    long_query[500] = '=';
    memset(long_query + 501, 'b', 500);
    long_query[1001] = '\0';
    
    enum llquery_error err = llquery_parse(long_query, 0, &query);
    ASSERT(err == LQE_OK, "Should parse long key/value");
    ASSERT_EQ(llquery_count(&query), 1, "Should have one pair");
    
    const struct llquery_kv *kv = llquery_get_kv(&query, 0);
    ASSERT_EQ(kv->key_len, 500, "Key length should be 500");
    ASSERT_EQ(kv->value_len, 500, "Value length should be 500");
    
    llquery_free(&query);
    TEST_PASS();
}

/* 测试边界值 - 空字符串各种情况 */
void test_boundary_empty_strings() {
    TEST_START("Boundary: various empty string cases");
    struct llquery query;
    
    llquery_init(&query, 0, LQF_DEFAULT | LQF_KEEP_EMPTY);
    
    // 只有分隔符
    llquery_parse("&&&", 0, &query);
    ASSERT_EQ(llquery_count(&query), 0, "Only separators should be 0");
    llquery_reset(&query);
    
    // 空键有值
    llquery_parse("=value", 0, &query);
    ASSERT_EQ(llquery_count(&query), 0, "Empty key should be skipped");
    llquery_reset(&query);
    
    // 多个等号
    llquery_parse("key=value=extra", 0, &query);
    ASSERT_EQ(llquery_count(&query), 1, "Multiple = should work");
    const char *val = llquery_get_value(&query, "key", 3);
    ASSERT(val != NULL && strstr(val, "value") != NULL, "Should parse first = as separator");
    
    llquery_free(&query);
    TEST_PASS();
}

/* 测试特殊字符处理 */
void test_special_characters() {
    TEST_START("Special characters handling");
    struct llquery query;
    
    llquery_init(&query, 0, LQF_AUTO_DECODE);
    
    // URL编码的特殊字符（不包括&因为它是分隔符）
    llquery_parse("special=%21%40%23%24%25%5E%2A%28%29", 0, &query);
    const char *val = llquery_get_value(&query, "special", 7);
    ASSERT(val != NULL, "Should decode special chars");
    // 检查部分解码字符
    ASSERT(strchr(val, '!') != NULL, "Should contain !");
    ASSERT(strchr(val, '@') != NULL, "Should contain @");
    ASSERT(strchr(val, '#') != NULL, "Should contain #");
    llquery_reset(&query);
    
    // 加号和空格
    llquery_parse("text=hello+world&more=test%20space", 0, &query);
    val = llquery_get_value(&query, "text", 4);
    ASSERT(val != NULL && strstr(val, "hello world") != NULL, "Plus should decode to space");
    val = llquery_get_value(&query, "more", 4);
    ASSERT(val != NULL && strstr(val, "test space") != NULL, "%20 should decode to space");
    
    llquery_free(&query);
    TEST_PASS();
}

/* 测试无效输入 */
void test_invalid_inputs() {
    TEST_START("Invalid inputs");
    struct llquery query;
    
    llquery_init(&query, 0, LQF_DEFAULT);
    
    // 无效的百分号编码
    llquery_parse("bad=%GG", 0, &query);
    ASSERT_EQ(llquery_count(&query), 1, "Should handle invalid hex gracefully");
    llquery_reset(&query);
    
    // 不完整的百分号编码
    llquery_parse("incomplete=%2", 0, &query);
    ASSERT_EQ(llquery_count(&query), 1, "Should handle incomplete hex");
    llquery_reset(&query);
    
    // 百分号在末尾
    llquery_parse("trailing=%", 0, &query);
    ASSERT_EQ(llquery_count(&query), 1, "Should handle trailing %");
    
    llquery_free(&query);
    TEST_PASS();
}

/* 测试内存分配失败场景 */
void test_memory_limits() {
    TEST_START("Memory allocation with limits");
    struct llquery query;
    
    // 测试小缓冲区
    llquery_init(&query, 1, LQF_DEFAULT);
    llquery_parse("a=1&b=2&c=3", 0, &query);
    ASSERT_EQ(llquery_count(&query), 1, "Should limit to max_pairs");
    llquery_free(&query);
    
    // 测试0作为最大值（使用默认）
    llquery_init(&query, 0, LQF_DEFAULT);
    llquery_parse("a=1&b=2", 0, &query);
    ASSERT_EQ(llquery_count(&query), 2, "Zero should use default max");
    llquery_free(&query);
    
    TEST_PASS();
}

/* 测试URL编码/解码边界 */
void test_url_codec_boundary() {
    TEST_START("URL encode/decode boundary cases");
    
    char encoded[512];
    char decoded[512];
    
    // 空字符串
    size_t len = llquery_url_encode("", 0, encoded, sizeof(encoded));
    ASSERT_EQ(len, 0, "Empty string should encode to length 0");
    
    len = llquery_url_decode("", 0, decoded, sizeof(decoded));
    ASSERT_EQ(len, 0, "Empty string should decode to length 0");
    
    // 所有需要编码的字符
    const char *all_special = "!@#$%^&*(){}[]|\\:;\"'<>,.?/";
    len = llquery_url_encode(all_special, 0, encoded, sizeof(encoded));
    ASSERT(len > strlen(all_special), "Encoded should be longer");
    
    // 缓冲区太小
    len = llquery_url_encode("hello world test", 0, encoded, 5);
    ASSERT(len > 5, "Should return required size");
    
    TEST_PASS();
}

/* 测试并发安全（基础） */
void test_thread_safety_basic() {
    TEST_START("Thread safety: independent instances");
    struct llquery q1, q2;
    
    llquery_init(&q1, 0, LQF_DEFAULT);
    llquery_init(&q2, 0, LQF_DEFAULT);
    
    llquery_parse("a=1&b=2", 0, &q1);
    llquery_parse("x=10&y=20", 0, &q2);
    
    // 验证互不干扰
    ASSERT_EQ(llquery_count(&q1), 2, "q1 should have 2 pairs");
    ASSERT_EQ(llquery_count(&q2), 2, "q2 should have 2 pairs");
    
    const char *val1 = llquery_get_value(&q1, "a", 1);
    const char *val2 = llquery_get_value(&q2, "x", 1);
    
    ASSERT_STR_EQ(val1, "1", "q1 value should be correct");
    ASSERT_STR_EQ(val2, "10", "q2 value should be correct");
    
    llquery_free(&q1);
    llquery_free(&q2);
    TEST_PASS();
}

/* 测试严格模式 */
void test_strict_mode() {
    TEST_START("Strict mode behavior");
    struct llquery query;
    
    // 严格模式：超过限制应该报错
    llquery_init(&query, 2, LQF_DEFAULT | LQF_STRICT);
    enum llquery_error err = llquery_parse("a=1&b=2&c=3", 0, &query);
    ASSERT(err == LQE_TOO_MANY_PAIRS, "Strict mode should error on too many pairs");
    llquery_free(&query);
    
    // 非严格模式：应该成功
    llquery_init(&query, 2, LQF_DEFAULT);
    err = llquery_parse("a=1&b=2&c=3", 0, &query);
    ASSERT(err == LQE_OK, "Non-strict should succeed");
    ASSERT_EQ(llquery_count(&query), 2, "Should parse up to limit");
    llquery_free(&query);
    
    TEST_PASS();
}

/* 测试组合选项 */
void test_combined_options() {
    TEST_START("Combined option flags");
    struct llquery query;
    
    uint16_t flags = LQF_AUTO_DECODE | LQF_LOWERCASE_KEYS | LQF_TRIM_VALUES | LQF_KEEP_EMPTY;
    llquery_init(&query, 0, flags);
    
    llquery_parse("KEY1=++value1++&KEY2=&key3=Value%203", 0, &query);
    
    // 检查小写转换
    ASSERT(llquery_has_key(&query, "key1", 4), "Should convert to lowercase");
    ASSERT(llquery_has_key(&query, "key2", 4), "Should convert to lowercase");
    
    // 检查空值保留
    ASSERT_EQ(llquery_count(&query), 3, "Should keep empty values");
    
    // 检查解码
    const char *val = llquery_get_value(&query, "key3", 4);
    ASSERT(val != NULL && strstr(val, "Value 3") != NULL, "Should decode %20");
    
    llquery_free(&query);
    TEST_PASS();
}

/* 测试 Fast Parse 边界 */
void test_fast_parse_limits() {
    TEST_START("Fast parse limitations");
    
    struct llquery_kv pairs[5];
    
    // 正常情况
    uint16_t count = llquery_parse_fast("a=1&b=2", 0, pairs, 5, LQF_NONE);
    ASSERT_EQ(count, 2, "Should parse 2 pairs");
    
    // 超过缓冲区
    count = llquery_parse_fast("a=1&b=2&c=3&d=4&e=5&f=6", 0, pairs, 3, LQF_NONE);
    ASSERT_EQ(count, 3, "Should limit to buffer size");
    
    // NULL输入
    count = llquery_parse_fast(NULL, 0, pairs, 5, LQF_NONE);
    ASSERT_EQ(count, 0, "NULL should return 0");
    
    // 空数组
    count = llquery_parse_fast("a=1", 0, NULL, 5, LQF_NONE);
    ASSERT_EQ(count, 0, "NULL array should return 0");
    
    TEST_PASS();
}

/* 主函数 */
int main() {
    printf("=== llquery Test Suite ===\n\n");
    
    // 基础功能测试
    test_basic_parse();
    test_url_decode();
    test_empty_values();
    test_leading_question_mark();
    test_duplicate_keys();
    test_sort();
    test_iterate();
    test_stringify();
    test_fast_parse();
    test_is_valid();
    test_count_pairs();
    test_url_encode_decode();
    test_clone();
    test_reset();
    test_error_handling();
    test_lowercase_keys();
    test_has_key();
    test_edge_cases();
    test_filter();
    
    // 边界测试
    test_boundary_large_params();
    test_boundary_long_values();
    test_boundary_empty_strings();
    
    // 特殊情况测试
    test_special_characters();
    test_invalid_inputs();
    test_memory_limits();
    test_url_codec_boundary();
    test_thread_safety_basic();
    test_strict_mode();
    test_combined_options();
    test_fast_parse_limits();
    
    printf("\n=== Test Results ===\n");
    printf("Total:  %d\n", test_count);
    printf("Passed: %d\n", test_passed);
    printf("Failed: %d\n", test_failed);
    
    if (test_failed == 0) {
        printf("\n✓ All tests passed!\n");
        return 0;
    } else {
        printf("\n✗ Some tests failed!\n");
        return 1;
    }
}
