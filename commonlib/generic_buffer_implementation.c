#include "buffer_implementation.h"
#include "../kpd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DECLARE_BUFFER(GENERIC_ARGMENT_1, GenericByteBuffer)
DECLARE_BUFFER(GENERIC_ARGMENT_2, GenericWordBuffer)
DECLARE_BUFFER(GENERIC_ARGMENT_4, GenericDWordBuffer)
DECLARE_BUFFER(GENERIC_ARGMENT_8, GenericQWordBuffer)

void generic_buffer_initialize(void *buffer)
{
    const struct GenericByteBuffer zero = ZERO_INIT;
    struct GenericByteBuffer* cast = (struct GenericByteBuffer*)buffer;
    *cast = zero;
}

void generic_buffer_finalize(void *buffer)
{
    const struct GenericByteBuffer zero = ZERO_INIT;
    struct GenericByteBuffer* cast = (struct GenericByteBuffer*)buffer;
    if (cast->p != NULL) free(cast->p);
    *cast = zero;
}

#define IMPLEMENT_GENERIC_BUFFER_RESIZE(TYPE, STRUCT_NAME, SIZE_EXPRESSION) \
{ \
    struct STRUCT_NAME* cast = (struct STRUCT_NAME*)buffer; \
    if (size > cast->capacity) \
    { \
        TYPE *new_p; \
        size_t new_capacity = (cast->capacity == 0) ? 1 : cast->capacity; \
        while (size > new_capacity) new_capacity *= 2; \
        new_p = (TYPE*)realloc(cast->p, new_capacity * SIZE_EXPRESSION); \
        ARET(ERR_MALLOC, new_p != NULL); \
        cast->capacity = new_capacity; \
        cast->p = new_p; \
    } \
    cast->size = size; \
    ERROR_RETURN_OK(); \
}
ERROR_TYPE generic_buffer_resize_n(void *buffer, size_t size, size_t element_sizeof)
IMPLEMENT_GENERIC_BUFFER_RESIZE(GENERIC_ARGMENT_1, GenericByteBuffer, element_sizeof)
