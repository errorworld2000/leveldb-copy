#include "leveldb/status.h"
namespace leveldb {

Status::Status(Code code, const Slice& msg, const Slice& msg2) {
  assert(code != kOk);
  const uint32_t len1 = static_cast<uint32_t>(msg.size());
  const uint32_t len2 = static_cast<uint32_t>(msg2.size());
  const uint32_t size = len1 + (len2 ? (2 + len2) : 0);
  char* result = new char[size + 5];
  std::memcpy(result, &size, sizeof(size));
  result[4] = static_cast<char>(code);
  std::memcpy(result + 5, msg.data(), len1);
  if (len2) {
    result[5 + len1] = ':';
    result[6 + len1] = ' ';
    std::memcpy(result + 7 + len1, msg2.data(), len2);
  }
  state_ = result;
}

std::string Status::ToString() const {
  std::string result;
  switch (code()) {
    case kOk:
      return "OK";
    case kNotFound:
      result = "NotFound: ";
      break;
    case kCorruption:
      result = "Corruption: ";
      break;
    case kNotSupported:
      result = "Not implemented: ";
      break;
    case kInvalidArgument:
      result = "Invalid argument: ";
      break;
    case kIOError:
      result = "IOError: ";
      break;
    default:
      result =
          "Unkown code(" + std::to_string(static_cast<int>(code())) + "): ";
      break;
  }
  uint32_t length;
  std::memcpy(&length, state_, sizeof(length));
  result.append(state_ + 5, length);
  return result;
}

}  // namespace leveldb