# CI/CD Documentation

本项目使用 GitHub Actions 进行持续集成和测试。

## 工作流程概览

### 1. 主 CI 工作流 (ci.yml)

**触发条件**: Push 到 main/master/develop 分支或针对这些分支的 Pull Request

**包含的任务**:

- **构建和测试** (Build and Test)
  - 在多个操作系统上测试 (Ubuntu, macOS)
  - 使用不同编译器 (GCC, Clang)
  - 运行功能测试套件
  - 构建示例程序

- **内存泄漏检测** (Memory Leak Detection)
  - 使用 Valgrind 进行内存泄漏检测
  - 检测内存错误和未初始化的内存访问
  - 运行完整测试套件和基准测试

- **性能基准测试** (Performance Benchmark)
  - 运行完整的性能基准测试
  - 生成性能报告
  - 上传基准测试结果作为构建产物

- **代码覆盖率** (Code Coverage)
  - 使用 gcov/gcovr 生成覆盖率报告
  - 生成 HTML 和 XML 格式的报告
  - 上传覆盖率报告

- **静态分析** (Static Analysis)
  - 使用 cppcheck 进行静态代码分析
  - 检测潜在的代码问题

- **多平台测试** (Multi-Platform Test)
  - 在多个 Ubuntu 版本上测试
  - 确保跨版本兼容性

### 2. 性能跟踪工作流 (performance.yml)

**触发条件**: Push 到 main/master 分支或针对这些分支的 Pull Request

**功能**:
- 运行性能基准测试
- 提取关键性能指标（吞吐量、每秒查询数）
- 在 PR 中自动评论性能结果
- 保存基准测试结果（保留 90 天）

### 3. Sanitizer 工作流 (sanitizers.yml)

**触发条件**: Push 到 main/master/develop 分支或针对这些分支的 Pull Request

**包含的 Sanitizer**:

- **AddressSanitizer (ASAN)**
  - 检测内存错误（缓冲区溢出、use-after-free 等）
  - 检测内存泄漏

- **UndefinedBehaviorSanitizer (UBSAN)**
  - 检测未定义行为（整数溢出、空指针解引用等）

- **ThreadSanitizer (TSAN)**
  - 检测数据竞争和线程安全问题

- **MemorySanitizer (MSAN)**
  - 检测未初始化的内存读取

## 查看 CI 结果

### GitHub Actions 界面

1. 在 GitHub 仓库页面，点击 "Actions" 标签
2. 选择要查看的工作流程
3. 点击具体的运行记录查看详情

### Pull Request 检查

在 Pull Request 页面，可以看到：
- 所有 CI 检查的状态
- 性能基准测试结果会自动评论到 PR 中
- 可以点击 "Details" 查看完整的 CI 日志

### 构建产物

以下构建产物会被保存：
- 性能基准测试结果（保留 90 天）
- 代码覆盖率报告（保留 30 天）

可以在 Actions 运行页面的 "Artifacts" 部分下载。

## 本地运行 CI 测试

### 功能测试
```bash
make clean
make test
```

### 内存泄漏检测
```bash
# 安装 valgrind
sudo apt-get install valgrind

# 使用 debug 模式构建
make debug

# 运行 valgrind 检测
valgrind --leak-check=full --show-leak-kinds=all ./test_llquery
```

### AddressSanitizer
```bash
make clean
CC=clang CFLAGS="-fsanitize=address -g" make test_llquery
ASAN_OPTIONS=detect_leaks=1 ./test_llquery
```

### 性能基准测试
```bash
make run-benchmark
```

### 代码覆盖率
```bash
make clean
CFLAGS="-O0 -g -fprofile-arcs -ftest-coverage" make test_llquery
./test_llquery
gcovr --root . --print-summary
```

### 静态分析
```bash
# 安装 cppcheck
sudo apt-get install cppcheck

# 运行分析
cppcheck --enable=all --std=c99 llquery.c llquery.h
```

## CI 状态徽章

可以在 README.md 中添加以下徽章来显示 CI 状态：

```markdown
[![CI](https://github.com/zhaozg/llquery/workflows/CI/badge.svg)](https://github.com/zhaozg/llquery/actions/workflows/ci.yml)
[![Sanitizers](https://github.com/zhaozg/llquery/workflows/Sanitizers/badge.svg)](https://github.com/zhaozg/llquery/actions/workflows/sanitizers.yml)
[![Performance](https://github.com/zhaozg/llquery/workflows/Performance%20Tracking/badge.svg)](https://github.com/zhaozg/llquery/actions/workflows/performance.yml)
```

## 故障排除

### Valgrind 和 AddressSanitizer 冲突
Valgrind 和 AddressSanitizer (ASan) 不能同时使用，因为它们都会对内存操作进行插桩，会相互冲突。

**解决方案**：
- Valgrind 测试使用不带 ASan 的构建：`CC=gcc CFLAGS="-Wall -Wextra -g -std=c99" make test_llquery`
- ASan 测试在单独的工作流中运行（sanitizers.yml）

如果看到以下警告，说明构建时使用了 ASan：
```
ASan runtime does not come first in initial library list
```

### Valgrind 在 macOS 上不可用
Valgrind 在较新的 macOS 版本上不可用。在 macOS 上，CI 会跳过 Valgrind 测试，使用 AddressSanitizer 代替。

### MemorySanitizer 构建失败
MemorySanitizer 需要特殊的运行时库，在某些系统上可能无法构建。CI 配置为即使 MSan 失败也会继续。

### 性能基准测试超时
基准测试在 CI 环境中可能较慢。如果超时，可以调整 Makefile 中的迭代次数或使用 `timeout` 命令限制运行时间。

## 维护和更新

- 定期检查 GitHub Actions 的更新
- 更新依赖版本（如 actions/checkout, actions/upload-artifact）
- 根据需要调整测试矩阵（操作系统、编译器版本）
- 监控 CI 运行时间，优化慢速步骤

## 参考资料

- [GitHub Actions 文档](https://docs.github.com/en/actions)
- [Valgrind 用户手册](https://valgrind.org/docs/manual/manual.html)
- [Clang Sanitizers](https://clang.llvm.org/docs/AddressSanitizer.html)
- [gcov 文档](https://gcc.gnu.org/onlinedocs/gcc/Gcov.html)
