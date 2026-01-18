#include "kpd.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#ifdef ENABLE_READLINE
    #include <readline/readline.h>
    #include <readline/history.h>
#endif

//Required by string_description_to_done_commit
static void string_description_substitute(struct CharBuffer *string, size_t segment_begin, size_t segment_size, const char *substitution, bool upper)
{
    const size_t substitution_size = strlen(substitution);
    string_substitute(string, segment_begin, segment_size, substitution, substitution_size);
    if (upper)
    {
        char *p = string->p + segment_begin;
        for (size_t i = 0; i < substitution_size; i++, p++) *p -= ('a' - 'A');
    }
}

//Required by string_set_input
#ifdef ENABLE_READLINE
static const char *string_set_input_prefill;
static int string_set_input_hook(void)
{
    rl_insert_text(string_set_input_prefill);
    rl_point = rl_end;
    return 0;
}
#endif

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
        //There are three possible actions to do,
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

void string_set_input(struct CharBuffer *string, const char *prompt, const char *prefill, const char *prefill_prompt)
{
    #ifdef ENABLE_READLINE
        (void)prefill_prompt;
        rl_startup_hook = string_set_input_hook;
        string_set_input_prefill = prefill;
        char *line = readline(prompt);
        string_finalize(string);
        if (line != NULL)
        {
            string->p = line;
            string->size = strlen(line);
            string->capacity = string->size;
        }
    #else
        printf("%s%s\n", prefill_prompt, prefill);
        printf("%s", prompt);
        string_set_line(string, stdin);
    #endif
    string_trim(string, 0, 0);
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

void string_substitute(struct CharBuffer *string, size_t segment_begin, size_t segment_size, const char *substitution, size_t substitution_size)
{
    char *segment_p = string->p + segment_begin;
    if (substitution_size != segment_size)
    {
        const size_t new_size = string->size + substitution_size - segment_size;
        if (substitution_size > segment_size)
        {
            //Expanding
            string_set_size(string, new_size);
        }
        memmove(segment_p + substitution_size, segment_p + segment_size, string->size - segment_begin);
        memcpy(segment_p, substitution, substitution_size);
        if (substitution_size < segment_size)
        {
            //Shrinking
            string->p[new_size] = '\0';
            string->size = new_size;
        }
    }
    memcpy(segment_p, substitution, substitution_size);
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

void string_description_to_done_commit(struct CharBuffer *string)
{
    const char *verbs[] =
    {
        "add", "adjust", "allow", "apply", "archive",
        "build",
        "change", "check", "clean", "close", "complete", "configure", "create",
        "debug", "delete", "disable", "document",
        "enable", "enforce", "enhance",
        "fix",
        "handle",
        "implement", "improve", "initialize", "install",
        "merge", "migrate",
        "optimize",
        "perform", "prevent",
        "refactor", "refine", "release", "remove", "replace", "report", "reset", "resolve", "restart", "revert", "review",
        "save", "send", "start", "stop",
        "test", "track",
        "update", "upgrade",
        "validate", "verify"
    };

    bool changed = false;
    for (size_t i = 0; i < sizeof(verbs) / sizeof(*verbs); i++)
    {
        //Find verb
        const char *verb = verbs[i];
        char *found = strcasestr(string->p, verb);
        if (found == NULL) continue; //verb not found
        const size_t verb_length = strlen(verb);
        if (isalnum(found[verb_length])) continue; //ending with valid character
        const size_t found_begin = (size_t)(found - string->p);
        const size_t found_end = found_begin + verb_length;

        //Change verb
        const char o1 = string->p[found_end - 1];
        const bool upper = o1 >= 'A' && o1 <= 'Z';
        const char c1 = verb[verb_length - 1];
        const char c2 = verb[verb_length - 2];
        if (c1 == 'e') //Archive -> archived
        {
            string_description_substitute(string, found_end, 0, "d", upper);
        }
        else if (c1 == 'y') //Apply -> Applied
        {
            string_description_substitute(string, found_end - 1, 1, "ied", upper);
        }
        else if ((c2 == 'l' || c2 == 'n') && (c1 == 'd')) //Build -> built
        {
            string_description_substitute(string, found_end - 1, 1, "t", upper);
        }
        else if ((c2 == 'a' || c2 == 'e' || c2 == 'i' || c2 == 'o' || c2 == 'y' || c2 == 'u') && (c1 == 'g' || c1 == 'p')) //Debug -> debugged
        {
            char dup[4] = { c1, 'e', 'd', '\0' };
            string_description_substitute(string, found_end, 0, &dup[0], upper);
        } 
        else //Adjust -> adjusted
        {
            string_description_substitute(string, found_end, 0, "ed", upper);
        }

        changed = true;
    }

    if (changed) return;
    const char *prefix = "Closed '";
    const char *suffix = "'";
    string_substitute(string, 0, 0, prefix, strlen(prefix));
    string_substitute(string, string->size, 0, suffix, strlen(suffix));
}

void string_description_to_undo_commit(struct CharBuffer *string)
{
    const char *prefix = "Reopened '";
    const char *suffix = "'";
    string_substitute(string, 0, 0, prefix, strlen(prefix));
    string_substitute(string, string->size, 0, suffix, strlen(suffix));
}

void string_description_to_remove_commit(struct CharBuffer *string)
{
    const char *prefix = "Removed '";
    const char *suffix = "'";
    string_substitute(string, 0, 0, prefix, strlen(prefix));
    string_substitute(string, string->size, 0, suffix, strlen(suffix));
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
