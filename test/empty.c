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

#include "test.h"

/**
 * redifinitions / wrapping
 *
 */

/*
 * Tests
 *
 */

/**
 * @brief Function reallocate should allocate memory, when it isn't already.
 *
 * @param state unused
 */
static void empty_test(void **state) {
    (void)state;
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
        cmocka_unit_test(empty_test),
    };
    return cmocka_run_group_tests(tests_nothing, NULL, NULL);
}
