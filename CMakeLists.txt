cmake_minimum_required(VERSION 3.16)
project(Projekt2 C)

set(CMAKE_C_STANDARD 99)
add_compile_options(-Wall -Werror -Wextra -g -O1)

add_executable(Projekt2 sps.c)