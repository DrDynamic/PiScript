/**
 * @file mylib.c
 * @author NikLeberg (niklaus.leuenb@gmail.com)
 * @brief Tests for example library
 * @version 0.1
 * @date 2021-06-23
 *
 * @copyright Copyright (c) 2021 Niklaus Leuenberger
 *
 */


/*
 * Includes
 *
 */
#include <stdlib.h>

#include "test.h"
#include "chunk.h"

/**
 * redifinitions / wrapping
 *
 */

/*
 * Tests
 *
 */

/**
 * @brief A chunk is initialized with integers set to 0 and data set to NULL
 *
 * @param state unused
 */
static void chunk_initializes(void **state) {
    (void)state;

    Chunk chunk;
    initChunk(&chunk);

    assert_int_equal(chunk.capacity, 0);
    assert_int_equal(chunk.count, 0);
    assert_ptr_equal(chunk.code, NULL);
}

/**
 * @brief A chunk can be written to
 *
 * @param state unused
 */
static void chunk_is_writable(void **state) {
    (void)state;

    Chunk chunk;
    initChunk(&chunk);
    writeChunk(&chunk, 0x42);

    assert_int_equal(chunk.capacity, 8);
    assert_int_equal(chunk.count, 1);
    assert_int_equal(chunk.code[0], 0x42);

    free(chunk.code);
}

/**
 * @brief The memory of a chunk can be freed. It will be initialized afterwards.
 *
 * @param state unused
 */
static void chunk_can_be_freed(void **state) {
    (void)state;

    Chunk chunk;
    initChunk(&chunk);
    writeChunk(&chunk, 0x42);
    freeChunk(&chunk);

    assert_int_equal(chunk.capacity, 0);
    assert_int_equal(chunk.count, 0);
    assert_ptr_equal(chunk.code, NULL);
}

/*
 * Main test program
 *
 */

/**
 * @brief Main
 *
 * @return int count of failed tests
 */
int main(void) {
    const struct CMUnitTest tests_nothing[] = {
        cmocka_unit_test(chunk_initializes),
        cmocka_unit_test(chunk_is_writable),
        cmocka_unit_test(chunk_can_be_freed),
    };
    return cmocka_run_group_tests(tests_nothing, NULL, NULL);
}
