# llquery - 超高速 URL 查询字符串解析库

[![CI](https://github.com/zhaozg/llquery/workflows/CI/badge.svg)](https://github.com/zhaozg/llquery/actions/workflows/ci.yml)
[![Sanitizers](https://github.com/zhaozg/llquery/workflows/Sanitizers/badge.svg)](https://github.com/zhaozg/llquery/actions/workflows/sanitizers.yml)
[![Performance](https://github.com/zhaozg/llquery/workflows/Performance%20Tracking/badge.svg)](https://github.com/zhaozg/llquery/actions/workflows/performance.yml)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C99](https://img.shields.io/badge/std-C99-blue.svg)](https://en.wikipedia.org/wiki/C99)

llquery 是一个高性能、零依赖的 C 语言 URL 查询字符串解析库，采用状态机与查表优化技术，提供零拷贝设计和单次遍历的高效解析能力。

## ✨ 特性

- **高性能**: 单次遍历完成解析和解码，采用查表优化减少分支
- **零依赖**: 纯 C99 标准实现，无外部依赖
- **零拷贝**: 直接在原始字符串上操作，减少内存拷贝
- **线程安全**: 无全局状态，完全可重入
- **内存安全**: 严格的边界检查，防止缓冲区溢出
- **灵活配置**: 支持自动 URL 解码、重复键合并、键排序等多种选项
- **易于集成**: 简单的 API 设计，支持自定义内存分配器

## 🚀 快速开始

### 编译

```bash
# 编译静态库和共享库
make

# 编译并运行示例
make run-example

# 运行测试
make test

# 运行性能基准测试
make run-benchmark
```

### 基本用法

```c
#include "llquery.h"
#include <stdio.h>

int main() {
    struct llquery query;
    
    // 初始化解析器
    llquery_init(&query, 0, LQF_DEFAULT);
    
    // 解析查询字符串
    const char *query_str = "name=John&age=30&city=New+York";
    llquery_parse(query_str, 0, &query);
    
    // 获取参数值
    const char *name = llquery_get_value(&query, "name", 4);
    printf("Name: %s\n", name);
    
    // 遍历所有参数
    printf("所有参数:\n");
    for (uint16_t i = 0; i < llquery_count(&query); i++) {
        const struct llquery_kv *kv = llquery_get_kv(&query, i);
        printf("  %.*s = %.*s\n",
               (int)kv->key_len, kv->key,
               (int)kv->value_len, kv->value);
    }
    
    // 清理资源
    llquery_free(&query);
    
    return 0;
}
```

## 📖 API 概览

### 核心函数

| 函数 | 说明 |
|------|------|
| `llquery_init()` | 初始化解析器 |
| `llquery_parse()` | 解析查询字符串 |
| `llquery_free()` | 释放资源 |
| `llquery_get_value()` | 根据键获取值 |
| `llquery_get_kv()` | 根据索引获取键值对 |
| `llquery_count()` | 获取键值对数量 |

### 高级功能

| 函数 | 说明 |
|------|------|
| `llquery_iterate()` | 遍历所有键值对 |
| `llquery_sort()` | 按键排序 |
| `llquery_filter()` | 过滤键值对 |
| `llquery_stringify()` | 格式化为查询字符串 |
| `llquery_clone()` | 复制解析器 |

### 实用工具

| 函数 | 说明 |
|------|------|
| `llquery_url_encode()` | URL 编码 |
| `llquery_url_decode()` | URL 解码 |
| `llquery_is_valid()` | 验证查询字符串格式 |
| `llquery_count_pairs()` | 快速统计键值对数量 |

详细 API 文档请参阅 [doc/API.md](doc/API.md)

## ⚙️ 配置选项

llquery 支持以下配置标志：

| 标志 | 说明 |
|------|------|
| `LQF_NONE` | 无特殊选项 |
| `LQF_AUTO_DECODE` | 自动 URL 解码（处理 %XX 和 +） |
| `LQF_MERGE_DUPLICATES` | 合并重复键为数组 |
| `LQF_KEEP_EMPTY` | 保留空键值对 |
| `LQF_STRICT` | 严格模式，遇到错误时返回 |
| `LQF_SORT_KEYS` | 按键名排序结果 |
| `LQF_LOWERCASE_KEYS` | 键名转换为小写 |
| `LQF_TRIM_VALUES` | 去除值的前后空白字符 |

可以使用位或操作组合多个选项：
```c
uint16_t flags = LQF_AUTO_DECODE | LQF_SORT_KEYS | LQF_LOWERCASE_KEYS;
llquery_init(&query, 128, flags);
```

## 🔧 高级特性

### 自定义内存分配器

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

### 快速解析（栈分配）

适用于高性能、内存受限场景。

```c
struct llquery_kv pairs[10];
uint16_t count = llquery_parse_fast("a=1&b=2&c=3", 0, pairs, 10, LQF_AUTO_DECODE);
```

**使用条件**:
- ✅ 查询字符串较短（<2048 字节）
- ✅ 键值对数量较少（建议 <50）
- ✅ 结果仅在当前函数内使用
- ✅ 追求极致性能（~10倍性能提升）

**注意**: 返回的指针指向原始字符串，确保原始字符串生命周期足够长。详见 [API 文档](doc/API.md)。

### 回调遍历

```c
int print_callback(const struct llquery_kv *kv, void *user_data) {
    printf("%.*s = %.*s\n",
           (int)kv->key_len, kv->key,
           (int)kv->value_len, kv->value);
    return 0;
}

llquery_iterate(&query, print_callback, NULL);
```

## 📊 性能

llquery 设计目标是达到与 [llhttp](https://github.com/nodejs/llhttp) 和 [llurl](https://github.com/zhaozg/llurl) 类似的高性能水平。

核心优化技术：
- 十六进制字符查找表，消除条件分支
- 单次遍历同时完成解析和解码
- 零拷贝设计，直接引用原始字符串
- 批量处理，减少函数调用开销

运行基准测试：
```bash
make run-benchmark
```

## 🛠️ 构建选项

### 调试构建

```bash
make debug
```

调试构建启用 AddressSanitizer 检测内存错误。

### 安装到系统

```bash
sudo make install
```

默认安装到 `/usr/local/lib` 和 `/usr/local/include`。

### 卸载

```bash
sudo make uninstall
```

## 📁 项目结构

```
llquery/
├── llquery.h          # 公共头文件
├── llquery.c          # 实现文件
├── example.c          # 示例程序
├── test_llquery.c     # 测试用例
├── benchmark.c        # 性能基准测试
├── Makefile           # 构建配置
├── README.md          # 项目说明
├── AGENTS.md          # AI 开发指南
├── .github/
│   └── workflows/     # GitHub Actions CI/CD 配置
└── doc/
    ├── API.md         # API 文档
    └── CI.md          # CI/CD 文档
```

## 🔄 持续集成

本项目使用 GitHub Actions 进行持续集成，包括：

- ✅ **功能测试**: 19 个测试用例，覆盖所有核心功能
- 🔍 **内存检测**: Valgrind 和 AddressSanitizer 检测内存泄漏
- 🚀 **性能测试**: 自动运行基准测试并报告性能指标
- 📊 **代码覆盖率**: 自动生成覆盖率报告
- 🔧 **静态分析**: cppcheck 检测潜在问题
- 🖥️ **多平台支持**: 在 Ubuntu 和 macOS 上测试

详细信息请参阅 [CI 文档](doc/CI.md)。

## 🧪 测试

项目包含完整的测试用例，覆盖各种边界情况和错误处理。

```bash
# 运行所有测试
make test

# 调试模式测试
make debug
```

## 🤝 贡献

欢迎贡献！请参阅 [AGENTS.md](AGENTS.md) 了解开发指南和代码规范。

### 贡献流程

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交更改 (`git commit -m 'Add some amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 开启 Pull Request

## 📝 许可证

本项目采用 MIT 许可证 - 详见 LICENSE 文件。

## 🔗 相关项目

- [llhttp](https://github.com/nodejs/llhttp) - 高性能 HTTP 解析器
- [llurl](https://github.com/zhaozg/llurl) - 高性能 URL 解析器

## 📮 联系方式

- 提交 Issue: [GitHub Issues](https://github.com/zhaozg/llquery/issues)
- Pull Request: [GitHub Pull Requests](https://github.com/zhaozg/llquery/pulls)

## 🙏 致谢

本项目参考了 llhttp 和 llurl 项目的设计理念和优化技术。
