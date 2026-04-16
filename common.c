#include "commonlib/error.h"
#include "commonlib/string.h"
#include "commonlib/output.h"
#include "commonlib/path.h"
#include "kpd.h"

#ifdef WIN32
    #include <Windows.h>
#else
    #include <sys/wait.h>
    #include <unistd.h>
#endif

#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef COMMON_WCHAR
    #include <wchar.h>
#endif

/* Needed by kpd_read_target */
static bool kpd_read_line(struct Entry *entry, struct CharBuffer *line)
{
    const cchar_t *markers[4] = { COMMON_L("(priority: low)"), COMMON_L("(priority: medium)"), COMMON_L("(priority: high)"), COMMON_L("(priority: critical)") };
    enum Priority priority_i;

    /* Empty lines */
    if (COMMON_WCS(spn(line->p, COMMON_L(" \t\n\r"))) == line->size) return false;

    /* Parse beginning */
    if (line->size < 7
    || line->p[0] != ' '
    || line->p[1] != '-'
    || line->p[2] != ' '
    || line->p[3] != '['
    || (line->p[4] != ' ' && line->p[4] != 'X')
    || line->p[5] != ']'
    || line->p[6] != ' ') error_print_die(ERR_FORMAT, COMMON_S2("invalid line '", "'"), line->p);
    entry->done = line->p[4] == 'X';
    
    /* Parse priority */
    entry->priority = PRI_MEDIUM;
    entry->priority_explicit = false;
    for (priority_i = 0; priority_i < 4; priority_i++)
    {
        cchar_t *marker_found = COMMON_WCS(str(line->p, markers[priority_i]));
        if (marker_found != NULL)
        {
            /* Remove marker */
            entry->priority = priority_i;
            entry->priority_explicit = true;
            string_replace_mem(line, (size_t)(marker_found - line->p), COMMON_WCS(len(markers[priority_i])), NULL, 0);
            break;
        }
    }

    /* Allocate */
    COMMON_W(memset(line->p, ' ', 7));
    string_trim(line); /* Not really efficient because requires one more memcpy*/
    entry->description = malloc((line->size + 1) * sizeof(*entry->description));
    if (entry->description == NULL) RET0(ERR_MALLOC, "malloc() failed");
    COMMON_W(memcpy(entry->description, line->p, line->size + 1));
    return true;
}

/* Needed by kpd_print_entry */
static unsigned int get_number_length(size_t number)
{
    const unsigned int max_length = (unsigned int)floor(log10((double)SIZE_MAX)) + 1;
    unsigned int length_i;
    size_t accumulator;

    accumulator = 1;
    for (length_i = 0; length_i < max_length-1; length_i++)
    {
        accumulator *= 10;
        if (number < accumulator) return length_i + 1;
    }
    return max_length;
}

static unsigned int get_marker_length(bool done, enum Priority priority)
{
    const unsigned int marker_lengths[4] =
    {
        sizeof("(low)")-1,
        sizeof("(medium)")-1,
        sizeof("(high)")-1,
        sizeof("(critical)")-1
    };

    return done ? (sizeof("(done)")-1) : marker_lengths[priority];
}

/* Needed by kpd_parse_number */
static bool kpd_parse_number_post_number(const cchar_t **current_string)
{
    switch (**current_string)
    {
    case '\0':
        return true;

    case ',':
        (*current_string)++;
        if (**current_string == '\0') return false; /* Comma at the end is not allowed */
        return true;
        
    default:
        return false;
    }
}

static bool kpd_parse_number_post_hyphen(const cchar_t **current_string, char *mask, size_t mask_size, bool begin_read, size_t begin)
{
    size_t end;
    cchar_t *next_string;

    /* Try to read number */
    end = COMMON_WCS(toul(*current_string, &next_string, 10));
    if (next_string == *current_string && !begin_read)
    {
        /* No number read and 'begin' was not yet read */
        return false;
    }
    else
    {
        if (next_string == *current_string)
        {
            /* No number read */
            end = mask_size;
        }
        else
        {
            /* Number read */
            *current_string = next_string;
            if (begin_read && begin > end) return false;
            if (mask != NULL && (end == 0 || end > mask_size)) error_print_die(ERR_USAGE, COMMON_L("'%u' is out of range"), (unsigned int)end);
        }
        if (mask != NULL) memset(mask+(begin-1), '\1', end-(begin-1));
        return kpd_parse_number_post_number(current_string);
    }
}

