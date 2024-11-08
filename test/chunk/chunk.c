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

#include "../test.h"
#include "chunk/chunk.h"

#include <stdio.h>
/**
 * helpers
 *
 */

static void assertChunk(Chunk* chunk, int capacity, int count, uint8_t* code)
{
    assert_int_equal(chunk->capacity, capacity);
    assert_int_equal(chunk->count, count);
    assert_ptr_equal(chunk->code, code);
}

static void assertConstants(Chunk* chunk, int capacity, int count, Value* values)
{
    assert_int_equal(chunk->constants.capacity, capacity);
    assert_int_equal(chunk->constants.count, count);
    assert_ptr_equal(chunk->constants.values, values);
}

static void assertSourceInfo(Chunk* chunk, int capacity, int count)
{
    assert_int_equal(chunk->sourceinfo.capacity, capacity);
    assert_int_equal(chunk->sourceinfo.count, count);
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
static void chunk_initializes(void** state)
{
    (void)state;

    Chunk chunk;
    initChunk(&chunk);

    assertChunk(&chunk, 0, 0, NULL);
    assertConstants(&chunk, 0, 0, NULL);
    assertSourceInfo(&chunk, 0, 0);
    assert_ptr_equal(chunk.sourceinfo.linenumbers, NULL);
    assert_ptr_equal(chunk.sourceinfo.linenumberCounter, NULL);
}

/**
 * @brief A chunk can be written to
 *
 * @param state unused
 */
static void chunk_is_writable(void** state)
{
    (void)state;

    Chunk chunk;
    initChunk(&chunk);
    writeChunk(&chunk, 0x42, 1);

    assertChunk(&chunk, 8, 1, chunk.code);
    assert_int_equal(chunk.code[0], 0x42);

    assertConstants(&chunk, 0, 0, NULL);
    assertSourceInfo(&chunk, 8, 1);
    assert_int_equal(chunk.sourceinfo.linenumbers[0], 1);
    assert_int_equal(chunk.sourceinfo.linenumberCounter[0], 1);

    freeChunk(&chunk);
}

/**
 * @brief Values can be added to a chunks constants and the Index to the added chunk is returned.
 *
 * @param state
 */
static void chunk_adds_constants(void** state)
{
    (void)state;

    Chunk chunk;
    initChunk(&chunk);

    int index = addConstant(&chunk, 13.37);
    assert_double_equal(chunk.constants.values[index], 13.37, 0);

    assertChunk(&chunk, 0, 0, NULL);
    assertConstants(&chunk, 8, 1, chunk.constants.values);
    assertSourceInfo(&chunk, 0, 0);
    assert_ptr_equal(chunk.sourceinfo.linenumbers, NULL);
    assert_ptr_equal(chunk.sourceinfo.linenumberCounter, NULL);

    freeChunk(&chunk);
}

/**
 * @brief Adds Value to chunks constants and writes opcode to load the constant to chunks code.
 * (OP_CONSTANT when index of constant fits into a single byte OP_CONSTANT_LONG otherwise)
 *
 * @param state
 */
static void chunk_writes_constants(void** state)
{
    (void)state;

    Chunk chunk;
    initChunk(&chunk);

    for (int i = 0; i < 0x1FF; i++) {
        writeConstant(&chunk, i, 1);
        assert_int_equal(chunk.constants.values[i], i);
    }

    int value = 0;
    for (int i = 0; i < 0x1FF;) {
        if (value <= 0xFF) {
            assert_int_equal(chunk.code[i], OP_CONSTANT);
            assert_int_equal(chunk.code[i + 1], value);
            i += 2;
        } else {
            printf("==============================\n");
            printf("0x%06X == 0x%02X 0x%02X 0x%02X\n", value, (value >> 16) & 0xFF,
                (value >> 8) & 0xFF, value & 0xFF);
            assert_int_equal(chunk.code[i], OP_CONSTANT);
            assert_int_equal(chunk.code[i + 1], ((value >> 16) & 0xFF));
            assert_int_equal(chunk.code[i + 2], ((value >> 8) & 0xFF));
            assert_int_equal(chunk.code[i + 3], (value & 0xFF));
            i += 4;
        }
        value += 1;
    }

    assertChunk(&chunk, 0x800, 0x5FC, chunk.code);
    assertConstants(&chunk, 0x200, 0x1FF, chunk.constants.values);
    assertSourceInfo(&chunk, 8, 1);
    assert_int_equal(chunk.sourceinfo.linenumbers[0], 1);
    assert_int_equal(chunk.sourceinfo.linenumberCounter[0], 0x5FC);

    freeChunk(&chunk);
}

/**
 * @brief The memory of a chunk can be freed. It will be initialized afterwards.
 *
 * @param state unused
 */
static void chunk_can_be_freed(void** state)
{
    (void)state;

    Chunk chunk;
    initChunk(&chunk);
    writeChunk(&chunk, 0x42, 1);
    freeChunk(&chunk);

    assertChunk(&chunk, 0, 0, NULL);
    assertConstants(&chunk, 0, 0, NULL);
    assertSourceInfo(&chunk, 0, 0);
    assert_ptr_equal(chunk.sourceinfo.linenumbers, NULL);
    assert_ptr_equal(chunk.sourceinfo.linenumberCounter, NULL);
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
int main(void)
{
    const struct CMUnitTest tests_nothing[] = {
        cmocka_unit_test(chunk_initializes),
        cmocka_unit_test(chunk_is_writable),
        cmocka_unit_test(chunk_adds_constants),
        cmocka_unit_test(chunk_can_be_freed),
        cmocka_unit_test(chunk_writes_constants),
    };
    return cmocka_run_group_tests(tests_nothing, NULL, NULL);
}
