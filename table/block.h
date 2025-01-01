#ifndef LEVELDB_TABLE_BLOCK_H_
#define LEVELDB_TABLE_BLOCK_H_

#include "leveldb/iterator.h"

namespace leveldb {

struct BlockContents;
class Comparator;

class Block {
 public:
  explicit Block(const BlockContents& contents);
  Block(const Block&) = delete;
  Block& operator=(const Block&) = delete;

  ~Block();
  size_t size() const { return size_; }

  Iterator* NewIterator(const Comparator* comparator);

 private:
  class Iter;
  uint32_t NumRestarts() const;
  const char* data_;
  size_t size_;
  uint32_t restart_offset_;  // Offset in data_ of restart array
  bool owned_;               // Block owns data_[]
};

}  // namespace leveldb

#endif  // LEVELDB_TABLE_BLOCK_H_