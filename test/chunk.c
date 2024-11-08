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
 * helpers
 *
 */

static void assertChunk(Chunk *chunk, int capacity, int count, uint8_t *code, int *lines) {
    assert_int_equal(chunk->capacity, capacity);
    assert_int_equal(chunk->count, count);
    assert_ptr_equal(chunk->code, code);
    assert_ptr_equal(chunk->lines, lines);
}

static void assertConstants(Chunk *chunk, int capacity, int count, Value *values) {
    assert_int_equal(chunk->constants.capacity, capacity);
    assert_int_equal(chunk->constants.count, count);
    assert_ptr_equal(chunk->constants.values, values);
}

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

    assertChunk(&chunk, 0, 0, NULL, NULL);
    assertConstants(&chunk, 0, 0, NULL);
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
    writeChunk(&chunk, 0x42, 1);

    assertChunk(&chunk, 8, 1, chunk.code, chunk.lines);
    assert_int_equal(chunk.code[0], 0x42);
    assert_int_equal(chunk.lines[0], 1);

    assertConstants(&chunk, 0, 0, NULL);

    freeChunk(&chunk);
}

/**
 * @brief Values can be added to a chunks constants and the Index to the added chunk is returned.
 *
 * @param state
 */
static void chunk_adds_constants(void **state) {
    (void)state;

    Chunk chunk;
    initChunk(&chunk);

    int index = addConstant(&chunk, 13.37);
    assert_double_equal(chunk.constants.values[index], 13.37, 0);

    assertChunk(&chunk, 0, 0, NULL, NULL);
    assertConstants(&chunk, 8, 1, chunk.constants.values);

    freeChunk(&chunk);
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
    writeChunk(&chunk, 0x42, 1);
    freeChunk(&chunk);

    assertChunk(&chunk, 0, 0, NULL, NULL);
    assertConstants(&chunk, 0, 0, NULL);
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
        cmocka_unit_test(chunk_adds_constants),
        cmocka_unit_test(chunk_can_be_freed),
    };
    return cmocka_run_group_tests(tests_nothing, NULL, NULL);
}
