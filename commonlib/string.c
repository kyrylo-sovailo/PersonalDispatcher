#include "string.h"
#include "../kpd.h"

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

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

void string_initialize(struct CharBuffer *string)
{
    const struct CharBuffer zero = ZERO_INIT;
    *string = zero;
}

void string_finalize(struct CharBuffer *string)
{
    const struct CharBuffer zero = ZERO_INIT;
    if (string->p != NULL) free(string->p);
    *string = zero;
}

const cchar_t *string_get(const struct CharBuffer *string)
{
    return (string->p == NULL) ? COMMON_L("") : string->p;
}

void string_zero(struct CharBuffer *string)
{
    if (string->p != NULL) string->p[0] = COMMON_L('\0');
    string->size = 0;
}

ERROR_TYPE string_resize(struct CharBuffer *string, size_t size)
{
    if (size + 1 > string->capacity)
    {
        cchar_t *new_p;
        size_t new_capacity = (string->capacity == 0) ? 1 : string->capacity;
        while (size + 1 > new_capacity) new_capacity *= 2;
        new_p = (cchar_t*)realloc(string->p, new_capacity * sizeof(*string->p));
        ARET(ERR_MALLOC, new_p != NULL);
        string->capacity = new_capacity;
        string->p = new_p;
    }
    string->size = size;
    string->p[size] = COMMON_L('\0');
    ERROR_RETURN_OK();
}

ERROR_TYPE string_reserve(struct CharBuffer *string, size_t capacity)
{
    if (capacity + 1 > string->capacity)
    {
        cchar_t *new_p;
        size_t new_capacity = (string->capacity == 0) ? 1 : string->capacity;
        while (capacity + 1 > new_capacity) new_capacity *= 2;
        new_p = (cchar_t*)realloc(string->p, new_capacity * sizeof(*string->p));
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

ERROR_TYPE string_copy_str(struct CharBuffer *string, const cchar_t *other)
{
    PRET(string_copy_mem(string, other, COMMON(str,wcs,len(other))));
    ERROR_RETURN_OK();
}

ERROR_TYPE string_copy_mem(struct CharBuffer *string, const cchar_t *other, size_t other_size)
{
    PRET(string_resize(string, other_size));
    memcpy(string->p, other, other_size);
    ERROR_RETURN_OK();
}

ERROR_TYPE string_push(struct CharBuffer *string, cchar_t other)
{
    if (string->size + 2 > string->capacity)
    {
        const size_t new_capacity = (string->capacity == 0) ? 1 : (string->capacity * 2);
        cchar_t *new_p = (cchar_t*)realloc(string->p, new_capacity * sizeof(*string->p));
        ARET(ERR_MALLOC, new_p != NULL);
        string->capacity = new_capacity;
        string->p = new_p;
    }
    string->p[string->size] = other;
    string->size++;
    string->p[string->size] = COMMON_L('\0');
    ERROR_RETURN_OK();
}

ERROR_TYPE string_append_mem(struct CharBuffer *string, const cchar_t *other, size_t other_size)
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
        cchar_t c;
        if (beginning_spaces == string->size)
        {
            /* The string is all spaces */
            string_zero(string);
            return;
        }
        c = string->p[beginning_spaces];
        if (!(c == COMMON_L(' ') || c == COMMON_L('\t') || c == COMMON_L('\n') || c == COMMON_L('\r') || c == COMMON_L('\v'))) break;
        beginning_spaces++;
    }
    
    /* Count ending spaces */
    while (true)
    {
        cchar_t c = string->p[string->size - ending_spaces - 1];
        if (!(c == COMMON_L(' ') || c == COMMON_L('\t') || c == COMMON_L('\n') || c == COMMON_L('\r') || c == COMMON_L('\v'))) break;
        ending_spaces++;
    }
    
    /* Move */
    spaces = beginning_spaces + ending_spaces;
    if (beginning_spaces > 0) COMMON_W(w,memmove(string->p, string->p + beginning_spaces, string->size - spaces));
    string->size -= spaces;
    string->p[string->size] = COMMON_L('\0');
}

