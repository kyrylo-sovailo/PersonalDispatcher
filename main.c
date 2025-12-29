#include "kpd.h"

#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

char resolve_all_done(const char *option)
{
    //Can be resolved using first letter only
    const char *resolved_option;
    char resolved_option_c;
    switch (option[0])
    {
        case 'a': resolved_option = "all"; resolved_option_c = 'a'; break;
        case 'd': resolved_option = "done"; resolved_option_c = 'd'; break;
        default: return false;
    }
    const size_t option_length = strlen(option);
    const size_t resolved_option_length = strlen(resolved_option);
    if (option_length <= resolved_option_length && memcmp(option, resolved_option, option_length) == 0) return resolved_option_c;
    else return '\0';
}

int kpd_add(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    kpd_error(ERR_NOT_IMPLEMENTED, "not implemented");
    return ERR_OK;
}

int kpd_clear(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    struct EntryBuffer entries = { 0 };
    kpd_read(&entries);

    kpd_error(ERR_NOT_IMPLEMENTED, "not implemented");
    return ERR_OK;
}

int kpd_done(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    kpd_error(ERR_NOT_IMPLEMENTED, "not implemented");
    return ERR_OK;
}

int kpd_edit(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    kpd_error(ERR_NOT_IMPLEMENTED, "not implemented");
    return ERR_OK;
}

int kpd_find(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    kpd_error(ERR_NOT_IMPLEMENTED, "not implemented");
    return ERR_OK;
}

int kpd_help(int argc, char **argv)
{
    (void)argv;
    if (argc > 0) kpd_error(ERR_USAGE, "too many options");
    printf(
        "Kyrylo's Personal Dispatcher, version " VERSION "\n"
        "Usage: kpd [COMMAND [OPTIONS]]\n"
        "\n"
        "Placeholders:\n"
        "  <number>       Entry number, defaults to first entry with highest priority\n"
        "  <priority>     One of: low | medium | high | critical, defaults to 'medium'\n"
        "  <directory>    Directory to contain " TARGET ", defaults to current directory\n"
        "  <description>  Description of the entry\n"
        "  <action>       Action to be performed on found entries, one of:\n"
        "                   clear [commit] | done [commit] | priority <priority> | edit\n"
        "\n"
        "Commands:\n"
        "  init     [<directory>]               Initialize kpd in a directory\n"
        "  add      [<priority>] <description>  Add entry\n"
        "\n"
        "  priority [<number>] <priority>       Set entry priority\n"
        "  edit     [<number>]                  Edit entry description\n"
        "  clear    [<number>] [commit]         Clear entry\n"
        "  done     [<number>] [commit]         Mark entry as done\n"
        "\n"
        "  find     <description> [<action>]    Find entry by description and execute command\n"
        "  list     [all|done] [<priority>]     List entries\n"
        "  sort     [all|done]                  List entries sorted by priority (default command)\n"
        "  next                                 Print next entry\n"
        "  test                                 Check if " TARGET " exists and has the correct format\n"
        "\n"
        "  help    | --help    | -h             Print this help\n"
        "  version | --version | -v             Print version\n"
        "\n"
        "All commands can be resolved by first letter\n"
        "\n"
        "Use 'commit' suffix to:\n"
        "  1. add changes to " "\n"
        "  2. stage " TARGET "\n"
        "  3. call 'git commit' with an automatic commit message\n"
    );
    return ERR_OK;
}

int kpd_init(int argc, char **argv)
{
    //Get directory
    struct CharBuffer path = { 0 };
    if (argc == 0)
    {
        string_set_cwd(&path);
    }
    else if (argc == 1)
    {
        struct stat status;
        const bool exists = stat(argv[0], &status) >= 0;
        if (!exists) kpd_error(ERR_NOT_FOUND, "directory '%s' not found", argv[0]);
        if ((status.st_mode & S_IFMT) == S_IFDIR) kpd_error(ERR_NOT_DIRECTORY, "path '%s' is not a directory", argv[0]);
        const size_t directory_length = strlen(argv[0]);
        string_set_size(&path, directory_length);
        memcpy(path.p, argv[0], directory_length);
    }
    else kpd_error(ERR_USAGE, "too many arguments");

    //Get TODO.md path
    string_append_file(&path);

    //Get status
    struct stat status;
    const bool exists = stat(path.p, &status) >= 0;

    //Decide what to do
    if (exists)
    {
        if ((status.st_mode & S_IFMT) == S_IFREG) printf(TARGET " already exists\n");
        else kpd_error(ERR_NOT_FILE, TARGET " exists, but is not a regular file");
    }
    else
    {
        int file = open(path.p, O_CREAT);
        if (file < 0) kpd_error(ERR_NOT_FILE, "open() failed");
        if (close(file) < 0) kpd_error(ERR_NOT_FILE, "close() failed");
    }

    //Cleanup
    free(path.p);
    return ERR_OK;
}