/* Needed by kpd_invoke_git */
static void kpd_invoke(cchar_t *const *arguments)
{
    cchar_t *const *argument_i;
    #ifndef WIN32
        int id;
    #endif

    output_open(false);
    for (argument_i = &arguments[0]; *argument_i != NULL; argument_i++)
    {
        const bool next = *(argument_i + 1) != NULL;
        const cchar_t *quotation;
        if (COMMON_WCS(chr(*argument_i, ' ')) == NULL) quotation = COMMON_E;
        else if (COMMON_WCS(chr(*argument_i, '\'')) == NULL) quotation = COMMON_L("\'");
        else quotation = COMMON_L("\"");
        output_print(false, COMMON_S COMMON_S COMMON_S COMMON_C, quotation, *argument_i, quotation, next ? COMMON_L(' ') : COMMON_L('\n'));
    }
    output_close(false);

    #ifdef WIN32
        /* TODO */
    #else
        id = fork();
        if (id < 0) error_print_die(ERR_FORK, COMMON_L("vfork() failed"));
        if (id == 0)
        {
            if (execvp(arguments[0], arguments) < 0) error_print_die(ERR_EXEC, COMMON_L("execvp() failed"));
        }
        else
        {
            int status;
            if (waitpid(id, &status, 0) < 0) error_print_die(ERR_WAIT, COMMON_L("waitpid() failed"));
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) error_print_die(ERR_GIT, COMMON_QS COMMON_L(" failed"), arguments[0]);
        }
    #endif
}

/* Needed by kpd_print_entry */
static void kpd_print_string(const cchar_t *string, const cchar_t *highlight)
{
    const cchar_t *first_found = NULL, *previous_found = NULL;
    size_t highlight_length;
    if (highlight == NULL) { output_print(false, COMMON_S, string); return; }

    highlight_length = COMMON_WCS(len(highlight));
    while (true)
    {
        const cchar_t *found = string_find_case(string, highlight);
        if (found == NULL)
        {
            /* No more entries */
            if (previous_found == NULL)
            {
                output_print(false, COMMON_S, string);
            }
            else
            {
                output_print_color(false, COMMON_RED, COMMON_PS, (int)(previous_found + highlight_length - first_found), first_found);
                output_print(false, COMMON_S, previous_found + highlight_length);
            };
            break;
        }
        else if (previous_found == NULL || found > previous_found + highlight_length)
        {
            /* Entry does not intersect with the previous entry */
            if (previous_found == NULL)
            {
                output_print(false, COMMON_PS, (int)(found - string), string);
            }
            else
            {
                output_print_color(false, COMMON_RED, COMMON_PS, (int)(previous_found + highlight_length - first_found), first_found);
            }
            first_found = found;
        }
        else
        {
            /* Entry intersects with the previous entry */
            /* Do nothing */
        }
        previous_found = found;
        string = found + 1;
    }
}

