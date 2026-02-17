#include "kpd.h"

#include <sys/wait.h>
#include <unistd.h>

#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Needed by kpd_print_entry */
#define BLACK           "\x1b[00;30m"
#define RED             "\x1b[00;31m"
#define GREEN           "\x1b[00;32m"
#define YELLOW          "\x1b[00;33m"
#define BLUE            "\x1b[00;34m"
#define MAGENTA         "\x1b[00;35m"
#define CYAN            "\x1b[00;36m"
#define WHITE           "\x1b[00;37m"
#define BRIGHT_BLACK    "\x1b[01;30m"
#define BRIGHT_RED      "\x1b[01;31m"
#define BRIGHT_GREEN    "\x1b[01;32m"
#define BRIGHT_YELLOW   "\x1b[01;33m"
#define BRIGHT_BLUE     "\x1b[01;34m"
#define BRIGHT_MAGENTA  "\x1b[01;35m"
#define BRIGHT_CYAN     "\x1b[01;36m"
#define BRIGHT_WHITE    "\x1b[01;37m"
#define DEFAULT         "\x1b[0m"

/* Needed by kpd_read_target */
static bool kpd_read_line(struct Entry *entry, struct CharBuffer *line)
{
    const char *markers[4] = { "(priority: low)", "(priority: medium)", "(priority: high)", "(priority: critical)" };
    enum Priority priority_i;

    /* Empty lines */
    if (strspn(line->p, " \t\n\r") == line->size) return false;

    /* Parse beginning */
    if (line->size < 7
    || line->p[0] != ' '
    || line->p[1] != '-'
    || line->p[2] != ' '
    || line->p[3] != '['
    || (line->p[4] != ' ' && line->p[4] != 'X')
    || line->p[5] != ']'
    || line->p[6] != ' ') kpd_error(ERR_FORMAT, "invalid line '%s'", line->p);
    entry->done = line->p[4] == 'X';
    
    /* Parse priority */
    entry->priority = PRI_MEDIUM;
    entry->priority_explicit = false;
    for (priority_i = 0; priority_i < 4; priority_i++)
    {
        char *marker_found = strstr(line->p, markers[priority_i]);
        if (marker_found != NULL)
        {
            /* Remove marker */
            entry->priority = priority_i;
            entry->priority_explicit = true;
            string_substitute(line, (size_t)(marker_found - line->p), strlen(markers[priority_i]), "", 0);
            break;
        }
    }

    /* Allocate */
    memset(line->p, ' ', 7);
    string_trim(line); /* Not really efficient because requires one more memcpy*/
    entry->description = malloc(line->size + 1);
    if (entry->description == NULL) kpd_error(ERR_MALLOC, "malloc() failed");
    memcpy(entry->description, line->p, line->size + 1);
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
static bool kpd_parse_number_post_number(const char **current_string)
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

static bool kpd_parse_number_post_hyphen(const char **current_string, char *mask, size_t mask_size, bool begin_read, size_t begin)
{
    size_t end;
    char *next_string;

    /* Try to read number */
    end = strtoul(*current_string, &next_string, 10);
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
            if (mask != NULL && (end == 0 || end > mask_size))
                kpd_error(ERR_USAGE, "'%u' is out of range", (unsigned int)end);
        }
        if (mask != NULL) memset(mask+(begin-1), '\1', end-(begin-1));
        return kpd_parse_number_post_number(current_string);
    }
}

/* Needed by kpd_invoke_git */
static void kpd_invoke(char *const *arguments)
{
    char *const *argument_i;
    int id;

    for (argument_i = &arguments[0]; *argument_i != NULL; argument_i++)
    {
        const bool next = *(argument_i + 1) != NULL;
        const char *quotation = (strchr(*argument_i, ' ') == NULL) ? "" : (
            (strchr(*argument_i, '\"') == NULL) ? "\"" : "\'"
        );
        printf("%s%s%s%c", quotation, *argument_i, quotation, next ? ' ' : '\n');
    }

    id = fork();
    if (id < 0)
    {
        kpd_error(ERR_FORK, "vfork() failed");
    }
    else if (id == 0)
    {
        if (execvp(arguments[0], arguments) < 0) kpd_error(ERR_EXEC, "execvp() failed");
    }
    else
    {
        int status;
        if (waitpid(id, &status, 0) < 0) kpd_error(ERR_WAIT, "waitpid() failed");
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) kpd_error(ERR_GIT, "'%s' failed", arguments[0]);
    }
}

