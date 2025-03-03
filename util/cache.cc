﻿#include "leveldb/cache.h"

#include "port/port.h"
#include "port/thread_annotations.h"
#include "util/hash.h"
#include "util/mutexlock.h"

namespace leveldb {
namespace {

// dynamic allocate key_data used by malloc
struct LRUHandle {
  void* value;
  void (*deleter)(const Slice& key, void* value);
  LRUHandle* next_hash;
  LRUHandle* next;
  LRUHandle* prev;
  size_t charge;
  size_t key_length;
  bool in_cache;
  uint32_t refs;
  uint32_t hash;
  char key_data[1];
  Slice key() const {
    assert(next != this);
    return Slice(key_data, key_length);
  }
};

class HandleTable {
 public:
  HandleTable() : length_(0), elems_(0), list_(nullptr) { Resize(); }
  ~HandleTable() { delete[] list_; }

  LRUHandle* Lookup(const Slice& key, uint32_t hash) {
    return *FindPointer(key, hash);
  }

  // todo: 为什么需要双重指针？
  //  特性对比：双重指针（Insert） vs 普通指针（LRU_Append）
  //  1. 目标数据结构：
  //     - 双重指针（Insert）：哈希表中的单链表。
  //     - 普通指针（LRU_Append）：双向链表。
  //  2. 访问方式：
  //     - 双重指针：需要修改指向链表节点的指针。
  //     - 普通指针：直接通过前驱和后继指针操作节点。
  //  3. 是否需要前驱节点：
  //     - 双重指针：不需要，通过双重指针直接修改链表头指针。
  //     - 普通指针：需要，通过节点的 prev 获取前驱信息。
  //  4. 修改链表指针的范围：
  //     - 双重指针：只修改链表头或特定位置。
  //     - 普通指针：修改前驱和后继节点的多个指针。
  LRUHandle* Insert(LRUHandle* h) {
    LRUHandle** ptr = FindPointer(h->key(), h->hash);
    LRUHandle* old = *ptr;
    // if (old != nullptr) {
    //   std::swap(h->refs, old->refs);
    //   h->refs++;
    //   old->refs--;
    // }
    h->next_hash = old ? old->next_hash : nullptr;
    *ptr = h;
    if (old == nullptr) {
      ++elems_;
      if (elems_ > length_) {
        Resize();
      }
    }
    return old;
  }

  LRUHandle* Remove(const Slice& key, uint32_t hash) {
    LRUHandle** ptr = FindPointer(key, hash);
    LRUHandle* result = *ptr;
    if (result != nullptr) {
      *ptr = result->next_hash;
      --elems_;
    }
    return result;
  }

 private:
  uint32_t length_;
  uint32_t elems_;
  LRUHandle** list_;

  LRUHandle** FindPointer(const Slice& key, uint32_t hash) {
    LRUHandle** ptr = &list_[hash & (length_ - 1)];
    while (*ptr != nullptr &&
           (((*ptr)->hash != hash || key != (*ptr)->key()))) {
      ptr = &(*ptr)->next_hash;
    }
    return ptr;
  }

  void Resize() {
    uint32_t new_length = 4;
    while (new_length < elems_) {
      new_length *= 2;
    }
    LRUHandle** new_list = new LRUHandle*[new_length];
    memset(new_list, 0, sizeof(new_list[0]) * new_length);
    uint32_t count = 0;
    for (uint32_t i = 0; i < length_; i++) {
      LRUHandle* h = list_[i];
      while (h != nullptr) {
        LRUHandle* next = h->next_hash;
        uint32_t hash = h->hash;
        LRUHandle** ptr = &new_list[hash & (new_length - 1)];
        h->next_hash = *ptr;
        *ptr = h;
        h = next;
        count++;
      }
    }
    assert(elems_ == count);
    delete[] list_;
    list_ = new_list;
    length_ = new_length;
  }
};

class LRUCache {
 public:
  LRUCache();
  ~LRUCache();

