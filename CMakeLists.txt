cmake_minimum_required (VERSION 3.8)
project (simplerf)

set (CMAKE_CXX_STANDARD 20)

set(CLANG_VERSION "18" CACHE STRING "clang and LLVM version to use")
set(CMAKE_C_COMPILER "/usr/bin/clang-${CLANG_VERSION}")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++-${CLANG_VERSION}")
set(ENZYME_PLUGIN_DLL "/opt/Enzyme/enzyme/build/Enzyme/ClangEnzyme-${CLANG_VERSION}.so")

include_directories(".")
include_directories("external/LiteMath")

add_executable(testapp main.cpp
                 example_tracer/plenoxels.cpp
                 external/LiteMath/Image2d.cpp)

set(ENZYME_FLAGS "-fplugin=${ENZYME_PLUGIN_DLL}")
target_compile_options(testapp PUBLIC ${OMP_FLAGS} ${ENZYME_FLAGS})

