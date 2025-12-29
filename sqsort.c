#include "kpd.h"

#include <string.h>
#include <stdlib.h>

void memswp(void *a, void *b, size_t size)
{
    char *as = a;
    char *bs = b;
    for (size_t i = 0; i < size; i++, as++, bs++)
    {
        char buffer = *as;
        *as = *bs;
        *bs = buffer;
    }
}

void sqsort_small(void *buffer, size_t size, size_t element_size, Comparison *comparison)
{
    char *as = buffer;
    char *bs = as + element_size;
    bool change = false;
    if (comparison(as, bs) < 0) { memswp(as, bs, element_size); change = true; } //first pair

    while (true)
    {
        for (size_t i = 2; i < size; i++) //all pairs except first
        {
            as += element_size;
            bs += element_size;
            if (comparison(as, bs) < 0) { memswp(as, bs, element_size); change = true; }
        }
        if (!change) return;
        change = false;

        for (size_t i = 2; i < size; i++) //all pairs except last
        {
            as -= element_size;
            bs -= element_size;
            if (comparison(as, bs) < 0) { memswp(as, bs, element_size); change = true; }
        }
        if (!change) return;
        change = false;
    }
}

void sqsort_internal(void *buffer, void *shadow, size_t size, size_t element_size, Comparison *comparison)
{
    if (size < 16) { sqsort_small(buffer, size, element_size, comparison); return; }
    char *ss = shadow;
    char *b1s = buffer;
    size_t b1n = size / 2;
    char *b2s = b1s + element_size * b1n;
    size_t b2n = (size + 1) / 2;

    //Recursive call
    sqsort_internal(b1s, shadow, b1n, element_size, comparison);
    sqsort_internal(b2s, shadow, b2n, element_size, comparison);

    //Merge to shadow
    while (true)
    {
        if (comparison(b1s, b2s) < 0)
        {
            memcpy(ss, b1s, element_size);
            ss += element_size;
            b1s += element_size;
            b1n--;
            if (b1n == 0) { memcpy(ss, b2s, element_size * b2n); break; }
        }
        else
        {
            memcpy(ss, b2s, element_size);
            ss += element_size;
            b2s += element_size;
            b2n--;
            if (b2n == 0) { memcpy(ss, b1s, element_size * b1n); break; }
        }
    }

    //Copy shadow back
    memcpy(buffer, shadow, size * element_size);
}

void sqsort(void *buffer, size_t size, size_t element_size, Comparison *comparison)
{
    if (size < 16) { sqsort_small(buffer, size, element_size, comparison); return; }
    void *shadow = malloc((size + 1) / 2);
    if (shadow == NULL) kpd_error(ERR_MALLOC, "malloc() failed");
    sqsort_internal(buffer, shadow, size, element_size, comparison);
    free(shadow);
}
