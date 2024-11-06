message(STATUS "Processing mylib directory...")

add_library(mylib mylib.c)
target_include_directories(mylib PUBLIC ${CMAKE_SOURCE_DIR}/src)
