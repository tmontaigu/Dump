add_executable (hello_world "hello_world.c")
target_link_libraries(hello_world PRIVATE coroutines)

add_executable (range "range.c")
target_link_libraries(range PRIVATE coroutines)
