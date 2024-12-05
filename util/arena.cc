#include "util/arena.h"

namespace leveldb {
static const int kBlockSize = 4096;
Arena::Arena()
    : alloc_ptr_(nullptr), alloc_bytes_remaining_(0), memory_usage_(0) {}

Arena::~Arena() {
  for (auto block : blocks_) {
    delete[] block;
  }
}

/**
 * @brief 在Arena内存池中分配内存
 *
 * 1.要分配的大于 block 的 1/4 →
 * AllocateNewBlock(bytes)。用的少，用多少申请多少（new char[bytes]）。
 *
 * 2.要分配的小于 block 的 1/4 →
 * AllocateNewBlock(kBlockSize)。用得小，但是我们申请一整块，这一块中空闲的就多了。以后
 * allocate
 * 时，内存池中满足要求的可能性大，就不再申请新的内存了，直接操作内存池的内存，而不再操作系统申请新的堆内存。
 *
 * @param bytes 请求的内存大小（以字节为单位）
 *
 * @return 指向分配的内存的指针，如果分配失败则返回nullptr
 */
char* Arena::AllocateFallback(size_t bytes) {
  if (bytes > kBlockSize / 4) {
    char* result = AllocateNewBlock(bytes);
    return result;
  }
  alloc_ptr_ = AllocateNewBlock(kBlockSize);
  alloc_bytes_remaining_ = kBlockSize;
  char* result = alloc_ptr_;
  alloc_ptr_ += bytes;
  alloc_bytes_remaining_ -= bytes;
  return result;
}

char* Arena::AllocateAligned(size_t bytes) {
  const int align = (sizeof(void*) > 8) ? sizeof(void*) : 8;
  static_assert((align & (align - 1)) == 0, "alignment must be power of 2");
  size_t current_mod = reinterpret_cast<uintptr_t>(alloc_ptr_) & (align - 1);
  size_t slop = (current_mod == 0) ? 0 : align - current_mod;
  size_t needed = bytes + slop;
  char* result;
  if (needed <= alloc_bytes_remaining_) {
    result = alloc_ptr_ + slop;
    alloc_ptr_ += needed;
    alloc_bytes_remaining_ -= needed;
  } else {
    result = AllocateFallback(bytes);
  }
  assert((reinterpret_cast<uintptr_t>(result) & (align - 1)) == 0);
  return result;
}

char* Arena::AllocateNewBlock(size_t block_bytes) {
  char* result = new char[block_bytes];
  blocks_.push_back(result);
  memory_usage_.fetch_add(block_bytes + sizeof(char*),
                          std::memory_order_relaxed);
  return result;
}
}  // namespace leveldb