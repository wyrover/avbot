cmake_minimum_required(VERSION 2.6)
project(libwebqq)
find_package(Boost 1.54 REQUIRED COMPONENTS log_setup log system thread)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(../avhttp/include/)
include_directories(..)

include_directories(../soci/src/core/)
include_directories(../soci/src/backends/sqlite3)
include_directories(${SQLITE3_INCLUDE_DIR})

add_library(webqq STATIC error_code.cpp webqq.cpp impl/webqq_face.cpp impl/webqq_impl.cpp)

target_link_libraries(webqq ${Boost_LIBRARIES} soci_sqlite3)
INCLUDE (CheckLibraryExists)
if(NOT WIN32)
check_library_exists(rt clock_gettime "" HAVE_CLOCK_GETTIME)
if(HAVE_CLOCK_GETTIME)
        target_link_libraries(webqq -lrt)
endif(HAVE_CLOCK_GETTIME)
endif()
