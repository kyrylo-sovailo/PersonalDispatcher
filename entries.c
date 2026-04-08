#include "kpd.h"
#include "commonlib/buffer_implementation.h"

#include <string.h>
#include <stdlib.h>

static void entries_initialize_element(struct Entry *entry) { const struct Entry zero = ZERO_INIT; *entry = zero; }
static void entries_finalize_element(struct Entry *entry) { if (entry->description != NULL) free(entry->description); }

IMPLEMENT_BUFFER_RESIZE(struct Entry, EntryBuffer, entries_)
IMPLEMENT_BUFFER_FINALIZE(struct Entry, EntryBuffer, entries_)

bool entries_highest_open(size_t *index, const struct EntryBuffer *entries)
{
    const struct Entry *entry, *highest;

    highest = NULL;
    for (entry = entries->p; entry < entries->p + entries->size; entry++)
    {
        if (!entry->done && (highest == NULL || entry->priority > highest->priority)) highest = entry;
    }
    if (highest == NULL)
    {
        return false;
    }
    else
    {
        *index = (size_t)(highest - entries->p);
        return true;
    }
}

static int entries_compare(const void *a, const void *b)
{
    const struct Entry *ac = a;
    const struct Entry *bc = b;
    int difference = (bc->done ? 0 : 1) - (ac->done ? 0 : 1);
    if (difference != 0) return difference;
    difference = (int)(bc->priority) - (int)(ac->priority);
    if (difference != 0) return difference;
    difference = (int)(ac->number) - (int)(bc->number);
    return difference;
}

void entries_sort(const struct EntryBuffer *entries)
{
    qsort(entries->p, entries->size, sizeof(*entries->p), entries_compare);
}
