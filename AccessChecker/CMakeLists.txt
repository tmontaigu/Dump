cmake_minimum_required(VERSION 3.28)

if (NOT WIN32)
    message(SEND_ERROR "This example only works on Windows")
endif()

project(AccessChecker C)

set(CMAKE_C_STANDARD 11)

option(WITH_UNICODE "Enable unicode support" OFF)

add_executable(AccessChecker main.c)
target_link_libraries(AccessChecker PRIVATE Advapi32)

if (WITH_UNICODE)
    target_compile_definitions(AccessChecker PUBLIC UNICODE _UNICODE)
endif()
