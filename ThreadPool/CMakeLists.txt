cmake_minimum_required(VERSION 3.22)
project(c_thrd_pool C)

set(CMAKE_C_STANDARD 11)

add_executable(c_thrd_pool main.c
        thread_pool.h
        thread_pool.c)
#target_link_libraries(c_thrd_pool PRIVATE m pthread)

add_executable(c_thrd_pool2 main2.c)