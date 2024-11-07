message(STATUS "Processing util directory...")

add_library(util memory.c)
target_include_directories(util PUBLIC ${CMAKE_SOURCE_DIR}/src)