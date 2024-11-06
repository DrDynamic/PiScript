/**
 * @file mylib.c
 * @author NikLeberg (niklaus.leuenb@gmail.com)
 * @brief Example library that says "Hello"
 * @version 0.1
 * @date 2021-06-22
 *
 * @copyright Copyright (c) 2021 Niklaus Leuenberger
 *
 */


/*
 * Includes
 *
 */

#include <string.h>

#include "mylib/mylib.h"


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
 * Private function prototypes
 *
 */

/* ... */


/*
 * Implementation of public functions
 *
 */

void mylib_sayHello(char *data) {
    strcpy(data, "Hello");
}


/*
 * Implementation of private functions
 *
 */

/* ... */
