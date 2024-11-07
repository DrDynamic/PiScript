#include <stdlib.h>
#include <stdio.h>

#include <setjmp.h>
#include <cmocka.h>


void *__real_realloc(void *mem_address, size_t newsize);

void *__wrap_realloc(void *mem_address, size_t size);
void *__wrap_realloc(void *mem_address, size_t size) {
    check_expected(mem_address);
    check_expected(size);
    return __real_realloc(mem_address, size);
}