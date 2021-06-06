#
# Copyright (C) 2021 Kian Cross
#

set(CMAKE_C_STANDARD 99)

add_executable(tests test.c) 
target_link_libraries(tests PRIVATE rash)

add_test(rash tests)