void kpd_read_target(struct EntryBuffer *entries, struct CharBuffer *path, bool relative)
{
    struct CharBuffer local_path = ZERO_INIT, line = ZERO_INIT;
    #ifdef COMMON_WCHAR
    struct NCharBuffer nline = ZERO_INIT;
    #endif
    size_t steps, entry_number;
    FILE *file;

    /* Search for TODO.md */
    path_get_working_directory(&local_path);
    path_append_mem(&local_path, COMMON_L(TARGET), strlen(TARGET));
    steps = 0;
    while (true)
    {
        file = COMMON__W(fopen(local_path.p, COMMON_L("r+")));
        if (file != NULL) break;
        if (!path_get_directory(&local_path, &local_path, true) || !path_get_directory(&local_path, &local_path, true))
            error_print_die(ERR_USAGE, COMMON_L("current_string directory does not contain ") COMMON_L(TARGET));
        path_append_mem(&local_path, COMMON_L(TARGET), strlen(TARGET));
        steps++;
    }

    /* Parse TODO.md */
    entry_number = 0;
    #ifdef COMMON_WCHAR
    while (nstring_get_line(&nline, file))
    #else
    while (string_get_line(&line, file))
    #endif
    {
        
        struct Entry entry = ZERO_INIT;
        #ifdef COMMON_WCHAR
            size_t size;
            nstring_to_wstring(nline.p, nline.size + 1, NULL, &size);
            string_resize(&line, size - 1);
            nstring_to_wstring(nline.p, nline.size + 1, line.p, &size);
        #endif
        entry.number = entry_number;
        if (!kpd_read_line(&entry, &line)) continue;
        if (entries == NULL)
        {
            free(entry.description);
        }
        else
        {
            entries_resize(entries, entry_number + 1);
            entries->p[entry_number] = entry;
        }
        entry_number++;
    }

    /* Make relative path */
    if (path != NULL && relative)
    {
        if (steps == 0)
        {
            string_copy_mem(&local_path, COMMON_L(TARGET), strlen(TARGET));
        }
        else
        {
            size_t step_i;
            string_zero(&local_path);
            for (step_i = 0; step_i < steps; step_i++) path_get_directory(&local_path, &local_path, true);
            path_append_mem(&local_path, COMMON_L(TARGET), strlen(TARGET));
        }
    }

    /* Cleanup */
    char_buffer_finalize(&line);
    #ifdef COMMON_WCHAR
    nchar_buffer_finalize(&nline);
    #endif
    fclose(file);
    if (path == NULL) string_finalize(&local_path);
    else *path = local_path;
}

void kpd_write_target(const struct EntryBuffer *entries, const struct CharBuffer *path)
{
    FILE *file;
    struct Entry *entry_i;

    file = COMMON__W(fopen(path->p, COMMON_L("w")));
    if (file == NULL) error_print_die(ERR_FILE_OPEN, COMMON_L("fopen() failed"));

    for (entry_i = entries->p; entry_i < entries->p + entries->size; entry_i++)
    {
        const nchar_t *markers[4] = { " (priority: low)", " (priority: medium)", " (priority: high)", " (priority: critical)" };
        const nchar_t *marker = entry_i->priority_explicit ? markers[entry_i->priority] : "";
        #ifdef WIN32
            const size_t wide_description_size = wcslen(entry_i->description) + 1;
            size_t description_size;
            char *description;
            wstring_to_nstring(entry_i->description, wide_description_size, NULL, &description_size);
            description = malloc(description_size);
            ARET(ERR_MALLOC, description != NULL);
            wstring_to_nstring(entry_i->description, wide_description_size, description, &description_size);
        #else
            const nchar_t *description = entry_i->description;
        #endif

        fprintf(file, " - [%c] %s%s\n", entry_i->done ? 'X' : ' ', description, marker);
        
        #ifdef WIN32
            free(description);
        #endif
    }

    fclose(file);
}

void kpd_print_entry(const struct Entry *entry, const cchar_t *highlight, unsigned int max_length, unsigned int max_marker_length)
{
    const cchar_t *markers[4] = { COMMON_L("(low)"), COMMON_L("(medium)"), COMMON_L("(high)"), COMMON_L("(critical)") };
    const bool marker_use_colors[4] = { false, true, true, true };
    const ccolor_t marker_colors[4] = { COMMON_WHITE, COMMON_CYAN, COMMON_YELLOW, COMMON_RED };

    const size_t number = entry->number + 1;
    const unsigned int number_length = get_number_length(number);
    const unsigned int number_spaces = (max_length == 0) ? 0 : (max_length - number_length);

    const cchar_t *marker = entry->done ? COMMON_L("(done)") : markers[entry->priority];
    bool marker_use_color = entry->done ? true : marker_use_colors[entry->priority];
    const ccolor_t marker_color = entry->done ? COMMON_GREEN : marker_colors[entry->priority];

    const unsigned int marker_length = get_marker_length(entry->done, entry->priority);
    const unsigned int marker_spaces = (max_marker_length == 0) ? 0 : (max_marker_length - marker_length);
    const unsigned int left_marker_spaces = (marker_spaces) / 2;
    const unsigned int right_marker_spaces = (marker_spaces + 1) / 2;

    output_print(false, COMMON_L("%u.") COMMON_WS, (unsigned int)number, number_spaces + left_marker_spaces + 1, COMMON_E);
    if (marker_use_color) output_print_color(false, marker_color, COMMON_S, marker);
    else output_print(false, COMMON_S, marker);
    output_print(false, COMMON_WS, right_marker_spaces + 1, COMMON_E);
    kpd_print_string(entry->description, highlight);
    output_print(false, COMMON_N);
}

