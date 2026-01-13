#include "kpd.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

void string_set_size(struct CharBuffer *string, size_t size)
{
    if (size + 1 > string->capacity)
    {
        size_t new_capacity = (string->capacity == 0) ? 1 : string->capacity;
        while (size + 1 > new_capacity) new_capacity <<= 1;
        char *new_p = realloc(string->p, new_capacity * sizeof(*string->p));
        if (new_p == NULL) kpd_error(ERR_REALLOC, "realloc() failed");
        string->capacity = new_capacity;
        string->p = new_p;
    }
    string->size = size;
    string->p[size] = '\0';
}

bool string_set_line(struct CharBuffer *string, void *file)
{
    string->size = 0;
    while (true)
    {
        //There are three possible actions to do: parse line, try again, stop
        const char *result = fgets(string->p + string->size, (int)(string->capacity - string->size), file); //Puts '\0'
        if (result == NULL)
        {
            if (string->size == 0) return false; //Nothing to parse, stop
            else return true; //Something left to parse
        }
        else
        {
            const char *endline = memchr(string->p, '\n', string->capacity - 1);
            if (endline == NULL)
            {
                //Endline not read, try again
                const size_t size = string->capacity - 1; //Meaningful read symbols
                string->size = size;
                string_set_size(string, 2 * size);
                string->size = size;
            }
            else
            {
                //Endline read, can parse
                string->size = (size_t)(endline - string->p) + 1; //String is one longer than endline
                return true;
            }
        }
    }    
}

void string_set_cwd(struct CharBuffer *path)
{
    string_set_size(path, 8);
    while (true)
    {
        if (getcwd(path->p, path->capacity) == NULL)
        {
            string_set_size(path, 2 * path->size);
        }
        else
        {
            path->size = strlen(path->p);
            break;
        }
    }
}

void string_finalize(struct CharBuffer *string)
{
    if (string->p == NULL) return;
    memset(string, 0, sizeof(*string));
}

void string_append_file(struct CharBuffer *path)
{
    const char *filename = "/" TARGET;
    const bool slash_last = path->p[path->size-1] == '/';
    string_set_size(path, path->size + strlen(filename) - (slash_last ? 1 : 0));
    memcpy(&path->p[path->size - strlen(filename)], filename, strlen(filename) + 1);
}

bool string_remove_file(struct CharBuffer *path)
{
    const bool slash_last = path->p[path->size-1] == '/';
    if (slash_last) { path->size--; path->p[path->size] = '\0'; }
    const char *previous_slash = strrchr(path->p, '/');
    if (previous_slash == NULL) return false; //No slash
    if (previous_slash == path->p) return false; //Slash on first position
    path->size = (size_t)(previous_slash - path->p);
    path->p[path->size] = '\0';
    return true;
}

void string_remove(struct CharBuffer *string, size_t begin, size_t size)
{
    memmove(string->p + begin, string->p + begin + size, string->size - begin - size + 1);
    string->size -= size;
}

void string_trim(struct CharBuffer *string, size_t beginning_spaces, size_t ending_spaces)
{
    //Count beginning space
    beginning_spaces = strspn(string->p + beginning_spaces, " \t\n\r") + beginning_spaces;
    if (beginning_spaces == string->size)
    {
        //Spaces only
        string->size = 0;
        string->p[0] = '\0';
        return;
    }
    
    //Count ending space
    while (ending_spaces < string->size && strchr(" \t\n\r", string->p[string->size - ending_spaces - 1]) != NULL)
    {
        ending_spaces++;
    }
    
    //Move
    const size_t spaces = beginning_spaces + ending_spaces;
    if (beginning_spaces > 0) memmove(string->p, string->p + beginning_spaces, string->size - spaces);
    string->size -= spaces;
    string->p[string->size] = '\0';
}

bool string_resolve(size_t *index, const char *option, const char *const *options, size_t options_size)
{
    //All options can be (so far) resolved by the first letter, so don't care about ambiguity
    const size_t option_length = strlen(option);
    for (size_t i = 0; i < options_size; i++)
    {
        const size_t option_i_length = strlen(options[i]);
        if (option_length <= option_i_length && memcmp(option, options[i], option_length) == 0)
        {
            *index = i;
            return true;
        }
    }
    return false;
}
