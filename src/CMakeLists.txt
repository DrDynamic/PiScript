message(STATUS "Processing src directory...")

add_subdirectory(util)

add_executable(${CMAKE_PROJECT_NAME} main.c)

target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE util)
