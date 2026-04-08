#include "string.h"
#include "../kpd.h"

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char Flag;
enum
{
    FLAG_HASH           = 0x01,
    FLAG_ZERO           = 0x02,
    FLAG_MINUS          = 0x04,
    FLAG_PLUS           = 0x08,
    FLAG_SPACE          = 0x10
};

typedef unsigned char Length;
enum
{
    LENGTH_NONE,
    LENGTH_SHORT_SHORT, /* hh */
    LENGTH_SHORT,       /* h  */
    LENGTH_LONG,        /* l  */
    LENGTH_LONG_LONG,   /* ll */
    LENGTH_MAX,         /* j  */
    LENGTH_SIZE,        /* z  */
    LENGTH_PTRDIFF,     /* t  */
    LENGTH_LONG_DOUBLE  /* L  */
};

void string_finalize(struct CharBuffer *string)
{
    const struct CharBuffer zero = ZERO_INIT;
    if (string->p != NULL) free(string->p);
    *string = zero;
}

const char *string_get(const struct CharBuffer *string)
{
    return (string->p == NULL) ? "" : string->p;
}

void string_zero(struct CharBuffer *string)
{
    if (string->p != NULL) string->p[0] = '\0';
    string->size = 0;
}

ERROR_TYPE string_resize(struct CharBuffer *string, size_t size)
{
    if (size + 1 > string->capacity)
    {
        char *new_p;
        size_t new_capacity = (string->capacity == 0) ? 1 : string->capacity;
        while (size + 1 > new_capacity) new_capacity *= 2;
        new_p = (char*)realloc(string->p, new_capacity * sizeof(*string->p));
        ARET(ERR_MALLOC, new_p != NULL);
        string->capacity = new_capacity;
        string->p = new_p;
    }
    string->size = size;
    string->p[size] = '\0';
    ERROR_RETURN_OK();
}

ERROR_TYPE string_reserve(struct CharBuffer *string, size_t capacity)
{
    if (capacity + 1 > string->capacity)
    {
        char *new_p;
        size_t new_capacity = (string->capacity == 0) ? 1 : string->capacity;
        while (capacity + 1 > new_capacity) new_capacity *= 2;
        new_p = (char*)realloc(string->p, new_capacity * sizeof(*string->p));
        ARET(ERR_MALLOC, new_p != NULL);
        string->capacity = new_capacity;
        string->p = new_p;
    }
    ERROR_RETURN_OK();
}

ERROR_TYPE string_copy(struct CharBuffer *string, const struct CharBuffer *other)
{
    PRET(string_copy_mem(string, other->p, other->size));
    ERROR_RETURN_OK();
}

ERROR_TYPE string_copy_str(struct CharBuffer *string, const char *other)
{
    PRET(string_copy_mem(string, other, strlen(other)));
    ERROR_RETURN_OK();
}

ERROR_TYPE string_copy_mem(struct CharBuffer *string, const char *other, size_t other_size)
{
    PRET(string_resize(string, other_size));
    memcpy(string->p, other, other_size);
    ERROR_RETURN_OK();
}

ERROR_TYPE string_push(struct CharBuffer *string, char other)
{
    if (string->size + 2 > string->capacity)
    {
        const size_t new_capacity = (string->capacity == 0) ? 1 : (string->capacity * 2);
        char *new_p = (char*)realloc(string->p, new_capacity * sizeof(*string->p));
        ARET(ERR_MALLOC, new_p != NULL);
        string->capacity = new_capacity;
        string->p = new_p;
    }
    string->p[string->size] = other;
    string->size++;
    string->p[string->size] = '\0';
    ERROR_RETURN_OK();
}

ERROR_TYPE string_append(struct CharBuffer *string, const struct CharBuffer *other)
{
    PRET(string_append_mem(string, other->p, other->size));
    ERROR_RETURN_OK();
}

ERROR_TYPE string_append_str(struct CharBuffer *string, const char *other)
{
    PRET(string_append_mem(string, other, strlen(other)));
    ERROR_RETURN_OK();
}

