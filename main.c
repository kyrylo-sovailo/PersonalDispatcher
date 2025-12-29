#include "kpd.h"

#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

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
    //Parse options
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

    //Get status of TODO.md
    string_append_file(&path);
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

int kpd_list_or_sort(int argc, char **argv, bool sort)
{
    //Parse options
    const char *options[] = { "all", "done" };
    size_t option_index = (size_t)-1;
    if (argc > 1)
    {
        kpd_error(ERR_USAGE, "too many arguments");
    }
    else if (argc == 1)
    {
        const char *option = argv[0];
        if (!string_resolve(&option_index, option, options, sizeof(options)/sizeof(*options)))
            kpd_error(ERR_USAGE, "'%s' is not a valid option", option);
    }

    //Parse TODO.md
    struct EntryBuffer entries = { 0 };
    kpd_read(&entries);
    if (sort) entries_sort(&entries);

    //Print
    for (const struct Entry *entry = entries.p; entry < entries.p + entries.size; entry++)
    {
        bool print;
        if (option_index == 0) print = true;
        else if (option_index == 1) print = entry->done;
        else print = !entry->done;
        if (print) kpd_print(entry);
    }

    //Cleanup
    entries_finalize(&entries);
    return ERR_OK;
}

int kpd_list(int argc, char **argv)
{
    return kpd_list_or_sort(argc, argv, false);
}

int kpd_next(int argc, char **argv)
{
    (void)argv;
    if (argc > 0) kpd_error(ERR_USAGE, "too many arguments");
    struct EntryBuffer entries = { 0 };
    kpd_read(&entries);
    size_t highest_index;
    if (!entries_highest(&highest_index, &entries))
    {
        printf("Nothing to do\n");
    }
    else
    {
        kpd_print(&entries.p[highest_index]);
        entries_finalize(&entries);
    }
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
    return kpd_list_or_sort(argc, argv, true);
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

int main(int argc, char **argv)
{
    if (argc <= 1)
    {
        //No arguments
        return kpd_sort(0, NULL);
    }
    else if (argv[1][0] == '-')
    {
        //Auxiliary arguments
        if (argc != 2) kpd_error(ERR_USAGE, "too many options");
        const char *option_string = argv[1];
        if (strcmp(option_string, "-h") == 0 || strcmp(option_string, "--help") == 0) return kpd_help(0, NULL);
        else if (strcmp(option_string, "-v") == 0 || strcmp(option_string, "--version") == 0) return kpd_version(0, NULL);
        else kpd_error(ERR_USAGE, "'%s' is not a valid option", option_string);
    }
    else
    {
        //Main operation
        Command *commands[] =
        {
            kpd_add, kpd_clear, kpd_done, kpd_edit, kpd_find, kpd_help, kpd_init, kpd_list, kpd_next, kpd_priority, kpd_sort, kpd_test, kpd_version
        };
        const char *command_strings[] =
        {
            "add", "clear", "done", "edit", "find", "help", "init", "list", "next", "priority", "sort", "test", "version"
        };
        const char *command_string = argv[1];
        size_t command_index;
        if (!string_resolve(&command_index, command_string, command_strings, sizeof(command_strings)/sizeof(*command_strings)))
            kpd_error(ERR_USAGE, "'%s' is not a valid command", command_string);
        Command *command = commands[command_index];
        if (command != NULL) return command(argc - 2, argv + 2);
    }
}
