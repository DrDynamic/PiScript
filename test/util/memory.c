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

#include "../test.h"
#include "util/memory.h"

/**
 * redifinitions / wrapping
 *
 */
void *__real_realloc(void *ptr, size_t size);
void *__wrap_realloc(void *ptr, size_t size) {
    check_expected(ptr);
    check_expected(size);

    return __real_realloc(ptr, size);
}

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
    expect_value(__wrap_realloc, size, 9);

    reallocate(data, 0, 8);
    data = 0;
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
    };
    return cmocka_run_group_tests(tests_memory, NULL, NULL);
}
