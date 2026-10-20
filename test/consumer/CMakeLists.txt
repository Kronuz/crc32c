# Takes only crc32c::crc32c, with fetching disabled so any attempt to fetch fails the configure.
cmake_minimum_required(VERSION 3.14)
project(crc32c_consumer LANGUAGES CXX)

include(FetchContent)
set(FETCHCONTENT_FULLY_DISCONNECTED ON)
FetchContent_Declare(crc32c SOURCE_DIR "${CRC32C_SOURCE_DIR}")
FetchContent_MakeAvailable(crc32c)

add_executable(consumer consumer.cc)
target_link_libraries(consumer PRIVATE crc32c::crc32c)
