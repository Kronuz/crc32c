cmake_minimum_required(VERSION 3.14)
project(crc32c LANGUAGES CXX)

# crc32c is header-only and includes only the standard library: nothing is fetched.
# The target puts include/ on the consumer's include path (so #include <crc32c/crc32c.hh>
# resolves) and requests cxx_std_20.
add_library(crc32c INTERFACE)
target_include_directories(crc32c INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(crc32c INTERFACE cxx_std_20)
# Alias so consumers can link `crc32c::crc32c` whether they add this via
# add_subdirectory or FetchContent.
add_library(crc32c::crc32c ALIAS crc32c)

# Only build tests, examples and benchmarks when this repo is the top-level project, so
# consumers (e.g. via FetchContent) get the header target and nothing else.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  enable_testing()
  add_executable(crc32c_test test/crc32c_test.cc)
  target_link_libraries(crc32c_test PRIVATE crc32c::crc32c)
  add_test(NAME crc32c COMMAND crc32c_test)

  # A consumer that takes only crc32c::crc32c, with fetching disabled so any attempt to
  # fetch fails the configure; built with this project's compiler, flags and build type.
  get_property(multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
  if(multi_config)
    set(config_args --build-config "$<CONFIG>")
  endif()
  add_test(NAME crc32c_consumer
    COMMAND ${CMAKE_CTEST_COMMAND} --build-and-test
      "${CMAKE_CURRENT_SOURCE_DIR}/test/consumer" "${CMAKE_CURRENT_BINARY_DIR}/consumer"
      --build-generator "${CMAKE_GENERATOR}"
      ${config_args}
      --build-options
        "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DCRC32C_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
      --test-command "${CMAKE_CURRENT_BINARY_DIR}/consumer/consumer")

  # A runnable tour of the library (not a test): build & run ./build/crc32c_demo
  add_executable(crc32c_demo examples/demo.cc)
  target_link_libraries(crc32c_demo PRIVATE crc32c::crc32c)

  # GB/s per path (hardware, slice-by-8, byte table):
  # cmake -B build -DCMAKE_BUILD_TYPE=Release -DCRC32C_BUILD_BENCHMARKS=ON
  option(CRC32C_BUILD_BENCHMARKS "Build the throughput benchmark" OFF)
  if(CRC32C_BUILD_BENCHMARKS)
    add_executable(crc32c_bench bench/crc32c.cc)
    target_link_libraries(crc32c_bench PRIVATE crc32c::crc32c)

    # Reference: Google's crc32c, built for this benchmark only and never a dependency of the
    # library. It is built in a project of its own because its target is also called crc32c.
    #   cmake -B build -DCMAKE_BUILD_TYPE=Release -DCRC32C_BUILD_BENCHMARKS=ON -DCRC32C_BENCH_REFERENCE=ON
    option(CRC32C_BENCH_REFERENCE "Build google/crc32c 1.1.2 as a benchmark-only reference" OFF)
    if(CRC32C_BENCH_REFERENCE)
      include(ExternalProject)
      set(google_args -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
        -DBUILD_SHARED_LIBS=OFF -DCRC32C_BUILD_TESTS=OFF -DCRC32C_BUILD_BENCHMARKS=OFF -DCRC32C_USE_GLOG=OFF)
      if(CMAKE_OSX_ARCHITECTURES)
        list(APPEND google_args -DCMAKE_OSX_ARCHITECTURES=${CMAKE_OSX_ARCHITECTURES})
      endif()
      ExternalProject_Add(google_crc32c
        GIT_REPOSITORY https://github.com/google/crc32c.git
        GIT_TAG        02e65f4fd3065d27b2e29324800ca6d04df16126  # 1.1.2
        GIT_SUBMODULES ""  # its tests and benchmarks (submodules) are off
        CMAKE_ARGS ${google_args}
        BUILD_BYPRODUCTS <INSTALL_DIR>/lib/libcrc32c.a)
      ExternalProject_Get_Property(google_crc32c INSTALL_DIR)
      add_dependencies(crc32c_bench google_crc32c)
      target_include_directories(crc32c_bench PRIVATE "${INSTALL_DIR}/include")
      target_link_libraries(crc32c_bench PRIVATE "${INSTALL_DIR}/lib/libcrc32c.a")
      target_compile_definitions(crc32c_bench PRIVATE CRC32C_BENCH_REFERENCE=1)
    endif()
  endif()

  # libFuzzer target (Clang): cmake -DCRC32C_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++
  # Without it a replay driver runs the seeds and the same checks as a test.
  option(CRC32C_FUZZ "Build the libFuzzer target" OFF)
  if(CRC32C_FUZZ)
    add_executable(crc32c_fuzz fuzz/fuzz_update.cc)
    target_link_libraries(crc32c_fuzz PRIVATE crc32c::crc32c)
    target_compile_options(crc32c_fuzz PRIVATE -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)
    target_link_options(crc32c_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
  else()
    add_executable(crc32c_fuzz_replay fuzz/fuzz_update.cc fuzz/replay.cc)
    target_link_libraries(crc32c_fuzz_replay PRIVATE crc32c::crc32c)
    add_test(NAME crc32c_fuzz_replay COMMAND crc32c_fuzz_replay)
  endif()
endif()
