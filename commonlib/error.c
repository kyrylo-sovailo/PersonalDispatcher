#include "error.h"
#include "output.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

void error_print_die(int code, const cchar_t *format, ...)
{
    va_list va;
    output_open(true);
    va_start(va, format);
    output_vprint(true, format, va);
    va_end(va);
    output_print(true, COMMON_N);
    output_print_time(true);
    output_close(true);
    exit(code);
}
