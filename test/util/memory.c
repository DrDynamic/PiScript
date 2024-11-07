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
#include "util/memory.h"

/*
 * Tests
 *
 */

/**
 * @brief Function reallocate should allocate memory, when it isn't already.
 *
 * @param state unused
 */
static void reallocate_allocates_memory(void **state) {
    (void)state;

    void *data = NULL;

    expect_value(__wrap_realloc, mem_address, data);
    expect_value(__wrap_realloc, size, 8);
    data = reallocate(data, 0, 8);
    free(data);
}

/**
 * @brief Function reallocate should reallocate memory, when it is already
 * allocated.
 *
 * @param state unused
 */
static void reallocate_reallocates_memory(void **state) {
    (void)state;

    void *data = NULL;

    expect_value(__wrap_realloc, mem_address, data);
    expect_value(__wrap_realloc, size, 16);
    data = reallocate(data, 8, 16);
    free(data);
}

/**
 * @brief Function reallocate should free memory, when it new size is 0.
 *
 * @param state unused
 */
static void reallocate_frees_memory(void **state) {
    (void)state;

    void *data = malloc(8);
    data = reallocate(data, 8, 0);
}

/**
 * @brief Macro GROW_CAPACITY returns a minimum value of 8, when the given capacity is smaler.
 *
 * @param state unused
 */
static void GROW_CAPACITY_has_minimum_capacity(void **state) {
    (void)state;

    assert_int_equal(GROW_CAPACITY(0), 8);
    assert_int_equal(GROW_CAPACITY(7), 8);
}

/**
 * @brief Macro GROW_CAPACITY returns the doulbe of the given capacity, when the given capacity is
 * bigger or equal to 8.
 *
 * @param state unused
 */
static void GROW_CAPACITY_doubles_capacity(void **state) {
    (void)state;

    assert_int_equal(GROW_CAPACITY(8), 16);
    assert_int_equal(GROW_CAPACITY(16), 32);
}

/**
 * @brief Macro GROW_CAPACITY returns the doulbe of the given capacity, when the given capacity is
 * bigger or equal to 8.
 *
 * @param state unused
 */
static void GROW_ARRAY_reallocates_memory(void **state) {
    (void)state;

    uint8_t *data = NULL;

    expect_value(__wrap_realloc, mem_address, data);
    expect_value(__wrap_realloc, size, 8);
    data = GROW_ARRAY(uint8_t, data, 0, 8);

    expect_value(__wrap_realloc, mem_address, data);
    expect_value(__wrap_realloc, size, 16);
    data = GROW_ARRAY(uint8_t, data, 8, 16);

    // no realloc -> calls free()
    data = GROW_ARRAY(uint8_t, data, 16, 0);
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
    const struct CMUnitTest tests_memory[] = {
        cmocka_unit_test(reallocate_allocates_memory),
        cmocka_unit_test(reallocate_reallocates_memory),
        cmocka_unit_test(reallocate_frees_memory),
        cmocka_unit_test(GROW_CAPACITY_has_minimum_capacity),
        cmocka_unit_test(GROW_CAPACITY_doubles_capacity),
        cmocka_unit_test(GROW_ARRAY_reallocates_memory),
    };
    return cmocka_run_group_tests(tests_memory, NULL, NULL);
}
