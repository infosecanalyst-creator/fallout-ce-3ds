cmake_minimum_required(VERSION 3.16)

project(fpattern
    LANGUAGES C
    VERSION "1.0.0"
)

add_library(fpattern-static src/fpattern.c)
add_library(fpattern::fpattern ALIAS fpattern-static)

if(NOT WIN32)
    target_compile_definitions(fpattern-static PRIVATE
        "-Dunix"
    )
endif()

target_include_directories(fpattern-static PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
