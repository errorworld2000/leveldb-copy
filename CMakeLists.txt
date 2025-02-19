cmake_minimum_required(VERSION 3.11)
project(leveldb VERSION 1.23.0 LANGUAGES C CXX)

message(STATUS "Compiler definitions: ${CMAKE_CXX_FLAGS}")


if(NOT CMAKE_C_STANDARD)
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED OFF)
    set(CMAKE_C_EXTENSIONS OFF)
endif(NOT CMAKE_C_STANDARD)

if(NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif(NOT CMAKE_CXX_STANDARD)

if (WIN32)
  set(LEVELDB_PLATFORM_NAME LEVELDB_PLATFORM_WINDOWS)
  # TODO(cmumford): Make UNICODE configurable for Windows.
  add_definitions(-D_UNICODE -DUNICODE)
else (WIN32)
  set(LEVELDB_PLATFORM_NAME LEVELDB_PLATFORM_POSIX)
endif (WIN32)

message(STATUS "LEVELDB_PLATFORM_NAME: ${LEVELDB_PLATFORM_NAME}")
message(STATUS "PROJECT_BINARY_DIR: ${PROJECT_BINARY_DIR}")


include(CheckLibraryExists)
check_library_exists(crc32c crc32c_value "" HAVE_CRC32C)
check_library_exists(snappy snappy_compress "" HAVE_SNAPPY)
check_library_exists(zstd zstd_compress "" HAVE_ZSTD)
check_library_exists(tcmalloc malloc "" HAVE_TCMALLOC)

# ///////////////////////////////////文件//////////////////////////////////////////
# 定义一个通用函数来分离普通源文件和测试源文件
function(separate_sources DIR HEADERS_VAR SOURCES_VAR TEST_SOURCES_VAR)
  # 获取指定目录中的所有 .h 文件
  file(GLOB ALL_HEADERS "${DIR}/*.h")
  set(${HEADERS_VAR} ${ALL_HEADERS} PARENT_SCOPE)

  # 获取指定目录中的所有 .cc 文件
  file(GLOB ALL_SOURCES "${DIR}/*.cc")

  # 筛选测试文件 (_test.cc)
  list(FILTER ALL_SOURCES INCLUDE REGEX "_test\\.cc$")
  set(${TEST_SOURCES_VAR} ${ALL_SOURCES} PARENT_SCOPE)

  # 获取非测试文件
  file(GLOB NON_TEST_SOURCES "${DIR}/*.cc")
  list(FILTER NON_TEST_SOURCES EXCLUDE REGEX "_test\\.cc$")
  set(${SOURCES_VAR} ${NON_TEST_SOURCES} PARENT_SCOPE)
endfunction()

# 使用该函数对 db、port、table 和 util 文件夹进行分类
separate_sources("${CMAKE_CURRENT_SOURCE_DIR}/db" DB_HEADERS DB_SOURCES DB_TEST_SOURCES)
separate_sources("${CMAKE_CURRENT_SOURCE_DIR}/port" PORT_HEADERS PORT_SOURCES PORT_TEST_SOURCES)
separate_sources("${CMAKE_CURRENT_SOURCE_DIR}/table" TABLE_HEADERS TABLE_SOURCES TABLE_TEST_SOURCES)
separate_sources("${CMAKE_CURRENT_SOURCE_DIR}/util" UTIL_HEADERS UTIL_SOURCES UTIL_TEST_SOURCES)
file(GLOB TEST_UTIL "test/util/*")
# message(STATUS "PORT_HEADERS: ${PORT_HEADERS}")

set(LEVELDB_PUBLIC_INCLUDE_DIR "include/leveldb")
set(LEVELDB_PORT_CONFIG_DIR "include/port")

configure_file(
  "port/port_config.h.in"
  "${PROJECT_BINARY_DIR}/${LEVELDB_PORT_CONFIG_DIR}/port_config.h"
)

include_directories(
  "${PROJECT_BINARY_DIR}/include"
  # "include"
  "."
)

if(BUILD_SHARED_LIBS)
  # Only export LEVELDB_EXPORT symbols from the shared library.
  add_compile_options(-fvisibility=hidden)
endif(BUILD_SHARED_LIBS)



# ///////////////////////////////////构建leveldb库//////////////////////////////////////////
add_library(leveldb "")
target_sources(leveldb 
  PRIVATE
    "${PROJECT_BINARY_DIR}/${LEVELDB_PORT_CONFIG_DIR}/port_config.h"
    ${DB_SOURCES} ${DB_HEADERS}
    ${PORT_SOURCES} ${PORT_HEADERS}
    ${TABLE_SOURCES} ${TABLE_HEADERS}
    ${UTIL_SOURCES} ${UTIL_HEADERS}
  # $<$<VERSION_GREATER:CMAKE_VERSION,3.2>:PUBLIC>
  #   "${LEVELDB_PUBLIC_INCLUDE_DIR}/cache.h"
  #   "${LEVELDB_PUBLIC_INCLUDE_DIR}/export.h"
  #   "${LEVELDB_PUBLIC_INCLUDE_DIR}/iterator.h"
  #   "${LEVELDB_PUBLIC_INCLUDE_DIR}/slice.h"
  #   "${LEVELDB_PUBLIC_INCLUDE_DIR}/status.h"
)

target_include_directories(leveldb 
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

set_target_properties(leveldb
  PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

target_compile_definitions(leveldb
  PRIVATE
    # Used by include/export.h when building shared libraries.
    LEVELDB_COMPILE_LIBRARY
    # Used by port/port.h.
    ${LEVELDB_PLATFORM_NAME}=1
)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(leveldb
    PUBLIC
      # Used by include/export.h.
      LEVELDB_SHARED_LIBRARY
  )
endif(BUILD_SHARED_LIBS)


# Needed by port_stdcxx.h
find_package(Threads REQUIRED)
target_link_libraries(leveldb Threads::Threads)

# ///////////////////////////////////构建测试//////////////////////////////////////////

option(LEVELDB_BUILD_TESTS "Build LevelDB's unit tests" ON)

if(LEVELDB_BUILD_TESTS)
  enable_testing()
  # Prevent overriding the parent project's compiler/linker settings on Windows.
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(install_gtest OFF)
  set(install_gmock OFF)
  set(build_gmock ON)
  add_subdirectory("third_party/googletest")
  add_executable(leveldb_tests "")
  target_sources(leveldb_tests 
  PRIVATE
    "${PROJECT_BINARY_DIR}/${LEVELDB_PORT_CONFIG_DIR}/port_config.h"
    ${DB_TEST_SOURCES}
    ${PORT_TEST_SOURCES}
    ${TABLE_TEST_SOURCES}
    ${UTIL_TEST_SOURCES}
    ${TEST_UTIL}
  )
  target_link_libraries(leveldb_tests leveldb gmock gtest gtest_main)
  target_compile_definitions(leveldb_tests
    PRIVATE
      ${LEVELDB_PLATFORM_NAME}=1
  )
  add_test(NAME "leveldb_tests" COMMAND "leveldb_tests")
endif(LEVELDB_BUILD_TESTS)

add_executable(simple_test "test/simple_test.cpp")
add_test(NAME "simple_test" COMMAND "simple_test")

add_executable(db_test "test/db_test.cpp")
target_link_libraries(db_test leveldb)
add_test(NAME "db_test" COMMAND "db_test")


# ///////////////////////////////////构建安装//////////////////////////////////////////

option(LEVELDB_INSTALL "Install LevelDB's header and library" ON)
if(LEVELDB_INSTALL)
  install(TARGETS leveldb
    EXPORT leveldbTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  )
  install(
    DIRECTORY "${LEVELDB_PUBLIC_INCLUDE_DIR}/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/leveldb"
    FILES_MATCHING PATTERN "*.h"
  )

  # include(CMakePackageConfigHelpers)
  #   configure_package_config_file(
  #   "cmake/${PROJECT_NAME}Config.cmake.in"
  #   "${PROJECT_BINARY_DIR}/cmake/${PROJECT_NAME}Config.cmake"
  #   INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  # )
  # write_basic_package_version_file(
  #   "${PROJECT_BINARY_DIR}/cmake/${PROJECT_NAME}ConfigVersion.cmake"
  #   COMPATIBILITY SameMajorVersion
  # )
  # install(
  #   EXPORT leveldbTargets
  #   NAMESPACE leveldb::
  #   DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  # )
  # install(
  #   FILES
  #     "${PROJECT_BINARY_DIR}/cmake/${PROJECT_NAME}Config.cmake"
  #     "${PROJECT_BINARY_DIR}/cmake/${PROJECT_NAME}ConfigVersion.cmake"
  #   DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
  # )
endif(LEVELDB_INSTALL)