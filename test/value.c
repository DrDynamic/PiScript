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
#include "value.h"

/**
 * redifinitions / wrapping
 *
 */

/*
 * Tests
 *
 */

/**
 * @brief A ValueArray is initialized with integers set to 0 and values set to NULL
 *
 * @param state unused
 */
static void ValueArray_initializes(void **state) {
    (void)state;

    ValueArray valueArray;
    initValueArray(&valueArray);

    assert_int_equal(valueArray.capacity, 0);
    assert_int_equal(valueArray.count, 0);
    assert_ptr_equal(valueArray.values, NULL);
}

/**
 * @brief A ValueArray can be written to
 *
 * @param state unused
 */
static void ValueArray_is_writable(void **state) {
    (void)state;

    ValueArray valueArray;
    initValueArray(&valueArray);
    writeValueArray(&valueArray, 0x42);

    assert_int_equal(valueArray.capacity, 8);
    assert_int_equal(valueArray.count, 1);
    assert_int_equal(valueArray.values[0], 0x42);

    free(valueArray.values);
}

/**
 * @brief The memory of a ValueArray can be freed. It will be initialized afterwards.
 *
 * @param state unused
 */
static void ValueArray_can_be_freed(void **state) {
    (void)state;

    ValueArray valueArray;
    initValueArray(&valueArray);
    writeValueArray(&valueArray, 0x42);
    freeValueArray(&valueArray);

    assert_int_equal(valueArray.capacity, 0);
    assert_int_equal(valueArray.count, 0);
    assert_ptr_equal(valueArray.values, NULL);
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
        cmocka_unit_test(ValueArray_initializes),
        cmocka_unit_test(ValueArray_is_writable),
        cmocka_unit_test(ValueArray_can_be_freed),
    };
    return cmocka_run_group_tests(tests_nothing, NULL, NULL);
}
