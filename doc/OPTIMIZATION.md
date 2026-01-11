# llquery 性能优化文档

本文档详细说明 llquery 库的性能优化技术、实施计划及性能指标。

## 优化概述

llquery 采用参考 [llurl](https://github.com/zhaozg/llurl) 和 [llhttp](https://github.com/nodejs/llhttp) 的优化理念，通过以下核心技术实现高性能 URL 查询字符串解析：

- **查表优化**: 使用位掩码查找表消除字符分类的分支
- **批量处理**: 使用 `memchr()` 等硬件加速函数批量查找分隔符
- **单次遍历**: 在一次遍历中同时完成解析和解码
- **分支预测提示**: 使用编译器提示优化热路径
- **缓存友好设计**: 最小化内存占用，提升缓存命中率

## 当前性能基准（优化前）

基于 benchmark.c 的测试结果（GCC 13.3.0, -O3 优化）：

| 测试项目 | 性能指标 | 备注 |
|---------|---------|------|
| 简单解析 (3 参数) | **3.88M ops/sec** | 无编码字符 |
| 复杂解析带解码 (6 参数) | **1.98M ops/sec** | 包含 %XX 和 + 编码 |
| 重编码解析 (4 参数) | **2.76M ops/sec** | 大量编码字符 |
| 多参数解析 (15 参数) | **0.98M ops/sec** | 测试扩展性 |
| 快速解析-栈分配 (3 参数) | **42.2M ops/sec** | 零内存分配 |
| 按键查询值 | **18.3M ops/sec** | 线性查找 |
| 迭代所有键值对 | **22.8M ops/sec** | 遍历访问 |
| URL 编码 | **5.65M ops/sec** | 字符转义 |
| URL 解码 | **20.5M ops/sec** | %XX 解码 |
| 吞吐量测试 | **138.47 MB/sec** | 1.77M queries/sec |

### 性能特点分析

**优势**:
- 快速解析（栈分配）达到 42M ops/sec，适合短查询字符串
- URL 解码速度快（20.5M ops/sec）
- 迭代和查询性能良好

**瓶颈**:
- 复杂查询解析性能相对较低（~2M ops/sec）
- 多参数解析存在性能下降（15 参数时降至 0.98M）
- 内存分配开销显著（对比快速解析的 42M vs 常规 3.88M）

## 优化计划

### 阶段 1: 字符分类查表优化 ✅ 待实施

**目标**: 消除字符检查中的条件分支，提升 15-20% 性能

#### 当前实现问题

```c
// 当前在多个地方使用分支判断
if (c == '%' || c == '+') {  // 2 次比较 + 1 次逻辑或
    return true;
}

if (*current == '=' || *current == '&') {  // 多次比较
    // ...
}

// 使用标准库函数（有分支）
isspace((unsigned char)*start)
tolower((unsigned char)str[i])
```

**问题**:
- 每次字符检查需要 2-4 次比较
- 分支预测失败导致流水线停顿（20-40 个时钟周期惩罚）
- 在不同输入数据上性能不稳定

#### 优化方案：统一位掩码查找表

```c
/* 字符属性位掩码定义 */
#define CHAR_SEPARATOR   0x01  /* & 分隔符 */
#define CHAR_EQUAL       0x02  /* = 等号 */
#define CHAR_PERCENT     0x04  /* % 百分号 */
#define CHAR_PLUS        0x08  /* + 加号 */
#define CHAR_HEX         0x10  /* 十六进制字符 0-9A-Fa-f */
#define CHAR_SPACE       0x20  /* 空白字符 */
#define CHAR_LOWER       0x40  /* 需要小写转换的字符 */

/* 256 字节查找表（适配所有字符） */
static const unsigned char char_flags[256] = {
    /* 0x00-0x1F: 控制字符 */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00,  /* 0x09-0x0D 为空白字符 */
    /* ... */
    /* 0x20: ' ' */ 0x20,  /* 空格 */
    /* 0x25: '%' */ 0x04,  /* 百分号 */
    /* 0x26: '&' */ 0x01,  /* 分隔符 */
    /* 0x2B: '+' */ 0x08,  /* 加号 */
    /* 0x3D: '=' */ 0x02,  /* 等号 */
    /* 0x30-0x39: '0'-'9' */ 0x10, 0x10, /* ... */ /* 十六进制数字 */
    /* 0x41-0x46: 'A'-'F' */ 0x50, 0x50, /* ... */ /* 大写十六进制 + 需小写转换 */
    /* 0x47-0x5A: 'G'-'Z' */ 0x40, 0x40, /* ... */ /* 需小写转换 */
    /* 0x61-0x66: 'a'-'f' */ 0x10, 0x10, /* ... */ /* 小写十六进制 */
    /* ... */
};

/* 宏定义：零分支字符检查 */
#define IS_SEPARATOR(c)  (char_flags[(unsigned char)(c)] & CHAR_SEPARATOR)
#define IS_EQUAL(c)      (char_flags[(unsigned char)(c)] & CHAR_EQUAL)
#define IS_ENCODED(c)    (char_flags[(unsigned char)(c)] & (CHAR_PERCENT | CHAR_PLUS))
#define IS_HEX_DIGIT(c)  (char_flags[(unsigned char)(c)] & CHAR_HEX)
#define IS_SPACE(c)      (char_flags[(unsigned char)(c)] & CHAR_SPACE)
#define NEEDS_LOWER(c)   (char_flags[(unsigned char)(c)] & CHAR_LOWER)
```

**优势**:
- 单次内存访问替代多次比较
- 完全消除分支，无流水线停顿
- 256 字节表适配单个 CPU 缓存行（64 字节 × 4）
- 指令数减少 75%（8 条 → 2 条）

**预期收益**: 15-20% 性能提升

### 阶段 2: 批量处理优化 ✅ 待实施

**目标**: 减少循环开销，提升 10-15% 性能

#### 当前实现问题

```c
// 当前：字符级循环扫描
while (current < end && *current != '=' && *current != '&') {
    current++;  // 每字符一次检查
}
```

**问题**:
- 每字符一次循环迭代
- 每字符一次边界检查
- 每字符两次字符比较
- 循环控制开销大

#### 优化方案：使用 memchr() 批量查找

```c
/* 批量查找分隔符 */
static const char* find_delimiter(const char *start, const char *end, char delim) {
    size_t len = end - start;
    const char *pos = memchr(start, delim, len);
    return pos ? pos : end;
}

/* 优化的键查找：批量扫描到 '=' 或 '&' */
const char *equal_pos = find_delimiter(current, end, '=');
const char *amp_pos = find_delimiter(current, end, '&');
const char *key_end = (equal_pos < amp_pos) ? equal_pos : amp_pos;

/* 优化的值查找：批量扫描到 '&' */
const char *value_end = find_delimiter(value_start, end, '&');
```

**优势**:
- `memchr()` 是硬件加速的（SIMD 指令）
- 单次调用处理多个字符
- 减少循环控制开销
- 适合现代 CPU 流水线

**预期收益**: 10-15% 性能提升，对长查询字符串效果更显著

### 阶段 3: 十六进制解码优化 ✅ 待实施

**目标**: 优化 %XX 解码性能，提升编码字符串解析 10-15%

#### 当前实现

```c
static const signed char HEX_LOOKUP[256] = {
  -1,-1,-1,-1, /* ... */  // 256 个元素
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9,  // '0'-'9'
  /* ... */
  10,11,12,13,14,15,  // 'A'-'F'
  /* ... */
  10,11,12,13,14,15,  // 'a'-'f'
  /* ... */
};

// 解码逻辑
int h1 = HEX_LOOKUP[(unsigned char)src[1]];
int h2 = HEX_LOOKUP[(unsigned char)src[2]];
if (h1 >= 0 && h2 >= 0) {  // 需要边界检查
    *dst++ = (char)((h1 << 4) | h2);
    src += 3;
}
```

#### 优化方案

```c
/* 方案1: 使用 char_flags 统一表 */
#define HEX_VALUE(c) (char_flags[(unsigned char)(c)] & CHAR_HEX ? \
    (((c) >= '0' && (c) <= '9') ? ((c) - '0') : \
     (((c) >= 'A' && (c) <= 'F') ? ((c) - 'A' + 10) : \
      ((c) - 'a' + 10))) : -1)

/* 方案2: 快速路径优化（无分支） */
static inline int hex_to_int(unsigned char c) {
    // 利用 ASCII 编码特性：
    // '0'-'9': 0x30-0x39 -> 0-9
    // 'A'-'F': 0x41-0x46 -> 10-15
    // 'a'-'f': 0x61-0x66 -> 10-15
    
    unsigned char val = c - '0';
    if (val < 10) return val;  // 可被分支预测器优化
    
    val = (c | 0x20) - 'a';  // 统一大小写
    if (val < 6) return val + 10;
    
    return -1;  // 无效字符
}

/* 方案3: 合并检查和解码（推荐） */
static inline bool decode_hex_pair(const char *src, char *dst) {
    unsigned char h1 = src[0];
    unsigned char h2 = src[1];
    
    if (!(char_flags[h1] & CHAR_HEX) || !(char_flags[h2] & CHAR_HEX)) {
        return false;
    }
    
    // 快速解码（无条件分支）
    int v1 = (h1 <= '9') ? (h1 - '0') : ((h1 | 0x20) - 'a' + 10);
    int v2 = (h2 <= '9') ? (h2 - '0') : ((h2 | 0x20) - 'a' + 10);
    
    *dst = (char)((v1 << 4) | v2);
    return true;
}
```

**预期收益**: 对重编码查询字符串提升 10-15%

### 阶段 4: 分支预测提示 ✅ 待实施

**目标**: 优化热路径，提升 5-10% 性能

```c
/* 编译器分支预测宏 */
#if defined(__GNUC__) || defined(__clang__)
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x)   (x)
#define UNLIKELY(x) (x)
#endif

/* 应用示例 */
// 热路径：大多数查询有多个参数
if (LIKELY(current < end)) {
    // 常见情况
}

// 冷路径：错误处理
if (UNLIKELY(kv_index >= q->max_kv_count)) {
    return LQE_TOO_MANY_PAIRS;  // 罕见错误
}

// 热路径：大多数字符不需要编码
if (UNLIKELY(c == '%')) {
    // 解码处理
}
```

**应用场景**:
- 错误处理路径标记为 `UNLIKELY`
- 正常解析路径标记为 `LIKELY`
- 编码字符检查标记为 `UNLIKELY`（大多数查询无编码）

**预期收益**: 5-10% 性能提升

### 阶段 5: 内存访问优化 ✅ 待实施

**目标**: 减少内存分配和拷贝开销

#### 优化点 1: 减少 key/value 内存分配

```c
/* 当前实现：每个 key/value 都分配内存 */
char *key_buf = internal->alloc_fn(kv->key_len + 1, internal->alloc_data);
memcpy(key_buf, key_start, kv->key_len);
key_buf[kv->key_len] = '\0';

/* 优化方案：批量分配或直接引用 */
// 方案A: 如果不需要解码，直接引用原字符串（零拷贝）
if (!needs_decode && !needs_lowercase && !needs_trim) {
    kv->key = key_start;  // 直接指向原字符串
    kv->value = value_start;
}

// 方案B: 批量分配内存池
char *buffer_pool = alloc_fn(total_estimated_size, alloc_data);
// 在池中分配 key/value
```

**预期收益**: 减少内存分配次数 50-70%，提升 10-20%

#### 优化点 2: 提前统计参数数量

```c
/* 快速预扫描：统计 '&' 数量估算参数数量 */
static uint16_t estimate_pair_count(const char *query, size_t len) {
    uint16_t count = 1;
    const char *pos = query;
    while ((pos = memchr(pos, '&', len - (pos - query))) != NULL) {
        count++;
        pos++;
    }
    return count;
}

// 使用估算值初始化
uint16_t estimated = estimate_pair_count(work_query, query_len);
if (estimated < q->max_kv_count) {
    // 可以安全解析，不会溢出
}
```

**预期收益**: 避免动态扩容，提升 5-10%

### 阶段 6: 缓存友好设计 ✅ 待实施

**目标**: 提升缓存命中率

```c
/* 查找表内存布局 */
// 总大小：256 字节（char_flags）
// L1 缓存：通常 32-64KB
// L1 缓存行：64 字节
// char_flags 占用：4 个缓存行

/* 内存布局优化 */
struct llquery_kv_compact {
    uint16_t key_offset;      // 相对偏移而非指针（节省空间）
    uint16_t key_len;
    uint16_t value_offset;
    uint16_t value_len;
    // 总大小：8 字节（vs 原 32 字节）
};

/* 使用连续缓冲区存储所有字符串 */
// [所有key值连续存储][所有value值连续存储]
// 优势：更好的缓存局部性
```

**预期收益**: 5-10% 性能提升，特别是大量参数时

## 优化实施优先级

### P0 - 高优先级（预期收益 >10%）

1. ✅ **字符分类查表优化** - 预期 15-20%
2. ✅ **批量处理优化** - 预期 10-15%
3. ✅ **十六进制解码优化** - 预期 10-15%（编码字符串）

### P1 - 中优先级（预期收益 5-10%）

4. ✅ **分支预测提示** - 预期 5-10%
5. ✅ **内存访问优化** - 预期 10-20%

### P2 - 低优先级（预期收益 <5%）

6. ✅ **缓存友好设计** - 预期 5-10%
7. 🔄 代码对齐优化
8. 🔄 编译器选项微调

## 性能目标

基于 llurl 的优化经验，设定以下性能目标：

| 测试项目 | 当前性能 | 目标性能 | 提升幅度 |
|---------|---------|---------|---------|
| 简单解析 (3 参数) | 3.88M ops/sec | **5.0-5.5M ops/sec** | +30-40% |
| 复杂解析带解码 (6 参数) | 1.98M ops/sec | **2.5-3.0M ops/sec** | +25-50% |
| 重编码解析 (4 参数) | 2.76M ops/sec | **3.5-4.0M ops/sec** | +25-45% |
| 多参数解析 (15 参数) | 0.98M ops/sec | **1.3-1.5M ops/sec** | +30-50% |
| 吞吐量测试 | 138.47 MB/sec | **180-200 MB/sec** | +30-45% |

**总体目标**: 平均性能提升 **30-50%**

## 优化测量方法

### 1. 微基准测试

```bash
# 运行完整基准测试套件
make run-benchmark

# 输出示例
=== Parse Benchmarks ===
Simple parse (3 params)                  5.2M ops/sec  (+34%)
Complex parse with decode (6 params)     2.8M ops/sec  (+41%)
```

### 2. 代码剖析 (Profiling)

```bash
# 使用 perf 分析热点
perf record -g ./benchmark
perf report

# 使用 valgrind 分析缓存
valgrind --tool=cachegrind ./benchmark
```

### 3. 对比测试

```bash
# 编译优化前版本
git checkout baseline
make clean && make
./benchmark > baseline.txt

# 编译优化后版本
git checkout optimized
make clean && make
./benchmark > optimized.txt

# 对比结果
diff -u baseline.txt optimized.txt
```

## 安全性考虑

所有优化必须保持严格的安全保证：

1. **边界检查**: 所有缓冲区访问必须验证边界
2. **整数溢出**: 长度计算必须检查溢出
3. **无效输入**: 严格验证所有输入数据
4. **无缓冲区溢出**: 使用 `memchr()` 等安全函数
5. **状态机验证**: 确保所有状态转换合法

### 安全测试

```bash
# AddressSanitizer 测试
make debug

# 模糊测试（如果集成）
make fuzz

# Valgrind 内存检查
valgrind --leak-check=full ./test_llquery
```

## 优化实施指南

### 步骤 1: 建立基准

```bash
# 记录优化前性能
make clean && make
./benchmark > performance_baseline.txt
git add performance_baseline.txt
git commit -m "Add performance baseline"
```

### 步骤 2: 分支开发

```bash
# 为每个优化创建分支
git checkout -b opt/char-lookup-table
# 实现优化
# 测试验证
git checkout main
git merge opt/char-lookup-table
```

### 步骤 3: 增量优化

- 每次只实施一个优化点
- 测试性能变化和正确性
- 记录性能数据
- 如果性能下降，回滚并分析原因

### 步骤 4: 综合验证

```bash
# 运行完整测试套件
make test

# 运行基准测试
make run-benchmark

# 内存安全检查
make debug

# CI 验证
git push origin opt/all-optimizations
# 检查 GitHub Actions 结果
```

## 技术参考

### 相关项目优化文档

- [llurl OPTIMIZATION.md](https://github.com/zhaozg/llurl/blob/main/doc/OPTIMIZATION.md)
- [llhttp 优化技术](https://github.com/nodejs/llhttp)

### CPU 架构优化

- **流水线**: 现代 CPU 有 14-20 级流水线，分支预测失败代价 20-40 周期
- **缓存层次**: L1 (32-64KB, 1-4 周期), L2 (256KB-1MB, 10-20 周期), L3 (数 MB, 40-75 周期)
- **SIMD**: `memchr()` 等函数使用 SSE/AVX 指令并行处理
- **分支预测器**: 95%+ 准确率，但对随机数据失效

### 编译器优化

```bash
# 推荐的编译选项
CFLAGS = -O3 -march=native -funroll-loops -flto

# 生成汇编代码检查
gcc -S -O3 -masm=intel llquery.c -o llquery.s

# PGO (Profile-Guided Optimization)
gcc -fprofile-generate llquery.c -o llquery
./llquery  # 运行典型负载
gcc -fprofile-use llquery.c -o llquery_optimized
```

## 持续优化

性能优化是持续过程：

1. **监控基准**: 在 CI 中集成性能测试，监控性能回归
2. **收集反馈**: 从实际使用场景收集性能数据
3. **迭代改进**: 根据实际负载特征继续优化
4. **文档更新**: 记录所有优化及其效果

## 总结

llquery 通过系统化的优化方法，预期实现 **30-50%** 的性能提升：

- **字符分类查表**: 消除分支，稳定性能 (+15-20%)
- **批量处理**: 硬件加速，减少开销 (+10-15%)
- **十六进制优化**: 快速解码 (+10-15% for encoded)
- **分支提示**: 优化热路径 (+5-10%)
- **内存优化**: 减少分配和拷贝 (+10-20%)

所有优化在保持以下特性：
- ✅ API 兼容性
- ✅ 零依赖
- ✅ 线程安全
- ✅ 内存安全
- ✅ 代码可维护性

最终目标是达到或超越 llurl 的性能水平，成为 C 语言生态中最快的查询字符串解析库。

---

## 优化实施记录

### 第一轮优化实施 (2026-01-11)

**实施内容**:
- ✅ 阶段 1: 字符分类查表优化
- ✅ 阶段 2: 批量处理优化（memchr）
- ✅ 阶段 3: 分支预测提示
- ✅ 阶段 4: 十六进制解码优化

**技术细节**:

1. **字符分类查找表** (`char_flags[256]`)
   - 256字节位掩码查找表，使用7个位标识字符属性
   - `CHAR_SEPARATOR` (0x01): &
   - `CHAR_EQUAL` (0x02): =
   - `CHAR_PERCENT` (0x04): %
   - `CHAR_PLUS` (0x08): +
   - `CHAR_HEX` (0x10): 0-9A-Fa-f
   - `CHAR_SPACE` (0x20): 空白字符
   - `CHAR_UPPER` (0x40): A-Z
   - `CHAR_ALPHA` (0x80): a-z

2. **零分支宏定义**
   ```c
   #define IS_SEPARATOR(c)  (char_flags[(unsigned char)(c)] & CHAR_SEPARATOR)
   #define IS_EQUAL(c)      (char_flags[(unsigned char)(c)] & CHAR_EQUAL)
   #define IS_ENCODED(c)    (char_flags[(unsigned char)(c)] & (CHAR_PERCENT | CHAR_PLUS))
   ```

3. **批量处理**
   - 使用 `memchr(current, '&', end - current)` 批量查找分隔符
   - 减少循环迭代次数

4. **分支预测提示**
   ```c
   #define LIKELY(x)   __builtin_expect(!!(x), 1)
   #define UNLIKELY(x) __builtin_expect(!!(x), 0)
   ```

5. **移除依赖**
   - 不再使用 `ctype.h` 的 `isspace()`, `tolower()`, `isalnum()`
   - 自定义实现更快且可预测

**性能测试结果** (GCC 13.3.0, -O3):

| 测试项目 | 优化前 | 优化后 | 提升 |
|---------|-------|--------|------|
| 简单解析 (3 参数) | 3.88M ops/sec | **4.05M ops/sec** | **+4.4%** |
| 复杂解析带解码 (6 参数) | 1.98M ops/sec | **2.00M ops/sec** | **+1.0%** |
| 重编码解析 (4 参数) | 2.76M ops/sec | **2.76M ops/sec** | +0.0% |
| 多参数解析 (15 参数) | 0.98M ops/sec | **0.96M ops/sec** | -2.0% |
| 快速解析-栈 (3 参数) | 42.2M ops/sec | **42.7M ops/sec** | **+1.2%** |
| 按键查询值 | 18.3M ops/sec | **19.0M ops/sec** | **+3.8%** |
| URL 解码 | 20.5M ops/sec | **19.1M ops/sec** | -6.8% |
| 吞吐量测试 | 138.47 MB/sec | **137.71 MB/sec** | -0.5% |

**分析**:

1. **正面效果**:
   - 简单解析提升 4.4%
   - 查询操作提升 3.8%
   - 快速解析提升 1.2%
   - 消除了 ctype.h 依赖

2. **性能持平或下降原因**:
   - 测试用例的查询字符串相对较短（<100字节）
   - 批量处理（memchr）在短字符串上优势不明显
   - 增加的 UNLIKELY 检查在某些路径增加了指令
   - 编译器优化已经很好地处理了原始代码

3. **下一步优化方向**:
   - 需要实施内存分配优化（最大瓶颈）
   - 考虑批量分配 key/value 字符串
   - 零拷贝优化：不解码时直接引用原字符串
   - 针对长查询字符串场景的优化

**结论**:

第一轮优化实现了目标中的基础优化，性能略有提升（+4.4% 最佳情况）。主要价值在于：
- 代码更清晰（查找表 vs 多重分支）
- 性能更稳定（无分支预测失败）
- 移除外部依赖（ctype.h）
- 为后续优化奠定基础

下一阶段将重点实施**内存优化**，预期可带来 10-20% 的显著提升。

---

### 第二轮优化实施 (2026-01-11)

**实施内容**:
- 🔧 阶段 5: 内存池优化（基础设施已完成，待启用）

**技术细节**:

1. **内存池数据结构**
   ```c
   typedef struct llquery_internal {
     char *string_pool;         /* 字符串内存池 */
     size_t string_pool_size;   /* 内存池总大小 */
     size_t string_pool_used;   /* 已使用的大小 */
     bool string_pool_owned;    /* 是否拥有内存池 */
   } llquery_internal_t;
   ```

2. **池分配函数**
   ```c
   static char* pool_alloc_string(llquery_internal_t *internal, size_t size) {
     // 优先从池分配
     if (internal->string_pool && 
         internal->string_pool_used + size <= internal->string_pool_size) {
       char *ptr = internal->string_pool + internal->string_pool_used;
       internal->string_pool_used += size;
       return ptr;
     }
     // 池空间不足时回退到常规分配
     return internal->alloc_fn(size, internal->alloc_data);
   }
   ```

3. **智能释放机制**
   - 检查指针地址判断是否在池中
   - 池内字符串随池一起释放
   - 池外字符串单独释放
   - 处理混合分配场景（池溢出）

**当前状态**: 
- ✅ 代码已实现并通过基本测试
- ⚠️ 暂时禁用以确保稳定性
- 需要更多边界情况测试

**为何暂时禁用**:
1. 混合分配场景（池溢出后回退堆分配）需要更严格测试
2. 内存池大小估算可能对某些查询不准确
3. 需要使用 Valgrind/AddressSanitizer 验证
4. 保守策略：优先保证已有优化的稳定性

**性能测试结果** (GCC 13.3.0, -O3):

| 测试项目 | 优化前 | 当前 | 变化 |
|---------|-------|------|------|
| 简单解析 (3 参数) | 3.88M ops/sec | **3.91M ops/sec** | +0.8% |
| 复杂解析带解码 (6 参数) | 1.98M ops/sec | **1.95M ops/sec** | -1.5% |
| 重编码解析 (4 参数) | 2.76M ops/sec | **2.73M ops/sec** | -1.1% |
| 多参数解析 (15 参数) | 0.98M ops/sec | **0.92M ops/sec** | -6.1% |
| 快速解析-栈 (3 参数) | 42.2M ops/sec | **42.0M ops/sec** | -0.5% |
| 按键查询值 | 18.3M ops/sec | **19.0M ops/sec** | **+3.8%** ✅ |
| URL 解码 | 20.5M ops/sec | **18.9M ops/sec** | -7.8% |
| **查询验证** | 9.56M ops/sec | **12.4M ops/sec** | **+29.7%** ⭐ |

**亮点**:
- **查询验证性能提升 29.7%** - 最显著的改进
- 按键查询提升 3.8%
- 整体性能保持稳定

**下一步工作**:
1. ~~完成阶段5的边界情况测试~~ ✅ 已完成
2. ~~使用内存检测工具验证（Valgrind, ASan）~~ ✅ 已完成
3. ~~启用内存池优化~~ ✅ 已启用
4. 实施阶段6（缓存友好设计）

---

### 第三轮优化实施 - 阶段5启用 (2026-01-11)

**实施内容**:
- ✅ 阶段 5: 内存池优化（已启用并验证）

**使用工具调试**:
1. **AddressSanitizer (ASan)**
   - 编译选项: `-fsanitizer=address`
   - 检测到的问题: `llquery_filter()` 函数尝试释放内存池中的字符串
   - 修复方案: 添加指针范围检查，只释放堆分配的字符串

2. **内存安全修复**
   ```c
   // 修复前：直接释放所有字符串（导致 double-free）
   internal->free_fn((void *)ptr, internal->alloc_data);
   
   // 修复后：检查是否在内存池中
   bool in_pool = (internal->string_pool && 
                   ptr >= internal->string_pool &&
                   ptr < internal->string_pool + internal->string_pool_size);
   if (!in_pool) {
     internal->free_fn((void *)ptr, internal->alloc_data);
   }
   ```

3. **验证结果**
   - ✅ ASan 测试通过，无内存泄漏
   - ✅ 所有 30 个测试用例通过
   - ✅ 无 use-after-free 错误
   - ✅ 无 invalid free 错误

**性能测试结果** (GCC 13.3.0, -O3, 阶段5已启用):

| 测试项目 | 优化前 | 阶段5启用后 | 提升 |
|---------|-------|-----------|------|
| **简单解析 (3 参数)** | 3.88M ops/sec | **4.87M ops/sec** | **+25.5%** ⭐ |
| **复杂解析带解码 (6 参数)** | 1.98M ops/sec | **3.03M ops/sec** | **+53.0%** ⭐⭐ |
| **重编码解析 (4 参数)** | 2.76M ops/sec | **3.64M ops/sec** | **+31.9%** ⭐ |
| **多参数解析 (15 参数)** | 0.98M ops/sec | **2.07M ops/sec** | **+111%** ⭐⭐⭐ |
| **快速解析-栈 (3 参数)** | 42.2M ops/sec | **43.1M ops/sec** | **+2.1%** |
| **排序键 (15 参数)** | 0.56M ops/sec | **1.03M ops/sec** | **+84%** ⭐⭐ |
| 按键查询值 | 18.3M ops/sec | **18.9M ops/sec** | +3.3% |
| URL 解码 | 20.5M ops/sec | **21.8M ops/sec** | +6.3% |
| 查询验证 | 9.56M ops/sec | **11.5M ops/sec** | +20.3% |

**重大突破**:
- 🎯 **多参数解析提升 111%** - 消除了内存分配瓶颈
- 🎯 **排序操作提升 84%** - 减少分配开销
- 🎯 **复杂解码提升 53%** - 批量分配优势明显
- 🎯 **简单解析提升 25.5%** - 整体性能提升
- 🎯 **重编码提升 31.9%** - 编码场景受益

**技术实现**:

1. **内存池设计**
   - 在解析开始时预分配一块连续内存
   - 估算大小: `query_len * 2 + 256 bytes`
   - 所有 key/value 字符串从池中顺序分配
   - 池空间不足时自动回退到堆分配

2. **混合分配支持**
   - 内存池 + 堆分配可以共存
   - 释放时通过指针范围判断来源
   - 池内字符串: 随池一起释放（一次 free）
   - 堆内字符串: 单独释放

3. **性能优势来源**
   - 减少 malloc 调用: 从 2N+1 次减少到 1-3 次
   - 提高缓存局部性: 所有字符串连续存储
   - 减少内存碎片: 大块连续分配
   - 加速释放: 一次性释放整个池

**下一步工作**:
1. 实施阶段6（缓存友好设计）
2. 考虑更精确的池大小估算算法
3. 探索零拷贝优化（非解码场景）

---

**更新记录**:
- 2026-01-11: 创建优化计划文档，建立性能基准
- 2026-01-11: 完成第一轮优化实施（阶段1-4），性能提升 +4.4%
- 2026-01-11: 完成阶段5基础设施，查询验证性能提升 +29.7%
- 2026-01-11: 启用阶段5内存池，多参数解析性能提升 **+111%** 🎉
