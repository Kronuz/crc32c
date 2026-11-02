# Usage: cmake -DCXX=<compiler> -DSRC=<codegen_probe.cc> -DINC=<include dir> -P check_codegen.cmake
#
# crc32c() for a short buffer is a few instructions and one call of the hardware routine. If detail::update() stays
# out of line (its table fallback inlined into it makes it big, with a frame of saved registers), the call site calls
# update() and update() calls the routine. This compiles the probe and requires that nothing in the probe's own text
# refers to update().
execute_process(
  COMMAND "${CXX}" -std=c++20 -O3 -DNDEBUG "-I${INC}" -S -o - "${SRC}"
  OUTPUT_VARIABLE asm
  ERROR_VARIABLE err
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "cannot compile ${SRC} to assembly: ${err}")
endif()
# The probe's own text: from its label to the first end of function directive after it.
string(FIND "${asm}" "crc32c_probe:" start)
if(start EQUAL -1)
  message(FATAL_ERROR "the probe function is missing from the assembly")
endif()
string(SUBSTRING "${asm}" ${start} -1 rest)
string(FIND "${rest}" ".cfi_endproc" stop)
if(stop EQUAL -1)
  string(FIND "${rest}" ".size" stop)
endif()
if(stop EQUAL -1)
  message(FATAL_ERROR "cannot find the end of the probe function in the assembly (${CXX})")
endif()
string(SUBSTRING "${rest}" 0 ${stop} body)
# The mangled name of kronuz::crc32c::detail::update(unsigned, unsigned char const*, unsigned long) holds 6detail6updateE.
if(body MATCHES "6detail6updateE")
  message(FATAL_ERROR "crc32c() reaches an out-of-line detail::update() (${CXX}):\n${body}")
endif()
message(STATUS "crc32c() does not go through detail::update()")
