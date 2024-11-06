/**
 * @file main.c
 * @author NikLeberg (niklaus.leuenb@gmail.com)
 * @brief
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

#include <stdio.h>

#include "mylib/mylib.h"


/*
 * Main program
 *
 */

/**
 * @brief Main
 *
 * @return int allways 0
 */
int main() {
    char str[10];
    mylib_sayHello(str);
    printf("\n%s", str);
    return 0;
}
