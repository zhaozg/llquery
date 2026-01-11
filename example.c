#include <stdio.h>
#include "llquery.h"

int print_callback(const struct llquery_kv *kv, void *user_data) {
  printf("  %.*s = %.*s\n",
         (int)kv->key_len, kv->key,
         (int)kv->value_len, kv->value);
  return 0;
}

int main() {
  struct llquery query;
  const char *test_str = "name=John&age=30&city=New+York&lang=zh%2Fcn";

  // 初始化解析器
  enum llquery_error err = llquery_init(&query, 0, LQF_DEFAULT);
  if (err != LQE_OK) {
    printf("初始化失败: %s\n", llquery_strerror(err));
    return 1;
  }

  // 解析查询字符串
  err = llquery_parse(test_str, 0, &query);
  if (err != LQE_OK) {
    printf("解析失败: %s\n", llquery_strerror(err));
    llquery_free(&query);
    return 1;
  }

  // 打印结果
  printf("解析到 %u 个参数:\n", llquery_count(&query));
  llquery_iterate(&query, print_callback, NULL);

  // 查询特定值
  const char *name = llquery_get_value(&query, "name", 4);
  if (name) {
    printf("\nname 的值: %s\n", name);
  }

  // 排序键值对
  llquery_sort(&query, NULL);
  printf("\n排序后:\n");
  llquery_iterate(&query, print_callback, NULL);

  // 清理
  llquery_free(&query);

  // 测试快速解析
  printf("\n=== 快速解析测试 ===\n");
  struct llquery_kv fast_pairs[10];
  uint16_t count = llquery_parse_fast("a=1&b=2&c=3", 0, fast_pairs, 10, LQF_AUTO_DECODE);
  printf("快速解析到 %u 个参数\n", count);
  for (uint16_t i = 0; i < count; i++) {
    printf("  %.*s = %.*s\n",
           (int)fast_pairs[i].key_len, fast_pairs[i].key,
           (int)fast_pairs[i].value_len, fast_pairs[i].value);
  }

  return 0;
}
