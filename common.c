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

//Needed by kpd_print_entry
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

//Needed by kpd_read_target
static bool kpd_read_line(struct Entry *entry, struct CharBuffer *line)
{
    //Empty lines
    if (strspn(line->p, " \t\n\r") == line->size) return false;

    //Parse beginning
    if (line->size < 7
    || line->p[0] != ' '
    || line->p[1] != '-'
    || line->p[2] != ' '
    || line->p[3] != '['
    || (line->p[4] != ' ' && line->p[4] != 'X')
    || line->p[5] != ']'
    || line->p[6] != ' ') kpd_error(ERR_FORMAT, "invalid line '%s'", line->p);
    entry->done = line->p[4] == 'X';
    
    //Parse priority
    entry->priority = PRI_MEDIUM;
    entry->priority_explicit = false;
    const char *markers[4] = { "(priority: low)", "(priority: medium)", "(priority: high)", "(priority: critical)" };
    for (enum Priority priority = 0; priority < 4; priority++)
    {
        char *marker_found = strstr(line->p, markers[priority]);
        if (marker_found != NULL)
        {
            //Remove marker
            entry->priority = priority;
            entry->priority_explicit = true;
            string_substitute(line, (size_t)(marker_found - line->p), strlen(markers[priority]), "", 0);
            break;
        }
    }

    //Allocate
    string_trim(line, 7, 0); //Not really efficient
    entry->description = malloc(line->size + 1);
    if (entry->description == NULL) kpd_error(ERR_MALLOC, "malloc() failed");
    memcpy(entry->description, line->p, line->size + 1);
    return true;
}

//Needed by kpd_print_entry
static unsigned int get_number_length(size_t number)
{
    const unsigned int max_length = (unsigned int)floor(log10((double)SIZE_MAX)) + 1;
    size_t accumulator = 1;
    for (unsigned int i = 0; i < max_length-1; i++)
    {
        accumulator *= 10;
        if (number < accumulator) return i + 1;
    }
    return max_length;
}

static unsigned int get_marker_length(bool done, enum Priority priority)
{
    const unsigned int marker_lengths[4] =
    {
        strlen("(low)"),
        strlen("(medium)"),
        strlen("(high)"),
        strlen("(critical)")
    };

    return done ? strlen("(done)") : marker_lengths[priority];
}

//Needed by kpd_parse_number
static bool kpd_parse_number_post_number(const char **current_string)
{
    switch (**current_string)
    {
    case '\0':
        return true;

    case ',':
        (*current_string)++;
        if (**current_string == '\0') return false; //Comma at the end is not allowed
        return true;
        
    default:
        return false;
    }
}

