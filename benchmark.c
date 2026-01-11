#include "llquery.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _POSIX_C_SOURCE
#include <sys/time.h>
#endif

/* 高精度计时器 */
typedef struct {
    double start_time;
} llquery_timer_t;

static double get_time() {
#ifdef _POSIX_C_SOURCE
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
#else
    return (double)clock() / CLOCKS_PER_SEC;
#endif
}

static void timer_start(llquery_timer_t *timer) {
    timer->start_time = get_time();
}

static double timer_elapsed(llquery_timer_t *timer) {
    return get_time() - timer->start_time;
}

/* 基准测试宏 */
#define BENCHMARK(name, iterations, code) \
    do { \
        llquery_timer_t timer; \
        printf("%-40s ", name); \
        fflush(stdout); \
        timer_start(&timer); \
        for (int _i = 0; _i < iterations; _i++) { \
            code \
        } \
        double elapsed = timer_elapsed(&timer); \
        double ops_per_sec = (double)iterations / elapsed; \
        printf("%10.2f ops/sec  (%8.3f ms total)\n", ops_per_sec, elapsed * 1000.0); \
    } while(0)

/* 测试数据 */
static const char *simple_query = "key1=value1&key2=value2&key3=value3";
static const char *complex_query = "name=John+Doe&age=30&city=New+York&country=USA&email=john%40example.com&lang=en-US";
static const char *encoded_query = "search=hello%20world&filter=tag%3Dred&sort=date%20desc&page=1";
static const char *many_params = "p1=v1&p2=v2&p3=v3&p4=v4&p5=v5&p6=v6&p7=v7&p8=v8&p9=v9&p10=v10&p11=v11&p12=v12&p13=v13&p14=v14&p15=v15";
static const char *duplicate_keys = "tag=red&tag=blue&tag=green&tag=yellow&tag=orange";

/* 基准测试函数 */

void benchmark_simple_parse(int iterations) {
    BENCHMARK("Simple parse (3 params)", iterations, {
        struct llquery query;
        llquery_init(&query, 0, LQF_DEFAULT);
        llquery_parse(simple_query, 0, &query);
        llquery_free(&query);
    });
}

void benchmark_complex_parse(int iterations) {
    BENCHMARK("Complex parse with decode (6 params)", iterations, {
        struct llquery query;
        llquery_init(&query, 0, LQF_AUTO_DECODE);
        llquery_parse(complex_query, 0, &query);
        llquery_free(&query);
    });
}

void benchmark_encoded_parse(int iterations) {
    BENCHMARK("Heavy encoding parse (4 params)", iterations, {
        struct llquery query;
        llquery_init(&query, 0, LQF_AUTO_DECODE);
        llquery_parse(encoded_query, 0, &query);
        llquery_free(&query);
    });
}

void benchmark_many_params(int iterations) {
    BENCHMARK("Many parameters (15 params)", iterations, {
        struct llquery query;
        llquery_init(&query, 0, LQF_DEFAULT);
        llquery_parse(many_params, 0, &query);
        llquery_free(&query);
    });
}

void benchmark_duplicate_keys(int iterations) {
    BENCHMARK("Duplicate keys (5 params)", iterations, {
        struct llquery query;
        llquery_init(&query, 0, LQF_DEFAULT);
        llquery_parse(duplicate_keys, 0, &query);
        const char *values[10];
        llquery_get_all_values(&query, "tag", 3, values, 10);
        llquery_free(&query);
    });
}

void benchmark_get_value(int iterations) {
    struct llquery query;
    llquery_init(&query, 0, LQF_DEFAULT);
    llquery_parse(many_params, 0, &query);
    
    BENCHMARK("Get value by key", iterations, {
        llquery_get_value(&query, "p5", 2);
        llquery_get_value(&query, "p10", 3);
        llquery_get_value(&query, "p15", 3);
    });
    
    llquery_free(&query);
}

void benchmark_has_key(int iterations) {
    struct llquery query;
    llquery_init(&query, 0, LQF_DEFAULT);
    llquery_parse(many_params, 0, &query);
    
    BENCHMARK("Has key check", iterations, {
        llquery_has_key(&query, "p5", 2);
        llquery_has_key(&query, "p10", 3);
        llquery_has_key(&query, "p99", 3);
    });
    
    llquery_free(&query);
}

void benchmark_iterate(int iterations) {
    struct llquery query;
    llquery_init(&query, 0, LQF_DEFAULT);
    llquery_parse(many_params, 0, &query);
    
    BENCHMARK("Iterate all pairs (15 params)", iterations, {
        for (uint16_t i = 0; i < llquery_count(&query); i++) {
            llquery_get_kv(&query, i);
        }
    });
    
    llquery_free(&query);
}

void benchmark_sort(int iterations) {
    BENCHMARK("Sort keys (15 params)", iterations, {
        struct llquery query;
        llquery_init(&query, 0, LQF_DEFAULT);
        llquery_parse(many_params, 0, &query);
        llquery_sort(&query, NULL);
        llquery_free(&query);
    });
}

