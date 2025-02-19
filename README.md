<div align=center>
  
[![Star History Chart](https://api.star-history.com/svg?repos=errorworld2000/leveldb-copy&type=Date)](https://star-history.com/#errorworld2000/leveldb-copy&Date)

</div>

以下是优化后的 **TODO 列表** 和 **文档链接**，格式更加清晰，内容更加详细：

---

## **TODO 列表**

### **1. Compaction 过程**
#### **目标**
- 详细分析 LevelDB 的 Compaction 过程，包括 Minor Compaction 和 Major Compaction。
- 研究 Compaction 的触发条件、执行流程以及对性能的影响。

#### **任务**
- [ ] 阅读 LevelDB 源码，梳理 Compaction 的核心逻辑。
- [ ] 分析 Compaction 过程中 SSTable 文件的合并策略。
- [ ] 测试不同场景下 Compaction 的性能表现（如写入压力、数据量变化等）。
- [ ] 编写文档，总结 Compaction 的优化点和潜在问题。

---

### **2. DB 测试**
#### **目标**
- 对 LevelDB 的核心功能进行全面测试，确保其正确性和稳定性。
- 覆盖读写操作、持久化、恢复、并发等场景。

#### **任务**
- [ ] 编写单元测试，覆盖以下功能：
  - 数据写入和读取。
  - 数据删除和更新。
  - 快照功能。
  - 数据库恢复。
- [ ] 测试并发场景下的行为：
  - 多线程读写。
  - 多进程访问。
- [ ] 测试极端场景：
  - 大数据量写入。
  - 频繁删除和更新。
- [ ] 使用 CTest 集成测试框架，自动化运行测试用例。

---

### **3. DB 基准测试**
#### **目标**
- 对 LevelDB 的性能进行基准测试，评估其在不同场景下的表现。
- 提供性能优化建议。

#### **任务**
- [ ] 设计基准测试场景：
  - 纯写入性能测试。
  - 纯读取性能测试。
  - 读写混合性能测试。
  - 不同数据规模下的性能测试。
- [ ] 使用工具（如 `google/benchmark`）进行性能测试。
- [ ] 分析测试结果，识别性能瓶颈。
- [ ] 编写性能优化建议文档。

---

## **文档链接**
- **[整体架构](doc/leveldb.md)**  
  详细说明 LevelDB 的核心组件及其工作原理，包括 MemTable、Immutable MemTable、Log、SSTable、Manifest 等。

---
