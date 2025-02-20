﻿#include "db/skiplist.h"

#include "leveldb/env.h"

#include "port/port.h"
#include "port/thread_annotations.h"
#include "util/hash.h"

#include "gtest/gtest.h"
#include "test/util/testutil.h"

namespace leveldb {
typedef uint64_t Key;
struct Comparator {
  int operator()(const Key& a, const Key& b) const {
    if (a < b)
      return -1;
    else if (a > b)
      return 1;
    else
      return 0;
  }
};

TEST(SkipListTest, Empty) {
  Arena arena;
  Comparator cmp;
  SkipList<Key, Comparator> list(cmp, &arena);
  ASSERT_TRUE(!list.Contains(1));
  SkipList<Key, Comparator>::Iterator iter(&list);
  ASSERT_TRUE(!iter.Valid());
  iter.SeekToFirst();
  ASSERT_TRUE(!iter.Valid());
  iter.SeekToLast();
  ASSERT_TRUE(!iter.Valid());
  iter.Seek(100);
  ASSERT_TRUE(!iter.Valid());
}

TEST(SkipListTest, InsertAndLookup) {
  const int N = 2000;
  const int R = 5000;
  Random rnd(1000);
  std::set<Key> keys;
  Arena arena;
  Comparator cmp;
  SkipList<Key, Comparator> list(cmp, &arena);
  for (int i = 0; i < N; i++) {
    Key key = rnd.Next() % R;
    if (keys.insert(key).second) {
      list.Insert(key);
    }
  }
  for (int i = 0; i < R; i++) {
    if (list.Contains(i)) {
      ASSERT_EQ(keys.count(i), 1);
    } else {
      ASSERT_EQ(keys.count(i), 0);
    }
  }

  /////// Iteration test ///////
  SkipList<Key, Comparator>::Iterator iter(&list);
  ASSERT_TRUE(!iter.Valid());
  iter.Seek(0);
  ASSERT_TRUE(iter.Valid());
  ASSERT_EQ(*(keys.begin()), iter.key());
  iter.SeekToFirst();
  ASSERT_TRUE(iter.Valid());
  ASSERT_EQ(*(keys.begin()), iter.key());
  iter.SeekToLast();
  ASSERT_TRUE(iter.Valid());
  ASSERT_EQ(*(keys.rbegin()), iter.key());

  /////// forward iteration test ///////
  {
    SkipList<Key, Comparator>::Iterator iter(&list);
    for (int i = 0; i < R; i++) {
      iter.Seek(i);
      auto model_iter = keys.lower_bound(i);
      for (int j = 0; j < 3; j++) {
        if (model_iter == keys.end()) {
          ASSERT_TRUE(!iter.Valid());
          break;
        } else {
          ASSERT_TRUE(iter.Valid());
          ASSERT_EQ(*model_iter, iter.key());
          ++model_iter;
          iter.Next();
        }
      }
    }
  }

  /////// backward iteration test ///////
  {
    SkipList<Key, Comparator>::Iterator iter(&list);
    iter.SeekToLast();
    for (auto model_iter = keys.rbegin(); model_iter != keys.rend();
         ++model_iter) {
      ASSERT_TRUE(iter.Valid());
      ASSERT_EQ(*model_iter, iter.key());
      iter.Prev();
    }
    ASSERT_TRUE(!iter.Valid());
  }
}

class ConcurrentTest {
 private:
  static constexpr uint32_t K = 4;

  // Helper functions for key operations
  static uint64_t key(Key key) { return key >> 40; }
  static uint64_t gen(Key key) { return (key >> 8) & 0xffffffffu; }
  static uint64_t hash(Key key) { return key & 0xff; }

  // Combine k and g to produce a hash value
  static uint64_t HashNumbers(uint64_t k, uint64_t g) {
    uint64_t data[] = {k, g};
    return Hash(reinterpret_cast<char*>(data), sizeof(data), 0);
  }

  // Create a Key from k and g
  static Key MakeKey(uint64_t k, uint64_t g) {
    static_assert(sizeof(Key) == sizeof(uint64_t), "Key must be 64 bits");
    assert(k <= K && g <= 0xffffffffu);
    return (k << 40) | (g << 8) | (HashNumbers(k, g) & 0xff);
  }

  // Validate a given Key
  static bool IsValidKey(Key k) {
    return hash(k) == (HashNumbers(key(k), gen(k)) & 0xff);
  }

  static Key RandomTarget(Random* rnd) {
    switch (rnd->Next() % 10) {
      case 0:
        return MakeKey(0, 0);
      case 1:
        return MakeKey(K, 0);
      default:
        return MakeKey(rnd->Next() % K, 0);
    }
  }

  struct State {
    std::atomic<int> generation[K];
    void Set(int k, int v) {
      generation[k].store(v, std::memory_order_release);
    }
    int Get(int k) { return generation[k].load(std::memory_order_acquire); }
    State() {
      for (int k = 0; k < K; k++) Set(k, 0);
    }
  };
  State current_;
  Arena arena_;
  SkipList<Key, Comparator> list_;