void benchmark_stringify(int iterations) {
    struct llquery query;
    llquery_init(&query, 0, LQF_DEFAULT);
    llquery_parse(many_params, 0, &query);
    char buffer[1024];
    
    BENCHMARK("Stringify (15 params)", iterations, {
        llquery_stringify(&query, buffer, sizeof(buffer), false);
    });
    
    llquery_free(&query);
}

void benchmark_clone(int iterations) {
    struct llquery src;
    llquery_init(&src, 0, LQF_DEFAULT);
    llquery_parse(many_params, 0, &src);
    
    BENCHMARK("Clone parser (15 params)", iterations, {
        struct llquery dst;
        llquery_clone(&dst, &src);
        llquery_free(&dst);
    });
    
    llquery_free(&src);
}

void benchmark_fast_parse(int iterations) {
    BENCHMARK("Fast parse (3 params, stack)", iterations, {
        struct llquery_kv pairs[10];
        llquery_parse_fast(simple_query, 0, pairs, 10, LQF_NONE);
    });
}

void benchmark_url_encode(int iterations) {
    const char *text = "Hello World! This is a test string with special chars: @#$%";
    char buffer[256];
    
    BENCHMARK("URL encode", iterations, {
        llquery_url_encode(text, 0, buffer, sizeof(buffer));
    });
}

void benchmark_url_decode(int iterations) {
    const char *encoded = "Hello+World%21+This+is+a+test%20string";
    char buffer[256];
    
    BENCHMARK("URL decode", iterations, {
        llquery_url_decode(encoded, 0, buffer, sizeof(buffer));
    });
}

void benchmark_count_pairs(int iterations) {
    BENCHMARK("Count pairs (15 params)", iterations, {
        llquery_count_pairs(many_params, 0);
    });
}

void benchmark_is_valid(int iterations) {
    BENCHMARK("Validate query string", iterations, {
        llquery_is_valid(many_params, 0);
        llquery_is_valid(complex_query, 0);
    });
}

/* 内存分配基准测试 */
void benchmark_memory_allocation(int iterations) {
    BENCHMARK("Init + Free (no parse)", iterations, {
        struct llquery query;
        llquery_init(&query, 128, LQF_DEFAULT);
        llquery_free(&query);
    });
}

void benchmark_parse_with_options(int iterations) {
    BENCHMARK("Parse with all options", iterations, {
        struct llquery query;
        uint16_t flags = LQF_AUTO_DECODE | LQF_SORT_KEYS | LQF_LOWERCASE_KEYS | LQF_TRIM_VALUES;
        llquery_init(&query, 0, flags);
        llquery_parse(complex_query, 0, &query);
        llquery_free(&query);
    });
}

/* 吞吐量测试 */
void benchmark_throughput() {
    printf("\n=== Throughput Test ===\n");
    
    int total_iterations = 1000000;
    size_t total_bytes = 0;
    llquery_timer_t timer;
    
    timer_start(&timer);
    for (int i = 0; i < total_iterations; i++) {
        struct llquery query;
        llquery_init(&query, 0, LQF_AUTO_DECODE);
        llquery_parse(complex_query, 0, &query);
        total_bytes += strlen(complex_query);
        llquery_free(&query);
    }
    double elapsed = timer_elapsed(&timer);
    
    double mb_per_sec = (total_bytes / 1024.0 / 1024.0) / elapsed;
    double queries_per_sec = total_iterations / elapsed;
    
    printf("Processed: %d queries, %.2f MB in %.3f seconds\n",
           total_iterations, total_bytes / 1024.0 / 1024.0, elapsed);
    printf("Throughput: %.2f MB/sec, %.2f queries/sec\n",
           mb_per_sec, queries_per_sec);
}

/* 主函数 */
int main() {
    printf("=== llquery Benchmark Suite ===\n");
    printf("Running on: ");
#ifdef __GNUC__
    printf("GCC %d.%d.%d", __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    printf("MSVC %d", _MSC_VER);
#else
    printf("Unknown compiler");
#endif
    printf("\n\n");
    
    int iterations = 100000;
    
    printf("=== Parse Benchmarks ===\n");
    benchmark_simple_parse(iterations);
    benchmark_complex_parse(iterations);
    benchmark_encoded_parse(iterations);
    benchmark_many_params(iterations);
    benchmark_duplicate_keys(iterations);
    benchmark_fast_parse(iterations);
    
    printf("\n=== Query Benchmarks ===\n");
    benchmark_get_value(iterations);
    benchmark_has_key(iterations);
    benchmark_iterate(iterations);
    
    printf("\n=== Manipulation Benchmarks ===\n");
    benchmark_sort(iterations / 10);  // 更慢，减少迭代
    benchmark_stringify(iterations);
    benchmark_clone(iterations);
    
    printf("\n=== Utility Benchmarks ===\n");
    benchmark_url_encode(iterations);
    benchmark_url_decode(iterations);
    benchmark_count_pairs(iterations);
    benchmark_is_valid(iterations);
    
    printf("\n=== Advanced Benchmarks ===\n");
    benchmark_memory_allocation(iterations);
    benchmark_parse_with_options(iterations);
    
    benchmark_throughput();
    
    printf("\n=== Benchmark Complete ===\n");
    return 0;
}
