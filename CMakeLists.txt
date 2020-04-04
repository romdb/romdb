cmake_minimum_required(VERSION 3.14.0 FATAL_ERROR)

INCLUDE(CheckTypeSize)

project(romdb)

find_package(SQLite3 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(LibLZMA REQUIRED)

include_directories(${SQLite3_INCLUDE_DIRS})
include_directories(${ZLIB_INCLUDE_DIRS})
include_directories(${LIBLZMA_INCLUDE_DIRS})

include_directories(${CMAKE_CURRENT_LIST_DIR}/thirdparty/clipp/include)
include_directories(${CMAKE_CURRENT_LIST_DIR}/thirdparty/Hash/src)
include_directories(${CMAKE_CURRENT_LIST_DIR}/thirdparty/sqlite3pp/headeronly_src)
include_directories(${CMAKE_CURRENT_LIST_DIR}/thirdparty/xdelta3)

set(SOURCE_FILES
    src/file.cpp
    src/main.cpp
    src/romdb.cpp
    src/utils.cpp
    thirdparty/xdelta3/xdelta3.c
)

check_type_size("size_t" SIZEOF_SIZE_T)
check_type_size("unsigned long long" SIZEOF_UNSIGNED_LONG_LONG)

add_definitions(-DXD3_MAIN=0)
add_definitions(-DXD3_DEBUG=0)
add_definitions(-DXD3_DEFAULT_LEVEL=0)
add_definitions(-DXD3_DEFAULT_SECONDARY_LEVEL=0)
add_definitions(-DSIZEOF_SIZE_T=${SIZEOF_SIZE_T})
add_definitions(-DSIZEOF_UNSIGNED_LONG_LONG=${SIZEOF_UNSIGNED_LONG_LONG})

add_executable(${PROJECT_NAME} ${SOURCE_FILES})

target_link_libraries(${PROJECT_NAME} ${SQLite3_LIBRARIES} ${ZLIB_LIBRARIES} ${LIBLZMA_LIBRARIES})

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 17)
set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD_REQUIRED ON)
