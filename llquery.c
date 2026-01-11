#include "llquery.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* 默认配置 */
#define DEFAULT_MAX_PAIRS 128
#define DEFAULT_DECODE_BUF_SIZE 1024
#define MAX_STACK_BUF 2048

/* 分支预测提示 */
#if defined(__GNUC__) || defined(__clang__)
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x)   (x)
#define UNLIKELY(x) (x)
#endif

/* 字符属性位掩码 */
#define CHAR_SEPARATOR   0x01  /* & 分隔符 */
#define CHAR_EQUAL       0x02  /* = 等号 */
#define CHAR_PERCENT     0x04  /* % 百分号 */
#define CHAR_PLUS        0x08  /* + 加号 */
#define CHAR_HEX         0x10  /* 十六进制字符 0-9A-Fa-f */
#define CHAR_SPACE       0x20  /* 空白字符 */
#define CHAR_UPPER       0x40  /* 大写字母 A-Z */
#define CHAR_ALPHA       0x80  /* 字母字符 (A-Z 和 a-z 都设置此位) */

/* 内部结构体，用于管理内存分配器 */
typedef struct llquery_internal {
  llquery_alloc_fn alloc_fn;
  llquery_free_fn free_fn;
  void *alloc_data;
  bool use_custom_alloc;
  bool decode_buffer_owned;
  char *string_pool;         /* 字符串内存池 */
  size_t string_pool_size;   /* 内存池总大小 */
  size_t string_pool_used;   /* 已使用的大小 */
  bool string_pool_owned;    /* 是否拥有内存池 */
} llquery_internal_t;

/* 字符分类查找表：使用位掩码实现零分支字符检查 */
static const unsigned char char_flags[256] = {
  /* 0x00-0x08 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0x08-0x0F */ 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00,
  /* 0x10-0x17 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0x18-0x1F */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0x20-0x27 */ 0x20, 0x00, 0x00, 0x00, 0x00, 0x04, 0x01, 0x00,
  /* 0x28-0x2F */ 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00,
  /* 0x30-0x37 */ 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
  /* 0x38-0x3F */ 0x10, 0x10, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
  /* 0x40-0x47 */ 0x00, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xD0, 0xC0,
  /* 0x48-0x4F */ 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
  /* 0x50-0x57 */ 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0, 0xC0,
  /* 0x58-0x5F */ 0xC0, 0xC0, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0x60-0x67 */ 0x00, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x80,
  /* 0x68-0x6F */ 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  /* 0x70-0x77 */ 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
  /* 0x78-0x7F */ 0x80, 0x80, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00,
  /* 0x80-0xFF */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/* 十六进制字符查找表 */
static const signed char HEX_LOOKUP[256] = {
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,
  -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,10,11,12,13,14,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
};

/* 宏定义：零分支字符检查 */
#define IS_SEPARATOR(c)  (char_flags[(unsigned char)(c)] & CHAR_SEPARATOR)
#define IS_EQUAL(c)      (char_flags[(unsigned char)(c)] & CHAR_EQUAL)
#define IS_ENCODED(c)    (char_flags[(unsigned char)(c)] & (CHAR_PERCENT | CHAR_PLUS))
#define IS_HEX_DIGIT(c)  (char_flags[(unsigned char)(c)] & CHAR_HEX)
#define IS_SPACE(c)      (char_flags[(unsigned char)(c)] & CHAR_SPACE)
#define IS_UPPER(c)      (char_flags[(unsigned char)(c)] & CHAR_UPPER)
#define IS_ALPHA(c)      (char_flags[(unsigned char)(c)] & CHAR_ALPHA)
#define IS_ALNUM(c)      (char_flags[(unsigned char)(c)] & (CHAR_HEX | CHAR_ALPHA))  /* 0-9 A-Z a-z */

/* ASCII 转换常量 */
#define ASCII_CASE_OFFSET 32  /* 'A' to 'a' offset */

/* 默认内存分配器 */
static void *default_alloc(size_t size, void *user_data) {
  (void)user_data;
  if (size == 0) return NULL;
  return malloc(size);
}

static void default_free(void *ptr, void *user_data) {
  (void)user_data;
  if (ptr) free(ptr);
}

/* 内部辅助函数 */

static llquery_internal_t *get_internal(struct llquery *q) {
  return (llquery_internal_t *)q->_reserved;
}

static bool has_encoded_chars(const char *str, size_t len) {
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if (UNLIKELY(IS_ENCODED(c))) {
      return true;
    }
  }
  return false;
}

/* 从内存池分配字符串 */
static char* pool_alloc_string(llquery_internal_t *internal, size_t size) {
  if (UNLIKELY(!internal->string_pool)) {
    // 没有内存池，使用常规分配
    return internal->alloc_fn(size, internal->alloc_data);
  }
  
  // 检查内存池是否有足够空间
  if (internal->string_pool_used + size <= internal->string_pool_size) {
    char *ptr = internal->string_pool + internal->string_pool_used;
    internal->string_pool_used += size;
    return ptr;
  }
  
  // 内存池空间不足，使用常规分配
  // 注意：这会导致混合分配，需要在free时检查地址
  return internal->alloc_fn(size, internal->alloc_data);
}