ERROR_TYPE string_replace_mem(struct CharBuffer *string, size_t begin, size_t size, const cchar_t *other, size_t other_size)
{
    cchar_t *segment_p;
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
            string->p[new_size] = COMMON_L('\0');
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

ERROR_TYPE nstring_to_wstring(const nchar_t *np, size_t nsize, wchar_t *wp, size_t *wsize)
{
    *wsize = 0;
    while (nsize > 0)
    {
        /* Decode UTF-8 */
        const unsigned int nc = (unsigned int)*np;
        size_t symbol_size;
        unsigned int code;
        if ((nc & 0x80) == 0)
        {
            code = nc;
            symbol_size = 1;
        }
        else if ((nc & 0xE0) == 0xC0)
        {
            ARET(nsize >= 2 && ((unsigned int)np[1] & 0xC0) == 0x80);
            code = (unsigned int)(((nc & 0x1F) << 6) | ((unsigned int)np[1] & 0x3F));
            symbol_size = 2;
        }
        else if ((nc & 0xF0) == 0xE0)
        {
            ARET(nsize >= 3 && ((unsigned int)np[1] & 0xC0) == 0x80 && ((unsigned int)np[2] & 0xC0) == 0x80);
            code = (unsigned int)(((nc & 0x0F) << 12) | (((unsigned int)np[1] & 0x3F) << 6) | ((unsigned int)np[2] & 0x3F));
            symbol_size = 3;
        }
        else if ((nc & 0xF8) == 0xF0)
        {
            ARET(nsize >= 4 && ((unsigned int)np[1] & 0xC0) == 0x80 && ((unsigned int)np[2] & 0xC0) == 0x80 && ((unsigned int)np[3] & 0xC0) == 0x80);
            code = (unsigned int)(((nc & 0x07) << 18) | (((unsigned int)np[1] & 0x3F) << 12) | (((unsigned int)np[2] & 0x3F) << 6) | ((unsigned int)np[3] & 0x3F));
            symbol_size = 4;
        }
        else RET0("Invalid UTF-8 symbol");
        np += symbol_size;
        nsize -= symbol_size;

        if (sizeof(wchar_t) > 2)
        {
            /* Encode UTF-32 */
            if (wp != NULL) *wp = (wchar_t)code;
            symbol_size = 1;
        }
        else
        {
            /* Encode UTF-16 */
            ARET(code < 0xE000 || (code >= 0xD800 && code < 0x110000));
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
        }
        if (wp != NULL) wp += symbol_size;
        *wsize += 1;
    }
    ERROR_RETURN_OK();
}

ERROR_TYPE wstring_to_nstring(const wchar_t *wp, size_t wsize, nchar_t *np, size_t *nsize)
{
    *nsize = 0;
    while (wsize > 0)
    {
        const unsigned int wc = (unsigned int)*wp;
        unsigned int code;
        size_t symbol_size;
        if (sizeof(wchar_t) > 2)
        {
            /* Decode UTF-32 */
            code = wc;
            symbol_size = 1;
        }
        else
        {
            /* Decode UTF-16 */
            if (!(wc >= 0xD800 && wc < 0xE000))
            {
                code = (unsigned int)*wp;
                symbol_size = 1;
            }
            else if (wc >= 0xD800 && wc < 0xDC00)
            {
                ARET(wsize >= 2 && ((unsigned int)wp[1] >= 0xDC00 && (unsigned int)wp[1] < 0xE000));
                code = (((wc - 0xD800) << 10) | ((unsigned int)wp[1] - 0xDC00)) + 0x10000;
                symbol_size = 2;
            }
            else RET0("Invalid UTF-16 symbol");
        }
        wp += symbol_size;
        wsize -= symbol_size;

        /* Encode UTF-8 */
        ARET(code < 0x110000);
        if (code < 0x80)
        {
            if (np != NULL) *np = (nchar_t)code;
            symbol_size = 1;
        }
        else if (code < 0x800)
        {
            if (np != NULL)
            {
                np[0] = (nchar_t)(0xC0 | ((code >> 6) & 0x1F));
                np[1] = (nchar_t)(0x80 | (code & 0x3F));
            }
            symbol_size = 2;
        }
        else if (code < 0x10000)
        {
            if (np != NULL)
            {
                np[0] = (nchar_t)(0xE0 | ((code >> 12) & 0x0F));
                np[1] = (nchar_t)(0x80 | ((code >> 6) & 0x3F));
                np[2] = (nchar_t)(0x80 | (code & 0x3F));
            }
            symbol_size = 3;
        }
        else
        {
            if (np != NULL)
            {
                np[0] = (nchar_t)(0xE0 | ((code >> 18) & 0x07));
                np[1] = (nchar_t)(0x80 | ((code >> 12) & 0x3F));
                np[2] = (nchar_t)(0x80 | ((code >> 6) & 0x3F));
                np[3] = (nchar_t)(0x80 | (code & 0x3F));
            }
            symbol_size = 4;
        }
        if (np != NULL) np += symbol_size;
        *nsize += symbol_size;
    }
    ERROR_RETURN_OK();
}

#endif
