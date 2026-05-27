cmake_minimum_required(VERSION 4.2)
project(AntEngine C)

set(CMAKE_C_STANDARD 11)

add_executable(AntEngine main.c
        stb_image.h
        core/hash_table.c
        core/hash_table.h
        core/vector.c
        core/vector.h)

find_package(glad CONFIG REQUIRED)
target_link_libraries(AntEngine PRIVATE glad::glad)
find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(AntEngine PRIVATE glfw)