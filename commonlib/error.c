#include "error.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void error_print_die(int code, const char *format, ...)
{
    va_list va;
    va_start(va, format);
    vfprintf(stderr, format, va);
    va_end(va);
    exit(code);
}
