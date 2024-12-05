#ifndef LEVELDB_INCLUDE_EXPORT_H_
#define LEVELDB_INCLUDE_EXPORT_H_

#ifndef LEVELDB_EXPORT

#ifdef LEVELDB_SHARED_LIBRARY
#ifdef _WIN32

#ifdef LEVELDB_COMPILE_LIBRARY
#define LEVELDB_EXPORT __declspec(dllexport)
#else
#define LEVELDB_EXPORT __declspec(dllimport)
#endif  // LEVELDB_COMPILE_LIBRARY

#else  //_WIN32

#ifdef LEVELDB_COMPILE_LIBRARY
#define LEVELDB_EXPORT __attribute__((visibility("default")))
#else
#define LEVELDB_EXPORT
#endif  // LEVELDB_COMPILE_LIBRARY
#endif  //_WIN32

#else  // LEVELDB_SHARED_LIBRARY
#define LEVELDB_EXPORT
#endif  // LEVELDB_SHARED_LIBRARY

#endif  // LEVELDB_EXPORT

#endif  // LEVELDB_INCLUDE_EXPORT_H_