void kpd_print_entries(const struct EntryBuffer *entries, const cchar_t *highlight, const char *mask)
{
    bool mask_valid;
    size_t max_number;
    unsigned int max_length, max_marker_length;
    const struct Entry *entry_i;
    const char *mask_i;

    mask_valid = mask != NULL;
    max_number = 0;
    max_marker_length = 0;
    for (entry_i = entries->p, mask_i = mask; entry_i < entries->p + entries->size; entry_i++)
    {
        const bool print = (!mask_valid || *mask_i != '\0');
        if (print)
        {
            /* Could optimize get_marker_length() calls for sorted arrays, doesn't improve Big O though */
            const unsigned int marker_length = get_marker_length(entry_i->done, entry_i->priority);
            if (entry_i->number > max_number) max_number = entry_i->number;
            if (marker_length > max_marker_length) max_marker_length = marker_length;
        }
        if (mask_valid) mask_i++;
    }
    
    max_length = get_number_length(max_number + 1);
    for (entry_i = entries->p, mask_i = mask; entry_i < entries->p + entries->size; entry_i++)
    {
        const bool print = (!mask_valid || *mask_i != '\0');
        if (print) kpd_print_entry(entry_i, highlight, max_length, max_marker_length);
        if (mask_valid) mask_i++;
    }
}

bool kpd_parse_number(char *mask, size_t mask_size, const cchar_t *number_string)
{
    const cchar_t *current_string;

    if (mask != NULL) memset(mask, 0, mask_size);
    current_string = number_string;
    while (*current_string != '\0')
    {
        /* Try to read number */
        cchar_t *next_string;
        size_t begin = COMMON_WCS(toul(current_string, &next_string, 10));
        if (next_string != current_string)
        {
            /* Number read */
            const bool hyphen = (*next_string == '-');
            current_string = hyphen ? (next_string + 1) : (next_string);
            if (mask != NULL && (begin == 0 || begin > mask_size))
                error_print_die(ERR_USAGE, COMMON_L("'%u' is out of range"), (unsigned int)begin);
            if (hyphen)
            {
                /* Hyphen after number */
                if (!kpd_parse_number_post_hyphen(&current_string, mask, mask_size, true, begin)) return false;
            }
            else
            {
                /* Something else after number */
                if (mask != NULL) memset(mask+(begin-1), '\1', 1);
                if (!kpd_parse_number_post_number(&current_string)) return false;
            }
        }
        else if (*next_string == '-')
        {
            /* Number not read, hyphen read */
            current_string = next_string + 1;
            if (!kpd_parse_number_post_hyphen(&current_string, mask, mask_size, false, 0)) return false;
        }
        else
        {
            /* No number and no hyphen */
            return false;
        }
    }
    return true;
}

char *kpd_create_mask(size_t mask_size, const cchar_t *number_string)
{
    char *mask = malloc(mask_size);
    ARET(ERR_MALLOC, mask != NULL);
    kpd_parse_number(mask, mask_size, number_string);
    return mask;
}

char *kpd_create_mask_highest_open(const struct EntryBuffer *entries)
{
    char *mask;
    size_t highest;
    
    if (!entries_highest_open(&highest, entries)) error_print_die(ERR_USAGE, COMMON_L("no entries"));
    mask = malloc(entries->size);
    ARET(ERR_MALLOC, mask != NULL);
    memset(mask, '\0', entries->size);
    mask[highest] = '\1';
    return mask;
}

