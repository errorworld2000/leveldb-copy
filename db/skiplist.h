#ifndef LEVELDB_DB_SKIPLIST_H
#define LEVELDB_DB_SKIPLIST_H

#include "util/arena.h"
#include "util/random.h"
namespace leveldb {
template <typename Key, class Comparator>
class SkipList {
 private:
  struct Node;

 public:
  explicit SkipList(Comparator cmp, Arena* arena);
  SkipList(const SkipList&) = delete;
  SkipList& operator=(const SkipList&) = delete;

  void Insert(const Key& key);
  bool Contains(const Key& key) const;
  class Iterator {
   public:
    explicit Iterator(const SkipList* list);
    // Returns true iff the iterator is positioned at a valid node.
    bool Valid() const;
    const Key& key() const;
    void Next();
    void Prev();
    void Seek(const Key& target);
    void SeekToFirst();
    void SeekToLast();

   private:
    const SkipList* list_;
    Node* node_;
  };

 private:
  enum { kMaxHeight = 12 };

  inline int GetMaxHeight() const {
    return max_height_.load(std::memory_order_relaxed);
  }
  Node* NewNode(const Key& key, int height);
  int RandomHeight();
  bool Equal(const Key& a, const Key& b) const { return (compare_(a, b) == 0); }
  bool KeyIsAfterNode(const Key& key, Node* n) const;
  Node* FindGreaterOrEqual(const Key& key, Node** prev) const;
  Node* FindLessThan(const Key& key) const;
  Node* FindLast() const;

  Comparator const compare_;
  Arena* const arena_;
  Node* const head_;

  std::atomic<int> max_height_;
  Random rnd_;
};

template <typename Key, class Comparator>
struct SkipList<Key, Comparator>::Node {
  explicit Node(const Key& k) : key(k) {}
  Key const key;
  // memory_order_relaxed: 只确保操作是原子性的, 不对内存顺序做任何保证
  // memory_order_release: 用于写操作, 比如std::atomic::store(T,
  // memory_order_release), 会在写操作之前插入一个StoreStore屏障,
  // 确保屏障之前的所有操作不会重排到屏障之后 memory_order_acquire: 用于读操作,
  // 比如std::atomic::load(memory_order_acquire),
  // 会在读操作之后插入一个LoadLoad屏障,
  // 确保屏障之后的所有操作不会重排到屏障之前 memory_order_acq_rel:
  // 等效于memory_order_acquire和memory_order_release的组合,
  // 同时插入一个StoreStore屏障与LoadLoad屏障. 用于读写操作,
  // 比如std::atomic::fetch_add(T, memory_order_acq_rel). 线程 A (生产者)：
  // node->data = 42;  // 初始化数据
  // list.SetNext(0, node);  // 通过 memory_order_release 让数据对其他线程可见
  // 防止还没初始化完就给个数据
  // 线程 B (消费者)：
  // Node* node = list.Next(0);  // 使用 memory_order_acquire 获取节点
  // int value = node->data;  // 确保读取的是线程 A 完全初始化后的数据
  // 编译器优化
  // 编译器可能会认为：
  // node->data = 42 和 list.SetNext(0, node) 没有直接依赖关系。
  // 因此，它可能将 list.SetNext(0, node) 提前。
  // 编译器的这种优化是合法的，因为在单线程语境下，它认为不会改变行为。
  Node* Next(int n) {
    assert(n >= 0);
    return next_[n].load(std::memory_order_acquire);
  }

  void SetNext(int n, Node* x) {
    assert(n >= 0);
    next_[n].store(x, std::memory_order_release);
  }
  Node* NoBarrier_Next(int n) {
    assert(n >= 0);
    return next_[n].load(std::memory_order_relaxed);
  }
  void NoBarrier_SetNext(int n, Node* x) {
    assert(n >= 0);
    next_[n].store(x, std::memory_order_relaxed);
  }