/* 估算需要的总字符串大小 */
static size_t estimate_string_size(const char *query __attribute__((unused)), size_t len) {
  // 估算：每个字符最多需要2字节（\0终止符），加上一些缓冲
  // 实际大小通常小于查询字符串长度的2倍
  return len * 2 + 256;  // 额外的256字节缓冲
}

static size_t decode_in_place(char *str) {
  if (UNLIKELY(!str)) return 0;

  char *src = str;
  char *dst = str;

  while (*src) {
    unsigned char c = (unsigned char)*src;

    if (UNLIKELY(c == '+')) {
      *dst++ = ' ';
      src++;
    } else if (UNLIKELY(c == '%' && src[1] && src[2])) {
      int h1 = HEX_LOOKUP[(unsigned char)src[1]];
      int h2 = HEX_LOOKUP[(unsigned char)src[2]];

      if (LIKELY(h1 >= 0 && h2 >= 0)) {
        *dst++ = (char)((h1 << 4) | h2);
        src += 3;
      } else {
        // 无效的百分号编码，保留原字符
        *dst++ = *src++;
      }
    } else {
      *dst++ = *src++;
    }
  }

  *dst = '\0';
  return (size_t)(dst - str);
}

static char* trim_string(char *str, size_t *len) {
  if (UNLIKELY(!str || *len == 0)) return str;

  // 去除前导空白
  char *start = str;
  while (*start && IS_SPACE(*start)) {
    start++;
  }

  // 去除尾随空白
  char *end = str + *len - 1;
  while (end >= start && IS_SPACE(*end)) {
    end--;
  }

  size_t new_len = (size_t)(end - start + 1);
  memmove(str, start, new_len);
  str[new_len] = '\0';
  *len = new_len;
  return str;
}

static int compare_keys(const char *a, size_t a_len, const char *b, size_t b_len) {
  size_t min_len = a_len < b_len ? a_len : b_len;
  int cmp = memcmp(a, b, min_len);
  if (cmp != 0) return cmp;
  return (int)a_len - (int)b_len;
}

static void lowercase_string(char *str, size_t len) {
  for (size_t i = 0; i < len; i++) {
    unsigned char c = (unsigned char)str[i];
    if (IS_UPPER(c)) {
      str[i] = (char)(c + ASCII_CASE_OFFSET);  /* Convert A-Z to a-z */
    }
  }
}

/* 公共API实现 */

enum llquery_error llquery_init(struct llquery *q,
                                uint16_t max_pairs,
                                uint16_t flags) {
  return llquery_init_ex(q, max_pairs, flags,
                         default_alloc, default_free, NULL);
}

enum llquery_error llquery_init_ex(struct llquery *q,
                                   uint16_t max_pairs,
                                   uint16_t flags,
                                   llquery_alloc_fn alloc_fn,
                                   llquery_free_fn free_fn,
                                   void *alloc_data) {
  if (!q || !alloc_fn || !free_fn) {
    return LQE_NULL_INPUT;
  }

  memset(q, 0, sizeof(struct llquery));

  // 设置默认值
  if (max_pairs == 0) {
    max_pairs = DEFAULT_MAX_PAIRS;
  }

  // 分配内部结构
  llquery_internal_t *internal = alloc_fn(sizeof(llquery_internal_t), alloc_data);
  if (!internal) {
    return LQE_MEMORY_ERROR;
  }

  internal->alloc_fn = alloc_fn;
  internal->free_fn = free_fn;
  internal->alloc_data = alloc_data;
  internal->use_custom_alloc = true;
  internal->decode_buffer_owned = false;
  internal->string_pool = NULL;
  internal->string_pool_size = 0;
  internal->string_pool_used = 0;
  internal->string_pool_owned = false;

  // 分配键值对数组
  struct llquery_kv *kv_pairs = alloc_fn(sizeof(struct llquery_kv) * max_pairs, alloc_data);
  if (!kv_pairs) {
    free_fn(internal, alloc_data);
    return LQE_MEMORY_ERROR;
  }

  memset(kv_pairs, 0, sizeof(struct llquery_kv) * max_pairs);

  q->kv_pairs = kv_pairs;
  q->max_kv_count = max_pairs;
  q->flags = flags;
  q->_reserved = internal;

  return LQE_OK;
}

enum llquery_error llquery_parse(const char *query,
                                 size_t query_len,
                                 struct llquery *q) {
  return llquery_parse_ex(query, query_len, q, NULL, 0);
}

