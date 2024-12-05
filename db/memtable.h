#ifndef LEVELDB_DB_MEMTABLE_H
#define LEVELDB_DB_MEMTABLE_H

#include "db/dbformat.h"
#include "db/skiplist.h"

#include "leveldb/db.h"

#include "util/arena.h"


namespace leveldb {
class InternalKeyComparator;
class MemTableIterator;

class MemTable {
 public:
  explicit MemTable(const InternalKeyComparator& comparator);
  MemTable(const MemTable&) = delete;
  MemTable& operator=(const MemTable&) = delete;
  void Ref() { ++refs_; }
  void Unref() {
    --refs_;
    assert(refs_ >= 0);
    if (refs_ <= 0) {
      delete this;
    }
  }
  // Returns an estimate of the number of bytes of data in use by this
  // data structure. It is safe to call when MemTable is being modified.
  size_t ApproximateMemoryUsage();

  // Return an iterator that yields the contents of the memtable.
  //
  // The caller must ensure that the underlying MemTable remains live
  // while the returned iterator is live.  The keys returned by this
  // iterator are internal keys encoded by AppendInternalKey in the
  // db/format.{h,cc} module.
  Iterator* NewIterator();

  // Add an entry into memtable that maps key to value at the
  // specified sequence number and with the specified type.
  // Typically value will be empty if type==kTypeDeletion.
  void Add(SequenceNumber seq, ValueType type, const Slice& key,
           const Slice& value);

  // If memtable contains a value for key, store it in *value and return true.
  // If memtable contains a deletion for key, store a NotFound() error
  // in *status and return true.
  // Else, return false.
  bool Get(const LookupKey& key, std::string* value, Status* s);

 private:
  ~MemTable();
  friend class MemTableIterator;
  friend class MemTableBackwardIterator;

  struct KeyComparator {
    const InternalKeyComparator comparator;
    explicit KeyComparator(const InternalKeyComparator& c) : comparator(c) {}
    int operator()(const char* a, const char* b) const;
  };
  using Table = SkipList<const char*, KeyComparator>;

  KeyComparator comparator_;
  int refs_;
  Arena arena_;
  Table table_;
};
}  // namespace leveldb

#endif  // LEVELDB_DB_MEMTABLE_H