ERROR_TYPE string_append_mem(struct CharBuffer *string, const char *other, size_t other_size)
{
    const size_t old_size = string->size;
    PRET(string_resize(string, string->size + other_size));
    memcpy(string->p + old_size, other, other_size);
    ERROR_RETURN_OK();
}

void string_trim(struct CharBuffer *string)
{
    /* Count beginning spaces */
    size_t beginning_spaces = 0, ending_spaces = 0, spaces;
    while (true)
    {
        char c;
        if (beginning_spaces == string->size)
        {
            /* The string is all spaces */
            string_zero(string);
            return;
        }
        c = string->p[beginning_spaces];
        if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v')) break;
        beginning_spaces++;
    }
    
    /* Count ending spaces */
    while (true)
    {
        char c = string->p[string->size - ending_spaces - 1];
        if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v')) break;
        ending_spaces++;
    }
    
    /* Move */
    spaces = beginning_spaces + ending_spaces;
    if (beginning_spaces > 0) memmove(string->p, string->p + beginning_spaces, string->size - spaces);
    string->size -= spaces;
    string->p[string->size] = '\0';
}

ERROR_TYPE string_replace_mem(struct CharBuffer *string, size_t begin, size_t size, const char *other, size_t other_size)
{
    char *segment_p;
    ARET(ERR_REPLACE, begin + size < string->size);
    if (other_size != size)
    {
        const size_t old_size = string->size;
        const size_t new_size = old_size + other_size - size;
        if (other_size > size)
        {
            /* Expanding */
            PRET(string_resize(string, new_size));
        }
        segment_p = string->p + begin;
        memmove(segment_p + other_size, segment_p + size, old_size - size);
        if (other_size < size)
        {
            /* Shrinking */
            string->p[new_size] = '\0';
            string->size = new_size;
        }
    }
    else
    {
        segment_p = string->p + begin;
    }
    memcpy(segment_p, other, other_size);
    ERROR_RETURN_OK();
}

#ifdef WIN32

static ERROR_TYPE string_internal_to_wstring(const char *p, size_t size, wchar_t *wp, size_t *wsize) NODISCARD;
static ERROR_TYPE string_internal_to_wstring(const char *p, size_t size, wchar_t *wp, size_t *wsize)
{
    *wsize = 0;
    while (size > 0)
    {
        /* Decode UTF-8 */
        const unsigned char *cast = (const unsigned char*)p;
        const unsigned char c = *cast;
        size_t symbol_size;
        unsigned int code;
        if ((c & 0x80) == 0)
        {
            symbol_size = 1;
            code = c;
        }
        else if ((c & 0xE0) == 0xC0)
        {
            ARET(ERR_CODEC, size >= 2 && (cast[1] & 0xC0) == 0x80);
            symbol_size = 2;
            code = (unsigned int)(((c & 0x1F) << 6) | (cast[1] & 0x3F));
        }
        else if ((c & 0xF0) == 0xE0)
        {
            ARET(ERR_CODEC, size >= 3 && (cast[1] & 0xC0) == 0x80 && (cast[2] & 0xC0) == 0x80);
            symbol_size = 3;
            code = (unsigned int)(((c & 0x0F) << 12) | ((cast[1] & 0x3F) << 6) | (cast[2] & 0x3F));
        }
        else if ((c & 0xF8) == 0xF0)
        {
            ARET(ERR_CODEC, size >= 4 && (cast[1] & 0xC0) == 0x80 && (cast[2] & 0xC0) == 0x80 && (cast[3] & 0xC0) == 0x80);
            symbol_size = 4;
            code = (unsigned int)(((c & 0x07) << 18) | ((cast[1] & 0x3F) << 12) | ((cast[2] & 0x3F) << 6) | (cast[3] & 0x3F));
        }
        else RET0(ERR_CODEC, "Invalid UTF-8 symbol");
        p += symbol_size;
        size -= symbol_size;

        /* Encode UTF-16 */
        ARET(ERR_CODEC, code < 0xE000 || (code >= 0xD800 && code < 0x110000));
        if (code < 0x10000)
        {
            if (wp != NULL) *wp = (wchar_t)code;
            symbol_size = 1;
        }
        else
        {
            if (wp != NULL)
            {
                wp[0] = ((code - 0x10000) >> 10) & 0x3FF;
                wp[1] = (code - 0x10000) & 0x3FF;
            }
            symbol_size = 2;
        }
        if (wp != NULL) wp += symbol_size;
        *wsize += symbol_size;
    }
    ERROR_RETURN_OK();
}

