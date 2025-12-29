#include "kpd.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

void string_set_size(struct CharBuffer *string, size_t size)
{
    if (size + 1 > string->capacity)
    {
        size_t new_capacity = (string->capacity == 0) ? 1 : string->capacity;
        while (size + 1 > new_capacity) new_capacity <<= 1;
        char *new_p = realloc(string->p, (new_capacity + 1) * sizeof(*string->p));
        if (new_p == NULL) kpd_error(ERR_REALLOC, "realloc() failed");
        string->capacity = new_capacity;
        string->p = new_p;
        memset(&string->p[string->size], 0, (size - string->size + 1) * sizeof(*string->p));
    }
    string->size = size;
}

void string_set_cwd(struct CharBuffer *path)
{
    string_set_size(path, 128);
    while (true)
    {
        if (getcwd(path->p, path->size + 1) == NULL) string_set_size(path, path->size << 1);
        else { path->size = strlen(path->p); break; }
    }
}

void string_append_file(struct CharBuffer *path)
{
    const char *filename = "/" TARGET;
    const bool slash_last = path->p[path->size-1] == '/';
    string_set_size(path, path->size + strlen(filename) - (slash_last ? 1 : 0));
    memcpy(&path->p[path->size - strlen(filename)], filename, strlen(filename));
}

void string_remove_file(struct CharBuffer *path)
{
    const bool slash_last = path->p[path->size-1] == '/';
    if (slash_last) { path->size--; path->p[path->size] = '0'; }
    char *previous_slash = strrchr(path->p, '/');
    if (previous_slash == NULL || previous_slash == path->p)
    {
        kpd_error(ERR_PATH, TARGET " not found in current directory or its parents", path->p);
    }
    else
    {
        *previous_slash = '\0';
        path->size = (size_t)(previous_slash - path->p);
    }
}
