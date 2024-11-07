message(STATUS "Processing src directory...")

file(GLOB sources
  "${PROJECT_SOURCE_DIR}/src/**/*.h"
  "${PROJECT_SOURCE_DIR}/src/**/*.c")

add_executable(${CMAKE_PROJECT_NAME} main.c)