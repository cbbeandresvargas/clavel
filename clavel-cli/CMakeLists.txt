cmake_minimum_required(VERSION 3.15)

# El CLI no necesita las cabeceras del framework — es un programa standalone
add_executable(clavel-cli clavel.c)

target_include_directories(clavel-cli PRIVATE "${CMAKE_SOURCE_DIR}")

set_target_properties(clavel-cli PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)

if(MSVC)
    target_compile_options(clavel-cli PRIVATE /W3 /wd4996)
else()
    target_compile_options(clavel-cli PRIVATE -Wall -Wextra -Wno-unused-parameter -O2)
endif()
