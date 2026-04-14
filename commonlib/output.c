#include "output.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

void output_module_initialize(void)
{
    /* Nothing to do */
}

void output_module_finalize(void)
{
    /* Nothing to do */
}

void output_open(bool error)
{
    /* Nothing to do */
    (void)error;
}

void output_close(bool error)
{
    fflush(error ? stderr : stdout);
}

void output_print(bool error, const cchar_t* format, ...)
{
    va_list va;
    va_start(va, format);
    output_vprint(error, format, va);
    va_end(va);
}

void output_vprint(bool error, const cchar_t* format, va_list va)
{
    COMMON(vf,vfw,printf(error ? stderr : stdout, format, va));
}

void output_print_time(bool error)
{
    time_t global_time;
    struct tm* p_global_calender, global_calender;
    struct tm* p_local_calender, local_calender;
    nchar_t calender_buffer[64];

    /* Print time */
    global_time = time(NULL);
    p_global_calender = gmtime(&global_time);
    if (p_global_calender != NULL) global_calender = *p_global_calender;
    p_local_calender = localtime(&global_time);
    if (p_local_calender != NULL) local_calender = *p_local_calender;
    if (p_global_calender != NULL)
    {
        strftime(calender_buffer, sizeof(calender_buffer), "%a, %d %b %Y %H:%M:%S GMT", &global_calender); /* Hardcode GMT because Windows identified it incorrectly */
        output_print(error, COMMON_L("%s\n"), calender_buffer);
    }
    if (p_local_calender != NULL)
    {
        strftime(calender_buffer, sizeof(calender_buffer), "%a, %d %b %Y %H:%M:%S %Z", &local_calender);
        output_print(error, COMMON_L("%s\n"), calender_buffer);
    }
}
