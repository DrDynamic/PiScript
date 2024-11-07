function(ADD_CMOCKA_TEST _TARGET_NAME)
	set(one_value_arguments
    )

	set(multi_value_arguments
        TEST_FILE
        COMPILE_OPTIONS
        LINK_OPTIONS
		EXTRA_SOURCES
    )

    cmake_parse_arguments(_add_cmocka_test
        ""
        "${one_value_arguments}"
        "${multi_value_arguments}"
        ${ARGN}
    )
	if (NOT DEFINED _add_cmocka_test_TEST_FILE)
        message(FATAL_ERROR "No sources provided for target ${_TARGET_NAME}")
    endif()
	set(TEST_FILE "${_add_cmocka_test_TEST_FILE}")
	get_property(TEST_NAMES GLOBAL PROPERTY TEST_NAMES_PROPERTY)

	if(DEFINED _add_cmocka_test_EXTRA_SOURCES)
		list(APPEND sources ${_add_cmocka_test_EXTRA_SOURCES})
	endif()

	get_filename_component(TEST_FILE_NO_EXT ${TEST_FILE} NAME_WE)
	set(TEST_NAME test_${TEST_FILE_NO_EXT})
	list(APPEND TEST_NAMES ${TEST_NAME})
	add_executable(${TEST_NAME} ${sources} ${TEST_FILE})
	target_link_libraries(${TEST_NAME} PRIVATE cmocka)
	add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
	# Enable USAN, LSAN and ASAN for all cmocka tests
	if(NOT DEFINED _add_cmocka_test_COMPILE_OPTIONS)
		target_compile_options(${TEST_NAME} PRIVATE ${DEFAULT_C_COMPILE_FLAGS})
	else()
		target_compile_options(${TEST_NAME} PRIVATE ${_add_cmocka_test_COMPILE_OPTIONS})
	endif()

	if(NOT DEFINED _add_cmocka_test_LINK_OPTIONS)
		target_link_options(${TEST_NAME} PRIVATE ${DEFAULT_LINK_FLAGS})
	else()
	message(STATUS "${TEST_FILE} has custom linker options: ${_add_cmocka_test_LINK_OPTIONS}")
		target_link_options(${TEST_NAME} PRIVATE ${_add_cmocka_test_LINK_OPTIONS})
	endif()

	set_property(GLOBAL PROPERTY TEST_NAMES_PROPERTY "${TEST_NAMES}")

endfunction(ADD_CMOCKA_TEST)
