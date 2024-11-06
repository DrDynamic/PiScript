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
#include "mylib/mylib.h"


/*
 * Tests
 *
 */

/**
 * @brief Function sayHello should copy "Hello" into char-array.
 *
 * @param state unused
 */
static void says_hello(void **state) {
    (void)state;
    char str[10];
    mylib_sayHello(str);
    assert_string_equal("Hello", str);
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
    const struct CMUnitTest tests_mylib[] = {
        cmocka_unit_test(says_hello),
    };
    return cmocka_run_group_tests(tests_mylib, NULL, NULL);
}
