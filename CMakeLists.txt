#
# Copyright (C) 2021 Kian Cross
#

cmake_minimum_required(VERSION 3.13)
project(rash VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)

add_library(coverage_config INTERFACE)
option(CODE_COVERAGE "Enable coverage reporting" OFF)

if(CODE_COVERAGE AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(coverage_config INTERFACE -O0 -g --coverage)
  target_link_options(coverage_config INTERFACE --coverage)
endif()

add_subdirectory(src) 

if (PROJECT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  include (CTest)
  enable_testing()
  add_subdirectory(tests)
endif()
