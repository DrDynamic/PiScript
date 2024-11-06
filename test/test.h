/**
 * @file test.h
 * @author NikLeberg (niklaus.leuenb@gmail.com)
 * @brief Common header for tests with cmocka.
 * @version 0.1
 * @date 2021-07-12
 *
 * @copyright Copyright (c) 2021 Niklaus Leuenberger
 *
 */

#pragma once


/*
 * Includes
 *
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>


/*
 * Type declarations
 *
 */

/* ... */


/*
 * Variable declarations
 *
 */

/* ... */


/*
 * Public functions
 *
 */

/**
 * @brief Options for ASAN
 *
 * The cmocka-tests get compiled and linked with "-fsanitize" e.g. ASAN. These
 * options then extend the default configuration of ASAN.
 *
 * @return const char* options, separated with a colon
 */
const char *__asan_default_options() {
    return "detect_stack_use_after_return=true:"
           "strict_string_checks=true";
}
