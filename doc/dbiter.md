~~~C++
Iterator* DBImpl::NewIterator(const ReadOptions& options) {
  SequenceNumber latest_snapshot;
  uint32_t seed;
  Iterator* iter = NewInternalIterator(options, &latest_snapshot, &seed);
  return NewDBIterator(this, user_comparator(), iter,
                       (options.snapshot != nullptr
                            ? static_cast<const SnapshotImpl*>(options.snapshot)
                                  ->sequence_number()
                            : latest_snapshot),
                       seed);
}

Iterator* DBImpl::NewInternalIterator(const ReadOptions& options,
                                      SequenceNumber* latest_snapshot,
                                      uint32_t* seed) {
  mutex_.Lock();
  *latest_snapshot = versions_->LastSequence();

  // Collect together all needed child iterators
  std::vector<Iterator*> list;
  list.push_back(mem_->NewIterator());
  mem_->Ref();
  if (imm_ != nullptr) {
    list.push_back(imm_->NewIterator());
    imm_->Ref();
  }
  versions_->current()->AddIterators(options, &list);
  Iterator* internal_iter = NewMergingIterator(&internal_comparator_, &list[0],
                                               static_cast<int>(list.size()));
  versions_->current()->Ref();

  IterState* cleanup = new IterState(&mutex_, mem_, imm_, versions_->current());
  internal_iter->RegisterCleanup(CleanupIteratorState, cleanup, nullptr);

  *seed = ++seed_;
  mutex_.Unlock();
  return internal_iter;
}
~~~

具体代码解释如下

~~~C++
void DBIter::Next() {
  assert(valid_);

  if (direction_ == kReverse) {  // Switch directions?
    direction_ = kForward;
    // iter_ is pointing just before the entries for this->key(),
    // so advance into the range of entries for this->key() and then
    // use the normal skipping code below.
    if (!iter_->Valid()) {
      iter_->SeekToFirst();
    } else {
      iter_->Next();
    }
    if (!iter_->Valid()) {
      valid_ = false;
      saved_key_.clear();
      return;
    }
    // saved_key_ already contains the key to skip past.
  } else {
    // Store in saved_key_ the current key so we skip it below.
    SaveKey(ExtractUserKey(iter_->key()), &saved_key_);

    // iter_ is pointing to current key. We can now safely move to the next to
    // avoid checking current key.
    iter_->Next();
    if (!iter_->Valid()) {
      valid_ = false;
      saved_key_.clear();
      return;
    }
  }

  FindNextUserEntry(true, &saved_key_);
}

void DBIter::FindNextUserEntry(bool skipping, std::string* skip) {
  // Loop until we hit an acceptable entry to yield
  assert(iter_->Valid());
  assert(direction_ == kForward);
  do {
    ParsedInternalKey ikey;	
    if (ParseKey(&ikey) && ikey.sequence <= sequence_) {	// 根据当前iter_的key封装成一个新ikey，同时要求ikey的序列号小于创建DBIter时传入的序列号
      switch (ikey.type) {
        case kTypeDeletion:
          // Arrange to skip all upcoming entries for this key since
          // they are hidden by this deletion.
          SaveKey(ikey.user_key, skip);		
          skipping = true;		// 如果当前ikey的类型是删除，将跳过整个 user_key = ikey.user_key()的nodes
          break;
        case kTypeValue:
          if (skipping &&	// 跳过
              user_comparator_->Compare(ikey.user_key, *skip) <= 0) {	// 旧条目不再需要
            // Entry hidden
          } else {	// 否则就是本 user_key 所在nodes中的最新节点
            valid_ = true;
            saved_key_.clear();
            return;
          }
          break;
      }
    }
    iter_->Next();	// 定位到下一个
  } while (iter_->Valid());
  saved_key_.clear();
  valid_ = false;
}
~~~

#### 1. `DBIter::Next()`

- **作用：**
  实现迭代器前进到下一个用户可见的条目。

- **核心逻辑：**

  1. **断言当前状态有效：**
     `assert(valid_);` 确保调用 `Next()` 时，迭代器处于有效状态。

  2. **处理不同方向：**

     - 如果当前迭代方向为 `kReverse`，说明之前的操作是反向遍历：

       切换方向为 `kForward`。因为反向遍历时迭代器指向当前 key 之前的位置，所以需要先移动一步使其进入当前 key 的区间：若当前迭代器无效，则调用 `SeekToFirst()` 重置到首元素。否则调用 `iter_->Next()`。若移动后迭代器变为无效，则标记迭代器为无效并清空 `saved_key_`，返回。此时，`saved_key_` 内保存了需要跳过的 key。

     - 当方向已经是`kForward`时：

       首先将当前 key 提取后保存到 `saved_key_`，用于后续跳过同一用户键的所有条目（防止重复返回）。调用 `iter_->Next()` 移动到下一个内部条目。同样，若移动后无效，则更新状态并返回。

  3. **寻找下一个用户条目：**
     最后调用 `FindNextUserEntry(true, &saved_key_)`，从当前位置继续寻找一个用户可见的有效条目。

------

#### 2. `DBIter::FindNextUserEntry(bool skipping, std::string* skip)`

- **作用：**
  在内部迭代器当前位置开始，寻找下一个用户层面上“有效”（即未被删除、未被隐藏）的条目。

- **核心逻辑：**

  1. **循环遍历：**
     循环直到找到一个符合条件的用户条目，或者迭代器到达末尾。

  2. **解析内部键：**
     每次循环调用 `ParseKey(&ikey)` 解析当前内部键 `iter_->key()`，同时检查 `ikey.sequence` 是否在允许的序列号范围内（`<= sequence_`）。

  3. **根据操作类型处理：**

     - 删除类型（`kTypeDeletion`）：

       将当前 `ikey.user_key` 保存到 `skip`（即 `saved_key_`），并设置 `skipping` 为 `true`，表示后续相同用户键的条目应当被跳过（删除操作“屏蔽”了旧值）。

     - 值类型（`kTypeValue`）：

       如果处于跳过状态（`skipping` 为 true），并且当前 `ikey.user_key` 与 `*skip` 比较小于等于（即还处于需要跳过的范围内），则认为该条目被删除或隐藏，不予返回。否则，找到了一个有效的用户条目，将 `valid_` 标记为 `true`，清空 `saved_key_`（因为找到有效数据后，不需要再跳过）。返回，结束循环。

  4. **结束条件：**

     - 如果循环结束时 `iter_->Valid()` 为 false，则表示已经遍历完所有条目，
     - 清空 `saved_key_`，并将 `valid_` 标记为 false。