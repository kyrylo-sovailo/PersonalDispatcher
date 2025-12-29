cmake_minimum_required(VERSION 3.10)
project(PersonalDispatcher LANGUAGES C)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_VERBOSE_MAKEFILE TRUE)
cmake_policy(SET CMP0074 NEW)
add_compile_options(-Wall -Wextra -Wconversion -Wsign-conversion -Werror -pedantic -fanalyzer)

add_executable(kpd
common.c
entries.c
main.c
sqsort.c
string.c
)