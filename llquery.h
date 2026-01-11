#ifndef LLQUERY_H
#define LLQUERY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/**
 * @defgroup llquery 查询字符串解析库
 *
 * 提供高性能、零拷贝的URL查询字符串解析功能。
 * 支持自动URL解码、重复键合并、多种输出格式。
 *
 * 特性：
 * - 零拷贝设计：直接在原始字符串上操作
 * - 高性能：单次遍历完成解析和解码
 * - 线程安全：无全局状态，可重入
 * - 内存安全：严格的边界检查
 * - 灵活配置：支持多种解析模式
 *
 * @{
 */

/* 查询字符串解析配置选项 */
enum llquery_option_flags {
    LQF_NONE             = 0,      /**< 默认选项 */
    LQF_AUTO_DECODE      = 1 << 0, /**< 自动URL解码（处理%XX和+） */
    LQF_MERGE_DUPLICATES = 1 << 1, /**< 合并重复键为数组 */
    LQF_KEEP_EMPTY       = 1 << 2, /**< 保留空键值对 */
    LQF_STRICT           = 1 << 3, /**< 严格模式，遇到错误时返回错误 */
    LQF_SORT_KEYS        = 1 << 4, /**< 按键名排序结果 */
    LQF_LOWERCASE_KEYS   = 1 << 5, /**< 键名转换为小写 */
    LQF_TRIM_VALUES      = 1 << 6, /**< 去除值的前后空白字符 */
    LQF_DEFAULT          = LQF_AUTO_DECODE /**< 默认配置：自动解码 */
};

/* 查询参数组件标识符 */
enum llquery_field_types {
    LQF_KEY   = 0,   /**< 键字段 */
    LQF_VALUE = 1,   /**< 值字段 */
    LQF_MAX_FIELDS   /**< 字段类型数量 */
};

/* 解析结果结构体中的单个键值对 */
struct llquery_kv {
    const char *key;         /**< 键的起始指针（指向原始字符串或解码缓冲区） */
    size_t key_len;          /**< 键的长度 */
    const char *value;       /**< 值的起始指针 */
    size_t value_len;        /**< 值的长度 */
    bool is_encoded;         /**< 是否包含URL编码字符 */
};

/* 完整的查询字符串解析结果 */
struct llquery {
    uint32_t field_set;               /**< 位掩码，表示设置了哪些字段 */
    uint16_t kv_count;                /**< 键值对数量 */
    uint16_t max_kv_count;            /**< 最大支持的键值对数量 */
    uint16_t flags;                   /**< 解析时使用的选项标志 */

    /* 如果设置了LQF_AUTO_DECODE且需要解码，指向解码缓冲区 */
    char *decode_buffer;              /**< 解码缓冲区指针（如果需要） */
    size_t decode_buffer_size;        /**< 解码缓冲区大小 */

    /* 键值对数组 */
    struct llquery_kv *kv_pairs;      /**< 键值对数组指针 */

    /* 私有数据，用于内部管理 */
    void *_reserved;                  /**< 保留字段，供内部使用 */
};

/* 错误码 */
enum llquery_error {
    LQE_OK = 0,                       /**< 成功 */
    LQE_NULL_INPUT,                   /**< 输入字符串为空指针 */
    LQE_EMPTY_STRING,                 /**< 输入字符串为空 */
    LQE_INVALID_HEX,                  /**< 无效的十六进制编码 */
    LQE_BUFFER_TOO_SMALL,             /**< 缓冲区太小 */
    LQE_MEMORY_ERROR,                 /**< 内存分配错误 */
    LQE_TOO_MANY_PAIRS,               /**< 键值对数量超过限制 */
    LQE_INVALID_FORMAT,               /**< 格式无效 */
    LQE_INTERNAL_ERROR                /**< 内部错误 */
};

/* 回调函数类型，用于遍历键值对 */
typedef int (*llquery_iter_cb)(const struct llquery_kv *kv, void *user_data);

/* 回调函数类型，用于键值对比较（排序用） */
typedef int (*llquery_compare_cb)(const struct llquery_kv *a,
                                  const struct llquery_kv *b);

/* 内存分配器函数类型 */
typedef void* (*llquery_alloc_fn)(size_t size, void *user_data);
typedef void  (*llquery_free_fn)(void *ptr, void *user_data);

/**
 * @brief 初始化查询解析器结构体
 *
 * 在使用 llquery_parse() 或 llquery_parse_ex() 之前，
 * 必须调用此函数初始化结构体。
 *
 * @param q 指向 llquery 结构体的指针
 * @param max_pairs 最大键值对数量，0表示使用默认值(128)
 * @param flags 解析选项标志
 *
 * @return 错误码
 */
enum llquery_error llquery_init(struct llquery *q,
                                uint16_t max_pairs,
                                uint16_t flags);

/**
 * @brief 使用自定义内存分配器初始化查询解析器
 *
 * 适用于需要控制内存分配的场景，如嵌入式系统。
 *
 * @param q 指向 llquery 结构体的指针
 * @param max_pairs 最大键值对数量
 * @param flags 解析选项标志
 * @param alloc_fn 内存分配函数
 * @param free_fn 内存释放函数
 * @param alloc_data 传递给分配函数的用户数据
 *
 * @return 错误码
 */
enum llquery_error llquery_init_ex(struct llquery *q,
                                   uint16_t max_pairs,
                                   uint16_t flags,
                                   llquery_alloc_fn alloc_fn,
                                   llquery_free_fn free_fn,
                                   void *alloc_data);

/**
 * @brief 解析查询字符串
 *
 * 解析URL查询字符串（格式如 "key1=value1&key2=value2"），
 * 根据配置选项进行自动URL解码。
 *
 * 性能特点：
 * - 零拷贝：尽可能直接引用原始字符串
 * - 单次遍历：同时完成解析和解码
 * - 内存高效：需要时才分配解码缓冲区
 *
 * @param query 要解析的查询字符串（可包含前导'?'）
 * @param query_len 查询字符串长度，0表示自动计算
 * @param q 已初始化的 llquery 结构体指针
 *
 * @return 错误码
 *
 * @note 成功解析后，必须调用 llquery_free() 释放资源
 */
enum llquery_error llquery_parse(const char *query,
                                 size_t query_len,
                                 struct llquery *q);

/**
 * @brief 解析查询字符串的扩展版本
 *
 * 提供更多控制选项的解析函数。
 *
 * @param query 要解析的查询字符串
 * @param query_len 查询字符串长度
 * @param q 已初始化的 llquery 结构体指针
 * @param decode_buf 外部提供的解码缓冲区
 * @param decode_buf_size 解码缓冲区大小
 *
 * @return 错误码
 */
enum llquery_error llquery_parse_ex(const char *query,
                                    size_t query_len,
                                    struct llquery *q,
                                    char *decode_buf,
                                    size_t decode_buf_size);

/**
 * @brief 释放查询解析器占用的资源
 *
 * 解析完成后必须调用此函数，否则会导致内存泄漏。
 *
 * @param q 指向 llquery 结构体的指针
 */
void llquery_free(struct llquery *q);

/**
 * @brief 获取键值对数量
 *
 * @param q 指向 llquery 结构体的指针
 *
 * @return 键值对数量
 */
uint16_t llquery_count(const struct llquery *q);

/**
 * @brief 根据索引获取键值对
 *
 * @param q 指向 llquery 结构体的指针
 * @param index 键值对索引（0-based）
 *
 * @return 指向键值对的指针，如果索引无效则返回NULL
 */
const struct llquery_kv *llquery_get_kv(const struct llquery *q,
                                        uint16_t index);

/**
 * @brief 根据键名查找值
 *
 * 查找第一个匹配的键名对应的值。
 *
 * @param q 指向 llquery 结构体的指针
 * @param key 要查找的键名
 * @param key_len 键名长度
 *
 * @return 指向值的指针，如果未找到则返回NULL
 */
const char *llquery_get_value(const struct llquery *q,
                              const char *key,
                              size_t key_len);

/**
 * @brief 根据键名查找所有值（用于重复键）
 *
 * 查找指定键名对应的所有值，适用于重复键的情况。
 *
 * @param q 指向 llquery 结构体的指针
 * @param key 要查找的键名
 * @param key_len 键名长度
 * @param values 输出值数组
 * @param max_values 最大返回值数量
 *
 * @return 实际找到的值数量
 */
uint16_t llquery_get_all_values(const struct llquery *q,
                                const char *key,
                                size_t key_len,
                                const char **values,
                                uint16_t max_values);

/**
 * @brief 检查是否包含指定键
 *
 * @param q 指向 llquery 结构体的指针
 * @param key 要检查的键名
 * @param key_len 键名长度
 *
 * @return 如果包含则返回true，否则返回false
 */
bool llquery_has_key(const struct llquery *q,
                     const char *key,
                     size_t key_len);

/**
 * @brief 遍历所有键值对
 *
 * 使用回调函数遍历查询结果中的所有键值对。
 *
 * @param q 指向 llquery 结构体的指针
 * @param callback 回调函数
 * @param user_data 传递给回调函数的用户数据
 *
 * @return 遍历的键值对数量
 */
uint16_t llquery_iterate(const struct llquery *q,
                         llquery_iter_cb callback,
                         void *user_data);

/**
 * @brief 按键名排序键值对
 *
 * 根据键名对解析结果进行排序。
 *
 * @param q 指向 llquery 结构体的指针
 * @param compare_fn 比较函数，NULL表示使用默认字典序比较
 *
 * @return 错误码
 */
enum llquery_error llquery_sort(struct llquery *q,
                                llquery_compare_cb compare_fn);

/**
 * @brief 过滤键值对
 *
 * 根据回调函数过滤键值对。
 *
 * @param q 指向 llquery 结构体的指针
 * @param filter_fn 过滤函数，返回true表示保留
 * @param user_data 传递给过滤函数的用户数据
 *
 * @return 过滤后的键值对数量
 */
uint16_t llquery_filter(struct llquery *q,
                        bool (*filter_fn)(const struct llquery_kv *kv, void *user_data),
                        void *user_data);

/**
 * @brief 将解析结果格式化为查询字符串
 *
 * 将解析后的键值对重新格式化为查询字符串。
 * 可以选择是否进行URL编码。
 *
 * @param q 指向 llquery 结构体的指针
 * @param buffer 输出缓冲区
 * @param buffer_size 输出缓冲区大小
 * @param encode 是否进行URL编码
 *
 * @return 格式化后的字符串长度（不包括终止符），如果缓冲区太小则返回需要的长度
 */
size_t llquery_stringify(const struct llquery *q,
                         char *buffer,
                         size_t buffer_size,
                         bool encode);

/**
 * @brief 复制查询解析器
 *
 * 创建查询解析器的深拷贝。
 *
 * @param dst 目标查询解析器
 * @param src 源查询解析器
 *
 * @return 错误码
 */
enum llquery_error llquery_clone(struct llquery *dst,
                                 const struct llquery *src);

/**
 * @brief 重置查询解析器
 *
 * 重置查询解析器到初始状态，但不释放内存。
 * 可以用于重复解析不同的查询字符串。
 *
 * @param q 指向 llquery 结构体的指针
 */
void llquery_reset(struct llquery *q);

/**
 * @brief 设置内存分配器
 *
 * 运行时更改内存分配器。
 *
 * @param q 指向 llquery 结构体的指针
 * @param alloc_fn 内存分配函数
 * @param free_fn 内存释放函数
 * @param alloc_data 传递给分配函数的用户数据
 */
void llquery_set_allocator(struct llquery *q,
                           llquery_alloc_fn alloc_fn,
                           llquery_free_fn free_fn,
                           void *alloc_data);

/**
 * @brief 获取错误描述
 *
 * @param error 错误码
 *
 * @return 错误描述字符串
 */
const char *llquery_strerror(enum llquery_error error);

/**
 * @brief URL编码字符串
 *
 * 对字符串进行URL编码。
 *
 * @param input 输入字符串
 * @param input_len 输入字符串长度
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 *
 * @return 编码后的字符串长度，如果缓冲区太小则返回需要的长度
 */
size_t llquery_url_encode(const char *input,
                          size_t input_len,
                          char *output,
                          size_t output_size);

/**
 * @brief URL解码字符串
 *
 * 对字符串进行URL解码。
 *
 * @param input 输入字符串
 * @param input_len 输入字符串长度
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 *
 * @return 解码后的字符串长度，如果缓冲区太小则返回需要的长度
 */
size_t llquery_url_decode(const char *input,
                          size_t input_len,
                          char *output,
                          size_t output_size);

/**
 * @brief 快速解析查询字符串（简化接口）
 *
 * 适用于简单场景的快速解析函数。
 * 使用栈上分配的解析器，避免动态内存分配。
 *
 * @param query 查询字符串
 * @param query_len 查询字符串长度
 * @param kv_pairs 输出键值对数组
 * @param max_pairs 最大键值对数量
 * @param flags 解析选项
 *
 * @return 实际解析的键值对数量
 */
uint16_t llquery_parse_fast(const char *query,
                            size_t query_len,
                            struct llquery_kv *kv_pairs,
                            uint16_t max_pairs,
                            uint16_t flags);

/**
 * @brief 检查字符串是否为有效的查询字符串
 *
 * 快速检查字符串是否符合查询字符串的基本格式。
 *
 * @param str 要检查的字符串
 * @param len 字符串长度
 *
 * @return 如果是有效的查询字符串则返回true
 */
bool llquery_is_valid(const char *str, size_t len);

/**
 * @brief 统计查询字符串中的键值对数量
 *
 * 快速统计查询字符串中的键值对数量，不进行完整解析。
 *
 * @param query 查询字符串
 * @param query_len 查询字符串长度
 *
 * @return 键值对数量
 */
uint16_t llquery_count_pairs(const char *query, size_t query_len);

/** @} */ /* end of llquery group */

#ifdef __cplusplus
}
#endif

#endif /* LLQUERY_H */