 private:
  std::atomic<Node*> next_[1];
};

// template <typename T>
// class Example {
//  public:
// struct NestedType {};     // 嵌套类型
// static int value;         // 静态非类型成员
// };
// template <typename T>
// void Func() {
// Example<T>::NestedType* ptr;  // 嵌套类型
//  Example<T>::value = 42;       // 静态成员
//}
// 对于
// Example<T>::NestedType，编译器可能无法区分它是一个类型还是非类型成员（如变量、函数等）。
// 在模板类或函数中，如果某个嵌套成员依赖于模板参数，编译器无法在模板实例化之前确定它的具体含义。

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node* SkipList<Key, Comparator>::NewNode(
    const Key& key, int height) {
  char* const node_memory = arena_->AllocateAligned(
      sizeof(Node) +
      sizeof(std::atomic<Node*>) * (height - 1));  // 动态内存分配Node*
  return new (node_memory) Node(key);
}

template <typename Key, class Comparator>
inline SkipList<Key, Comparator>::Iterator::Iterator(const SkipList* list) {
  list_ = list;
  node_ = nullptr;
}

template <typename Key, class Comparator>
inline bool SkipList<Key, Comparator>::Iterator::Valid() const {
  return node_ != nullptr;
}

template <typename Key, class Comparator>
inline const Key& SkipList<Key, Comparator>::Iterator::key()
    const {  // 返回当前节点的key
  assert(Valid());
  return node_->key;
}
template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::Next() {
  assert(Valid());
  node_ = node_->Next(0);
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::Prev() {
  // Instead of using explicit "prev" links, we just search for the
  // last node that falls before key.
  assert(Valid());
  node_ = list_->FindLessThan(node_->key);
  if (node_ == list_->head_) {
    node_ = nullptr;
  }
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::Seek(const Key& target) {
  node_ = list_->FindGreaterOrEqual(target, nullptr);
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::SeekToFirst() {
  node_ = list_->head_->Next(0);
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::SeekToLast() {
  node_ = list_->FindLast();
  if (node_ == list_->head_) {
    node_ = nullptr;
  }
}

template <typename Key, class Comparator>
int SkipList<Key, Comparator>::RandomHeight() {
  // Increase height with probability 1 in kBranching
  static const unsigned int kBranching = 4;
  int height = 1;
  while (height < kMaxHeight && rnd_.OneIn(kBranching)) {
    height++;
  }
  assert(height > 0);
  assert(height <= kMaxHeight);
  return height;
}

template <typename Key, class Comparator>
bool SkipList<Key, Comparator>::KeyIsAfterNode(const Key& key, Node* n) const {
  return (n != nullptr) && (compare_(n->key, key) < 0);
}

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node*
SkipList<Key, Comparator>::FindGreaterOrEqual(const Key& key,
                                              Node** prev) const {
  Node* x = head_;
  int level = GetMaxHeight() - 1;
  while (true) {
    Node* next = x->Next(level);
    if (KeyIsAfterNode(key, next)) {
      x = next;
    } else {
      if (prev != nullptr) prev[level] = x;
      if (level == 0)
        return next;
      else
        level--;
    }
  }
}

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node*
SkipList<Key, Comparator>::FindLessThan(const Key& key) const {
  Node* x = head_;
  int level = GetMaxHeight() - 1;
  while (true) {
    assert(x == head_ || compare_(x->key, key) < 0);
    Node* next = x->Next(level);
    if (next == nullptr || compare_(next->key, key) >= 0) {
      if (level == 0)
        return x;
      else
        level--;
    } else {
      x = next;
    }
  }
}

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node* SkipList<Key, Comparator>::FindLast()
    const {
  Node* x = head_;
  int level = GetMaxHeight() - 1;
  while (true) {
    Node* next = x->Next(level);
    if (next == nullptr) {
      if (level == 0) {
        return x;
      } else {
        // Switch to next list
        level--;
      }
    } else {
      x = next;
    }
  }
}

template <typename Key, class Comparator>
SkipList<Key, Comparator>::SkipList(Comparator cmp, Arena* arena)
    : compare_(cmp),
      arena_(arena),
      head_(NewNode(0 /* any key will do */, kMaxHeight)),
      max_height_(1),
      rnd_(0xdeadbeef) {
  for (int i = 0; i < kMaxHeight; i++) {
    head_->SetNext(i, nullptr);
  }
}

template <typename Key, class Comparator>
void SkipList<Key, Comparator>::Insert(const Key& key) {
  Node* prev[kMaxHeight];
  Node* x = FindGreaterOrEqual(key, prev);
  assert(x == nullptr || !Equal(key, x->key));
  int height = RandomHeight();
  if (height > GetMaxHeight()) {
    for (int i = GetMaxHeight(); i < height; i++) {
      prev[i] = head_;
    }
    max_height_.store(height, std::memory_order_relaxed);
  }
  x = NewNode(key, height);
  for (int i = 0; i < height; i++) {
    // NoBarrier_SetNext() suffices since we will add a barrier when
    // we publish a pointer to "x" in prev[i].
    x->NoBarrier_SetNext(i, prev[i]->NoBarrier_Next(i));
    prev[i]->SetNext(i, x);
  }
}

template <typename Key, class Comparator>
bool SkipList<Key, Comparator>::Contains(const Key& key) const {
  Node* x = FindGreaterOrEqual(key, nullptr);
  if (x != nullptr && Equal(key, x->key)) {
    return true;
  } else {
    return false;
  }
}

}  // namespace leveldb

#endif  // LEVELDB_DB_SKIPLIST_H