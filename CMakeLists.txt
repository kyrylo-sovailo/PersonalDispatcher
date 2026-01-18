# Header
cmake_minimum_required(VERSION 3.10)
project(PersonalDispatcher LANGUAGES C)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_VERBOSE_MAKEFILE TRUE)
cmake_policy(SET CMP0074 NEW)
add_compile_options(-Wall -Wextra -Wconversion -Wsign-conversion -Werror -pedantic -fanalyzer)

unset(ENABLE_READLINE)

# Dependencies
if (ENABLE_READLINE)
    add_library(readline SHARED IMPORTED)
    find_file(READLINE_INCLUDE_DIR "readline/readline.h" REQUIRED)
    find_library(READLINE_LIBRARY "libreadline.so" REQUIRED)
    get_filename_component(READLINE_INCLUDE_DIR "${READLINE_INCLUDE_DIR}" DIRECTORY)
    message("-- Readline found. Headers: ${READLINE_INCLUDE_DIR}, Library: ${READLINE_LIBRARY}")
    get_filename_component(READLINE_INCLUDE_DIR "${READLINE_INCLUDE_DIR}" DIRECTORY)
    set_target_properties(readline PROPERTIES INCLUDE_DIRECTORIES "${READLINE_INCLUDE_DIR}")
    set_target_properties(readline PROPERTIES IMPORTED_LOCATION "${READLINE_LIBRARY}")
endif()

# Binary
add_executable(kpd
    common.c
    entries.c
    main.c
    string.c
)
if (ENABLE_READLINE)
    target_compile_definitions(kpd PRIVATE ENABLE_READLINE)
    target_link_libraries(kpd PRIVATE readline)
endif()