################################################################################
# CMake build file for libfec
#
# Usage (Linux):
#   $ mkdir build && cd build
#   $ cmake ..
#   $ make
#
# Optional (to run tests):
#   $ make test_all
#
# Optional (to install, may need to preface with sudo):
#   $ make install
#

################################################################################
# Project options

cmake_minimum_required (VERSION 3.0)
project (libfec C ASM)

include (CheckIncludeFile)
include (CheckFunctionExists)
include (CheckLibraryExists)

################################################################################
# Global flags

add_definitions ("-Wall -fPIC")

################################################################################
# Configuration tests

check_include_file (getopt.h HAVE_GETOPT_H)
check_include_file (memory.h HAVE_MEMORY_H)
check_include_file (stdio.h HAVE_STDIO_H)
check_include_file (stdlib.h HAVE_STDLIB_H)
check_include_file (getopt.h HAVE_GETOPT_H)
check_function_exists (getopt_long HAVE_GETOPT_LONG)
check_function_exists (memset HAVE_MEMSET)
check_function_exists (memmove HAVE_MEMMOVE)
check_library_exists (c malloc "" HAVE_LIBC)

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/config.h)

################################################################################
# Targets

include_directories (${CMAKE_CURRENT_SOURCE_DIR})
include_directories (${CMAKE_CURRENT_BINARY_DIR})

# Platform specific source files
if ("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "i.86")
    add_definitions ("-march=${CMAKE_SYSTEM_PROCESSOR} -mmmx -msse -msse2")
    set (LIBFEC_MLIBS viterbi27_mmx.c mmxbfly27.s viterbi27_sse.c ssebfly27.s
        viterbi27_sse2.c sse2bfly27.s viterbi29_mmx.c mmxbfly29.s
        viterbi29_sse.c ssebfly29.s viterbi29_sse2.c sse2bfly29.s
        viterbi39_sse2.c viterbi39_sse.c viterbi39_mmx.c viterbi615_mmx.c
        viterbi615_sse.c viterbi615_sse2.c dotprod_mmx.c dotprod_mmx_assist.s
        dotprod_sse2.c dotprod_sse2_assist.s peakval_mmx.c peakval_mmx_assist.s
        peakval_sse.c peakval_sse_assist.s peakval_sse2.c peakval_sse2_assist.s
        sumsq.c sumsq_port.c sumsq_sse2.c sumsq_sse2_assist.s sumsq_mmx.c
        sumsq_mmx_assist.s cpu_features.s cpu_mode_x86.c)
elseif ("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "power.*")
    add_definitions ("-fno-common -faltivec")
    set (LIBFEC_MLIBS viterbi27_av.c viterbi29_av.c viterbi39_av.c
        viterbi615_av.c	encode_rs_av.c dotprod_av.c sumsq_av.c peakval_av.c
        cpu_mode_ppc.c)
elseif ("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "x86_64")
    set (LIBFEC_MLIBS cpu_mode_unknown.c)
else ()
    set (LIBFEC_MLIBS cpu_mode_unknown.c)
endif ()

# Generated source files
add_executable (gen_ccsds gen_ccsds.c init_rs_char_local.c)
add_custom_command (OUTPUT ccsds_tab.c COMMAND gen_ccsds > ccsds_tab.c)

add_executable (gen_ccsds_tal gen_ccsds_tal.c)
add_custom_command (OUTPUT ccsds_tal.c COMMAND gen_ccsds_tal > ccsds_tal.c)

set (LIBFEC_GENLIBS ccsds_tal.c ccsds_tab.c)

# Libfec library target
add_library (fec ${LIBFEC_MLIBS} ${LIBFEC_GENLIBS} fec.c sim.c viterbi27.c
    viterbi27_port.c viterbi29.c viterbi29_port.c viterbi39.c viterbi39_port.c
    viterbi615.c viterbi615_port.c encode_rs_char.c encode_rs_int.c
    encode_rs_8.c decode_rs_char.c decode_rs_int.c decode_rs_8.c init_rs_char.c
    init_rs_int.c encode_rs_ccsds.c decode_rs_ccsds.c dotprod.c dotprod_port.c
    peakval.c peakval_port.c sumsq.c sumsq_port.c)
target_link_libraries (fec INTERFACE m)
set_target_properties (fec PROPERTIES 
    VERSION "3.0.0"
    SOVERSION 3)

install (TARGETS fec
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)
install (FILES fec.h DESTINATION include)
install (FILES simd-viterbi.3 rs.3 dsp.3 DESTINATION share/man/man3)

# Additional executables for the test targets below.
add_executable (vtest27 vtest27.c)
target_link_libraries (vtest27 m fec)

add_executable (vtest29 vtest29.c)
target_link_libraries (vtest29 m fec)

add_executable (vtest39 vtest39.c)
target_link_libraries (vtest39 m fec)

add_executable (vtest615 vtest615.c)
target_link_libraries (vtest615 m fec)

add_executable (rstest rstest.c)
target_link_libraries (rstest m fec)

add_executable (dtest dtest.c)
target_link_libraries (dtest m fec)

add_executable (sumsq_test sumsq_test.c)
target_link_libraries (sumsq_test m fec)

add_executable (peaktest peaktest.c)
target_link_libraries (peaktest m fec)

# These tests don't completely conform to the CMake convention of returning 0
# for success and non-zero for failure. Some of their results are printed to
# the console, so we use add_custom_target instead of add_test from the CTest
# framework. Requires that the user call "make test_all" instead of "ctest" to
# run the tests.
add_custom_target(test_correctness
    COMMENT "Correctness tests:"
    COMMAND echo "vtest27 -e 3.0 -n 1000 -v"
    COMMAND vtest27 -e 3.0 -n 1000 -v
    COMMAND echo "vtest29 -e 2.5 -n 1000 -v"
    COMMAND vtest29 -e 2.5 -n 1000 -v
    COMMAND echo "vtest39 -e 2.5 -n 1000 -v"
    COMMAND vtest39 -e 2.5 -n 1000 -v
    COMMAND echo "vtest615 -e 1.0 -n 100 -v"
    COMMAND vtest615 -e 1.0 -n 100 -v
    COMMAND echo "rstest"
    COMMAND rstest
    COMMAND echo "dtest"
    COMMAND dtest
    COMMAND echo "sumsq_test"
    COMMAND sumsq_test
    COMMAND echo "peaktest"
    COMMAND peaktest
    DEPENDS vtest27 vtest29 vtest39 vtest615 rstest dtest sumsq_test peaktest)

add_custom_target(test_speed
    COMMENT "Speed tests:"
    COMMAND echo "vtest27"
    COMMAND vtest27
    COMMAND echo "vtest29"
    COMMAND vtest29
    COMMAND echo "vtest39"
    COMMAND vtest39
    COMMAND echo "vtest615"
    COMMAND vtest615
    DEPENDS vtest27 vtest29 vtest39 vtest615)

add_custom_target(test_all
    DEPENDS test_correctness test_speed)
