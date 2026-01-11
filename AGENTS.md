# AGENTS.md

llquery 为超高速 C 语言 query 解析库（llquery）， 采用状态机与查表优化。

本文档为 AI 开发者提供开发约束、任务指导。

## 1. 概述

- Agent 角色：本项目未直接实现智能体 但所有解析、测试、优化、文档等任务
均可通过智能体化（如 AI 代码助手）辅助完成。
- 核心目标：高性能、零依赖、API 兼容、易于扩展和测试。

## 2. 主要接口与职责

- `http_parser_query_init(struct http_parser_query *u)`：初始化 query 结构体。
- `http_parser_parse_query(const char *buf, size_t buflen, int is_connect, struct http_parser_query *u)`：解析 query，填充结构体。
- 结构体 `http_parser_query`：存储各 query 字段偏移、长度、端口等。
- 详见 `llquery.h` 头文件。

## 3. 任务类型

### 3.1 开发任务

- 新增或优化解析状态机、查表逻辑。
- 扩展支持更多协议或特殊 query 形式。
- 保持 API 兼容性，避免破坏现有接口。

### 3.2 优化任务

- 基于性能报告（见 `doc/BENCHMARKS.md`）定位瓶颈，分支预测等优化。
- 优化内存访问、减少分支、提升查表效率。
- 保持线程安全与零全局状态。

### 3.3 文档任务

- 完善 API 注释、用例说明、边界条件说明。
- 维护测试用例文档（见 `doc/TESTING.md`）。
- 记录优化思路、性能对比、设计权衡（见 `doc/OPTIMIZATION.md`、`doc/BENCHMARKS.md`）。

## 4.4 开发与扩展指南

- 遵循 C99 标准，避免平台相关代码。
- 所有结构体、函数必须文档化，注释需说明线程安全性、输入输出、边界条件。
- 新增字段或接口需兼容旧 API，必要时通过宏控制。
- 测试驱动开发：所有新功能需配套测试。
- 性能优化建议：优先查表、批量处理、减少循环内分支。

## 5. 贡献与协作流程

1. Fork 仓库，创建 feature 分支。
2. 开发/优化/补充文档，确保所有测试通过（`make test-all`）。
3. 补充/更新相关文档与注释。
4. 提交 Pull Request，描述变更点与优化点。
5. 由维护者 review 并合并。

## 6. 文档与代码规范

- 代码注释需中英文结合，关键逻辑建议详细说明。
- 文档结构清晰，分章节、分任务类型描述。
- 参考现有 `README.md`、`doc/` 目录文档风格。

## 7. Agent 能力建议

- 能自动分析状态机与查表逻辑，定位性能瓶颈。
- 能自动生成/补全测试用例。
- 能自动补全 API 注释与用例文档。
- 能根据性能报告自动提出优化建议。

## 8. 参考资料

- [llhttp](https://github.com/nodejs/llhttp)
- [llurl](https://github.com/zhaozg/llurl)
- RFC 3986
- 本项目 `README.md`、`doc/` 目录

---

如需智能体协助开发、优化、文档任务，请参考本指南，结合现有代码与文档规范进行。