char *kpd_create_mask_last_closed(const struct EntryBuffer *entries)
{
    char *mask;
    const struct Entry *entry_i, *last;
    
    mask = malloc(entries->size);
    ARET(ERR_MALLOC, mask != NULL);

    last = NULL;
    for (entry_i = entries->p + entries->size; entry_i-- > entries->p;)
    {
        if (!entry_i->done)
        {
            last = entry_i;
            break;
        }
    }
    if (last == NULL) error_print_die(ERR_USAGE, COMMON_L("no entries"));
    memset(mask, '\0', entries->size);
    mask[last - entries->p] = '\1';
    return mask;
}

bool kpd_resolve_action(enum Action *action, const cchar_t *action_string)
{
    const cchar_t *action_strings[] = { COMMON_L("commit"), COMMON_L("remove"), COMMON_L("done"), COMMON_L("undo"), COMMON_L("priority"), COMMON_L("edit") };
    size_t action_index;
    bool result;
    
    result = string_resolve(&action_index, action_string, action_strings, sizeof(action_strings)/sizeof(*action_strings));
    if (result) *action = (enum Action)action_index;
    return result;
}

bool kpd_resolve_status(enum Status *status, const cchar_t *status_string)
{
    const cchar_t *status_strings[] = { COMMON_L("all"), COMMON_L("open"), COMMON_L("done") };
    size_t status_index;
    bool result;
    
    result = string_resolve(&status_index, status_string, status_strings, sizeof(status_strings)/sizeof(*status_strings));
    if (result) *status = (enum Status)status_index;
    return result;
}

bool kpd_resolve_priority(enum Priority *priority, const cchar_t *priority_string)
{
    const cchar_t *priority_strings[] = { COMMON_L("low"), COMMON_L("medium"), COMMON_L("high"), COMMON_L("critical") };
    size_t priority_index;
    bool result;

    result = string_resolve(&priority_index, priority_string, priority_strings, sizeof(priority_strings)/sizeof(*priority_strings));
    if (result) *priority = (enum Priority)priority_index;
    return result;
}

bool kpd_resolve_commit(const cchar_t *commit_string)
{
    const size_t commit_length = COMMON_WCS(len(commit_string));
    const size_t only_option_length = strlen("commit");
    return commit_length <= only_option_length && COMMON_W(memcmp(commit_string, COMMON_L("commit"), commit_length)) == 0;
}

bool kpd_resolve_relative(const cchar_t *relative_string)
{
    const size_t relative_length = COMMON_WCS(len(relative_string));
    const size_t only_option_length = strlen("relative");
    return relative_length <= only_option_length && COMMON_W(memcmp(relative_string, COMMON_L("relative"), relative_length)) == 0;
}

void kpd_invoke_git(const cchar_t *path, const cchar_t *commit_message)
{
    size_t path_length_1, commit_message_length_1;
    cchar_t *path_copy, *commit_message_copy;
    cchar_t *arguments[5];

    path_length_1 = COMMON_WCS(len(path)) + 1;
    commit_message_length_1 = COMMON_WCS(len(commit_message)) + 1;
    path_copy = malloc(path_length_1 * sizeof(*path_copy));
    commit_message_copy = malloc(commit_message_length_1 * sizeof(*commit_message_copy));
    if (path_copy == NULL || commit_message_copy == NULL) error_print_die(ERR_MALLOC, COMMON_L("malloc() failed"));
    COMMON_W(memcpy(path_copy, path, path_length_1));
    COMMON_W(memcpy(commit_message_copy, commit_message, commit_message_length_1));
    
    arguments[0] = COMMON_L("git");
    arguments[1] = COMMON_L("add");
    arguments[2] = path_copy;
    arguments[3] = NULL;
    kpd_invoke(arguments);

    arguments[0] = COMMON_L("git");
    arguments[1] = COMMON_L("commit");
    arguments[2] = COMMON_L("-m");
    arguments[3] = commit_message_copy;
    arguments[4] = NULL;
    kpd_invoke(arguments);

    free(path_copy);
    free(commit_message_copy);
}