enum llquery_error llquery_parse_ex(const char *query,
                                    size_t query_len,
                                    struct llquery *q,
                                    char *decode_buf,
                                    size_t decode_buf_size) {
  if (!query || !q || !q->_reserved) {
    return LQE_NULL_INPUT;
  }

  // 获取字符串长度
  if (query_len == 0) {
    query_len = strlen(query);
  }
  if (query_len == 0) {
    return LQE_EMPTY_STRING;
  }

  // 重置解析器
  llquery_reset(q);

  // 跳过前导'?'
  const char *work_query = query;
  if (*work_query == '?') {
    work_query++;
    query_len--;
    if (query_len == 0) {
      return LQE_OK;
    }
  }

  // 获取内部结构
  llquery_internal_t *internal = get_internal(q);

  // 判断是否需要解码
  bool needs_decode = (q->flags & LQF_AUTO_DECODE) != 0;
  bool has_encoded = needs_decode && has_encoded_chars(work_query, query_len);

  // 准备工作指针
  const char *current = work_query;
  const char *end = work_query + query_len;

  // 处理解码
  if (has_encoded) {
    if (decode_buf && decode_buf_size > 0) {
      // 使用外部提供的缓冲区
      if (decode_buf_size < query_len + 1) {
        return LQE_BUFFER_TOO_SMALL;
      }
      memcpy(decode_buf, work_query, query_len);
      decode_buf[query_len] = '\0';
      size_t decoded_len = decode_in_place(decode_buf);
      current = decode_buf;
      end = decode_buf + decoded_len;
      q->decode_buffer = decode_buf;
      q->decode_buffer_size = decode_buf_size;
    } else {
      // 分配解码缓冲区
      char *buf = internal->alloc_fn(query_len + 1, internal->alloc_data);
      if (!buf) {
        return LQE_MEMORY_ERROR;
      }
      memcpy(buf, work_query, query_len);
      buf[query_len] = '\0';
      size_t decoded_len = decode_in_place(buf);
      current = buf;
      end = buf + decoded_len;
      q->decode_buffer = buf;
      q->decode_buffer_size = query_len + 1;
      internal->decode_buffer_owned = true;
    }
  }

  // 阶段5优化：预分配字符串内存池以减少分配次数
  size_t estimated_pool_size = estimate_string_size(work_query, query_len);
  char *string_pool = internal->alloc_fn(estimated_pool_size, internal->alloc_data);
  if (LIKELY(string_pool != NULL)) {
    internal->string_pool = string_pool;
    internal->string_pool_size = estimated_pool_size;
    internal->string_pool_used = 0;
    internal->string_pool_owned = true;
  }

  // 主解析循环 - 优化版本使用批量处理
  uint16_t kv_index = 0;

  while (LIKELY(current < end && kv_index < q->max_kv_count)) {
    // 跳过前导'&'
    while (LIKELY(current < end) && IS_SEPARATOR(*current)) current++;
    if (UNLIKELY(current >= end)) break;

    const char* key_start = current;
    
    // 批量查找 key 结束位置（'=' 或 '&'）
    const char *key_end = current;
    while (LIKELY(key_end < end) && !IS_EQUAL(*key_end) && !IS_SEPARATOR(*key_end)) {
      key_end++;
    }
    current = key_end;

    const char *value_start = NULL;
    const char *value_end = NULL;

    if (LIKELY(current < end) && IS_EQUAL(*current)) {
      // 有值
      current++;
      value_start = current;
      
      // 批量查找值结束位置（'&'）
      const char *amp = (const char *)memchr(current, '&', end - current);
      if (amp) {
        value_end = amp;
        current = amp;
      } else {
        value_end = end;
        current = end;
      }
    } else {
      // 无值
      value_start = value_end = current;
    }

    // 跳过空 key
    if (UNLIKELY(key_end - key_start == 0)) {
      if (current < end && IS_SEPARATOR(*current)) current++;
      continue;
    }

    // 存储 kv
    struct llquery_kv *kv = &q->kv_pairs[kv_index];
    // 先计算长度
    kv->key_len = (size_t)(key_end - key_start);
    kv->value_len = (size_t)(value_end - value_start);
    
    // 阶段5优化：使用内存池分配 key 和 value
    char *key_buf = pool_alloc_string(internal, kv->key_len + 1);
    if (UNLIKELY(!key_buf)) {
      // 设置当前已成功解析的数量，然后返回错误
      q->kv_count = kv_index;
      return LQE_MEMORY_ERROR;
    }
    memcpy(key_buf, key_start, kv->key_len);
    key_buf[kv->key_len] = '\0';
    kv->key = key_buf;
    
    char *val_buf = pool_alloc_string(internal, kv->value_len + 1);
    if (UNLIKELY(!val_buf)) {
      // 释放key_buf（如果不在池中）
      bool key_in_pool = (internal->string_pool && 
                          key_buf >= internal->string_pool &&
                          key_buf < internal->string_pool + internal->string_pool_size);
      if (!key_in_pool) {
        internal->free_fn(key_buf, internal->alloc_data);
      }
      q->kv_count = kv_index;
      return LQE_MEMORY_ERROR;
    }
    memcpy(val_buf, value_start, kv->value_len);
    val_buf[kv->value_len] = '\0';
    kv->value = val_buf;
    kv->is_encoded = has_encoded;

    if (UNLIKELY(q->flags & LQF_LOWERCASE_KEYS))
      lowercase_string((char *)kv->key, kv->key_len);
    if (UNLIKELY(q->flags & LQF_TRIM_VALUES))
      kv->value = trim_string((char *)kv->value, &kv->value_len);

    // 检查是否保留空值
    if (UNLIKELY(!(q->flags & LQF_KEEP_EMPTY) && kv->value_len == 0)) {
      // 检查字符串是否在内存池中，只释放不在池中的
      bool key_in_pool = (internal->string_pool && 
                          key_buf >= internal->string_pool &&
                          key_buf < internal->string_pool + internal->string_pool_size);
      bool val_in_pool = (internal->string_pool && 
                          val_buf >= internal->string_pool &&
                          val_buf < internal->string_pool + internal->string_pool_size);
      if (!key_in_pool) {
        internal->free_fn(key_buf, internal->alloc_data);
      }
      if (!val_in_pool) {
        internal->free_fn(val_buf, internal->alloc_data);
      }
      if (current < end && IS_SEPARATOR(*current)) current++;
      continue;
    }

    kv_index++;
    if (current < end && IS_SEPARATOR(*current)) current++;
  }

  q->kv_count = kv_index;
  q->field_set = 0xFF; // 设置所有字段

  // 检查是否超过限制
  if (UNLIKELY(current < end && kv_index >= q->max_kv_count)) {
    if (q->flags & LQF_STRICT) {
      return LQE_TOO_MANY_PAIRS;
    }
  }

  return LQE_OK;
}