  void SetCapacity(size_t capacity) { capacity_ = capacity; }
  Cache::Handle* Insert(const Slice& key, uint32_t hash, void* value,
                        size_t charge,
                        void (*deleter)(const Slice& key, void* value));
  Cache::Handle* Lookup(const Slice& key, uint32_t hash);
  void Release(Cache::Handle* handle);
  void Erase(const Slice& key, uint32_t hash);  // 从缓存中删除节点

  void Prune();
  size_t TotalCharge() const {
    MutexLock l(&mutex_);
    return usage_;
  }

 private:
  void LRU_Remove(LRUHandle* e);
  void LRU_Append(LRUHandle* list, LRUHandle* e);
  void Ref(LRUHandle* e);
  void Unref(LRUHandle* e);  // 节点引用等于0 ，才能调用free函数

  bool FinishErase(LRUHandle* e) EXCLUSIVE_LOCKS_REQUIRED(mutex_);
  size_t capacity_;
  mutable port::Mutex mutex_;
  size_t usage_ GUARDED_BY(mutex_);
  LRUHandle lru_
      GUARDED_BY(mutex_);  // lru.prev is newest entry, lru.next is oldest
                           // entry;Entries have refs==1 and in_cache==true

  LRUHandle in_use_ GUARDED_BY(mutex_);  // Entries are in use by clients, and
                                         // have refs >= 2 and in_cache==true

  HandleTable table_ GUARDED_BY(mutex_);
};

LRUCache::LRUCache() : capacity_(0), usage_(0) {
  lru_.next = &lru_;
  lru_.prev = &lru_;
  in_use_.next = &in_use_;
  in_use_.prev = &in_use_;
}

LRUCache::~LRUCache() {
  assert(in_use_.next == &in_use_);
  for (LRUHandle* e = lru_.next; e != &lru_;) {
    LRUHandle* next = e->next;
    assert(e->in_cache);
    e->in_cache = false;
    assert(e->refs == 1);
    Unref(e);
    e = next;
  }
}

void LRUCache::Ref(LRUHandle* e) {
  if (e->refs == 1 && e->in_cache) {
    LRU_Remove(e);
    LRU_Append(&in_use_, e);
  }
  e->refs++;
}

void LRUCache::Unref(LRUHandle* e) {
  assert(e->refs > 0);
  e->refs--;
  if (e->refs == 0) {
    assert(!e->in_cache);
    (*e->deleter)(e->key(), e->value);
    free(e);
  } else if (e->refs == 1 && e->in_cache) {
    LRU_Remove(e);
    LRU_Append(&lru_, e);
  }
}

void LRUCache::LRU_Remove(LRUHandle* e) {
  e->next->prev = e->prev;
  e->prev->next = e->next;
}

// make "e" newest entry by inserting just before *list
void LRUCache::LRU_Append(LRUHandle* list, LRUHandle* e) {
  e->next = list;
  e->prev = list->prev;
  e->prev->next = e;
  e->next->prev = e;
}

Cache::Handle* LRUCache::Lookup(const Slice& key, uint32_t hash) {
  MutexLock l(&mutex_);
  LRUHandle* e = table_.Lookup(key, hash);
  if (e != nullptr) {
    Ref(e);
  }
  return reinterpret_cast<Cache::Handle*>(e);
}

void LRUCache::Release(Cache::Handle* handle) {
  MutexLock l(&mutex_);
  Unref(reinterpret_cast<LRUHandle*>(handle));
}

Cache::Handle* LRUCache::Insert(const Slice& key, uint32_t hash, void* value,
                                size_t charge,
                                void (*deleter)(const Slice& key,
                                                void* value)) {
  MutexLock l(&mutex_);
  LRUHandle* e =
      reinterpret_cast<LRUHandle*>(malloc(sizeof(LRUHandle) - 1 + key.size()));
  e->value = value;
  e->deleter = deleter;
  e->charge = charge;
  e->key_length = key.size();
  e->hash = hash;
  e->in_cache = false;
  e->refs = 1;
  std::memcpy(e->key_data, key.data(), key.size());

  if (capacity_ > 0) {
    e->refs++;
    e->in_cache = true;
    LRU_Append(&in_use_, e);
    usage_ += charge;
    FinishErase(table_.Insert(e));
  } else {  // don't cache. (capacity_==0 is supported and turns off caching.)
    // next is read by key() in an assert, so it must be initialized
    // todo: don't put on lru_???
    e->next = nullptr;
  }
  while (usage_ > capacity_ && lru_.next != &lru_) {
    LRUHandle* old = lru_.next;
    assert(old->refs == 1);
    bool erased = FinishErase(table_.Remove(old->key(), old->hash));
    if (!erased) {  // to avoid unused variable when compiled NDEBUG
      assert(erased);
    }
  }
  return reinterpret_cast<Cache::Handle*>(e);
}

/**
 * @brief 完成删除缓存项的操作
 *
 * 删除指定的缓存项，并更新缓存使用情况，注意，删除只是从缓存中删除，in_cache值为false，但外面可能还在使用，所以外面仍需要release该缓存项
 *
 * @param e 要删除的缓存项的句柄
 * @return 如果成功删除，返回 true；否则返回 false
 */
bool LRUCache::FinishErase(LRUHandle* e) {
  if (e != nullptr) {
    assert(e->in_cache);
    LRU_Remove(e);
    e->in_cache = false;
    usage_ -= e->charge;
    Unref(e);
  }
  return e != nullptr;
}

// 包括哈希表内移除和lru链表移除操作，即把in_cache置为false，并把引用计数减一
void LRUCache::Erase(const Slice& key, uint32_t hash) {
  MutexLock l(&mutex_);
  FinishErase(table_.Remove(key, hash));
}

void LRUCache::Prune() {
  MutexLock l(&mutex_);
  while (lru_.next != &lru_) {
    LRUHandle* e = lru_.next;
    assert(e->refs == 1);
    bool erased = FinishErase(table_.Remove(e->key(), e->hash));
    if (!erased) {  // to avoid unused variable when compiled NDEBUG
      assert(erased);
    }
  }
}

static const int kNumShardBits = 4;
static const int kNumShards = 1 << kNumShardBits;

class ShardedLRUCache : public Cache {
 private:
  LRUCache shards_[kNumShards];
  port::Mutex id_mutex_;
  uint64_t last_id_;

