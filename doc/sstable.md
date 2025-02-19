

------

## SSTable

### 物理结构

SSTable 按块划分，默认数据块大小为 4KiB。每个数据块除了存储数据外，还包括两个辅助字段：压缩类型和 CRC 校验码。

### 逻辑结构

![SSTable 逻辑结构](img/sst-format.png)

1. **Data Block**：存储键值对。
2. **Filter Block**：存储过滤器相关数据，采用专门的 FilterBlock。
3. **Meta Index Block**：存储 Filter Block 在 SSTable 文件中的位置和长度。
4. **Index Block**：存储每个 Data Block 的索引信息。
5. **Footer**：存储 Meta Index Block 和 Index Block 的索引信息。

### 读写过程

#### 写过程

![写过程](file:///D:/%5CRepos%5Cleveldb-copy%5Cdoc%5Cimg%5Ctable-builder.png)

#### 读过程

![读过程](file:///D:/%5CRepos%5Cleveldb-copy%5Cdoc%5Cimg%5Csst-read.png)

SSTable 使用两种缓存机制：

1. **Table Cache**：存储已打开的 SSTable 文件。
2. **Block Cache**：存储已读取的数据块。

Table Cache 的结构如下：

![Table Cache 结构](file:///D:/%5CRepos%5Cleveldb-copy%5Cdoc%5Cimg%5Ctable-cache.png)

### Table 的迭代器

#### TwoLevelIterator

`TwoLevelIterator` 用于高效处理两层索引结构。它结合索引块和数据块，提供了一种高效的访问机制。

```cpp
using BlockFunction = Iterator* (*)(void*, const ReadOptions&, const Slice&);
explicit TwoLevelIterator(Iterator* index_iter, BlockFunction block_function,
                          void* arg, const ReadOptions& options);
//使用 arg 而不是直接传递 Table* 主要是为了设计的灵活性和接口的一致性。
```

#### 实现思路

1. **索引块迭代器**：遍历索引块的迭代器，用来获取每个数据块的元信息。
2. **数据块读取函数**：提供读取数据块的函数，该函数由 `TwoLevelIterator` 调用以加载具体的数据块。
3. **封装 `TwoLevelIterator`**：结合索引块迭代器和数据块读取函数，创建 `TwoLevelIterator`。

------