static ERROR_TYPE string_internal_to_string(const wchar_t *wp, size_t wsize, char *p, size_t *size) NODISCARD;
static ERROR_TYPE string_internal_to_string(const wchar_t *wp, size_t wsize, char *p, size_t *size)
{
    *size = 0;
    while (wsize > 0)
    {
        /* Decode UTF-16 */
        size_t symbol_size;
        unsigned int code;
        unsigned char *cast = (unsigned char*)p;
        if (!((unsigned int)*wp >= 0xD800 && (unsigned int)*wp < 0xE000))
        {
            code = (unsigned int)*wp;
            symbol_size = 1;
        }
        else if ((unsigned int)*wp >= 0xD800 && (unsigned int)*wp < 0xDC00)
        {
            ARET(ERR_CODEC, wsize >= 2 && ((unsigned int)wp[1] >= 0xDC00 && (unsigned int)wp[1] < 0xE000));
            code = ((((unsigned int)*wp - 0xD800) << 10) | ((unsigned int)wp[1] - 0xDC00)) + 0x10000;
            symbol_size = 2;
        }
        else RET0("Invalid UTF-16 symbol");
        wp += symbol_size;
        wsize -= symbol_size;

        /* Encode UTF-8 */
        ARET(ERR_CODEC, code < 0x110000);
        if (code < 0x80)
        {
            if (cast != NULL) *cast = (unsigned char)code;
            symbol_size = 1;
        }
        else if (code < 0x800)
        {
            if (cast != NULL)
            {
                cast[0] = (unsigned char)(0xC0 | ((code >> 6) & 0x1F));
                cast[1] = (unsigned char)(0x80 | (code & 0x3F));
            }
            symbol_size = 2;
        }
        else if (code < 0x10000)
        {
            if (cast != NULL)
            {
                cast[0] = (unsigned char)(0xE0 | ((code >> 12) & 0x0F));
                cast[1] = (unsigned char)(0x80 | ((code >> 6) & 0x3F));
                cast[2] = (unsigned char)(0x80 | (code & 0x3F));
            }
            symbol_size = 3;
        }
        else
        {
            if (cast != NULL)
            {
                cast[0] = (unsigned char)(0xE0 | ((code >> 18) & 0x07));
                cast[1] = (unsigned char)(0x80 | ((code >> 12) & 0x3F));
                cast[2] = (unsigned char)(0x80 | ((code >> 6) & 0x3F));
                cast[3] = (unsigned char)(0x80 | (code & 0x3F));
            }
            symbol_size = 4;
        }
        if (p != NULL) p += symbol_size;
        *size += symbol_size;
    }
    ERROR_RETURN_OK();
}

ERROR_TYPE string_to_wstring(struct WCharBuffer *wstring, const struct CharBuffer *string) NODISCARD
{
    size_t wsize;
    PRET(string_internal_to_wstring(string->p, string->size, NULL, &wsize));
    PRET(wchar_buffer_resize(wstring, wsize + 1));
    PIGNORE(string_internal_to_wstring(string->p, string->size, wstring->p, &wsize));
    wstring->size = wsize;
    wstring->p[wsize] = '\0';
    ERROR_RETURN_OK();
}

ERROR_TYPE string_to_string(struct CharBuffer *string, const struct WCharBuffer *wstring) NODISCARD
{
    size_t size;
    PRET(string_internal_to_string(wstring->p, wstring->size, NULL, &size));
    PRET(char_buffer_resize(string, size + 1));
    PIGNORE(string_internal_to_string(wstring->p, wstring->size, string->p, &size));
    string->size = size;
    string->p[size] = '\0';
    ERROR_RETURN_OK();
}

#endif
