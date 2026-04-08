#include "kpd.h"
#include "commonlib/error.h"
#include "commonlib/string.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#ifdef ENABLE_READLINE
    #include <readline/readline.h>
    #include <readline/history.h>
#endif

#define ascii_isalnum()

/* Required by string_get_input */
#ifdef ENABLE_READLINE
static const char *string_set_input_prefill;
static int string_set_input_hook(void)
{
    rl_insert_text(string_set_input_prefill);
    rl_point = rl_end;
    return 0;
}
#endif

const char *string_find_case(const char *haystack, const char *needle)
{
    const size_t needle_length = strlen(needle);
    for (; *haystack != '\0'; haystack++)
    {
        bool difference;
        size_t i;
        difference = false;
        for (i = 0; i < needle_length; i++)
        {
            char needle_c = needle[i];
            char haystack_c = haystack[i];
            if (needle_c >= 'A' && needle_c <= 'Z') needle_c += ('a' - 'A'); /* Needed for highlighting */
            if (haystack_c >= 'A' && haystack_c <= 'Z') haystack_c += ('a' - 'A');
            if (needle_c != haystack_c) { difference = true; break; }
        }
        if (!difference) return haystack;
    }
    return NULL;
}

bool string_get_line(struct CharBuffer *string, void *file)
{
    if (string->capacity < INITIAL_BUFFER_SIZE + 1) string_resize(string, INITIAL_BUFFER_SIZE);
    string->size = 0;
    while (true)
    {
        /* There are three possible actions to do, */
        const char *result = fgets(string->p + string->size, (int)(string->capacity - string->size), file); /* Puts '\0' */
        if (result == NULL)
        {
            if (string->size == 0) return false; /* Nothing to parse, stop */
            else return true; /* Something left to parse */
        }
        else
        {
            const char *endline = memchr(string->p, '\n', string->capacity - 1);
            if (endline == NULL)
            {
                /* Endline not read, try again */
                const size_t size = string->capacity - 1; /* Meaningful read symbols */
                string->size = size;
                string_resize(string, 2 * size);
                string->size = size;
            }
            else
            {
                /* Endline read, can parse */
                string->size = (size_t)(endline - string->p) + 1; /* String is one longer than endline */
                return true;
            }
        }
    }    
}

void string_get_input(struct CharBuffer *string, const char *prompt, const char *prefill, const char *prefill_prompt)
{
    #ifdef ENABLE_READLINE
        char *line;
        (void)prefill_prompt;
        rl_startup_hook = string_set_input_hook;
        string_set_input_prefill = prefill;
        line = readline(prompt);
        string_finalize(string);
        if (line != NULL)
        {
            string->p = line;
            string->size = strlen(line);
            string->capacity = string->size + 1;
        }
        else
        {
            memset(line, 0, sizeof(*string));
        }
    #else
        printf("%s%s\n", prefill_prompt, prefill);
        printf("%s", prompt);
        string_get_line(string, stdin);
    #endif
    string_trim(string);
}

void string_description_to_done_commit(struct CharBuffer *string)
{
    const char *verbs[] =
    {
        "add",
        "build",
        "change", "check", "clean", "close", "complete",
        "debug", "delete", "disable", "do", "document",
        "enable",
        "find", "fix",
        "handle",
        "implement", "improve",
        "make", "merge", "migrate",
        "optimize",
        "refactor", "remove", "replace", "resolve", "revert", "rewrite",
        "solve",
        "test",
        "update", "upgrade",
        "validate",
        "write"
    };

    const char *verbs_perfect[] =
    {
        "added",
        "built",
        "changed", "checked", "cleaned", "closed", "completed",
        "debugged", "deleted", "disabled", "done", "documented",
        "enabled",
        "found", "fixed",
        "handled",
        "implemented", "improved",
        "made", "merged", "migrated",
        "optimized",
        "refactored", "removed", "replaced", "resolved", "reverted", "rewrote",
        "solved",
        "tested",
        "updated", "upgraded",
        "validated",
        "wrote"
    };

    const char *prefix = "Closed '";
    const char *suffix = "'";
    bool changed;
    size_t i;

    changed = false;
    for (i = 0; i < sizeof(verbs) / sizeof(*verbs); i++)
    {
        const char *verb, *verb_perfect, *found;
        size_t verb_length, verb_perfect_length;
        char pre, last, post;
        size_t found_begin, verb_match;
        bool upper;

        /* Find verb */
        verb = verbs[i];
        verb_length = strlen(verb);
        found = string_find_case(string->p, verb);
        if (found == NULL) continue; /* verb not found */
        pre = (found > string->p) ? found[-1] : '\0';
        last = found[verb_length - 1];
        post = found[verb_length];
        if ((pre >= '0' && pre <= '9') || (pre >= 'A' && pre <= 'Z') || (pre >= 'a' && pre <= 'z')) continue; /* beginning with valid character */
        if ((post >= '0' && post <= '9') || (post >= 'A' && post <= 'Z') || (post >= 'a' && post <= 'z')) continue; /* ending with valid character */
        
        /* Change verb */
        found_begin = (size_t)(found - string->p);
        upper = last >= 'A' && last <= 'Z';
        verb_perfect = verbs_perfect[i];
        verb_perfect_length = strlen(verb_perfect);
        verb_match = 0;
        while (verb[verb_match] == verb_perfect[verb_match]) verb_match++;
        string_replace_mem(string, found_begin + verb_match, verb_length - verb_match, verb_perfect + verb_match, verb_perfect_length - verb_match);
        if (upper)
        {
            char *found_w = string->p + found_begin;
            char *p;
            for (p = found_w + verb_match; p < found_w + verb_length; p++) *p -= ('a' - 'A');
        }
        changed = true;
    }

    if (changed) return;
    string_replace_mem(string, 0, 0, prefix, strlen(prefix));
    string_replace_mem(string, string->size, 0, suffix, strlen(suffix));
}

void string_description_to_undo_commit(struct CharBuffer *string)
{
    const char *prefix = "Reopened '";
    const char *suffix = "'";
    string_replace_mem(string, 0, 0, prefix, strlen(prefix));
    string_replace_mem(string, string->size, 0, suffix, strlen(suffix));
}

void string_description_to_remove_commit(struct CharBuffer *string)
{
    const char *prefix = "Removed '";
    const char *suffix = "'";
    string_replace_mem(string, 0, 0, prefix, strlen(prefix));
    string_replace_mem(string, string->size, 0, suffix, strlen(suffix));
}

bool string_resolve(size_t *index, const char *option, const char *const *options, size_t options_size)
{
    /* All options can be (so far) resolved by the first letter, so don't care about ambiguity */
    size_t option_length;
    size_t i;
    
    option_length = strlen(option);
    for (i = 0; i < options_size; i++)
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