  static inline uint32_t HashSlice(const Slice& key) {
    return Hash(key.data(), key.size(), 0);
  }

  static uint32_t Shard(uint32_t hash) { return hash >> (32 - kNumShardBits); }

 public:
  explicit ShardedLRUCache(size_t capacity) : last_id_(0) {
    const size_t per_shard = (capacity + kNumShards - 1) / kNumShards;
    for (int s = 0; s < kNumShards; s++) {
      shards_[s].SetCapacity(per_shard);
    }
  }
  ~ShardedLRUCache() override{};
  Handle* Insert(const Slice& key, void* value, size_t charge,
                 void (*deleter)(const Slice& key, void* value)) override {
    const uint32_t hash = HashSlice(key);
    return shards_[Shard(hash)].Insert(key, hash, value, charge, deleter);
  }

  Handle* Lookup(const Slice& key) override {
    const uint32_t hash = HashSlice(key);
    return shards_[Shard(hash)].Lookup(key, hash);
  }

  void Release(Handle* handle) override {
    LRUHandle* h = reinterpret_cast<LRUHandle*>(handle);
    shards_[Shard(h->hash)].Release(handle);
  }

  void Erase(const Slice& key) override {
    const uint32_t hash = HashSlice(key);
    shards_[Shard(hash)].Erase(key, hash);
  }

  void* Value(Handle* handle) override {
    return reinterpret_cast<LRUHandle*>(handle)->value;
  }

  uint64_t NewId() override {
    MutexLock l(&id_mutex_);
    return ++last_id_;
  }
  void Prune() override {
    for (int s = 0; s < kNumShards; s++) {
      shards_[s].Prune();
    }
  }
  size_t TotalCharge() const override {
    size_t total = 0;
    for (int s = 0; s < kNumShards; s++) {
      total += shards_[s].TotalCharge();
    }
    return total;
  }
};

}  // namespace
Cache* NewLRUCache(size_t capacity) { return new ShardedLRUCache(capacity); }
}  // namespace leveldb