#ifndef LEVELDB_INCLUDE_SLICE_H_
#define LEVELDB_INCLUDE_SLICE_H_

#include <cassert>
#include <string>

#include "leveldb/export.h"

namespace leveldb {
class LEVELDB_EXPORT Slice {
 public:
  Slice() : data_(""), size_(0) {}
  Slice(const char* d, size_t n) : data_(d), size_(n) {}
  Slice(const std::string& s) : data_(s.data()), size_(s.size()) {}
  Slice(const char* s) : data_(s), size_(strlen(s)) {}
  Slice(const Slice& s) = default;
  Slice& operator=(const Slice& s) = default;
  const char* data() const { return data_; }
  size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  const char* begin() const { return data_; }
  const char* end() const { return data_ + size_; }
  char operator[](size_t n) const {
    assert(n < size_);
    return data_[n];
  }
  void clear() {
    data_ = "";
    size_ = 0;
  }
  void remove_prefix(size_t n) {
    assert(n <= size_);
    data_ += n;
    size_ -= n;
  }
  std::string ToString() const { return std::string(data_, size_); }
  int compare(const Slice& s) const;
  bool starts_with(const Slice& x) const {
    return (size_ >= x.size_) && (memcmp(data_, x.data_, x.size_) == 0);
  }

 private:
  const char* data_;
  size_t size_;
};

inline bool operator==(const Slice& x, const Slice& y) {
  return ((x.size() == y.size()) &&
          (memcmp(x.data(), y.data(), x.size()) == 0));
}

inline bool operator!=(const Slice& x, const Slice& y) { return !(x == y); }

inline int Slice::compare(const Slice& s) const {
  const size_t min_len = std::min(size_, s.size_);
  int r = memcmp(data_, s.data_, min_len);
  if (r == 0) {
    if (size_ < s.size_)
      return -1;
    else if (size_ > s.size_)
      return 1;
  }
  return r;
}

}  // namespace leveldb

#endif  // LEVELDB_INCLUDE_SLICE_H_