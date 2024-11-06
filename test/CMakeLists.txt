message(STATUS "Processing test directory...")

# Files with tests, library under test is assumed to be named identical.
set(TEST_FILES
	mylib.c
)

# Enable ctest-system and create test targets
enable_testing()

find_package(cmocka REQUIRED)

foreach(TEST_FILE IN ITEMS ${TEST_FILES})
	get_filename_component(TEST_FILE_NO_EXT ${TEST_FILE} NAME_WE)
	set(TEST_NAME test_${TEST_FILE_NO_EXT})
	list(APPEND TEST_NAMES ${TEST_NAME})
	add_executable(${TEST_NAME} ${TEST_FILE})
	target_link_libraries(${TEST_NAME} PRIVATE ${TEST_FILE_NO_EXT} cmocka)
	add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
	# Enable USAN, LSAN and ASAN for all cmocka tests
	target_compile_options(${TEST_NAME} PRIVATE -fsanitize=undefined,leak,address)
	target_link_options(${TEST_NAME} PRIVATE -fsanitize=undefined,leak,address)
endforeach()

# Add target that runs all tests
add_custom_target(
    test_cmocka
    COMMAND ctest --verbose
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running tests..."
    VERBATIM
)
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
