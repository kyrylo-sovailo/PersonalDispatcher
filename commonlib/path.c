#include "path.h"
#include "string.h"
#include "../kpd.h"

#ifdef WIN32
    #include <Windows.h>
#else
    #include <unistd.h>
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#ifdef COMMON_WCHAR
    #include <wchar.h>
#endif

static const cchar_t *path_next_separator(const cchar_t *p, size_t size)
{
    #ifdef WIN32
        const cchar_t *found = NULL;
        const cchar_t *copy = p;
        for (copy = p; copy < p + size; copy++)
        {
            if (*copy == COMMON_L('/') || *copy == COMMON_L('\\')) { found = copy; break; }
        }
        return found;
    #else
        return (const cchar_t*)COMMON_W(w,memchr(p, COMMON_L('/'), size));
    #endif
}

static size_t path_root_size(const cchar_t *p, size_t size)
{
    #ifdef WIN32
        /* Path like C:\ */
        if (size >= 3
        && ((p[0] >= COMMON_L('a') && p[0] <= COMMON_L('z')) || (p[0] >= COMMON_L('A') && p[0] <= COMMON_L('Z')))
        && p[1] == COMMON_L(':')
        && (p[2] == COMMON_L('/') || p[2] == COMMON_L('\\')))
        {
            return 3;
        }

        /* Path like \\.\ */
        else if (size >= 2
        && (p[0] == COMMON_L('/') || p[0] == COMMON_L('\\'))
        && (p[1] == COMMON_L('/') || p[1] == COMMON_L('\\')))
        {
            const cchar_t *last_separator = path_next_separator(p + 2, size - 2);
            if (last_separator == NULL) return 2; /* Shouldn't happen actually */
            else return (size_t)(last_separator - p) + 1;
        }
    #else
        if (size >= 1 && p[0] == COMMON_L('/')) return 1;
    #endif
    return 0;
}


static ERROR_TYPE path_append_mem_noroot(struct CharBuffer *path, const cchar_t *other, size_t other_size) NODISCARD;
static ERROR_TYPE path_append_mem_noroot(struct CharBuffer *path, const cchar_t *other, size_t other_size)
{
    const cchar_t *p = other;
    size_t size = other_size;
    while (true)
    {
        const cchar_t *found = path_next_separator(p, size);
        const size_t part_size = (found == NULL) ? size : (size_t)(found - p);
        if (part_size == 0)
        {
            /* Double slash, do nothing */
        }
        else if (part_size == 1 && COMMON_W(w,memcmp(p, COMMON_L("."), 1)) == 0)
        {
            /* Dot, do nothing */
        }
        else if (part_size == 2 && COMMON_W(w,memcmp(p, COMMON_L(".."), 2)) == 0)
        {
            /* Double dot, step back */
            PRET(path_get_directory(path, path, true));
        }
        else
        {
            /* Regular case */
            if (path->size > 0 && path->p[path->size - 1] != COMMON_SEPARATOR) PRET(string_push(path, COMMON_SEPARATOR)); /* Protection against root */
            PRET(string_append_mem(path, p, part_size));
        }
        if (found == NULL) break;
        p = found + 1;
        size = size - part_size - 1;
    }
    ERROR_RETURN_OK();
}

ERROR_TYPE path_append_mem(struct CharBuffer *path, const cchar_t *other, size_t other_size)
{
    ARET(ERR_PATH, !path_absolute_mem(other, other_size));
    PRET(path_append_mem_noroot(path, other, other_size));
    ERROR_RETURN_OK();
}

bool path_absolute_mem(const cchar_t *path, size_t path_size)
{
    return path_root_size(path, path_size) > 0;
}

ERROR_TYPE path_get_working_directory(struct CharBuffer *path)
{
    #ifdef WIN32
        size_t size = GetCurrentDirectoryW(0, NULL);
        PRET(string_resize(path, size - 1));
        (void)GetCurrentDirectoryW((DWORD)size, path->p);
        ERROR_RETURN_OK();
    #else
        PRET(string_resize(path, 256));
        while (true)
        {
            if (getcwd(path->p, path->capacity) == NULL)
            {
                PRET(string_resize(path, 2 * path->size));
            }
            else
            {
                path->size = COMMON(str,wcs,len(path->p));
                break;
            }
        }
        if (path->p[path->size - 1] == COMMON_L('/')) { path->size--; path->p[path->size] = COMMON_L('\0'); }
        ERROR_RETURN_OK();
    #endif
}

bool path_get_directory(struct CharBuffer *directory, const struct CharBuffer *path, bool append_dotdot_if_dotdot)
{
    #ifdef WIN32
    const size_t root_size = path_root_size(path->p, path->size);
    #endif
    size_t size = path->size;
    while (size > 0)
    {
        size--;
        if (path->p[size] != COMMON_SEPARATOR) continue;
        
        /* Reached separator in absolute or relative path */
        #ifdef WIN32
        if (root_size > 0 && size == root_size - 1)
        #else
        if (size == 0)
        #endif
        {
            /* Reached the root of absolute path */
            return false;
        }
        else if (append_dotdot_if_dotdot && path->size - size == 3 && COMMON_W(w,memcmp(path->p + size + 1, COMMON_L(".."), 2)) == 0)
        {
            /* Trying to step up from directory ending in .. */
            if (directory == path) { /* Do nothing */ }
            else { PRET(string_reserve(directory, path->size + 3)); PIGNORE(string_copy(directory, path)); }
            PRET(string_append_mem(directory, COMMON_SEPARATOR_STR COMMON_L(".."), 3));
        }
        else
        {
            /* Regular case, just delete directory */
            if (directory == path) { directory->size = size; directory->p[size] = COMMON_L('\0'); }
            else { PRET(string_copy_mem(directory, path->p, size)); }
        }
        return true;
    }

    /* Relative path with no separators */
    if (append_dotdot_if_dotdot && path->size == 2 && COMMON_W(w,memcmp(path->p, COMMON_L(".."), 2)) == 0)
    {
        /* Trying to step up from .. */
        if (directory == path) { /* Do nothing */ }
        else { PRET(string_reserve(directory, path->size + 3)); PIGNORE(string_copy(directory, path)); }
        PRET(string_append_mem(directory, COMMON_SEPARATOR_STR COMMON_L(".."), 3));
    }
    else if (append_dotdot_if_dotdot && path->size == 0)
    {
        /* Trying to step up from "" */
        PRET(string_copy_mem(directory, COMMON_L(".."), 2));
    }
    else
    {
        /* True basename */
        string_zero(directory);
    }
    return true;
}