void llquery_free(struct llquery *q) {
  if (!q || !q->_reserved) {
    return;
  }

  llquery_internal_t *internal = get_internal(q);

  // 阶段5优化：释放字符串
  if (q->kv_pairs) {
    for (uint16_t i = 0; i < q->kv_count; i++) {
      // 检查key是否需要释放
      if (q->kv_pairs[i].key) {
        // 如果有内存池，检查指针是否在池中
        bool in_pool = (internal->string_pool && 
                        q->kv_pairs[i].key >= internal->string_pool &&
                        q->kv_pairs[i].key < internal->string_pool + internal->string_pool_size);
        if (!in_pool) {
          internal->free_fn((void *)q->kv_pairs[i].key, internal->alloc_data);
        }
      }
      // 检查value是否需要释放
      if (q->kv_pairs[i].value) {
        bool in_pool = (internal->string_pool && 
                        q->kv_pairs[i].value >= internal->string_pool &&
                        q->kv_pairs[i].value < internal->string_pool + internal->string_pool_size);
        if (!in_pool) {
          internal->free_fn((void *)q->kv_pairs[i].value, internal->alloc_data);
        }
      }
    }
  }
  
  // 释放内存池（如果拥有）
  if (internal->string_pool_owned && internal->string_pool) {
    internal->free_fn(internal->string_pool, internal->alloc_data);
  }
  
  // 释放键值对数组
  if (q->kv_pairs) {
    internal->free_fn(q->kv_pairs, internal->alloc_data);
  }

  // 释放解码缓冲区
  if (q->decode_buffer && internal->decode_buffer_owned) {
    internal->free_fn((void *)q->decode_buffer, internal->alloc_data);
  }

  // 释放内部结构
  internal->free_fn(internal, internal->alloc_data);

  // 清零结构体
  memset(q, 0, sizeof(struct llquery));
}

uint16_t llquery_count(const struct llquery *q) {
  return q ? q->kv_count : 0;
}

const struct llquery_kv *llquery_get_kv(const struct llquery *q,
                                        uint16_t index) {
  if (!q || index >= q->kv_count) {
    return NULL;
  }
  return &q->kv_pairs[index];
}

const char *llquery_get_value(const struct llquery *q,
                              const char *key,
                              size_t key_len) {
  if (!q || !key) {
    return NULL;
  }

  if (key_len == 0) {
    key_len = strlen(key);
  }

  for (uint16_t i = 0; i < q->kv_count; i++) {
    const struct llquery_kv *kv = &q->kv_pairs[i];
    if (kv->key_len == key_len &&
      memcmp(kv->key, key, key_len) == 0) {
      return kv->value_len > 0 ? kv->value : "";
    }
  }

  return NULL;
}

uint16_t llquery_get_all_values(const struct llquery *q,
                                const char *key,
                                size_t key_len,
                                const char **values,
                                uint16_t max_values) {
  if (!q || !key || !values || max_values == 0) {
    return 0;
  }

  if (key_len == 0) {
    key_len = strlen(key);
  }

  uint16_t count = 0;
  for (uint16_t i = 0; i < q->kv_count && count < max_values; i++) {
    const struct llquery_kv *kv = &q->kv_pairs[i];
    if (kv->key_len == key_len &&
      memcmp(kv->key, key, key_len) == 0) {
      values[count++] = kv->value_len > 0 ? kv->value : "";
    }
  }

  return count;
}

bool llquery_has_key(const struct llquery *q,
                     const char *key,
                     size_t key_len) {
  if (!q || !key) {
    return false;
  }

  if (key_len == 0) {
    key_len = strlen(key);
  }

  for (uint16_t i = 0; i < q->kv_count; i++) {
    const struct llquery_kv *kv = &q->kv_pairs[i];
    if (kv->key_len == key_len &&
      memcmp(kv->key, key, key_len) == 0) {
      return true;
    }
  }

  return false;
}

