#ifndef COMMONLIB_PATH_H
#define COMMONLIB_PATH_H

#include "bool.h"
#include "char_buffer.h"
#include "error.h"

#include <stdarg.h>
#include <stddef.h>

/*
Path is a String
It refers to an absolute or relative path, file or directory
Path maintains the correct form at all times:
no double slashes, no trailing slashes (except for root), no slashes on backslash-based systems, no ., no reducible ..
empty path is a valid relative path, equivalent to .
*/

/* Appends file name to path, while also caring about slashes and parent directories */
ERROR_TYPE path_append_mem(struct CharBuffer *path, const char *other, size_t other_size) NODISCARD;

/* Returns whether the path is absolute */
bool path_absolute_mem(const char *path, size_t path_size);

/* Gets current working directory */
ERROR_TYPE path_get_working_directory(struct CharBuffer *path) NODISCARD;
/* Gets directory of the path (directory == path is allowed) */
bool path_get_directory(struct CharBuffer *directory, const struct CharBuffer *path, bool append_dotdot_if_dotdot) NODISCARD; /* Modified */ 

#endif
