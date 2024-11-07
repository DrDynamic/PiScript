message(STATUS "Processing test directory...")

include(AddCMockaTest)

# set defaults
set(DEFAULT_C_COMPILE_FLAGS "-fsanitize=undefined,leak,address")
set(DEFAULT_LINK_FLAGS "-fsanitize=undefined,leak,address")

include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories(${PROJECT_SOURCE_DIR}/src/util)

# Project files
file(GLOB sources
  "${PROJECT_SOURCE_DIR}/src/**/*.h"
  "${PROJECT_SOURCE_DIR}/src/**/*.c"
  )
list(REMOVE_ITEM sources "${PROJECT_SOURCE_DIR}/src/main.c")


# Enable ctest-system and create test targets
enable_testing()
find_package(cmocka REQUIRED)

# add tests here
add_cmocka_test(empty
				TEST_FILE empty.c)
add_cmocka_test(memory
				TEST_FILE util/memory.c
				COMPILE_OPTIONS "${DEFAULT_C_COMPILE_FLAGS}" "-Wl,-wrap,realloc"
				LINK_OPTIONS "${DEFAULT_LINK_FLAGS}" "-Wl,--wrap=realloc"
				EXTRA_SOURCES "${PROJECT_SOURCE_DIR}/test/wrappers/realloc_wrapper.c")




# Add target that runs all tests
add_custom_target(
    test_cmocka
    COMMAND ctest --verbose
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running tests..."
    VERBATIM
)
get_property(TEST_NAMES GLOBAL PROPERTY TEST_NAMES_PROPERTY)
add_dependencies(test_cmocka ${TEST_NAMES})

# Add target that checks all source code
find_program(CPPCHECK_EXECUTABLE cppcheck REQUIRED)
add_custom_target(
	test_cppcheck
	COMMAND ${CPPCHECK_EXECUTABLE} --project=./compile_commands.json
		--enable=all --inconclusive --suppress=unusedFunction
		--suppress=unmatchedSuppression --suppress=missingIncludeSystem
		--addon=cert --addon=threadsafety
		--error-exitcode=1
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/build
	COMMENT "Running cppcheck..."
	VERBATIM
)

# Provide one target that runs all tests
add_custom_target(tests)
add_dependencies(tests test_cppcheck test_cmocka)