uint16_t llquery_iterate(const struct llquery *q,
                         llquery_iter_cb callback,
                         void *user_data) {
  if (!q || !callback) {
    return 0;
  }

  uint16_t count = 0;
  for (uint16_t i = 0; i < q->kv_count; i++) {
    if (callback(&q->kv_pairs[i], user_data) != 0) {
      break;
    }
    count++;
  }

  return count;
}

enum llquery_error llquery_sort(struct llquery *q,
                                llquery_compare_cb compare_fn) {
  if (!q || q->kv_count < 2) {
    return LQE_OK;
  }

  // 简单的冒泡排序（对于小数据集足够）
  for (uint16_t i = 0; i < q->kv_count - 1; i++) {
    for (uint16_t j = 0; j < q->kv_count - i - 1; j++) {
      struct llquery_kv *a = &q->kv_pairs[j];
      struct llquery_kv *b = &q->kv_pairs[j + 1];

      int cmp;
      if (compare_fn) {
        cmp = compare_fn(a, b);
      } else {
        cmp = compare_keys(a->key, a->key_len, b->key, b->key_len);
      }

      if (cmp > 0) {
        // 交换
        struct llquery_kv temp = *a;
        *a = *b;
        *b = temp;
      }
    }
  }

  return LQE_OK;
}

uint16_t llquery_filter(struct llquery *q,
                        bool (*filter_fn)(const struct llquery_kv *kv, void *user_data),
                        void *user_data) {
  if (!q || !filter_fn) {
    return 0;
  }

  llquery_internal_t *internal = get_internal(q);
  uint16_t write_idx = 0;
  
  for (uint16_t read_idx = 0; read_idx < q->kv_count; read_idx++) {
    if (filter_fn(&q->kv_pairs[read_idx], user_data)) {
      // Keep this item
      if (write_idx != read_idx) {
        // Before overwriting write_idx position, free its old content if it hasn't been moved yet
        // We know it hasn't been moved if write_idx < read_idx and we haven't processed it yet
        // Actually, anything at write_idx that we're about to overwrite needs to be checked:
        // If the item at write_idx was already copied forward, its pointers are now at an earlier position
        // If the item at write_idx was filtered out, it was already freed
        // If the item at write_idx was kept but we're now overwriting it, we need to free it
        
        // The key insight: positions < write_idx have been finalized (either copied to or freed)
        // Position write_idx might still have old data that needs freeing
        // But ONLY if read_idx is far enough ahead that write_idx hasn't been processed yet
        
        // Simple solution: always free what's at write_idx before overwriting if it's different
        // But we must be careful not to double-free
        
        // Actually, let's use a different approach: mark positions as we move them
        // Or simpler: only free items that are definitely filtered out
        
        // New approach: track that position read_idx data is being moved
        // When we move data FROM read_idx TO write_idx, the write_idx position loses its data
        // We need to free write_idx's old data IFF it wasn't already moved elsewhere
        
        // Safest approach: mark pointers as NULL after moving
        q->kv_pairs[write_idx] = q->kv_pairs[read_idx];
        // Clear the source to prevent double-free
        q->kv_pairs[read_idx].key = NULL;
        q->kv_pairs[read_idx].value = NULL;
      }
      write_idx++;
    } else {
      // Free the filtered-out key/value pair
      // 检查是否在内存池中
      if (q->kv_pairs[read_idx].key) {
        bool in_pool = (internal->string_pool && 
                        q->kv_pairs[read_idx].key >= internal->string_pool &&
                        q->kv_pairs[read_idx].key < internal->string_pool + internal->string_pool_size);
        if (!in_pool) {
          internal->free_fn((void *)q->kv_pairs[read_idx].key, internal->alloc_data);
        }
        q->kv_pairs[read_idx].key = NULL;
      }
      if (q->kv_pairs[read_idx].value) {
        bool in_pool = (internal->string_pool && 
                        q->kv_pairs[read_idx].value >= internal->string_pool &&
                        q->kv_pairs[read_idx].value < internal->string_pool + internal->string_pool_size);
        if (!in_pool) {
          internal->free_fn((void *)q->kv_pairs[read_idx].value, internal->alloc_data);
        }
        q->kv_pairs[read_idx].value = NULL;
      }
    }
  }
  
  // Free any remaining items beyond write_idx that weren't freed yet
  for (uint16_t i = write_idx; i < q->kv_count; i++) {
    if (q->kv_pairs[i].key) {
      // 检查是否在内存池中
      bool in_pool = (internal->string_pool && 
                      q->kv_pairs[i].key >= internal->string_pool &&
                      q->kv_pairs[i].key < internal->string_pool + internal->string_pool_size);
      if (!in_pool) {
        internal->free_fn((void *)q->kv_pairs[i].key, internal->alloc_data);
      }
      q->kv_pairs[i].key = NULL;
    }
    if (q->kv_pairs[i].value) {
      bool in_pool = (internal->string_pool && 
                      q->kv_pairs[i].value >= internal->string_pool &&
                      q->kv_pairs[i].value < internal->string_pool + internal->string_pool_size);
      if (!in_pool) {
        internal->free_fn((void *)q->kv_pairs[i].value, internal->alloc_data);
      }
      q->kv_pairs[i].value = NULL;
    }
  }

  q->kv_count = write_idx;
  return write_idx;
}

