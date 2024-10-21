#include "leveldb/status.h"

#include "gtest/gtest.h"

namespace leveldb {
TEST(Status, MoveConstructor) {
  {
    Status ok = Status::OK();
    Status ok2 = std::move(ok);
  }

  {
    Status status = Status::NotFound("custom NotFound status message");
    Status status2 = std::move(status);
    ASSERT_TRUE(status2.IsNotFound());
    ASSERT_EQ("NotFound: custom NotFound status message", status2.ToString());
  }

  {
    Status self_moved = Status::IOError("custom IOError status message");
    Status& self_moved_ref = self_moved;
    self_moved_ref = std::move(self_moved);
  }
}

}  // namespace leveldb