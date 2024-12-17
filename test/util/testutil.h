#ifndef LEVELDB_UTIL_TESTUTIL_H_
#define LEVELDB_UTIL_TESTUTIL_H_

#include "leveldb/slice.h"

#include "util/random.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"


namespace leveldb {

namespace test {

MATCHER(IsOK, "") { return arg.ok(); }

#define EXPECT_LEVELDB_OK(expression) \
  EXPECT_THAT(expression, leveldb::test::IsOK())
#define ASSERT_LEVELDB_OK(expression) \
  ASSERT_THAT(expression, leveldb::test::IsOK())

inline int RandomSeed() {
  return testing::UnitTest::GetInstance()->random_seed();
}

// Store in *dst a random string of length "len" and return a Slice that
// references the generated data.
Slice RandomString(Random* rnd, int len, std::string* dst);

// Return a random key with the specified length that may contain interesting
// characters (e.g. \x00, \xff, etc.).
std::string RandomKey(Random* rnd, int len);

// Store in *dst a string of length "len" that will compress to
// "N*compressed_fraction" bytes and return a Slice that references
// the generated data.
Slice CompressibleString(Random* rnd, double compressed_fraction, size_t len,
                         std::string* dst);

}  // namespace test
}  // namespace leveldb

#endif  // LEVELDB_UTIL_TESTUTIL_H_