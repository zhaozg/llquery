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

/* 主函数 */
int main() {
    printf("=== llquery Test Suite ===\n\n");
    
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
