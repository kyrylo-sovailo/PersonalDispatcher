#ifndef COMMONLIB_OUTPUT_H
#define COMMONLIB_OUTPUT_H

#include "bool.h"
#include "char.h"
#include "macro.h"

#include <stdarg.h>
#include <stddef.h>

/* Initializes/finalizes output module (guaranteed to succeed) */
void output_module_initialize(void);
void output_module_finalize(void);

/* Opens normal/error output (guaranteed to succeed) */
void output_open(bool error);

/* Closes normal/error output (guaranteed to succeed) */
void output_close(bool error);

/* Writes message to normal/error output (guaranteed to succeed) */
void output_print(bool error, const cchar_t *format, ...) PRINTFLIKE(2, 3);

/* Writes message to normal/error output (guaranteed to succeed) */
void output_vprint(bool error, const cchar_t *format, va_list va) PRINTFLIKE(2, 0);

/* Write timestamp to normal/error output (guaranteed to succeed) */
void output_print_time(bool error);

#endif
