message(STATUS "Processing src directory...")

#include_directories("${PROJECT_SOURCE_DIR}/src")
#include_directories("${PROJECT_SOURCE_DIR}/src/util")

file(GLOB headers
  "${PROJECT_SOURCE_DIR}/src/*.h"
  "${PROJECT_SOURCE_DIR}/src/**/*.h"
  )

file(GLOB sources
  "${PROJECT_SOURCE_DIR}/src/*.c"
  "${PROJECT_SOURCE_DIR}/src/**/*.c"
  )
list(REMOVE_ITEM sources "${PROJECT_SOURCE_DIR}/src/main.c")


add_executable(pit ${headers} ${sources} main.c)