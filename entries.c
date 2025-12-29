#include "kpd.h"

#include <string.h>
#include <stdlib.h>

void entries_set_size(struct EntryBuffer *entries, size_t size)
{
    if (size > entries->capacity)
    {
        size_t new_capacity = (entries->capacity == 0) ? 1 : entries->capacity;
        while (size > new_capacity) new_capacity <<= 1;
        struct Entry *new_p = realloc(entries->p, new_capacity * sizeof(*entries->p));
        if (new_p == NULL) kpd_error(ERR_REALLOC, "realloc() failed");
        entries->capacity = new_capacity;
        entries->p = new_p;
        memset(&entries->p[entries->size], 0, (size - entries->size) * sizeof(*entries->p));
    }
    entries->size = size;
}

void entries_finalize(struct EntryBuffer *entries)
{
    for (const struct Entry *entry = entries->p + 1; entry < entries->p + entries->size; entry++)
    {
        free(entry->description);
    }
    free(entries->p);
}

int entries_compare(const void *a, const void *b)
{
    const struct Entry *ac = a;
    const struct Entry *bc = b;
    int difference = (int)(ac->priority) - (int)(bc->priority);
    if (difference != 0) return difference;
    difference = (int)(ac->number) - (int)(bc->number);
    return difference;
}

const struct Entry *entries_highest(const struct EntryBuffer *entries)
{
    const struct Entry *highest = entries->p;
    for (const struct Entry *entry = entries->p + 1; entry < entries->p + entries->size; entry++)
    {
        if (entry->priority > highest->priority) highest = entry; //dont invoke entries_compare, entries are already sorted by number
    }
    return highest;
}