/* Needed by kpd_print_entry */
static void kpd_print_string(const char *string, const char *highlight)
{
    const char *first_found = NULL, *previous_found = NULL;
    size_t highlight_length;
    if (highlight == NULL) { printf("%s", string); return; }

    highlight_length = strlen(highlight);
    while (true)
    {
        const char *found = string_find_case(string, highlight);
        if (found == NULL)
        {
            /* No more entries */
            if (previous_found == NULL) printf("%s", string);
            else printf(RED "%.*s" DEFAULT "%s", (int)(previous_found + highlight_length - first_found), first_found, previous_found + highlight_length);
            break;
        }
        else if (previous_found == NULL || found > previous_found + highlight_length)
        {
            /* Entry does not intersect with the previous entry */
            if (previous_found == NULL) printf("%.*s", (int)(found - string), string);
            else printf(RED "%.*s" DEFAULT, (int)(previous_found + highlight_length - first_found), first_found);
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

void kpd_error(enum Error error, const char *format, ...)
{
    va_list va;

    va_start(va, format);
    fprintf(stderr, "kpd: ");
    vfprintf(stderr, format, va);
    fprintf(stderr, "\n");
    va_end(va);
    exit((int)error);
}

void kpd_read_target(struct EntryBuffer *entries, struct CharBuffer *path)
{
    struct CharBuffer local_path = { 0 }, line = { 0 };
    size_t steps, entry_number;
    FILE *file;

    /* Search for TODO.md */
    string_set_cwd(&local_path);
    string_append_file(&local_path);
    steps = 0;
    while (true)
    {
        file = fopen(local_path.p, "r+");
        if (file != NULL) break;
        if (!string_remove_file(&local_path) || !string_remove_file(&local_path))
            kpd_error(ERR_USAGE, "current_string directory does not contain " TARGET);
        string_append_file(&local_path);
        steps++;
    }

    /* Parse TODO.md */
    entry_number = 0;
    while (string_set_line(&line, file))
    {
        struct Entry entry = { 0 };
        entry.number = entry_number;
        if (!kpd_read_line(&entry, &line)) continue;
        if (entries == NULL)
        {
            free(entry.description);
        }
        else
        {
            entries_set_size(entries, entry_number + 1);
            entries->p[entry_number] = entry;
        }
        entry_number++;
    }

    /* Make relative path */
    if (path != NULL)
    {
        string_set_size(&local_path, 0);
        if (steps == 0)
        {
            string_substitute(&local_path, 0, 0, TARGET, strlen(TARGET));
        }
        else
        {
            size_t step_i;
            for (step_i = 0; step_i < steps; step_i++) string_substitute(&local_path, local_path.size, 0, "../", 3);
            string_append_file(&local_path);
        }
    }

    /* Cleanup */
    string_finalize(&line);
    fclose(file);
    if (path == NULL) string_finalize(&local_path);
    else *path = local_path;
}

void kpd_write_target(const struct EntryBuffer *entries, const struct CharBuffer *path)
{
    FILE *file;
    struct Entry *entry_i;

    file = fopen(path->p, "w");
    if (file == NULL) kpd_error(ERR_FILE_OPEN, "fopen() failed");

    for (entry_i = entries->p; entry_i < entries->p + entries->size; entry_i++)
    {
        const char *markers[4] = { " (priority: low)", " (priority: medium)", " (priority: high)", " (priority: critical)" };
        const char *marker = entry_i->priority_explicit ? markers[entry_i->priority] : "";
        fprintf(file, " - [%c] %s%s\n", entry_i->done ? 'X' : ' ', entry_i->description, marker);
    }

    fclose(file);
}

void kpd_print_entry(const struct Entry *entry, const char *highlight, unsigned int max_length, unsigned int max_marker_length)
{
    const char *markers[4] =
    {
                "(low)",
        CYAN    "(medium)"      DEFAULT,
        YELLOW  "(high)"        DEFAULT,
        RED     "(critical)"    DEFAULT
    };

    const size_t number = entry->number + 1;
    const unsigned int number_length = get_number_length(number);
    const unsigned int number_spaces = (max_length == 0) ? 0 : (max_length - number_length);

    const char *marker = entry->done ? GREEN "(done)" DEFAULT : markers[entry->priority];
    const unsigned int marker_length = get_marker_length(entry->done, entry->priority);
    const unsigned int marker_spaces = (max_marker_length == 0) ? 0 : (max_marker_length - marker_length);
    const unsigned int left_marker_spaces = (marker_spaces) / 2;
    const unsigned int right_marker_spaces = (marker_spaces + 1) / 2;

    printf("%u.%*s %*s%s%*s ",
        (unsigned int)number,
        number_spaces, "",
        left_marker_spaces, "", marker, right_marker_spaces, "");
    kpd_print_string(entry->description, highlight);
    printf("\n");
}

void kpd_print_entries(const struct EntryBuffer *entries, const char *highlight, const char *mask)
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

bool kpd_parse_number(char *mask, size_t mask_size, const char *number_string)
{
    const char *current_string;

    if (mask != NULL) memset(mask, 0, mask_size);
    current_string = number_string;
    while (*current_string != '\0')
    {
        /* Try to read number */
        char *next_string;
        size_t begin = strtoul(current_string, &next_string, 10);
        if (next_string != current_string)
        {
            /* Number read */
            const bool hyphen = (*next_string == '-');
            current_string = hyphen ? (next_string + 1) : (next_string);
            if (mask != NULL && (begin == 0 || begin > mask_size))
                kpd_error(ERR_USAGE, "'%u' is out of range", (unsigned int)begin);
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

char *kpd_create_mask(size_t mask_size, const char *number_string)
{
    char *mask = malloc(mask_size);
    if (mask == NULL) kpd_error(ERR_MALLOC, "malloc() failed");
    kpd_parse_number(mask, mask_size, number_string);
    return mask;
}

char *kpd_create_mask_highest_open(const struct EntryBuffer *entries)
{
    char *mask;
    size_t highest;
    
    mask = malloc(entries->size);
    if (mask == NULL) kpd_error(ERR_MALLOC, "malloc() failed");
    if (!entries_highest_open(&highest, entries)) kpd_error(ERR_USAGE, "no entries");
    memset(mask, '\0', entries->size);
    mask[highest] = '\1';
    return mask;
}

char *kpd_create_mask_last_closed(const struct EntryBuffer *entries)
{
    char *mask;
    const struct Entry *entry_i, *last;
    
    mask = malloc(entries->size);
    if (mask == NULL) kpd_error(ERR_MALLOC, "malloc() failed");

    last = NULL;
    for (entry_i = entries->p + entries->size; entry_i-- > entries->p;)
    {
        if (!entry_i->done)
        {
            last = entry_i;
            break;
        }
    }
    if (last == NULL) kpd_error(ERR_USAGE, "no entries");
    memset(mask, '\0', entries->size);
    mask[last - entries->p] = '\1';
    return mask;
}

bool kpd_resolve_action(enum Action *action, const char *action_string)
{
    const char *action_strings[] = { "commit", "remove", "done", "undo", "priority", "edit" };
    size_t action_index;
    bool result;
    
    result = string_resolve(&action_index, action_string, action_strings, sizeof(action_strings)/sizeof(*action_strings));
    if (result) *action = (enum Action)action_index;
    return result;
}

bool kpd_resolve_status(enum Status *status, const char *status_string)
{
    const char *status_strings[] = { "all", "open", "done" };
    size_t status_index;
    bool result;
    
    result = string_resolve(&status_index, status_string, status_strings, sizeof(status_strings)/sizeof(*status_strings));
    if (result) *status = (enum Status)status_index;
    return result;
}

bool kpd_resolve_priority(enum Priority *priority, const char *priority_string)
{
    const char *priority_strings[] = { "low", "medium", "high", "critical" };
    size_t priority_index;
    bool result;

    result = string_resolve(&priority_index, priority_string, priority_strings, sizeof(priority_strings)/sizeof(*priority_strings));
    if (result) *priority = (enum Priority)priority_index;
    return result;
}

bool kpd_resolve_commit(const char *commit_string)
{
    const size_t commit_length = strlen(commit_string);
    const size_t only_option_length = strlen("commit");
    return commit_length <= only_option_length && memcmp(commit_string, "commit", commit_length) == 0;
}

void kpd_invoke_git(const char *path, const char *commit_message)
{
    size_t path_length_1, commit_message_length_1;
    char *path_copy, *commit_message_copy;
    char *arguments[5];

    path_length_1 = strlen(path) + 1;
    commit_message_length_1 = strlen(commit_message) + 1;
    path_copy = malloc(path_length_1);
    commit_message_copy = malloc(commit_message_length_1);
    if (path_copy == NULL || commit_message_copy == NULL) kpd_error(ERR_MALLOC, "malloc() failed");
    memcpy(path_copy, path, path_length_1);
    memcpy(commit_message_copy, commit_message, commit_message_length_1);
    
    arguments[0] = "git";
    arguments[1] = "add";
    arguments[2] = path_copy;
    arguments[3] = NULL;
    kpd_invoke(arguments);

    arguments[0] = "git";
    arguments[1] = "commit";
    arguments[2] = "-m";
    arguments[3] = commit_message_copy;
    arguments[4] = NULL;
    kpd_invoke(arguments);

    free(path_copy);
    free(commit_message_copy);
}