static bool kpd_parse_number_post_hyphen(const char **current_string, char *mask, size_t mask_size, bool begin_read, size_t begin)
{
    //Try to read number
    char *next_string;
    size_t end = strtoul(*current_string, &next_string, 10);
    if (next_string == *current_string && !begin_read)
    {
        //No number read and 'begin' was not yet read
        return false;
    }
    else
    {
        if (next_string == *current_string)
        {
            //No number read
            end = mask_size;
        }
        else
        {
            //Number read
            *current_string = next_string;
            if (begin_read && begin > end) return false;
            if (mask != NULL && (end == 0 || end > mask_size))
                kpd_error(ERR_USAGE, "'%u' is out of range", (unsigned int)end);
        }
        if (mask != NULL) memset(mask+(begin-1), '\1', end-(begin-1));
        return kpd_parse_number_post_number(current_string);
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

void kpd_read_target(void *file, struct EntryBuffer *entries, struct CharBuffer *path)
{
    //Search for TODO.md
    struct CharBuffer local_path = { 0 };
    string_set_cwd(&local_path);
    string_append_file(&local_path);
    FILE *local_file = NULL;
    while (true)
    {
        local_file = fopen(local_path.p, "r+");
        if (local_file != NULL) break;
        if (!string_remove_file(&local_path) || !string_remove_file(&local_path))
            kpd_error(ERR_USAGE, "current_string directory does not contain " TARGET);
        string_append_file(&local_path);
    }

    //Parse TODO.md
    struct CharBuffer line = { 0 };
    string_set_size(&line, 8);
    size_t number = 0;
    while (string_set_line(&line, local_file))
    {
        
        struct Entry entry = { 0 };
        entry.number = number;
        if (!kpd_read_line(&entry, &line)) continue;
        if (entries == NULL)
        {
            free(entry.description);
        }
        else
        {
            entries_set_size(entries, number + 1);
            entries->p[number] = entry;
        }
        number++;
    }

    //Cleanup
    string_finalize(&line);
    if (file == NULL) fclose(local_file);
    else *((FILE**)file) = local_file;
    if (path == NULL) string_finalize(&local_path);
    else *path = local_path;
}

void kpd_write_target(void *file, const struct EntryBuffer *entries)
{
    const int seek_result = fseek(file, 0, SEEK_SET);
    if (seek_result < 0) kpd_error(ERR_SEEK, "fseek() failed");

    for (struct Entry *entry = entries->p; entry < entries->p + entries->size; entry++)
    {
        const char *markers[4] = { " (priority: low)", " (priority: medium)", " (priority: high)", " (priority: critical)" };
        const char *marker = entry->priority_explicit ? markers[entry->priority] : "";
        fprintf(file, " - [%c] %s%s\n", entry->done ? 'X' : ' ', entry->description, marker);
    }

    const long tell_result = ftell(file);
    if (tell_result < 0) kpd_error(ERR_TELL, "ftell() failed");
    const int truncate_result = ftruncate(fileno(file), tell_result);
    if (truncate_result < 0) kpd_error(ERR_TRUNCATE, "ftell() failed");
}

void kpd_print_entry(const struct Entry *entry, unsigned int max_length, unsigned int max_marker_length)
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

    printf("%u.%*s %*s%s%*s %s\n",
        (unsigned int)number,
        number_spaces, "",
        left_marker_spaces, "", marker, right_marker_spaces, "",
        entry->description);
}

void kpd_print_entries(const struct EntryBuffer *entries, const char *mask)
{
    const bool mask_valid = mask != NULL;

    size_t max_number = 0; //Could optimize for sorted arrays, doesn't improve Big O though
    unsigned int max_marker_length = 0;
    const char *mask_i = mask;
    for (const struct Entry *entry = entries->p; entry < entries->p + entries->size; entry++)
    {
        const bool print = (!mask_valid || *mask_i != '\0');
        if (print)
        {
            const unsigned int marker_length = get_marker_length(entry->done, entry->priority);
            if (entry->number > max_number) max_number = entry->number;
            if (marker_length > max_marker_length) max_marker_length = marker_length;
        }
        if (mask_valid) mask_i++;
    }
    const unsigned int max_length = get_number_length(max_number + 1);

    mask_i = mask;
    for (const struct Entry *entry = entries->p; entry < entries->p + entries->size; entry++)
    {
        const bool print = (!mask_valid || *mask_i != '\0');
        if (print) kpd_print_entry(entry, max_length, max_marker_length);
        if (mask_valid) mask_i++;
    }
}

bool kpd_parse_number(char *mask, size_t mask_size, const char *number_string)
{
    if (mask != NULL) memset(mask, 0, mask_size);

    const char *current_string = number_string;
    while (*current_string != '\0')
    {
        //Try to read number
        char *next_string;
        size_t begin = strtoul(current_string, &next_string, 10);
        if (next_string != current_string)
        {
            //Number read
            const bool hyphen = (*next_string == '-');
            current_string = hyphen ? (next_string + 1) : (next_string);
            if (mask != NULL && (begin == 0 || begin > mask_size))
                kpd_error(ERR_USAGE, "'%u' is out of range", (unsigned int)begin);
            if (hyphen)
            {
                //Hyphen after number
                if (!kpd_parse_number_post_hyphen(&current_string, mask, mask_size, true, begin)) return false;
            }
            else
            {
                //Something else after number
                if (mask != NULL) memset(mask+(begin-1), '\1', 1);
                if (!kpd_parse_number_post_number(&current_string)) return false;
            }
        }
        else if (*next_string == '-')
        {
            //Number not read, hyphen read
            current_string = next_string + 1;
            if (!kpd_parse_number_post_hyphen(&current_string, mask, mask_size, false, 0)) return false;
        }
        else
        {
            //No number and no hyphen
            return false;
        }
    }
    return true;
}

char *kpd_create_mask(const char *number_string, const struct EntryBuffer *entries)
{
    char *mask = malloc(entries->size);
    if (mask == NULL) kpd_error(ERR_MALLOC, "malloc() failed");
    if (number_string == NULL)
    {
        size_t highest;
        if (!entries_highest(&highest, entries, NULL)) kpd_error(ERR_USAGE, "no entries");
        memset(mask, '\0', entries->size);
        mask[highest] = '\1';
    }
    else
    {
        kpd_parse_number(mask, entries->size, number_string);
    }
    return mask;
}

bool kpd_resolve_action(enum Action *action, const char *action_string)
{
    size_t action_index;
    const char *action_strings[] = { "commit", "remove", "done", "undo", "priority", "edit" };
    const bool result = string_resolve(&action_index, action_string, action_strings, sizeof(action_strings)/sizeof(*action_strings));
    if (result) *action = (enum Action)action_index;
    return result;
}

bool kpd_resolve_status(enum Status *status, const char *status_string)
{
    size_t status_index;
    const char *status_strings[] = { "all", "open", "done" };
    const bool result = string_resolve(&status_index, status_string, status_strings, sizeof(status_strings)/sizeof(*status_strings));
    if (result) *status = (enum Status)status_index;
    return result;
}

bool kpd_resolve_priority(enum Priority *priority, const char *priority_string)
{
    size_t priority_index;
    const char *priority_strings[] = { "low", "medium", "high", "critical" };
    const bool result = string_resolve(&priority_index, priority_string, priority_strings, sizeof(priority_strings)/sizeof(*priority_strings));
    if (result) *priority = (enum Priority)priority_index;
    return result;
}

bool kpd_resolve_commit(const char *commit_string)
{
    const size_t commit_length = strlen(commit_string);
    const size_t only_option_length = strlen("commit");
    return commit_length <= only_option_length && memcmp(commit_string, "commit", commit_length) == 0;
}

void kpd_execute(char *const *arguments)
{
    for (char *const *argument = &arguments[0]; *argument != NULL; argument++)
    {
        const bool next = *(argument + 1) != NULL;
        printf("%s%c", *argument, next ? ' ' : '\n');
    }
    return; //Pretend

    const int id = vfork();
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