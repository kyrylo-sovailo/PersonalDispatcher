#include "output.h"
#include "string.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#ifdef WIN32
    #include <Windows.h>

    struct ConsoleInfo
    {
        HANDLE handle;
        CONSOLE_SCREEN_BUFFER_INFO info;
        bool console;
    };
    static struct ConsoleInfo console_info[2];
#endif

static bool get_calender(time_t global_time, struct tm *global_calender, struct tm *local_calender)
{
    bool success = true;
    if (global_calender != NULL)
    {
        struct tm *p_global_calender = gmtime(&global_time);
        if (p_global_calender != NULL) *global_calender = *p_global_calender;
        else success = false;
    }
    if (local_calender != NULL)
    {
        struct tm *p_local_calender = localtime(&global_time);
        if (p_local_calender != NULL) *local_calender = *p_local_calender;
        else success = false;
    }
    return success;
}

void output_module_initialize(void)
{
    #ifdef WIN32
        unsigned char i;
        console_info[0].handle = GetStdHandle(STD_OUTPUT_HANDLE);
        console_info[1].handle = GetStdHandle(STD_ERROR_HANDLE);
        for (i = 0; i < 2; i++)
        {
            DWORD mode;
            GetConsoleScreenBufferInfo(console_info[i].handle, &console_info[i].info);
            console_info[i].console = GetConsoleMode(console_info[i].handle, &mode);
        }
    #else
        /* Nothing to do */
    #endif
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

void output_close(bool error_output)
{
    #ifdef WIN32
        /* Nothing to do */
        (void)error_output;
    #else
        fflush(error_output ? stderr : stdout);
    #endif
}

void output_print(bool error_output, const cchar_t *format, ...)
{
    va_list va;
    va_start(va, format);
    output_vprint(error_output, format, va);
    va_end(va);
}

void output_print_color(bool error_output, ccolor_t color, const cchar_t *format, ...)
{
    va_list va;
    va_start(va, format);
    output_vprint_color(error_output, color, format, va);
    va_end(va);
}

void output_vprint(bool error_output, const cchar_t *format, va_list va)
{
    #ifdef WIN32
        const struct ConsoleInfo *info = &console_info[error_output ? 1 : 0];
        struct CharBuffer buffer = ZERO_INIT;
        #if defined(ERROR_PRINT)
        bool success = true;
        #elif defined(ERROR_TRACE)
        struct Error *error = OK;
        #endif
        PGOTO(string_internal_vprint_append(&buffer, true, format, va));
        if (info->console)
        {
            WriteConsole(info->handle, buffer.p, (DWORD)buffer.size, NULL, NULL);
        }
        else
        {
            #ifdef UNICODE
                nchar_t *string;
                size_t string_size;
                PGOTO(string_internal_to_nstring(string_get(&buffer), buffer.size + 1, NULL, &string_size, true));
                string = (nchar_t*)malloc(string_size);
                AGOTO(string != NULL);
                PIGNORE(string_internal_to_nstring(string_get(&buffer), buffer.size + 1, string, &string_size, true));
                WriteFile(info->handle, string, (DWORD)string_size - 1, NULL, NULL);
                free(string);
            #else
                WriteFile(info->handle, buffer.p, (DWORD)buffer.size, NULL, NULL);
            #endif
        }
        #if defined(ERROR_PRINT)
        failure:
        (void)success;
        #elif defined(ERROR_TRACE)
        failure:
        error_finalize(error);
        #endif
        string_finalize(&buffer);
    #else
        COMMON_VFW(printf(error_output ? stderr : stdout, format, va));
    #endif
}

void output_vprint_color(bool error_output, ccolor_t color, const cchar_t *format, va_list va)
{
    #ifdef WIN32
        const struct ConsoleInfo *info = &console_info[error_output ? 1 : 0];
        SetConsoleTextAttribute(info->handle, color);
        output_vprint(error_output, format, va);
        SetConsoleTextAttribute(info->handle, info->info.wAttributes);
    #else
        output_print(error_output, COMMON_S, color);
        output_vprint(error_output, format, va);
        output_print(error_output, COMMON_L("\x1b[0m"));
    #endif
}

void output_print_time(bool error_output)
{
    cchar_t calender_buffer[64];
    const size_t size = sizeof(calender_buffer) / sizeof(*calender_buffer);
    struct tm global_calender, local_calender;
    if (!get_calender(time(NULL), &global_calender, &local_calender)) return;
    COMMON_WCS(ftime(calender_buffer, size, COMMON_L("%a, %d %b %Y %H:%M:%S GMT"), &global_calender)); /* Hardcode GMT because Windows identified it incorrectly */
    output_print(error_output, COMMON_S COMMON_N, calender_buffer);
    COMMON_WCS(ftime(calender_buffer, size, COMMON_L("%a, %d %b %Y %H:%M:%S %Z"), &local_calender));
    output_print(error_output, COMMON_S COMMON_N, calender_buffer);
}
