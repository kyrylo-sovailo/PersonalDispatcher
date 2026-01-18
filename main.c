#include "kpd.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int kpd_init(int argc, char **argv)
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
    string_finalize(&path);
    return ERR_OK;
}

static int kpd_add(int argc, char **argv)
{
    //Parse options
    struct Entry entry;
    if (argc == 0) kpd_error(ERR_USAGE, "too few arguments");
    if (argc > 2) kpd_error(ERR_USAGE, "too many arguments");
    entry.priority = PRI_MEDIUM;
    entry.priority_explicit = false;
    entry.description = strdup(argv[0]);
    if (entry.description == NULL) kpd_error(ERR_MALLOC, "strdup() failed");
    if (argc == 2)
    {
        if (kpd_resolve_priority(&entry.priority, argv[1])) entry.priority_explicit = true;
        else kpd_error(ERR_USAGE, "'%s' is not a valid priority", argv[1]);
    }

    //Parse TODO.md
    struct EntryBuffer entries = { 0 };
    FILE *file;
    kpd_read_target(&file, &entries, NULL);

    //Modify entries
    entry.number = entries.size;
    entry.done = false;
    entries_set_size(&entries, entry.number + 1);
    entries.p[entry.number] = entry;

    //Print
    kpd_print_entry(&entry, 0, 0);

    //Write TODO.md
    kpd_write_target(file, &entries);

    //Cleanup
    fclose(file);
    entries_finalize(&entries, true);
    return ERR_OK;
}

static int kpd_priority(int argc, char **argv)
{
    //Parse options
    const char *number_string = NULL;
    enum Priority priority = PRI_MEDIUM;
    bool priority_explicit = false;
    if (argc > 2) kpd_error(ERR_USAGE, "too many arguments");
    if (argc == 1)
    {
        if (kpd_parse_number(NULL, 0, argv[0])) number_string = argv[0];
        else if (kpd_resolve_priority(&priority, argv[0])) priority_explicit = true;
        else kpd_error(ERR_USAGE, "'%s' is not a valid number or priority", argv[0]);
    }
    else if (argc == 2)
    {
        if (!kpd_parse_number(NULL, 0, argv[0])) kpd_error(ERR_USAGE, "'%s' is not a valid number", argv[0]);
        if (!kpd_resolve_priority(&priority, argv[1])) kpd_error(ERR_USAGE, "'%s' is not a valid priority", argv[1]);
        number_string = argv[0];
        priority_explicit = true;
    }

    //Read TODO.md
    struct EntryBuffer entries = { 0 };
    FILE *file;
    kpd_read_target(&file, &entries, NULL);

    //Modify entries
    char *mask = (number_string != NULL) ? kpd_create_mask(entries.size, number_string) : kpd_create_mask_highest_open(&entries);
    const char *mask_i = mask;
    bool changes = false;
    for (struct Entry *entry = entries.p; entry < entries.p + entries.size; entry++, mask_i++)
    {
        if (!*mask_i) continue;
        changes |= (entry->priority != priority);
        changes |= (!!entry->priority_explicit != !!priority_explicit);
        entry->priority = priority;
        entry->priority_explicit = priority_explicit;
    }

    //Print
    kpd_print_entries(&entries, mask);

    //Write TODO.md
    if (changes) kpd_write_target(file, &entries);

    //Cleanup
    free(mask);
    fclose(file);
    entries_finalize(&entries, true);
    return ERR_OK;
}

