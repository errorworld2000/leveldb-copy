
#ifndef LEVELDB_UTIL_HASH_H
#define LEVELDB_UTIL_HASH_H

#include <cstdint>
namespace leveldb {
uint32_t Hash(const char* data, size_t n, uint32_t seed);
}  // namespace leveldb

#endif  // LEVELDB_UTIL_HASH_H