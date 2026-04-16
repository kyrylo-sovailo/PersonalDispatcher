# Header
cmake_minimum_required(VERSION 3.10)
project(PersonalDispatcher LANGUAGES C)

    #add_compile_options(-ffunction-sections -fdata-sections)
    #add_link_options(-Wl,--gc-sections,--print-gc-sections)

# Compiler identification
set(COMPILER "${CMAKE_C_COMPILER_ID}")
if("${COMPILER}" STREQUAL "MSVC")
    set(COMPILER_STYLE "MSVC")
elseif("${COMPILER}" STREQUAL "GNU" OR "${COMPILER}" STREQUAL "AppleClang")
    set(COMPILER_STYLE "GNU")
elseif("${COMPILER}" STREQUAL "Clang")
    if ("${CMAKE_C_COMPILER_FRONTEND_VARIANT}" STREQUAL "MSVC")
        set(COMPILER_STYLE "MSVC")
    else()
        set(COMPILER_STYLE "GNU")
    endif()
endif()

# Compiler options
if("${COMPILER_STYLE}" STREQUAL "GNU")
    add_compile_options(-Wall -Wextra -pedantic)
    add_compile_options(-Wconversion -Wsign-conversion -Wmissing-declarations -Wstrict-prototypes -Wunreachable-code)
    add_compile_options(-Werror)
    if("${COMPILER}" STREQUAL "GNU")
        add_compile_options(-fanalyzer)
    endif()
    add_link_options(-Wl,--no-undefined)
elseif("${COMPILER_STYLE}" STREQUAL "MSVC")
    add_compile_options(/Wall)
    # Warnings that come out of nowhere:
    add_compile_options(/wd4668) #C4668: Macro is not defined as a preprocessor macro
    add_compile_options(/wd4820) #C4820: N bytes padding added after data member
    add_compile_options(/wd5045) #C5045: Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
    
    # Warnings about things that are not bugs but features:
    add_compile_options(/wd4127) #C4127: Conditional expression is constant
    add_compile_options(/wd4464) #C4464: Relative include path contains '..'
    
    # Warnings that appear because MSVC isn't as smart as GCC
    # It may hide valid warnings, but I decide to hide them because otherwise I risk missing more important warnings
    #add_compile_options(/wd4244) #C4244: conversion from '<unnamed-enum>' to '<type>', possible loss of data
    #add_compile_options(/wd4701) #C4701: Potentially uninitialized local variable used
    #add_compile_options(/wd4774) #C4774: Format string expected in argument is not a string literal
    #add_compile_options(/wd4996) #C4996: This function or variable may be unsafe
endif()
if(WIN32)
    add_compile_definitions(WIN32 UNICODE)
endif()

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

# Source files
set(SOURCES
commonlib/error.c
commonlib/char_buffer.c
commonlib/generic_buffer_implementation.c
commonlib/output.c
commonlib/path.c
commonlib/string.c
common.c
entries.c
main.c
string.c)

# Custom add_executable
function(add_executable_relative TARGET_NAME)
    add_executable(${TARGET_NAME} ${ARGN})
    math(EXPR ARGC_WITHOUT_RESERVED "${ARGC} - 1")
    set(OPTIONS WIN32 MACOSX_BUNDLE EXCLUDE_FROM_ALL)
    foreach(I RANGE 1 ${ARGC_WITHOUT_RESERVED} 1)
        if (NOT "${ARGV${I}}" IN_LIST OPTIONS)
            set(FILE "${ARGV${I}}")
            if(IS_ABSOLUTE "${FILE}")
                file(RELATIVE_PATH RELATIVE_FILE "${CMAKE_SOURCE_DIR}" "${FILE}")
            else()
                set(RELATIVE_FILE "${FILE}")
            endif()
            get_filename_component(BASENAME_FILE "${FILE}" NAME)
            get_source_file_property(DEFINITIONS "${FILE}" COMPILE_DEFINITIONS)
            if("${DEFINITIONS}" STREQUAL "NOTFOUND")
                set(DEFINITIONS "")
            endif()
            list(APPEND DEFINITIONS "__RELATIVE_FILE__=\"${RELATIVE_FILE}\"")
            list(APPEND DEFINITIONS "__BASENAME_FILE__=\"${BASENAME_FILE}\"")
            set_source_files_properties("${FILE}" PROPERTIES COMPILE_DEFINITIONS "${DEFINITIONS}")
        endif()
    endforeach()
endfunction()

# Binary
add_executable_relative(kpd ${SOURCES})
if (ENABLE_READLINE)
    target_compile_definitions(kpd PRIVATE ENABLE_READLINE)
    target_link_libraries(kpd PRIVATE readline)
endif()