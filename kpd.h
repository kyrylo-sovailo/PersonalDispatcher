#ifndef KPD_H
#define KPD_H

#include <stdbool.h>
#include <stddef.h>

#define VERSION "0.1.0"
#define TARGET "TODO.md"

typedef int (Command)(int argc, char **argv);
typedef int (Comparison)(const void *a, const void *b);

enum Error
{
    ERR_OK = 0,

    //User error
    ERR_USAGE,
    ERR_FORMAT,

    //File operations
    ERR_SEEK,
    ERR_TRUNCATE,
    ERR_TELL,

    //Filesystem
    ERR_PATH,
    ERR_NOT_FOUND,
    ERR_NOT_FILE,
    ERR_NOT_DIRECTORY,

    //Memory
    ERR_MALLOC,
    ERR_REALLOC,

    //Other
    ERR_NOT_IMPLEMENTED
};

enum Priority
{
    PRI_LOW,
    PRI_MEDIUM,
    PRI_HIGH,
    PRI_CRITICAL
};

struct Entry
{
    size_t number;
    char *description;
    enum Priority priority;
    bool priority_present;
    bool done;
};

struct EntryBuffer
{
    struct Entry *p;
    size_t size;
    size_t capacity;
};

struct CharBuffer
{
    char *p;
    size_t size;
    size_t capacity;
};

//common.c
void kpd_error(enum Error error, const char *format, ...);
void /*FILE*/ *kpd_read(struct EntryBuffer *buffer);
void kpd_write(void /*FILE*/ *file, const struct EntryBuffer *entries);
void kpd_print(const struct Entry *entry);

//entries.c
void entries_set_size(struct EntryBuffer *entries, size_t size);
void entries_finalize(struct EntryBuffer *entries);
int entries_compare(const void *a, const void *b);
const struct Entry *entries_highest(const struct EntryBuffer *entries);

//sqsort.c
void sqsort(void *buffer, size_t size, size_t element_size, Comparison *comparison);

//string.c
void string_set_size(struct CharBuffer *string, size_t size);
void string_set_cwd(struct CharBuffer *path);
void string_append_file(struct CharBuffer *path);
void string_remove_file(struct CharBuffer *path);

#endif