size_t llquery_stringify(const struct llquery *q,
                         char *buffer,
                         size_t buffer_size,
                         bool encode) {
  // Note: encode parameter reserved for future URL encoding support
  (void)encode;  // Suppress unused parameter warning
  
  if (!q || q->kv_count == 0) {
    if (buffer && buffer_size > 0) {
      buffer[0] = '\0';
    }
    return 0;
  }

  size_t needed = 0;

  // 计算所需空间
  for (uint16_t i = 0; i < q->kv_count; i++) {
    const struct llquery_kv *kv = &q->kv_pairs[i];
    if (i > 0) needed++; // '&'
    needed += kv->key_len;
    needed++; // '='
    needed += kv->value_len;
  }

  needed++; // 终止符

  // 如果只计算大小，返回所需空间
  if (!buffer || buffer_size == 0) {
    return needed - 1; // 不包括终止符
  }

  // 检查缓冲区是否足够
  if (buffer_size < needed) {
    return needed - 1;
  }

  // 格式化字符串
  char *pos = buffer;
  for (uint16_t i = 0; i < q->kv_count; i++) {
    const struct llquery_kv *kv = &q->kv_pairs[i];

    if (i > 0) {
      *pos++ = '&';
    }

    // 复制键
    if (kv->key_len > 0) {
      memcpy(pos, kv->key, kv->key_len);
      pos += kv->key_len;
    }

    *pos++ = '=';

    // 复制值
    if (kv->value_len > 0) {
      memcpy(pos, kv->value, kv->value_len);
      pos += kv->value_len;
    }
  }

  *pos = '\0';
  return (size_t)(pos - buffer);
}

enum llquery_error llquery_clone(struct llquery *dst,
                                 const struct llquery *src) {
  if (!dst || !src) {
    return LQE_NULL_INPUT;
  }

  // 初始化目标
  enum llquery_error err = llquery_init(dst, src->max_kv_count, src->flags);
  if (err != LQE_OK) {
    return err;
  }

  llquery_internal_t *internal = get_internal(dst);

  // 复制基本字段
  dst->field_set = src->field_set;
  dst->kv_count = src->kv_count;

  // 深拷贝键值对
  for (uint16_t i = 0; i < src->kv_count; i++) {
    const struct llquery_kv *src_kv = &src->kv_pairs[i];
    struct llquery_kv *dst_kv = &dst->kv_pairs[i];

    // 复制 key
    char *key_buf = internal->alloc_fn(src_kv->key_len + 1, internal->alloc_data);
    if (!key_buf) {
      llquery_free(dst);
      return LQE_MEMORY_ERROR;
    }
    memcpy(key_buf, src_kv->key, src_kv->key_len);
    key_buf[src_kv->key_len] = '\0';
    dst_kv->key = key_buf;
    dst_kv->key_len = src_kv->key_len;

    // 复制 value
    char *val_buf = internal->alloc_fn(src_kv->value_len + 1, internal->alloc_data);
    if (!val_buf) {
      llquery_free(dst);
      return LQE_MEMORY_ERROR;
    }
    memcpy(val_buf, src_kv->value, src_kv->value_len);
    val_buf[src_kv->value_len] = '\0';
    dst_kv->value = val_buf;
    dst_kv->value_len = src_kv->value_len;
    dst_kv->is_encoded = src_kv->is_encoded;
  }

  // 如果需要，复制解码缓冲区
  if (src->decode_buffer && src->decode_buffer_size > 0) {
    char *buf = internal->alloc_fn(src->decode_buffer_size, internal->alloc_data);
    if (!buf) {
      llquery_free(dst);
      return LQE_MEMORY_ERROR;
    }
    memcpy(buf, src->decode_buffer, src->decode_buffer_size);
    dst->decode_buffer = buf;
    dst->decode_buffer_size = src->decode_buffer_size;
    internal->decode_buffer_owned = true;
  }

  return LQE_OK;
}

