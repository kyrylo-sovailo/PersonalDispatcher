#ifndef KPD_H
#define KPD_H

#include <stdbool.h>
#include <stddef.h>

#define VERSION "0.1.0"
#define TARGET "TODO.md"

typedef int (Command)(int argc, char **argv);

///Exit code
enum Error
{
    ERR_OK = 0,

    //User error
    ERR_USAGE = 10,
    ERR_FORMAT = 11,

    //File operations
    ERR_SEEK = 20,
    ERR_TRUNCATE = 21,
    ERR_TELL = 22,

    //Filesystem
    ERR_PATH = 30,
    ERR_NOT_FOUND = 31,
    ERR_NOT_FILE = 32,
    ERR_NOT_DIRECTORY = 33,

    //Memory
    ERR_MALLOC = 40,
    ERR_REALLOC = 41,

    //Processes
    ERR_FORK = 50,
    ERR_EXEC = 51,
    ERR_WAIT = 52,
    ERR_GIT = 53,

    //Other
    ERR_NOT_IMPLEMENTED = 60
};

///Action to be performed on after "find" command
enum Action
{
    ACT_COMMIT,
    ACT_REMOVE,
    ACT_DONE,
    ACT_UNDO,
    ACT_PRIORITY,
    ACT_EDIT
};

///Status to be added to "sort" and "list" commands
enum Status
{
    STA_ALL,
    STA_OPEN,
    STA_DONE
};

///Priority of a entry
enum Priority
{
    PRI_LOW,
    PRI_MEDIUM,
    PRI_HIGH,
    PRI_CRITICAL
};

///Entry aka task
struct Entry
{
    size_t number;          ///< Entry number, zero-based
    char *description;      ///< Plain text description
    enum Priority priority; ///< Priority
    bool priority_explicit; ///< Indicator if priority was given explicitly
    bool done;              ///< Task is done
};

///Vector of entries
struct EntryBuffer
{
    struct Entry *p;
    size_t size;
    size_t capacity;
};

///Vector of chars, size indicates the logical size, capacity indicates the allocated size (including null)
struct CharBuffer
{
    char *p;
    size_t size;
    size_t capacity;
};

//common.c
///Prints error message and exits
void kpd_error(enum Error error, const char *format, ...) __attribute__((noreturn)) __attribute__ ((format (printf, 2, 3)));
///Reads entries from TODO.md into buffer, returns open FILE* (buffer may be NULL)
void kpd_read_target(void *file, struct EntryBuffer *entries, struct CharBuffer *path);
///Writes entries to the open FILE*
void kpd_write_target(void *file, const struct EntryBuffer *entries);
///Prints entry to stdout (max_length/max_marker_length are zero for no spaces)
void kpd_print_entry(const struct Entry *entry, unsigned int max_length, unsigned int max_marker_length);
///Prints entries to stdout (if mask is NULL, prints all)
void kpd_print_entries(const struct EntryBuffer *entries, const char *mask);
///Parses number and sets mask (if mask is NULL, only checks format)
bool kpd_parse_number(char *mask, size_t mask_size, const char *number_string);
///Sets mask based on parsed number (number_string is non-NULL) or based on highest priority (number_string is NULL)
char *kpd_create_mask(const char *number_string, const struct EntryBuffer *entries);
///Parses action string (if action is NULL, only checks)
bool kpd_resolve_action(enum Action *action, const char *action_string);
///Parses status string (if status is NULL, only checks)
bool kpd_resolve_status(enum Status *status, const char *status_string);
///Parses priority string (if priority is NULL, only checks)
bool kpd_resolve_priority(enum Priority *priority, const char *priority_string);
///Returns if string can be resolved as 'commit'
bool kpd_resolve_commit(const char *commit_string);
//Executes command
void kpd_execute(char *const *arguments);

//entries.c
///Sets buffer size
void entries_set_size(struct EntryBuffer *entries, size_t size);
///Destroys buffer
void entries_finalize(struct EntryBuffer *entries, bool free_descriptions);
///Finds entry with highest priority (mask can be NULL)
bool entries_highest(size_t *index, const struct EntryBuffer *entries, const char *mask);
///Sorts entries by priority, critical first
void entries_sort(const struct EntryBuffer *entries);

//string.c
///Sets string size, size does not include '\0'
void string_set_size(struct CharBuffer *string, size_t size);
///Sets string to line read from file, returns whether read something
bool string_set_line(struct CharBuffer *string, void *file);
///Sets string to user input
void string_set_input(struct CharBuffer *string, const char *prompt, const char *prefill, const char *prefill_prompt);
///Sets string to current working directory
void string_set_cwd(struct CharBuffer *path);
///Destroys buffer
void string_finalize(struct CharBuffer *string);
///Substitutes a segment of the string
void string_substitute(struct CharBuffer *string, size_t segment_begin, size_t segment_size, const char *substitution, size_t substitution_size);
///Appends TODO.md to path
void string_append_file(struct CharBuffer *path);
///Removes last file or directory from path
bool string_remove_file(struct CharBuffer *path);
///Removes trailing and leading spaces from string
void string_trim(struct CharBuffer *string, size_t beginning_spaces, size_t ending_spaces);
///Transforms description to commit message
void string_description_to_done_commit(struct CharBuffer *string);
///Transforms description to commit message
void string_description_to_undo_commit(struct CharBuffer *string);
///Transforms description to commit message
void string_description_to_remove_commit(struct CharBuffer *string);
///Resolves string
bool string_resolve(size_t *index, const char *option, const char *const *options, size_t options_size);

#endif
