cmake_minimum_required(VERSION 3.18)
project(dlp_framework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Opciones por backend: se activan con -DENABLE_X=ON
option(ENABLE_SIMD   "Compilar el backend SIMD (AVX2)" OFF)
option(ENABLE_AVX512 "Agregar variantes AVX-512 al backend SIMD" OFF)
option(ENABLE_CUDA   "Compilar el backend CUDA" OFF)

add_executable(dlp_bench
  src/main.cpp
  common/image.cpp
  common/filter.cpp
  backends/registry.cpp
  backends/seq/seq_backend.cpp
  backends/fpga/fpga_backend.cpp
)
target_include_directories(dlp_bench PRIVATE ${CMAKE_SOURCE_DIR})

if(ENABLE_SIMD)
  target_sources(dlp_bench PRIVATE backends/simd/simd_backend.cpp)
  target_compile_definitions(dlp_bench PRIVATE DLP_HAS_SIMD)
  set_source_files_properties(backends/simd/simd_backend.cpp PROPERTIES COMPILE_OPTIONS "-mavx2;-mfma")
  if(ENABLE_AVX512)
    target_compile_definitions(dlp_bench PRIVATE DLP_HAS_AVX512)
    set_property(SOURCE backends/simd/simd_backend.cpp APPEND PROPERTY COMPILE_OPTIONS "-mavx512f")
  endif()
endif()

if(ENABLE_CUDA)
  enable_language(CUDA)
  target_sources(dlp_bench PRIVATE backends/cuda/cuda_backend.cu)
  target_compile_definitions(dlp_bench PRIVATE DLP_HAS_CUDA)
endif()