static int kpd_edit(int argc, char **argv)
{
    //Parse options
    const char *number_string = NULL;
    struct CharBuffer description = { 0 };
    if (argc > 2) kpd_error(ERR_USAGE, "too many arguments");
    if (argc == 1)
    {
        if (kpd_parse_number(NULL, 0, argv[0])) number_string = argv[0];
        else description.p = argv[0];
    }
    else if (argc == 2)
    {
        if (!kpd_parse_number(NULL, 0, argv[0])) kpd_error(ERR_USAGE, "'%s' is not a valid number", argv[0]);
        number_string = argv[0];
        description.p = argv[1];
    }

    //Read TODO.md
    struct EntryBuffer entries = { 0 };
    FILE *file;
    kpd_read_target(&file, &entries, NULL);

    //Modify entries
    char *mask = (number_string != NULL) ? kpd_create_mask(entries.size, number_string) : kpd_create_mask_highest_open(&entries);
    if (description.p == NULL)
    {
        const size_t index = (size_t)((char*)memchr(mask, '\1', entries.size) - mask); //guaranteed because if mask was empty, parsing would have failed
        const char *old_description = entries.p[index].description;
        const char *prompt         = "New description (Enter to accept): ";
        const char *prefill_prompt = "Old description                  : ";
        string_set_input(&description, prompt, old_description, prefill_prompt);
        #ifndef ENABLE_READLINE
        if (description.size == 0) goto exit; //user pressed enter, what else are we supposed to do?
        #endif
    }
    const char *mask_i = mask;
    bool changes = false;
    for (struct Entry *entry = entries.p; entry < entries.p + entries.size; entry++, mask_i++)
    {
        if (!*mask_i) continue;
        changes |= (strcmp(entry->description, description.p) != 0);
        free(entry->description);
        entry->description = strdup(description.p);
        if (entry->description == NULL) kpd_error(ERR_MALLOC, "strdup() failed");
    }

    //Print
    kpd_print_entries(&entries, mask);

    //Write TODO.md
    if (changes) kpd_write_target(file, &entries);

    //Cleanup
    exit:
    free(mask);
    fclose(file);
    entries_finalize(&entries, true);
    if (description.capacity != 0) string_finalize(&description);
    return ERR_OK;
}

static int kpd_commit(int argc, char **argv)
{
    //Parse options
    const char *number_string = NULL;
    struct CharBuffer commit_message = { 0 };
    if (argc > 2) kpd_error(ERR_USAGE, "too many arguments");
    if (argc == 1)
    {
        if (kpd_parse_number(NULL, 0, argv[0])) number_string = argv[0];
        else commit_message.p = argv[0];
    }
    else if (argc == 2)
    {
        if (!kpd_parse_number(NULL, 0, argv[0])) kpd_error(ERR_USAGE, "'%s' is not a valid number", argv[1]);
        number_string = argv[0];
        commit_message.p = argv[1];
    }

    //Read TODO.md
    struct EntryBuffer entries = { 0 };
    struct CharBuffer path;
    kpd_read_target(NULL, &entries, &path);

    //Print
    char *mask = (number_string != NULL) ? kpd_create_mask(entries.size, number_string) : kpd_create_mask_highest_open(&entries);
    kpd_print_entries(&entries, mask);

    //Ask user
    if (commit_message.p == NULL)
    {
        const size_t index = (size_t)((char*)memchr(mask, '\1', entries.size) - mask); //guaranteed because if mask was empty, parsing would have failed
        struct CharBuffer suggested_message = { 0 };
        string_substitute(&suggested_message, 0, 0, entries.p[index].description, strlen(entries.p[index].description));
        string_description_to_done_commit(&suggested_message);
        const char *prompt         = "Commit message (Enter to accept): ";
        const char *prefill_prompt = "Suggested commit message        : ";
        string_set_input(&commit_message, prompt, suggested_message.p, prefill_prompt);
        #ifndef ENABLE_READLINE
        if (commit_message.size == 0) { struct CharBuffer b = suggested_message; suggested_message = commit_message; commit_message = b; }
        #endif
        string_finalize(&suggested_message);
    }

    //Commit
    kpd_invoke_git(path.p, commit_message.p);

    //Cleanup
    free(mask);
    string_finalize(&path);
    entries_finalize(&entries, true);
    if (commit_message.capacity != 0) string_finalize(&commit_message);
    return ERR_OK;
}