 public:
  ConcurrentTest() : list_(Comparator(), &arena_) {}
  void WriteStep(Random* rnd) {
    const uint32_t k = rnd->Next() % K;
    const intptr_t g = current_.Get(k) + 1;
    const Key key = MakeKey(k, g);
    list_.Insert(key);
    current_.Set(k, static_cast<int>(g));
  }

  void ReadStep(Random* rnd) {
    // Remember the initial committed state of the skiplist.
    State initial_state;
    for (int k = 0; k < K; k++) {
      initial_state.Set(k, current_.Get(k));
    }

    Key pos = RandomTarget(rnd);
    SkipList<Key, Comparator>::Iterator iter(&list_);
    iter.Seek(pos);
    while (true) {
      Key current;
      if (!iter.Valid()) {
        current = MakeKey(K, 0);
      } else {
        current = iter.key();
        ASSERT_TRUE(IsValidKey(current)) << current;
      }
      ASSERT_LE(pos, current) << "should not go backwards";

      // Verify that everything in [pos,current) was not present in
      // initial_state.
      while (pos < current) {
        ASSERT_LT(key(pos), K) << pos;

        // Note that generation 0 is never inserted, so it is ok if
        // <*,0,*> is missing.
        ASSERT_TRUE((gen(pos) == 0) ||
                    (gen(pos) > static_cast<Key>(initial_state.Get(
                                    static_cast<int>(key(pos))))))
            << "key: " << key(pos) << "; gen: " << gen(pos)
            << "; init_gen: " << initial_state.Get(static_cast<int>(key(pos)));

        // Advance to next key in the valid key space
        if (key(pos) < key(current)) {
          pos = MakeKey(key(pos) + 1, 0);
        } else {
          pos = MakeKey(key(pos), gen(pos) + 1);
        }
      }

      if (!iter.Valid()) {
        break;
      }

      if (rnd->Next() % 2) {
        iter.Next();
        pos = MakeKey(key(pos), gen(pos) + 1);
      } else {
        Key new_target = RandomTarget(rnd);
        if (new_target > pos) {
          pos = new_target;
          iter.Seek(new_target);
        }
      }
    }
  }
};
constexpr uint32_t ConcurrentTest::K;

// Simple test that does single-threaded testing of the ConcurrentTest
// scaffolding.
TEST(SkipTest, ConcurrentWithoutThreads) {
  ConcurrentTest test;
  Random rnd(test::RandomSeed());
  for (int i = 0; i < 10000; i++) {
    test.ReadStep(&rnd);
    test.WriteStep(&rnd);
  }
}

class TestState {
 public:
  ConcurrentTest t_;
  int seed_;
  std::atomic<bool> quit_flag_;
  enum ReaderState { STARTING, RUNNING, DONE };
  explicit TestState(int s)
      : seed_(s), quit_flag_(false), state_(STARTING), state_cv_(&mu_) {}

  void Wait(ReaderState s) LOCKS_EXCLUDED(mu_) {
    mu_.Lock();
    while (state_ != s) {
      state_cv_.Wait();
    }
    mu_.Unlock();
  }
  void Change(ReaderState s) LOCKS_EXCLUDED(mu_) {
    mu_.Lock();
    state_ = s;
    state_cv_.Signal();
    mu_.Unlock();
  }

 private:
  port::Mutex mu_;
  ReaderState state_ GUARDED_BY(mu_);
  port::CondVar state_cv_ GUARDED_BY(mu_);
};

static void ConcurrentReader(void* arg) {
  TestState* state = reinterpret_cast<TestState*>(arg);
  Random rnd(state->seed_);
  int64_t reads = 0;
  state->Change(TestState::RUNNING);
  while (!state->quit_flag_.load(std::memory_order_acquire)) {
    state->t_.ReadStep(&rnd);
    ++reads;
  }
  state->Change(TestState::DONE);
}

static void RunConcurrent(int run) {
  const int seed = test::RandomSeed() + (run * 100);
  Random rnd(seed);
  const int N = 1000;
  const int kSize = 1000;
  for (int i = 0; i < N; i++) {
    if ((i % 100) == 0) {
      std::fprintf(stderr, "Run %d of %d\n", i, N);
    }
    TestState state(seed + 1);
    Env::Default()->Schedule(ConcurrentReader, &state);
    state.Wait(TestState::RUNNING);
    for (int i = 0; i < kSize; i++) {
      state.t_.WriteStep(&rnd);
    }
    state.quit_flag_.store(true, std::memory_order_release);
    state.Wait(TestState::DONE);
  }
}

TEST(SkipTest, Concurrent1) { RunConcurrent(1); }
TEST(SkipTest, Concurrent2) { RunConcurrent(2); }
TEST(SkipTest, Concurrent3) { RunConcurrent(3); }
TEST(SkipTest, Concurrent4) { RunConcurrent(4); }
TEST(SkipTest, Concurrent5) { RunConcurrent(5); }

}  // namespace leveldb