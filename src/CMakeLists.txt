message(STATUS "Processing src directory...")

add_subdirectory(util)

include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/src/util)

file(GLOB sources
  "${PROJECT_SOURCE_DIR}/src/**/*.h"
  "${PROJECT_SOURCE_DIR}/src/**/*.c")

add_executable(${CMAKE_PROJECT_NAME} main.c)