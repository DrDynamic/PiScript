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
#include "chunk/sourceinfo.h"

/*
 * Tests
 *
 */

/**
 * @brief SourceInfo can be initialized.
 *
 * @param state unused
 */
static void sourceinfo_initializes(void** state)
{
    (void)state;

    SourceInfo info;
    initSourceInfo(&info);

    assert_int_equal(info.capacity, 0);
    assert_int_equal(info.count, 0);
    assert_ptr_equal(info.linenumbers, NULL);
    assert_ptr_equal(info.linenumberCounter, NULL);
}

/**
 * @brief Can add linenumbers to Sourceinfo.
 *
 * @param state unused
 */
static void sourceinfo_add_linenumber(void** state)
{
    (void)state;

    SourceInfo info;
    initSourceInfo(&info);

    addLinenumer(&info, 1);

    assert_int_equal(info.capacity, 8);
    assert_int_equal(info.count, 1);
    assert_int_equal(info.linenumbers[0], 1);
    assert_int_equal(info.linenumberCounter[0], 1);

    freeSourceInfo(&info);
}


/**
 * @brief Same linenumbers in a sequence increment counter in Sourceinfo.
 *
 * @param state unused
 */
static void sourceinfo_incement_counter(void** state)
{
    (void)state;

    SourceInfo info;
    initSourceInfo(&info);

    addLinenumer(&info, 1);
    addLinenumer(&info, 1);
    addLinenumer(&info, 1);

    assert_int_equal(info.capacity, 8);
    assert_int_equal(info.count, 1);
    assert_int_equal(info.linenumbers[0], 1);
    assert_int_equal(info.linenumberCounter[0], 3);

    addLinenumer(&info, 2);

    assert_int_equal(info.capacity, 8);
    assert_int_equal(info.count, 2);
    assert_int_equal(info.linenumbers[1], 2);
    assert_int_equal(info.linenumberCounter[1], 1);


    addLinenumer(&info, 5);
    addLinenumer(&info, 5);
    addLinenumer(&info, 5);
    addLinenumer(&info, 5);
    addLinenumer(&info, 5);
    addLinenumer(&info, 5);

    assert_int_equal(info.capacity, 8);
    assert_int_equal(info.count, 3);
    assert_int_equal(info.linenumbers[2], 5);
    assert_int_equal(info.linenumberCounter[2], 6);

    freeSourceInfo(&info);
}

/**
 * @brief Function reallocate should allocate memory, when it isn't already.
 *
 * @param state unused
 */
static void sourceinfo_get_line(void** state)
{
    (void)state;

    SourceInfo info;
    initSourceInfo(&info);

    addLinenumer(&info, 1); // offset 0
    addLinenumer(&info, 2); // offset 1
    addLinenumer(&info, 2); // offset 2
    addLinenumer(&info, 3); // offset 3
    addLinenumer(&info, 5); // offset 4
    addLinenumer(&info, 5); // offset 5
    addLinenumer(&info, 5); // offset 6
    addLinenumer(&info, 5); // offset 7
    addLinenumer(&info, 5); // offset 8
    addLinenumer(&info, 5); // offset 9

    assert_int_equal(getSourceInfoLinenumber(&info, 0), 1);
    assert_int_equal(getSourceInfoLinenumber(&info, 1), 2);
    assert_int_equal(getSourceInfoLinenumber(&info, 2), 2);
    assert_int_equal(getSourceInfoLinenumber(&info, 3), 3);
    assert_int_equal(getSourceInfoLinenumber(&info, 4), 5);
    assert_int_equal(getSourceInfoLinenumber(&info, 5), 5);
    assert_int_equal(getSourceInfoLinenumber(&info, 6), 5);
    assert_int_equal(getSourceInfoLinenumber(&info, 7), 5);
    assert_int_equal(getSourceInfoLinenumber(&info, 8), 5);
    assert_int_equal(getSourceInfoLinenumber(&info, 9), 5);

    freeSourceInfo(&info);
}


/**
 * @brief Function reallocate should allocate memory, when it isn't already.
 *
 * @param state unused
 */
static void sourceinfo_can_be_freed(void** state)
{
    (void)state;

    SourceInfo info;
    initSourceInfo(&info);

    addLinenumer(&info, 1);
    freeSourceInfo(&info);

    assert_int_equal(info.capacity, 0);
    assert_int_equal(info.count, 0);
    assert_ptr_equal(info.linenumbers, NULL);
    assert_ptr_equal(info.linenumberCounter, NULL);
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
    const struct CMUnitTest tests_sourceinfo[] = {
        cmocka_unit_test(sourceinfo_initializes),
        cmocka_unit_test(sourceinfo_add_linenumber),
        cmocka_unit_test(sourceinfo_incement_counter),
        cmocka_unit_test(sourceinfo_can_be_freed),
        cmocka_unit_test(sourceinfo_get_line),
    };
    return cmocka_run_group_tests(tests_sourceinfo, NULL, NULL);
}