static int kpd_remove_or_done_or_undo(int argc, char **argv, enum Action action)
{
    //Parse options
    const char *number_string = NULL;
    bool commit_suffix = false;
    struct CharBuffer commit_message = { 0 };
    if (argc > 3) kpd_error(ERR_USAGE, "too many arguments");
    if (argc == 1)
    {
        if (kpd_parse_number(NULL, 0, argv[0])) number_string = argv[0];
        else if (kpd_resolve_commit(argv[0])) commit_suffix = true;
        else kpd_error(ERR_USAGE, "'%s' is not a valid number or 'commit' suffix", argv[0]);
    }
    else if (argc == 2)
    {
        if (kpd_parse_number(NULL, 0, argv[0]))
        {
            if (kpd_resolve_commit(argv[1])) commit_suffix = true;
            else kpd_error(ERR_USAGE, "'%s' is not a valid number or 'commit' suffix", argv[1]);
            number_string = argv[0];
        }
        else if (kpd_resolve_commit(argv[0]))
        {
            commit_suffix = true;
            commit_message.p = argv[1];
        }
        else kpd_error(ERR_USAGE, "'%s' is not a valid number or 'commit' suffix", argv[1]);
    }
    else if (argc == 3)
    {
        if (!kpd_parse_number(NULL, 0, argv[0])) kpd_error(ERR_USAGE, "'%s' is not a valid number", argv[1]);
        if (!kpd_resolve_commit(argv[1])) kpd_error(ERR_USAGE, "'%s' is not a valid 'commit' suffix", argv[1]);
        number_string = argv[0];
        commit_suffix = true;
        commit_message.p = argv[2];
    }

    //Read TODO.md
    struct EntryBuffer entries = { 0 };
    FILE *file;
    struct CharBuffer path;
    kpd_read_target(&file, &entries, &path);

    //Modify entries
    char *mask = (number_string != NULL) ? kpd_create_mask(entries.size, number_string) : (
        (action != ACT_UNDO) ? kpd_create_mask_highest_open(&entries) : kpd_create_mask_last_closed(&entries)
    );
    bool changes = false;
    struct EntryBuffer entries_copy = { 0 };
    struct EntryBuffer *entries_written = &entries;
    if (action == ACT_REMOVE)
    {
        entries_set_size(&entries_copy, entries.size);
        entries_copy.size = 0;
        const char *mask_i = mask;
        for (struct Entry *entry = entries.p; entry < entries.p + entries.size; entry++, mask_i++)
        {
            if (*mask_i) continue; //Skip removed entries
            entries_copy.p[entries_copy.size] = *entry;
            entries_copy.size++;
        }
        changes = true; //guaranteed because if mask was empty, parsing would have failed
        entries_written = &entries_copy; //print copy instead
    }
    else 
    {
        const char done = action == ACT_DONE;
        const char *mask_i = mask;
        for (struct Entry *entry = entries.p; entry < entries.p + entries.size; entry++, mask_i++)
        {
            if (!*mask_i) continue;
            changes |= (!!entry->done != !!done);
            entry->done = done;
        }
    }

    //Print
    kpd_print_entries(&entries, mask);

    //Ask user
    if (commit_suffix && commit_message.p == NULL)
    {
        const size_t index = (size_t)((char*)memchr(mask, '\1', entries.size) - mask); //guaranteed because if mask was empty, parsing would have failed
        struct CharBuffer suggested_message = { 0 };
        string_substitute(&suggested_message, 0, 0, entries.p[index].description, strlen(entries.p[index].description));
        if (action == ACT_DONE) string_description_to_done_commit(&suggested_message);
        else if (action == ACT_UNDO) string_description_to_undo_commit(&suggested_message);
        else string_description_to_remove_commit(&suggested_message);
        const char *prompt         = "Commit message (Enter to accept): ";
        const char *prefill_prompt = "Suggested commit message        : ";
        string_set_input(&commit_message, prompt, suggested_message.p, prefill_prompt);
        #ifndef ENABLE_READLINE
        if (commit_message.size == 0) { struct CharBuffer b = suggested_message; suggested_message = commit_message; commit_message = b; }
        #endif
        string_finalize(&suggested_message);
    }

    //Write TODO.md
    if (changes) { kpd_write_target(file, entries_written); fflush(file); }

    //Commit
    if (commit_suffix) kpd_invoke_git(path.p, commit_message.p);

    //Cleanup
    free(mask);
    string_finalize(&path);
    fclose(file);
    entries_finalize(&entries, true);
    if (commit_message.capacity != 0) string_finalize(&commit_message);
    return ERR_OK;
}

