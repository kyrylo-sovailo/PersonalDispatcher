#include "kpd.h"
#include "commonlib/error.h"
#include "commonlib/output.h"
#include "commonlib/string.h"

#ifdef ENABLE_READLINE
    #include <readline/readline.h>
    #include <readline/history.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef COMMON_WCHAR
    #include <wchar.h>
#endif

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

const cchar_t *string_find_case(const cchar_t *haystack, const cchar_t *needle)
{
    const size_t needle_length = COMMON_WCS(len(needle));
    for (; *haystack != '\0'; haystack++)
    {
        bool difference;
        size_t i;
        difference = false;
        for (i = 0; i < needle_length; i++)
        {
            cchar_t needle_c = needle[i];
            cchar_t haystack_c = haystack[i];
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
        const cchar_t *result = COMMON_TERNARY(fget, fgetw, s(string->p + string->size, (int)(string->capacity - string->size), file)); /* Puts '\0' */
        if (result == NULL)
        {
            if (string->size == 0) return false; /* Nothing to parse, stop */
            else return true; /* Something left to parse */
        }
        else
        {
            const cchar_t *endline = COMMON_W(memchr(string->p, '\n', string->capacity - 1));
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

#ifdef COMMON_WCHAR
bool string_get_nline(struct NCharBuffer *string, void *file)
{
    if (string->capacity < INITIAL_BUFFER_SIZE + 1) string_nresize(string, INITIAL_BUFFER_SIZE);
    string->size = 0;
    while (true)
    {
        /* There are three possible actions to do, */
        const nchar_t *result = fgets(string->p + string->size, (int)(string->capacity - string->size), file); /* Puts '\0' */
        if (result == NULL)
        {
            if (string->size == 0) return false; /* Nothing to parse, stop */
            else return true; /* Something left to parse */
        }
        else
        {
            const nchar_t *endline = memchr(string->p, '\n', string->capacity - 1);
            if (endline == NULL)
            {
                /* Endline not read, try again */
                const size_t size = string->capacity - 1; /* Meaningful read symbols */
                string->size = size;
                string_nresize(string, 2 * size);
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
#endif

void string_get_input(struct CharBuffer *string, const cchar_t *prompt, const cchar_t *prefill, const cchar_t *prefill_prompt)
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
        output_print(COMMON_S COMMON_S COMMON_N COMMON_S, prefill_prompt, prefill, prompt);
        string_get_line(string, stdin);
    #endif
    string_trim(string);
}

void string_description_to_done_commit(struct CharBuffer *string)
{
    const cchar_t *verbs[] =
    {
        COMMON_L("add"),
        COMMON_L("build"),
        COMMON_L("change"), COMMON_L("check"), COMMON_L("clean"), COMMON_L("close"), COMMON_L("complete"),
        COMMON_L("debug"), COMMON_L("delete"), COMMON_L("disable"), COMMON_L("do"), COMMON_L("document"),
        COMMON_L("enable"),
        COMMON_L("find"), COMMON_L("fix"),
        COMMON_L("handle"),
        COMMON_L("implement"), COMMON_L("improve"),
        COMMON_L("make"), COMMON_L("merge"), COMMON_L("migrate"),
        COMMON_L("optimize"),
        COMMON_L("refactor"), COMMON_L("remove"), COMMON_L("replace"), COMMON_L("resolve"), COMMON_L("revert"), COMMON_L("rewrite"),
        COMMON_L("solve"),
        COMMON_L("test"),
        COMMON_L("update"), COMMON_L("upgrade"),
        COMMON_L("validate"),
        COMMON_L("write")
    };

    const cchar_t *verbs_perfect[] =
    {
        COMMON_L("added"),
        COMMON_L("built"),
        COMMON_L("changed"), COMMON_L("checked"), COMMON_L("cleaned"), COMMON_L("closed"), COMMON_L("completed"),
        COMMON_L("debugged"), COMMON_L("deleted"), COMMON_L("disabled"), COMMON_L("done"), COMMON_L("documented"),
        COMMON_L("enabled"),
        COMMON_L("found"), COMMON_L("fixed"),
        COMMON_L("handled"),
        COMMON_L("implemented"), COMMON_L("improved"),
        COMMON_L("made"), COMMON_L("merged"), COMMON_L("migrated"),
        COMMON_L("optimized"),
        COMMON_L("refactored"), COMMON_L("removed"), COMMON_L("replaced"), COMMON_L("resolved"), COMMON_L("reverted"), COMMON_L("rewrote"),
        COMMON_L("solved"),
        COMMON_L("tested"),
        COMMON_L("updated"), COMMON_L("upgraded"),
        COMMON_L("validated"),
        COMMON_L("wrote")
    };

    const cchar_t *prefix = COMMON_L("Closed '");
    const cchar_t *suffix = COMMON_L("'");
    bool changed;
    size_t i;

    changed = false;
    for (i = 0; i < sizeof(verbs) / sizeof(*verbs); i++)
    {
        const cchar_t *verb, *verb_perfect, *found;
        size_t verb_length, verb_perfect_length;
        cchar_t pre, last, post;
        size_t found_begin, verb_match;
        bool upper;

        /* Find verb */
        verb = verbs[i];
        verb_length = COMMON_WCS(len(verb));
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
        verb_perfect_length = COMMON_WCS(len(verb_perfect));
        verb_match = 0;
        while (verb[verb_match] == verb_perfect[verb_match]) verb_match++;
        string_replace_mem(string, found_begin + verb_match, verb_length - verb_match, verb_perfect + verb_match, verb_perfect_length - verb_match);
        if (upper)
        {
            cchar_t *found_w = string->p + found_begin;
            cchar_t *p;
            for (p = found_w + verb_match; p < found_w + verb_length; p++) *p -= ('a' - 'A');
        }
        changed = true;
    }

    if (changed) return;
    string_replace_mem(string, 0, 0, prefix, COMMON_WCS(len(prefix)));
    string_replace_mem(string, string->size, 0, suffix, COMMON_WCS(len(suffix)));
}

void string_description_to_undo_commit(struct CharBuffer *string)
{
    const cchar_t *prefix = COMMON_L("Reopened '");
    const cchar_t *suffix = COMMON_L("'");
    string_replace_mem(string, 0, 0, prefix, COMMON_WCS(len(prefix)));
    string_replace_mem(string, string->size, 0, suffix, COMMON_WCS(len(suffix)));
}

void string_description_to_remove_commit(struct CharBuffer *string)
{
    const cchar_t *prefix = COMMON_L("Removed '");
    const cchar_t *suffix = COMMON_L("'");
    string_replace_mem(string, 0, 0, prefix, COMMON_WCS(len(prefix)));
    string_replace_mem(string, string->size, 0, suffix, COMMON_WCS(len(suffix)));
}

bool string_resolve(size_t *index, const cchar_t *option, const cchar_t *const *options, size_t options_size)
{
    /* All options can be (so far) resolved by the first letter, so don't care about ambiguity */
    size_t option_length;
    size_t i;
    
    option_length = COMMON_WCS(len(option));
    for (i = 0; i < options_size; i++)
    {
        const size_t option_i_length = COMMON_WCS(len(options[i]));
        if (option_length <= option_i_length && COMMON_W(memcmp(option, options[i], option_length)) == 0)
        {
            *index = i;
            return true;
        }
    }
    return false;
}
