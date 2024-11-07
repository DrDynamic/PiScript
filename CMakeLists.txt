cmake_minimum_required(VERSION 3.30)

project(PiScript LANGUAGES C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
set(CMAKE_SKIP_INSTALL_RULES TRUE)
set(CMAKE_C_STANDARD 11)
set(CMAKE_BUILD_TYPE Debug)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
	add_compile_options(-W -Wall -Wextra -pedantic -pedantic-errors -Werror)
else()
	message(FATAL_ERROR "For now, only GCC is supported.")
endif()

add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(doc)