static int kpd_remove(int argc, char **argv)
{
    return kpd_remove_or_done_or_undo(argc, argv, ACT_REMOVE);
}

static int kpd_done(int argc, char **argv)
{
    return kpd_remove_or_done_or_undo(argc, argv, ACT_DONE);
}

static int kpd_undo(int argc, char **argv)
{
    return kpd_remove_or_done_or_undo(argc, argv, ACT_UNDO);
}

static int kpd_find(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    kpd_error(ERR_NOT_IMPLEMENTED, "not implemented");
    return ERR_OK;
}

static int kpd_list(int argc, char **argv)
{
    //Parse options
    if (argc > 2) kpd_error(ERR_USAGE, "too many arguments");
    enum Status status = STA_OPEN;
    enum Priority priority = PRI_MEDIUM;
    bool priority_explicit = false;
    if (argc == 1)
    {
        if (kpd_resolve_status(&status, argv[0])) { /*do nothing*/ }
        else if (kpd_resolve_priority(&priority, argv[0])) priority_explicit = true;
        else kpd_error(ERR_USAGE, "'%s' is not a valid status or priority", argv[0]);
    }
    else if (argc == 2)
    {
        if (!kpd_resolve_status(&status, argv[0])) kpd_error(ERR_USAGE, "'%s' is not a valid status", argv[0]);
        if (!kpd_resolve_priority(&priority, argv[1])) kpd_error(ERR_USAGE, "'%s' is not a valid priority", argv[1]);
        priority_explicit = true;
    }

    //Parse TODO.md
    struct EntryBuffer entries = { 0 };
    kpd_read_target(NULL, &entries, NULL);

    //Print
    char *mask = NULL;
    if (status != STA_ALL || priority_explicit)
    {
        mask = malloc(entries.size);
        if (mask == NULL) kpd_error(ERR_MALLOC, "malloc() failed");
        memset(mask, '\0', entries.size);
        char *mask_i = mask;
        for (const struct Entry *entry = entries.p; entry < entries.p + entries.size; entry++, mask_i++)
        {
            const bool priority_match = !priority_explicit || entry->priority == priority;
            bool print;
            if (status == STA_OPEN) print = priority_match & !entry->done;
            else if (status == STA_DONE) print = priority_match & entry->done;
            else print = priority_match;
            if (print) *mask_i = '\1';
        }
    }
    kpd_print_entries(&entries, mask);

    //Cleanup
    if (mask != NULL) free(mask);
    entries_finalize(&entries, true);
    return ERR_OK;
}

static int kpd_sort(int argc, char **argv)
{
    //Parse options
    if (argc > 2) kpd_error(ERR_USAGE, "too many arguments");
    enum Status status = STA_OPEN;
    if (argc == 1)
    {
        if (!kpd_resolve_status(&status, argv[0])) kpd_error(ERR_USAGE, "'%s' is not a valid status or priority", argv[0]);
    }

    //Parse TODO.md
    struct EntryBuffer entries = { 0 };
    kpd_read_target(NULL, &entries, NULL);

    //Print
    entries_sort(&entries);
    char *mask = malloc(entries.size);
    if (mask == NULL) kpd_error(ERR_MALLOC, "malloc() failed");
    memset(mask, '\0', entries.size);
    char *mask_i = mask;
    for (const struct Entry *entry = entries.p; entry < entries.p + entries.size; entry++, mask_i++)
    {
        bool print;
        if (status == STA_OPEN) print = !entry->done;
        else if (status == STA_DONE) print = entry->done;
        else print = true;
        if (print) *mask_i = '\1';
    }
    kpd_print_entries(&entries, mask);

    //Cleanup
    free(mask);
    entries_finalize(&entries, true);
    return ERR_OK;
}

