#include "kpd.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

void kpd_parse(struct Entry *entry, struct CharBuffer *line)
{
    if (line->size < 5
    || line->p[0] != ' '
    || line->p[1] != '-'
    || line->p[2] != ' '
    || line->p[3] != '['
    || (line->p[4] != ' ' && line->p[4] != 'X')
    || line->p[5] != ']'
    || line->p[6] != ' ') kpd_error(ERR_FORMAT, "invalid line '%s'", line);
    entry->done = line->p[4] == 'X';
    
    entry->priority = PRI_MEDIUM;
    entry->priority_present = false;
    const char *markers[4] = { "(priority: low)", "(priority: medium)", "(priority: high)", "(priority: critical)" };
    for (enum Priority priority = 0; priority < 4; priority++)
    {
        const size_t marker_length = strlen(markers[priority]);
        char *marker_found = strstr(line->p, markers[priority]);
        if (marker_found != NULL)
        {
            entry->priority = priority;
            entry->priority_present = true;
            memmove(marker_found, marker_found + marker_length, line->size - marker_length + 1);
            line->size -= marker_length;
            break;
        }
    }

    entry->description = malloc(line->size + 1);
    if (entry->description == NULL) kpd_error(ERR_MALLOC, "malloc() failed");
    memcpy(entry->description, line->p, line->size + 1);
}

void *kpd_read(struct EntryBuffer *entries)
{
    //search for TODO.md
    struct CharBuffer path = { 0 };
    string_set_cwd(&path);
    string_append_file(&path);
    FILE *file = NULL;
    while (true)
    {
        file = fopen(path.p, "r+");
        if (file != NULL) break;
        string_remove_file(&path);
        string_remove_file(&path);
        string_append_file(&path);
    }

    //parse TODO.md
    struct CharBuffer line = { 0 };
    string_set_size(&line, 8);
    line.size = 0;
    while (true)
    {
        //there are three possible actions to do: parse line, try again, stop
        const char *result = fgets(line.p + line.size, (int)(line.capacity - line.size), file);
        if (result == NULL)
        {
            if (line.size == 0) break; //nothing to parse, stop
            else {} //something left to parse
        }
        else
        {
            const char *endline = strchr(line.p, '\n');
            if (endline == NULL)
            {
                //try again
                const size_t old_capacity = line.capacity;
                string_set_size(&line, 2 * line.capacity);
                line.size = old_capacity;
                continue;
            }
            else {} //something left to parse
        }

        //parse
        struct Entry entry = { 0 };
        entry.number = entries->size;
        kpd_parse(&entry, &line);
        entries_set_size(entries, entries->size + 1);
        entries->p[entries->size - 1] = entry;

        //stop
        if (result == NULL) break;
    }

    return file;
}

void kpd_write(void *file, const struct EntryBuffer *entries)
{
    const int seek_result = fseek(file, 0, SEEK_SET);
    if (seek_result < 0) kpd_error(ERR_SEEK, "fseek() failed");

    for (struct Entry *entry = entries->p; entry < entries->p + entries->size; entry++)
    {
        const char *markers[4] = { " (priority: low)", " (priority: medium)", " (priority: high)", " (priority: critical)" };
        const char *marker = entry->priority_present ? markers[entry->priority] : "";
        fprintf(file, " - [%c] %s%s\n", entry->done ? 'X' : ' ', entry->description, marker);
    }

    const long tell_result = ftell(file);
    if (tell_result < 0) kpd_error(ERR_TELL, "ftell() failed");
    const int truncate_result = ftruncate(fileno(file), tell_result);
    if (truncate_result < 0) kpd_error(ERR_TRUNCATE, "ftell() failed");
}

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

void kpd_print(const struct Entry *entry)
{
    const char *markers[4] =
    {
        "(low)",
        CYAN "(medium)" DEFAULT,
        YELLOW "(high)" DEFAULT,
        RED "(critical)" DEFAULT
    };
    const char *marker;
    if (entry->done) marker = " " GREEN "(done)" DEFAULT;
    else if (entry->priority_present) marker = markers[entry->priority];
    else marker = "";
    
    printf(" - %u. %s%s\n", (unsigned int)(entry->number + 1), entry->description, marker);
}