void llquery_reset(struct llquery *q) {
  if (!q) return;

  llquery_internal_t *internal = get_internal(q);
  
  // 阶段5优化：释放所有字符串（检查是否在内存池中）
  if (q->kv_pairs) {
    for (uint16_t i = 0; i < q->kv_count; i++) {
      if (q->kv_pairs[i].key) {
        // 检查是否在内存池中
        bool in_pool = (internal->string_pool && 
                        q->kv_pairs[i].key >= internal->string_pool &&
                        q->kv_pairs[i].key < internal->string_pool + internal->string_pool_size);
        if (!in_pool) {
          internal->free_fn((void *)q->kv_pairs[i].key, internal->alloc_data);
        }
        q->kv_pairs[i].key = NULL;
      }
      if (q->kv_pairs[i].value) {
        bool in_pool = (internal->string_pool && 
                        q->kv_pairs[i].value >= internal->string_pool &&
                        q->kv_pairs[i].value < internal->string_pool + internal->string_pool_size);
        if (!in_pool) {
          internal->free_fn((void *)q->kv_pairs[i].value, internal->alloc_data);
        }
        q->kv_pairs[i].value = NULL;
      }
    }
  }
  
  // 释放内存池
  if (internal->string_pool_owned && internal->string_pool) {
    internal->free_fn(internal->string_pool, internal->alloc_data);
    internal->string_pool = NULL;
    internal->string_pool_size = 0;
    internal->string_pool_used = 0;
    internal->string_pool_owned = false;
  }

  // 重置计数
  q->kv_count = 0;
  q->field_set = 0;

  // 重置解码缓冲区（不释放内存）
  if (q->decode_buffer) {
    if (internal->decode_buffer_owned) {
      internal->free_fn((void *)q->decode_buffer, internal->alloc_data);
      q->decode_buffer = NULL;
      q->decode_buffer_size = 0;
      internal->decode_buffer_owned = false;
    } else {
      q->decode_buffer = NULL;
      q->decode_buffer_size = 0;
    }
  }
}

void llquery_set_allocator(struct llquery *q,
                           llquery_alloc_fn alloc_fn,
                           llquery_free_fn free_fn,
                           void *alloc_data) {
  if (!q || !alloc_fn || !free_fn) {
    return;
  }

  llquery_internal_t *internal = get_internal(q);
  if (internal) {
    // 使用新的分配器重新分配键值对数组
    if (q->kv_pairs) {
      size_t size = sizeof(struct llquery_kv) * q->max_kv_count;
      struct llquery_kv *new_pairs = alloc_fn(size, alloc_data);
      if (new_pairs) {
        memcpy(new_pairs, q->kv_pairs, size);
        internal->free_fn(q->kv_pairs, internal->alloc_data);
        q->kv_pairs = new_pairs;
      }
    }

    // 更新内部结构
    internal->alloc_fn = alloc_fn;
    internal->free_fn = free_fn;
    internal->alloc_data = alloc_data;
    internal->use_custom_alloc = true;
  }
}

const char *llquery_strerror(enum llquery_error error) {
  switch (error) {
    case LQE_OK: return "Success";
    case LQE_NULL_INPUT: return "Null input";
    case LQE_EMPTY_STRING: return "Empty string";
    case LQE_INVALID_HEX: return "Invalid hex encoding";
    case LQE_BUFFER_TOO_SMALL: return "Buffer too small";
    case LQE_MEMORY_ERROR: return "Memory allocation error";
    case LQE_TOO_MANY_PAIRS: return "Too many key-value pairs";
    case LQE_INVALID_FORMAT: return "Invalid query format";
    case LQE_INTERNAL_ERROR: return "Internal error";
    default: return "Unknown error";
  }
}

size_t llquery_url_encode(const char *input,
                          size_t input_len,
                          char *output,
                          size_t output_size) {
  if (!input) return 0;

  if (input_len == 0) {
    input_len = strlen(input);
  }

  const char hex[] = "0123456789ABCDEF";
  const char *unreserved = "-_.~"; // RFC 3986未保留字符

  size_t needed = 0;

  // 计算所需空间
  for (size_t i = 0; i < input_len; i++) {
    unsigned char c = (unsigned char)input[i];
    if (IS_ALNUM(c) || strchr(unreserved, c)) {
      needed++;
    } else {
      needed += 3; // %XX
    }
  }

  needed++; // 终止符

  // 如果只计算大小
  if (!output || output_size == 0) {
    return needed - 1;
  }

  // 检查缓冲区
  if (output_size < needed) {
    return needed - 1;
  }

  // 编码
  char *pos = output;
  for (size_t i = 0; i < input_len; i++) {
    unsigned char c = (unsigned char)input[i];
    if (IS_ALNUM(c) || strchr(unreserved, c)) {
      *pos++ = (char)c;
    } else if (c == ' ') {
      *pos++ = '+';
    } else {
      *pos++ = '%';
      *pos++ = hex[c >> 4];
      *pos++ = hex[c & 0x0F];
    }
  }

  *pos = '\0';
  return (size_t)(pos - output);
}