static int kpd_next(int argc, char **argv)
{
    //Parse options
    (void)argv;
    if (argc > 0) kpd_error(ERR_USAGE, "too many arguments");

    //Parse TODO.md
    struct EntryBuffer entries = { 0 };
    kpd_read_target(NULL, &entries, NULL);

    //Print
    size_t highest_index;
    if (!entries_highest_open(&highest_index, &entries)) printf("Nothing to do\n");
    else kpd_print_entry(&entries.p[highest_index], 0, 0);

    //Cleanup
    entries_finalize(&entries, true);
    return ERR_OK;
}

static int kpd_test(int argc, char **argv)
{
    //Parse options
    (void)argv;
    if (argc > 0) kpd_error(ERR_USAGE, "too many arguments");

    //Parse TODO.md
    kpd_read_target(NULL, NULL, NULL);

    //Print
    printf("All correct\n");
    return ERR_OK;
}

static int kpd_help(int argc, char **argv)
{
    (void)argv;
    if (argc > 0) kpd_error(ERR_USAGE, "too many options");
    printf(
        "Kyrylo's Personal Dispatcher, version " VERSION "\n"
        "\n"
        "Placeholders:\n"
        "  <number>      Entry number or comma-separated list, defaults to task with highest priority\n"
        "  <priority>    One of: low | medium | high | critical, defaults to 'medium'\n"
        "  <status>      One of: all | open | done, defaults to 'open'\n"
        "  <directory>   Directory to contain TODO.md, defaults to current directory\n"
        "  <description> Description of the task\n"
        "  <action>      Action to be performed on found entries, one of:\n"
        "                  <commit> | remove <commit> | done <commit> | undo <commit> |\n"
        "                  priority <priority> | edit [<description>]\n"
        "  <commit>      'commit' suffix. Format: commit [<message>]\n"
        "                Use 'commit' suffix to:\n"
        "                  1. execute <action> and save TODO.md\n"
        "                  2. stage TODO.md\n"
        "                  3. call 'git commit' with a commit message\n"
        "                    (generated from <description> by default)\n"
        "\n"
        "Commands:\n"
        "  init      [<directory>]               Initialize kpd in a directory\n"
        "  add       <description> [<priority>]  Add task\n"
        "\n"
        "  priority  [<number>] [<priority>]     Set task priority\n"
        "  edit      [<number>] [<description>]  Edit or set task description\n"
        "  commit    [<number>] [<message>]      Perform git commit, see description of <commit>\n"
        "  remove    [<number>] [<commit>]       Remove task\n"
        "  done      [<number>] [<commit>]       Mark task as done\n"
        "  undo      [<number>] [<commit>]       Mark task as not done, defaults to last done task\n"
        "\n"
        "  list      [<status>] [<priority>]     List entries\n"
        "  sort      [<status>]                  List entries sorted by priority (default command)\n"
        "  next                                  Print next task\n"
        "  test                                  Check if TODO.md exists and has the correct format\n"
        "  find      <description>\n"
        "            [<status>] [<action>]       Find task by description and execute command\n"
        "\n"
        "  help    | --help    | -h              Print this help\n"
        "  version | --version | -v              Print version\n"
        "\n"
        "All keywords can be resolved by first letter\n"
    );
    return ERR_OK;
}

static int kpd_version(int argc, char **argv)
{
    //Parse options
    (void)argv;
    if (argc > 0) kpd_error(ERR_USAGE, "too many options");

    //Print
    printf("Kyrylo's Personal Dispatcher, version " VERSION "\n");

    //Cleanup
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
            kpd_init, kpd_add,
            kpd_priority, kpd_edit, kpd_commit, kpd_remove, kpd_done, kpd_undo,
            kpd_find, kpd_list, kpd_sort, kpd_next, kpd_test,
            kpd_help, kpd_version
        };
        const char *command_strings[] =
        {
            "init", "add",
            "priority", "edit", "commit", "remove", "done", "undo",
            "find", "list", "sort", "next", "test",
            "help", "version"
        };
        const char *command_string = argv[1];
        size_t command_index;
        if (!string_resolve(&command_index, command_string, command_strings, sizeof(command_strings)/sizeof(*command_strings)))
            kpd_error(ERR_USAGE, "'%s' is not a valid command", command_string);
        Command *command = commands[command_index];
        if (command != NULL) return command(argc - 2, argv + 2);
    }
}
