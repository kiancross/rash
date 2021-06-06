#
# Copyright (C) 2021 Kian Cross
#

set(CMAKE_C_STANDARD 99)
add_library(rash STATIC rash.c)
target_link_libraries(rash PUBLIC coverage_config)
