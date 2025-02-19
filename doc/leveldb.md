### Leveldb 整体架构

Leveldb 是一个高性能的键值存储库，由 Google 开发。它的设计目标是提供快速的读写操作，同时支持高效的压缩和持久化。以下是 Leveldb的核心组件及其工作原理的详细说明：

---

### **1. MemTable**
#### **概述**
- **MemTable** 是 Leveldb 在内存中用于组织和维护数据的结构。
- 写入操作不会直接刷新到磁盘文件，而是先写入 MemTable，因此写入性能非常高。

#### **特点**
- 基于 **SkipList** 实现，支持高效的插入、删除和查找操作。
- 数据按键排序存储，便于后续的持久化操作。

#### **详情**
- 详情见 [内存数据库](内存数据库.md)。

---

### **2. Immutable MemTable**
#### **概述**
- 当 MemTable 的大小达到预设阈值时，它会转换为 **Immutable MemTable**。
- Immutable MemTable 是不可修改的，只允许读取操作。

#### **作用**
- 后台压缩进程会将 Immutable MemTable 中的数据转换为 **SSTable**，并持久化到磁盘中。
- 这种设计避免了写入阻塞，同时保证了数据的持久化。

---

### **3. Log (Write-Ahead Log, WAL)**
#### **概述**
- Leveldb 的写入操作首先会顺序写入日志文件（Log），然后再写入 MemTable。
- 日志文件用于在系统崩溃时恢复未持久化的数据。

#### **特点**
- 日志文件是追加写入的，因此写入性能非常高。
- 日志文件与 MemTable 的数据保持一致，确保数据的可靠性。

#### **详情**
- 详情见 [日志](日志.md)。

---

### **4. SSTable (Sorted String Table)**
#### **概述**
- 当内存中的数据超过一定阈值时，Leveldb会将数据持久化到磁盘，形成 **SSTable** 文件。
- SSTable 是 Leveldb在磁盘上存储数据的主要形式。

#### **层级结构**
- SSTable 文件在逻辑上分为多个层级（Level），最底层为 **Level 0**，由内存直接持久化生成。
- 更高层级的 SSTable 文件通过 **Compaction** 操作整合生成，以减少文件之间的交集。

#### **详情**
- 详情见 [SSTable](SSTable.md)。

---

### **5. Manifest**
#### **概述**
- **Manifest** 文件记录了 Leveldb的元数据信息，包括 SSTable 文件的元信息、版本变更等。

#### **核心组件**
- **FileMetaData**：记录 SSTable 文件的元信息，包括文件编号、文件大小、最小键、最大键等。
- **VersionEdit**：记录数据库版本的变更，可能包含新增的文件元信息、删除的文件等。

#### **工作原理**
- Manifest 文件与 Write-Ahead Log (WAL) 使用相同的存储方式。
- 每次数据库启动时，都会通过 Manifest 文件恢复最新的版本信息。

#### **详情**
- 详情见 [Manifest](Manifest.md)。

---

### **6. 读写操作**
#### **概述**
- Leveldb的读写操作是其核心功能，设计目标是高效、可靠。

#### **写操作**
- 数据首先写入日志文件（Log），然后写入 MemTable。
- 当 MemTable 达到阈值时，转换为 Immutable MemTable，并触发后台压缩。

#### **读操作**
- 数据首先从 MemTable 和 Immutable MemTable 中查找。
- 如果未找到，则从磁盘中的 SSTable 文件中查找。

#### **详情**
- 详情见 [读写操作](读写操作.md)。

---

### **7. 缓存系统**
#### **概述**
- Leveldb使用缓存系统来加速读取操作，减少磁盘 I/O。

#### **核心组件**
- **Block Cache**：缓存 SSTable 文件中的数据块，加速读取操作。
- **Table Cache**：缓存 SSTable 文件的元信息，减少文件打开和关闭的开销。

#### **详情**
- 详情见 [缓存系统](缓存系统.md)。

---

### **8. Compaction**
#### **概述**
- **Compaction** 是 Leveldb 的核心机制之一，用于整合和优化 SSTable 文件。

#### **作用**
- 减少 SSTable 文件之间的交集，提高读取效率。
- 清理过期或删除的数据，释放磁盘空间。

#### **类型**
- **Minor Compaction**：将 Immutable MemTable 转换为 SSTable 文件。
- **Major Compaction**：整合多个 SSTable 文件，生成更高层级的文件。

#### **详情**
- 详情见 [Compaction](Compaction.md)。

---

### **总结**
Leveldb的整体架构通过 **MemTable**、**Immutable MemTable**、**Log**、**SSTable**、**Manifest** 等组件的协同工作，实现了高效的读写操作和持久化存储。其设计充分考虑了性能、可靠性和可扩展性，适用于多种应用场景。

---

通过以上扩展，Leveldb的架构和核心组件得到了更清晰的展示，便于理解和学习。