#ifndef LEVELDB_UTIL_ARENA_H
#define LEVELDB_UTIL_ARENA_H
#include <atomic>
#include <cassert>
#include <vector>
namespace leveldb {
class Arena {
 public:
  Arena();
  Arena(const Arena&) = delete;
  void operator=(const Arena&) = delete;
  ~Arena();
  char* Allocate(size_t bytes);
  char* AllocateAligned(size_t bytes);
  size_t MemoryUsage() const {
    return memory_usage_.load(std::memory_order_relaxed);
  }

 private:
  char* AllocateNewBlock(size_t block_bytes);
  char* AllocateFallback(size_t bytes);
  char* alloc_ptr_;
  size_t alloc_bytes_remaining_;
  std::vector<char*> blocks_;
  std::atomic<size_t> memory_usage_;
};

inline char* Arena::Allocate(size_t bytes) {
  assert(bytes > 0);
  if (bytes <= alloc_bytes_remaining_) {
    char* result = alloc_ptr_;
    alloc_ptr_ += bytes;
    alloc_bytes_remaining_ -= bytes;
    return result;
  } else {
    return AllocateFallback(bytes);
  }
}

}  // namespace leveldb

#endif  // LEVELDB_UTIL_ARENA_H