int kpd_list(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    kpd_error(ERR_NOT_IMPLEMENTED, "not implemented");
    return ERR_OK;
}

int kpd_next(int argc, char **argv)
{
    (void)argv;
    if (argc > 0) kpd_error(ERR_USAGE, "too many arguments");
    struct EntryBuffer entries = { 0 };
    kpd_read(&entries);
    const struct Entry *highest = entries_highest(&entries);
    printf("Next priority:\n");
    kpd_print(highest);
    entries_finalize(&entries);
    return ERR_OK;
}

int kpd_priority(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    kpd_error(ERR_NOT_IMPLEMENTED, "not implemented");
    return ERR_OK;
}

int kpd_sort(int argc, char **argv)
{
    (void)argv;
    char all_done = '\0';
    if (argc > 1)
    {
        kpd_error(ERR_USAGE, "too many arguments");
    }
    else if (argc == 1)
    {
        const char *option = argv[0];
        all_done = resolve_all_done(option);
        if (all_done == '\0') kpd_error(ERR_USAGE, "'%s' is not a valid option", option);
    }

    struct EntryBuffer entries = { 0 };
    kpd_read(&entries);
    sqsort(&entries.p, entries.size, sizeof(*entries.p), entries_compare);
    for (const struct Entry *entry = entries.p; entry < entries.p + entries.size; entry++)
    {
        kpd_print(entry);
    }
    entries_finalize(&entries);
    return ERR_OK;
}

int kpd_test(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    kpd_read(NULL);
    printf("All correct\n");
    return ERR_OK;
}

int kpd_version(int argc, char **argv)
{
    (void)argv;
    if (argc > 0) kpd_error(ERR_USAGE, "too many options");
    printf("Kyrylo's Personal Dispatcher, version " VERSION "\n");
    return ERR_OK;
}

Command *resolve_command(const char *command)
{
    //Can be resolved using first letter only
    const char *resolved_command;
    Command *resolved_command_f;
    switch (command[0])
    {
        case 'a': resolved_command = "add";         resolved_command_f = &kpd_add;      break;
        case 'c': resolved_command = "clear";       resolved_command_f = &kpd_clear;    break;
        case 'd': resolved_command = "done";        resolved_command_f = &kpd_done;     break;
        case 'e': resolved_command = "edit";        resolved_command_f = &kpd_edit;     break;
        case 'f': resolved_command = "find";        resolved_command_f = &kpd_find;     break;
        case 'h': resolved_command = "help";        resolved_command_f = &kpd_help;     break;
        case 'i': resolved_command = "init";        resolved_command_f = &kpd_init;     break;
        case 'l': resolved_command = "list";        resolved_command_f = &kpd_list;     break;
        case 'n': resolved_command = "next";        resolved_command_f = &kpd_next;     break;
        case 'p': resolved_command = "priority";    resolved_command_f = &kpd_priority; break;
        case 's': resolved_command = "sort";        resolved_command_f = &kpd_sort;     break;
        case 't': resolved_command = "test";        resolved_command_f = &kpd_test;     break;
        case 'v': resolved_command = "version";     resolved_command_f = &kpd_version;  break;
        default: return NULL;
    }
    const size_t command_length = strlen(command);
    const size_t resolved_command_length = strlen(resolved_command);
    if (command_length <= resolved_command_length && memcmp(command, resolved_command, command_length) == 0) return resolved_command_f;
    else return NULL;
}

int main(int argc, char **argv)
{
    if (argc <= 1)
    {
        //no arguments
        return kpd_sort(0, NULL);
    }
    else if (argv[1][0] == '-')
    {
        //auxiliary arguments
        if (argc != 2) kpd_error(ERR_USAGE, "too many options");
        const char *option = argv[1];
        if (strcmp(option, "-h") == 0 || strcmp(option, "--help") == 0) return kpd_help(0, NULL);
        else if (strcmp(option, "-v") == 0 || strcmp(option, "--version") == 0) return kpd_version(0, NULL);
        else kpd_error(ERR_USAGE, "'%s' is not a valid option", option);
    }
    else
    {
        //main operation
        const char *command = argv[1];
        Command *command_f = resolve_command(command);
        if (command_f != NULL) return command_f(argc - 2, argv + 2);
        else kpd_error(ERR_USAGE, "'%s' is not a valid command", command);
    }
}
