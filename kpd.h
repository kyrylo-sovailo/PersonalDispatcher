#ifndef KPD_H
#define KPD_H

#include "commonlib/buffer.h"
#include "commonlib/char_buffer.h"

#include <stdbool.h>
#include <stddef.h>

#define VERSION "1.1.0"
#define TARGET "TODO.md"
#define INITIAL_BUFFER_SIZE 127

typedef int (Command)(int argc, char **argv);

/* Exit code */
enum Error
{
    ERR_OK = 0,

    /* User error */
    ERR_USAGE = 10,
    ERR_FORMAT = 11,

    /* File operations */
    ERR_FILE_OPEN = 20,

    /* Filesystem */
    ERR_PATH = 30,
    ERR_OPEN_FILE_READ = 31,
    ERR_OPEN_FILE_WRITE = 32,
    ERR_OPEN_DIRECTORY = 33,

    /* Memory */
    ERR_MALLOC = 40,
    ERR_REALLOC = 41,

    /* Processes */
    ERR_FORK = 50,
    ERR_EXEC = 51,
    ERR_WAIT = 52,
    ERR_GIT = 53,

    /* String operations */
    ERR_REPLACE = 60,
    ERR_CODEC = 61,

    /* Other */
    ERR_NOT_IMPLEMENTED = 70,
    ERR_OTHER = 71
};

/* Action to be performed on after "find" command */
enum Action
{
    ACT_COMMIT,
    ACT_REMOVE,
    ACT_DONE,
    ACT_UNDO,
    ACT_PRIORITY,
    ACT_EDIT,
    ACT_NONE
};

/* Status to be added to "sort" and "list" commands */
enum Status
{
    STA_ALL,
    STA_OPEN,
    STA_DONE
};

/* Priority of a entry */
enum Priority
{
    PRI_LOW,
    PRI_MEDIUM,
    PRI_HIGH,
    PRI_CRITICAL
};

/* Entry aka task */
struct Entry
{
    size_t number;          /* Entry number, zero-based */
    char *description;      /* Plain text description */
    enum Priority priority; /* Priority */
    bool priority_explicit; /* Indicator if priority was given explicitly */
    bool done;              /* Task is done */
};
DECLARE_BUFFER(struct Entry, EntryBuffer)

/* common.c */
/* Reads entries from TODO.md into buffer, returns open FILE* (buffer may be NULL) */
void kpd_read_target(struct EntryBuffer *entries, struct CharBuffer *path);
/* Writes entries to the open FILE* */
void kpd_write_target(const struct EntryBuffer *entries, const struct CharBuffer *path);
/* Prints entry to stdout (max_length/max_marker_length are zero for no spaces) */
void kpd_print_entry(const struct Entry *entry, const char *highlight, unsigned int max_length, unsigned int max_marker_length);
/* Prints entries to stdout (if mask is NULL, prints all) */
void kpd_print_entries(const struct EntryBuffer *entries, const char *highlight, const char *mask);
/* Parses number and sets mask (if mask is NULL, only checks format) */
bool kpd_parse_number(char *mask, size_t mask_size, const char *number_string);
/* Sets mask based on parsed number */
char *kpd_create_mask(size_t mask_size, const char *number_string);
/* Sets mask based on open entry with highest priority */
char *kpd_create_mask_highest_open(const struct EntryBuffer *entries);
/* Sets mask based on last done entry */
char *kpd_create_mask_last_closed(const struct EntryBuffer *entries);
/* Parses action string (if action is NULL, only checks) */
bool kpd_resolve_action(enum Action *action, const char *action_string);
/* Parses status string (if status is NULL, only checks) */
bool kpd_resolve_status(enum Status *status, const char *status_string);
/* Parses priority string (if priority is NULL, only checks) */
bool kpd_resolve_priority(enum Priority *priority, const char *priority_string);
/* Returns if string can be resolved as 'commit' */
bool kpd_resolve_commit(const char *commit_string);
/* Invokes git */
void kpd_invoke_git(const char *path, const char *commit_message);

/* entries.c */
/* Find substring in a string, case-insensitive */
const char *string_find_case(const char *haystack, const char *needle);
/* Finds open entry with highest priority */
bool entries_highest_open(size_t *index, const struct EntryBuffer *entries);
/* Sorts entries by priority, critical first */
void entries_sort(const struct EntryBuffer *entries);

DECLARE_BUFFER_RESIZE(struct Entry, EntryBuffer, entries_)
DECLARE_BUFFER_FINALIZE(struct Entry, EntryBuffer, entries_)

/* string.c */
/* Sets string to line read from file, returns whether read something */
bool string_get_line(struct CharBuffer *string, void *file);
/* Sets string to user input */
void string_get_input(struct CharBuffer *string, const char *prompt, const char *prefill, const char *prefill_prompt);
/* Transforms description to commit message */
void string_description_to_done_commit(struct CharBuffer *string);
/* Transforms description to commit message */
void string_description_to_undo_commit(struct CharBuffer *string);
/* Transforms description to commit message */
void string_description_to_remove_commit(struct CharBuffer *string);
/* Resolves string */
bool string_resolve(size_t *index, const char *option, const char *const *options, size_t options_size);

#endif