size_t llquery_url_decode(const char *input,
                          size_t input_len,
                          char *output,
                          size_t output_size) {
  if (!input) return 0;

  if (input_len == 0) {
    input_len = strlen(input);
  }

  size_t needed = 0;
  const char *src = input;
  const char *end = input + input_len;

  // 计算所需空间
  while (src < end) {
    if (*src == '%' && src + 2 < end) {
      int h1 = HEX_LOOKUP[(unsigned char)src[1]];
      int h2 = HEX_LOOKUP[(unsigned char)src[2]];
      if (h1 >= 0 && h2 >= 0) {
        needed++;
        src += 3;
        continue;
      }
    } else if (*src == '+') {
      needed++;
      src++;
      continue;
    }
    needed++;
    src++;
  }

  needed++; // 终止符

  // 如果只计算大小
  if (!output || output_size == 0) {
    return needed - 1;
  }

  // 检查缓冲区
  if (output_size < needed) {
    return needed - 1;
  }

  // 解码
  src = input;
  char *dst = output;

  while (src < end) {
    if (*src == '%' && src + 2 < end) {
      int h1 = HEX_LOOKUP[(unsigned char)src[1]];
      int h2 = HEX_LOOKUP[(unsigned char)src[2]];
      if (h1 >= 0 && h2 >= 0) {
        *dst++ = (char)((h1 << 4) | h2);
        src += 3;
      } else {
        *dst++ = *src++;
      }
    } else if (*src == '+') {
      *dst++ = ' ';
      src++;
    } else {
      *dst++ = *src++;
    }
  }

  *dst = '\0';
  return (size_t)(dst - output);
}

uint16_t llquery_parse_fast(const char *query,
                            size_t query_len,
                            struct llquery_kv *kv_pairs,
                            uint16_t max_pairs,
                            uint16_t flags) {
  if (!query || !kv_pairs || max_pairs == 0) {
    return 0;
  }

  if (query_len == 0) {
    query_len = strlen(query);
  }
  if (query_len == 0) {
    return 0;
  }

  // 跳过前导'?'
  if (*query == '?') {
    query++;
    query_len--;
  }

  // 使用栈缓冲区处理解码
  char stack_buf[MAX_STACK_BUF];
  const char *work_query = query;

  bool needs_decode = (flags & LQF_AUTO_DECODE) != 0;
  bool has_encoded = needs_decode && has_encoded_chars(query, query_len);

  if (has_encoded) {
    if (query_len < MAX_STACK_BUF) {
      // 使用栈缓冲区
      memcpy(stack_buf, query, query_len);
      stack_buf[query_len] = '\0';
      decode_in_place(stack_buf);
      work_query = stack_buf;
    } else {
      // 需要堆分配，但快速函数不支持
      return 0;
    }
  }

  // 解析
  const char *current = work_query;
  const char *end = work_query + query_len;
  uint16_t count = 0;

  while (current < end && count < max_pairs) {
    // 跳过'&'
    while (current < end && *current == '&') {
      current++;
    }
    if (current >= end) break;

    const char *key_start = current;

    // 查找键结束
    while (current < end && *current != '=' && *current != '&') {
      current++;
    }

    if (current < end && *current == '=') {
      // 有值
      const char *key_end = current;
      const char *value_start = current + 1;

      // 查找值结束
      current = value_start;
      while (current < end && *current != '&') {
        current++;
      }

      kv_pairs[count].key = key_start;
      kv_pairs[count].key_len = (size_t)(key_end - key_start);
      kv_pairs[count].value = value_start;
      kv_pairs[count].value_len = (size_t)(current - value_start);
      kv_pairs[count].is_encoded = has_encoded;

      count++;

      if (current < end && *current == '&') {
        current++;
      }
    } else {
      // 无值
      kv_pairs[count].key = key_start;
      kv_pairs[count].key_len = (size_t)(current - key_start);
      kv_pairs[count].value = "";
      kv_pairs[count].value_len = 0;
      kv_pairs[count].is_encoded = has_encoded;

      count++;

      if (current < end && *current == '&') {
        current++;
      }
    }
  }

  return count;
}

bool llquery_is_valid(const char *str, size_t len) {
  if (!str) {
    return false;
  }

  if (len == 0) {
    len = strlen(str);
  }
  
  if (len == 0) {
    return false;
  }

  // 跳过前导'?'
  if (*str == '?') {
    str++;
    len--;
  }

  if (len == 0) {
    return true; // 空查询字符串有效
  }

  // 基本格式检查：至少包含一个键值对或键
  for (size_t i = 0; i < len; i++) {
    char c = str[i];
    // 允许的字符：字母数字、-_.~、% (用于编码)、+、=、&
    if (!IS_ALNUM((unsigned char)c) &&
      c != '-' && c != '_' && c != '.' && c != '~' &&
      c != '%' && c != '+' && c != '=' && c != '&') {
      return false;
    }
  }

  return true;
}

uint16_t llquery_count_pairs(const char *query, size_t query_len) {
  if (!query) {
    return 0;
  }

  if (query_len == 0) {
    query_len = strlen(query);
  }
  
  if (query_len == 0) {
    return 0;
  }

  // 跳过前导'?'
  if (*query == '?') {
    query++;
    query_len--;
  }

  if (query_len == 0) {
    return 0;
  }

  uint16_t count = 0;
  const char *p = query;
  const char *end = query + query_len;

  while (p < end) {
    // 跳过'&'
    while (p < end && *p == '&') {
      p++;
    }
    if (p >= end) break;

    // 找到下一个'&'
    while (p < end && *p != '&') {
      p++;
    }

    count++;

    if (p < end && *p == '&') {
      p++;
    }
  }

  return count;
}
