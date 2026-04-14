#ifndef COMMONLIB_STRING_H
#define COMMONLIB_STRING_H

#include "char_buffer.h"
#include "error.h"
#include "macro.h"

#include <stdarg.h>
#include <stddef.h>

/*
String is a CharBuffer that keeps a null terminator at the end.
CharBuffer fields are implemented in the following way:
 - size     - String size (does not include null terminator, unlike other buffers)
 - capacity - Buffer capacity (includes null terminator, just like other buffers)
*/

/* Initializes string */
void string_initialize(struct CharBuffer *string);

/* Finalizes string */
void string_finalize(struct CharBuffer *string);

/* Gets raw C string */
const cchar_t *string_get(const struct CharBuffer *string);

/* Sets the size to zero */
void string_zero(struct CharBuffer *string);

/* Resizes string (size does not include null terminator) */
ERROR_TYPE string_resize(struct CharBuffer *string, size_t size) NODISCARD;

/* Ensures that the string has enough capacity (capacity does not include null terminator) */
ERROR_TYPE string_reserve(struct CharBuffer *string, size_t capacity) NODISCARD;

/* Copies string */
ERROR_TYPE string_copy(struct CharBuffer *string, const struct CharBuffer *other) NODISCARD;
ERROR_TYPE string_copy_str(struct CharBuffer *string, const cchar_t *other) NODISCARD;
ERROR_TYPE string_copy_mem(struct CharBuffer *string, const cchar_t *other, size_t other_size) NODISCARD;

/* Adds character to the back of the string */
ERROR_TYPE string_push(struct CharBuffer *string, cchar_t other) NODISCARD;

/* Adds many characters to the back of the string */
ERROR_TYPE string_append_mem(struct CharBuffer *string, const cchar_t *other, size_t other_size) NODISCARD;

/* Removes beginning at trailing spaces from string */
void string_trim(struct CharBuffer *string);

/* Substitutes segment */
ERROR_TYPE string_replace_mem(struct CharBuffer *string, size_t begin, size_t size, const cchar_t *other, size_t other_size) NODISCARD;

#ifdef WIN32

ERROR_TYPE nstring_to_wstring(const nchar_t *np, size_t nsize, wchar_t *wp, size_t *wsize) NODISCARD;
ERROR_TYPE wstring_to_nstring(const wchar_t *wp, size_t wsize, nchar_t *np, size_t *nsize) NODISCARD;

#endif

#endif
