#include "kpd.h"
#include "commonlib/string.h"
#include "commonlib/path.h"

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool kpd_priority_f(struct EntryBuffer *entries, const char *mask, enum Priority priority, bool priority_explicit)
{
    struct Entry *entry_i;
    const char *mask_i;
    bool changes = false;
    for (entry_i = entries->p, mask_i = mask; entry_i < entries->p + entries->size; entry_i++, mask_i++)
    {
        if (!*mask_i) continue;
        changes |= (entry_i->priority != priority);
        changes |= (!!entry_i->priority_explicit != !!priority_explicit);
        entry_i->priority = priority;
        entry_i->priority_explicit = priority_explicit;
    }
    return changes;
}

static bool kpd_edit_f_loop(struct Entry *entry, const char *description, size_t description_length_1)
{
    char *new_description;
    if (strcmp(entry->description, description) == 0) return false;
    new_description = realloc(entry->description, description_length_1);
    if (new_description == NULL) error_print_die(ERR_MALLOC, "malloc() failed");
    memcpy(new_description, description, description_length_1);
    entry->description = new_description;
    return true;
}

static bool kpd_edit_f(struct EntryBuffer *entries, const char *mask, const char *description)
{
    const char *mask_i;
    struct Entry *entry_i;
    bool changes = false;
    size_t description_length_1 = strlen(description) + 1;
    for (entry_i = entries->p, mask_i = mask; entry_i < entries->p + entries->size; entry_i++, mask_i++)
    {
        if (!*mask_i) continue;
        changes |= kpd_edit_f_loop(entry_i, description, description_length_1); /*-fanalyzer not smart enough and complains about leaks*/
    }
    return changes;
}

static bool kpd_remove_f(struct EntryBuffer *entries, const char *mask)
{
    const struct Entry *entry_i;
    struct Entry *new_entry_i;
    const char *mask_i;
    bool changes = false;
    for (entry_i = entries->p, new_entry_i = entries->p, mask_i = mask; entry_i < entries->p + entries->size; entry_i++, mask_i++)
    {
        if (*mask_i)
        {
            /* Marked for deletion */
            free(entry_i->description);
            changes = true;
        }
        else
        {
            /* Not marked for deletion */
            if (new_entry_i != entry_i) *new_entry_i = *entry_i;
            new_entry_i++;
        }
    }
    entries->size = (size_t)(new_entry_i - entries->p);
    return changes;
}

static bool kpd_done_or_undo_f(struct EntryBuffer *entries, const char *mask, bool done)
{
    struct Entry *entry_i;
    const char *mask_i;
    bool changes = false;
    for (entry_i = entries->p, mask_i = mask; entry_i < entries->p + entries->size; entry_i++, mask_i++)
    {
        if (!*mask_i) continue;
        changes |= (!!entry_i->done != !!done);
        entry_i->done = done;
    }
    return changes;
}

static int kpd_init(int argc, char **argv)
{
    struct CharBuffer path = ZERO_INIT;
    FILE *file;

    /* Parse options */
    if (argc > 1) error_print_die(ERR_USAGE, "too many arguments");
    if (argc == 0) /* no arguments */
    {
        path_get_working_directory(&path);
    }
    else if (argc == 1) /* <directory> */
    {
        DIR *directory = opendir(argv[0]);
        if (directory == NULL) error_print_die(ERR_OPEN_DIRECTORY, "directory '%s' not found", argv[0]);
        closedir(directory);
        string_replace_mem(&path, 0, 0, argv[0], strlen(argv[0]));
    }

    /* Create TODO.md */
    path_append_mem(&path, TARGET, strlen(TARGET));
    file = fopen(path.p, "r");
    if (file == NULL)
    {
        file = fopen(path.p, "w");
        if (file == NULL) error_print_die(ERR_OPEN_FILE_WRITE, "could not create '%s' file", path.p);
    }
    else
    {
        printf(TARGET " already exists\n");
    }

    /* Cleanup */
    fclose(file);
    string_finalize(&path);
    return ERR_OK;
}

static int kpd_add(int argc, char **argv)
{
    size_t description_length_1;
    struct Entry entry = { /*Invalid*/ 0, /*Invalid*/ NULL, PRI_MEDIUM, false, false };
    struct EntryBuffer entries = ZERO_INIT;
    struct CharBuffer path = ZERO_INIT;

    /* Parse options */
    if (argc == 0) error_print_die(ERR_USAGE, "too few arguments");
    if (argc > 2) error_print_die(ERR_USAGE, "too many arguments");
    description_length_1 = strlen(argv[0]) + 1;
    entry.description = malloc(description_length_1);
    if (entry.description == NULL) error_print_die(ERR_MALLOC, "malloc() failed");
    memcpy(entry.description, argv[0], description_length_1); /* <description> */
    if (argc == 2) /* <priority> */
    {
        if (!kpd_resolve_priority(&entry.priority, argv[1])) error_print_die(ERR_USAGE, "'%s' is not a valid priority", argv[1]);
        entry.priority_explicit = true;
    }

    /* Read -> Modify entries -> Print modified entries -> [Write] */
    kpd_read_target(&entries, &path);
    entries_resize(&entries, entries.size + 1);
    entry.number = entries.size - 1;
    entries.p[entry.number] = entry;
    kpd_print_entry(&entries.p[entry.number], NULL, 0, 0);
    kpd_write_target(&entries, &path);

    /* Cleanup */
    string_finalize(&path);
    entries_finalize(&entries);
    return ERR_OK;
}

static int kpd_priority(int argc, char **argv)
{
    const char *number_string = NULL;
    enum Priority priority = PRI_MEDIUM;
    bool priority_explicit = false, changes;
    struct EntryBuffer entries = ZERO_INIT;
    struct CharBuffer path = ZERO_INIT;
    char *mask;
    
    /* Parse options */
    if (argc > 2) error_print_die(ERR_USAGE, "too many arguments");
    if (argc == 1) /* <number> or <priority> */
    {
        if (kpd_parse_number(NULL, 0, argv[0])) number_string = argv[0]; /* <number> */
        else if (kpd_resolve_priority(&priority, argv[0])) priority_explicit = true; /* <priority> */
        else error_print_die(ERR_USAGE, "'%s' is not a valid number or priority", argv[0]);
    }
    else if (argc == 2) /* <number> and <priority> */
    {
        if (!kpd_parse_number(NULL, 0, argv[0])) error_print_die(ERR_USAGE, "'%s' is not a valid number", argv[0]);
        if (!kpd_resolve_priority(&priority, argv[1])) error_print_die(ERR_USAGE, "'%s' is not a valid priority", argv[1]);
        number_string = argv[0];
        priority_explicit = true;
    }

    /* Read -> Modify entries -> Print modified entries -> [Write] */
    kpd_read_target(&entries, &path);
    mask = (number_string != NULL) ? kpd_create_mask(entries.size, number_string) : kpd_create_mask_highest_open(&entries);
    changes = kpd_priority_f(&entries, mask, priority, priority_explicit);
    kpd_print_entries(&entries, NULL, mask);
    if (changes) kpd_write_target(&entries, &path);

    /* Cleanup */
    free(mask);
    string_finalize(&path);
    entries_finalize(&entries);
    return ERR_OK;
}

static void kpd_edit_dialog(const struct EntryBuffer *entries, const char *mask, struct CharBuffer *description)
{
    const char *prompt         = "New description (Enter to accept): ";
    const char *prefill_prompt = "Old description                  : ";
    size_t index;
    const char *old_description;

    index = (size_t)((char*)memchr(mask, '\1', entries->size) - mask); /* guaranteed because if mask was empty, parsing would have failed */
    old_description = entries->p[index].description;
    string_get_input(description, prompt, old_description, prefill_prompt);
    #ifndef ENABLE_READLINE
    if (description->size == 0) error_print_die(ERR_OK, "%s", ""); /* user pressed enter, what else are we supposed to do? */
    #endif
}

static int kpd_edit(int argc, char **argv)
{
    const char *number_string = NULL;
    struct CharBuffer description = ZERO_INIT, path = ZERO_INIT;
    struct EntryBuffer entries = ZERO_INIT;
    char *mask;
    bool changes;
    
    /* Parse options */
    if (argc > 2) error_print_die(ERR_USAGE, "too many arguments");
    if (argc == 1) /* <number> or <description> */
    {
        if (kpd_parse_number(NULL, 0, argv[0])) number_string = argv[0]; /* <number> */
        else description.p = argv[0]; /* <description> */
    }
    else if (argc == 2) /* <number> and <description> */
    {
        if (!kpd_parse_number(NULL, 0, argv[0])) error_print_die(ERR_USAGE, "'%s' is not a valid number", argv[0]);
        number_string = argv[0];
        description.p = argv[1];
    }

    /* Read -> [Description dialog] -> Modify entries -> Print modified entries -> [Write] */
    kpd_read_target(&entries, &path);
    mask = (number_string != NULL) ? kpd_create_mask(entries.size, number_string) : kpd_create_mask_highest_open(&entries);
    if (description.p == NULL) kpd_edit_dialog(&entries, mask, &description);
    changes = kpd_edit_f(&entries, mask, description.p);
    kpd_print_entries(&entries, NULL, mask);
    if (changes) kpd_write_target(&entries, &path);

    /* Cleanup */
    free(mask);
    entries_finalize(&entries);
    string_finalize(&path);
    if (description.capacity > 0) string_finalize(&description);
    return ERR_OK;
}

static void kpd_commit_dialog(const struct EntryBuffer *entries, const char *mask, struct CharBuffer *commit_message, enum Action style)
{
    const char *prompt         = "Commit message (Enter to accept): ";
    const char *prefill_prompt = "Suggested commit message        : ";
    size_t index;
    struct CharBuffer suggested_message = ZERO_INIT;
    
    index = (size_t)((char*)memchr(mask, '\1', entries->size) - mask); /* guaranteed because if mask was empty, parsing would have failed */
    string_replace_mem(&suggested_message, 0, 0, entries->p[index].description, strlen(entries->p[index].description));
    if (style == ACT_DONE) string_description_to_done_commit(&suggested_message);
    else if (style == ACT_UNDO) string_description_to_undo_commit(&suggested_message);
    else if (style == ACT_REMOVE) string_description_to_remove_commit(&suggested_message);
    string_get_input(commit_message, prompt, suggested_message.p, prefill_prompt);
    #ifndef ENABLE_READLINE
        if (commit_message->size == 0) { struct CharBuffer b = suggested_message; suggested_message = *commit_message; *commit_message = b; }
    #endif

    string_finalize(&suggested_message);
}

static int kpd_commit(int argc, char **argv)
{
    const char *number_string = NULL;
    struct CharBuffer commit_message = ZERO_INIT, path = ZERO_INIT;
    struct EntryBuffer entries = ZERO_INIT;
    char *mask;
    
    /* Parse options */
    if (argc > 2) error_print_die(ERR_USAGE, "too many arguments");
    if (argc == 1) /* <number> or <message> */
    {
        if (kpd_parse_number(NULL, 0, argv[0])) number_string = argv[0]; /* <number> */
        else commit_message.p = argv[0]; /* <message> */
    }
    else if (argc == 2) /* <number> and <message> */
    {
        if (!kpd_parse_number(NULL, 0, argv[0])) error_print_die(ERR_USAGE, "'%s' is not a valid number", argv[1]);
        number_string = argv[0];
        commit_message.p = argv[1];
    }

    /* Read -> Print entries -> [Commit dialog] -> Commit */
    kpd_read_target(&entries, &path);
    mask = (number_string != NULL) ? kpd_create_mask(entries.size, number_string) : kpd_create_mask_highest_open(&entries);
    kpd_print_entries(&entries, NULL, mask);
    if (commit_message.p == NULL) kpd_commit_dialog(&entries, mask, &commit_message, ACT_DONE);
    kpd_invoke_git(path.p, commit_message.p);

    /* Cleanup */
    free(mask);
    entries_finalize(&entries);
    string_finalize(&path);
    if (commit_message.capacity > 0) string_finalize(&commit_message);
    return ERR_OK;
}

static int kpd_remove_or_done_or_undo(int argc, char **argv, enum Action action)
{
    const char *number_string = NULL;
    bool commit_suffix = false, changes = false;
    struct CharBuffer commit_message = ZERO_INIT, path = ZERO_INIT;
    struct EntryBuffer entries = ZERO_INIT;
    char *mask;

    /* Parse options */
    if (argc > 3) error_print_die(ERR_USAGE, "too many arguments");
    if (argc == 1) /* <number> or 'commit' */
    {
        if (kpd_parse_number(NULL, 0, argv[0])) number_string = argv[0]; /* <number> */
        else if (kpd_resolve_commit(argv[0])) commit_suffix = true; /* 'commit' */
        else error_print_die(ERR_USAGE, "'%s' is not a valid number or 'commit' suffix", argv[0]);
    }
    else if (argc == 2) /* (<number> and 'commit') or ('commit' and <message>) */
    {
        if (kpd_parse_number(NULL, 0, argv[0]))
        {
            if (!kpd_resolve_commit(argv[1])) error_print_die(ERR_USAGE, "'%s' is not a valid number or 'commit' suffix", argv[1]);
            number_string = argv[0]; /* <number> */
            commit_suffix = true; /* 'commit' */
        }
        else if (kpd_resolve_commit(argv[0]))
        {
            commit_suffix = true; /* 'commit' */
            commit_message.p = argv[1]; /* <message> */
        }
        else error_print_die(ERR_USAGE, "'%s' is not a valid number or 'commit' suffix", argv[1]);
    }
    else if (argc == 3) /* <number> and 'commit' and <message> */
    {
        if (!kpd_parse_number(NULL, 0, argv[0])) error_print_die(ERR_USAGE, "'%s' is not a valid number", argv[1]);
        if (!kpd_resolve_commit(argv[1])) error_print_die(ERR_USAGE, "'%s' is not a valid 'commit' suffix", argv[1]);
        number_string = argv[0];
        commit_suffix = true;
        commit_message.p = argv[2];
    }

    /* Read -> ... */
    kpd_read_target(&entries, &path);
    mask = (number_string != NULL) ? kpd_create_mask(entries.size, number_string) : (
        (action != ACT_UNDO) ? kpd_create_mask_highest_open(&entries) : kpd_create_mask_last_closed(&entries)
    );

    if (action == ACT_REMOVE)
    {
        /* Print NOT modified entries -> Modify entries */
        kpd_print_entries(&entries, NULL, mask);
        changes = kpd_remove_f(&entries, mask);
    }
    else
    {
        /* Modify entries -> Print modified entries */
        changes = kpd_done_or_undo_f(&entries, mask, action == ACT_DONE);
        kpd_print_entries(&entries, NULL, mask);
    }

    /* ... -> [Commit dialog] -> [Write] -> [Commit] */
    if (commit_suffix && commit_message.p == NULL) kpd_commit_dialog(&entries, mask, &commit_message, action);
    if (changes) kpd_write_target(&entries, &path);
    if (commit_suffix) kpd_invoke_git(path.p, commit_message.p);

    /* Cleanup */
    free(mask);
    entries_finalize(&entries);
    string_finalize(&path);
    if (commit_message.capacity > 0) string_finalize(&commit_message);
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
    const char *description_needle;
    const struct Entry *entry_i;
    enum Status status = STA_OPEN;
    enum Action action = ACT_NONE;
    enum Priority priority = PRI_MEDIUM;
    struct CharBuffer new_description_or_commit_message = ZERO_INIT, path = ZERO_INIT;
    bool commit_suffix = false, priority_explicit = false, any_match = false, changes = false;
    struct EntryBuffer entries = ZERO_INIT;
    char *mask, *mask_i;
    
    /* Parse options */
    if (argc == 0) error_print_die(ERR_USAGE, "too few arguments");
    if (argc > 5) error_print_die(ERR_USAGE, "too many arguments");
    description_needle = argv[0]; /* description */
    
    /*
    <description> and <status>
    <description> and <action>
    */
    if (argc == 2)
    {
        if (kpd_resolve_status(&status, argv[1])) { /*do nothing*/ } /* <status> */
        else if (kpd_resolve_action(&action, argv[1])) { /*do nothing*/ } /* <action> */
        else error_print_die(ERR_USAGE, "'%s' is not a valid status or action", argv[1]);
    }
    /*
    <description> and <status> and <action>
    <description> and <action>=commit/edit and <description>
    <description> and <action>=remove/done/undo and 'commit'
    <description> and <action>=priority and <priority>
    */
    else if (argc == 3)
    {
        if (kpd_resolve_status(&status, argv[1]))
        {
            if (!kpd_resolve_action(&action, argv[2])) error_print_die(ERR_USAGE, "'%s' is not a valid action", argv[1]);
        }
        else
        {
            if (!kpd_resolve_action(&action, argv[1])) error_print_die(ERR_USAGE, "'%s' is not a valid status or action", argv[1]);
            if (action == ACT_COMMIT || action == ACT_EDIT)
            {
                new_description_or_commit_message.p = argv[2];
            }
            else if (action == ACT_REMOVE || action == ACT_DONE || action == ACT_UNDO)
            {
                if (!kpd_resolve_commit(argv[2])) error_print_die(ERR_USAGE, "'%s' is not a valid 'commit' suffix", argv[2]);
                commit_suffix = true;
            }
            else
            {
                if (!kpd_resolve_priority(&priority, argv[2])) error_print_die(ERR_USAGE, "'%s' is not a valid priority", argv[2]);
                priority_explicit = true;
            }
        }
    }
    /*
    <description> and <status> and <action>=commit/edit and <description>
    <description> and <status> and <action>=remove/done/undo and 'commit'
    <description> and <status> and <action>=priority and <priority>
    <description> and <action>=remove/done/undo and 'commit' and <description>
    */
    else if (argc == 4)
    {
        if (kpd_resolve_status(&status, argv[1]))
        {
            if (!kpd_resolve_action(&action, argv[2])) error_print_die(ERR_USAGE, "'%s' is not a valid action", argv[2]);
            if (action == ACT_COMMIT || action == ACT_EDIT)
            {
                new_description_or_commit_message.p = argv[3];
            }
            else if (action == ACT_REMOVE || action == ACT_DONE || action == ACT_UNDO)
            {
                if (!kpd_resolve_commit(argv[3])) error_print_die(ERR_USAGE, "'%s' is not a valid 'commit' suffix", argv[3]);
                commit_suffix = true;
            }
            else
            {
                if (!kpd_resolve_priority(&priority, argv[3])) error_print_die(ERR_USAGE, "'%s' is not a valid priority", argv[3]);
                priority_explicit = true;
            }
        }
        else
        {
            if (!kpd_resolve_action(&action, argv[1])) error_print_die(ERR_USAGE, "'%s' is not a valid status or action", argv[1]);
            if (!(action == ACT_REMOVE || action == ACT_DONE || action == ACT_UNDO)) error_print_die(ERR_USAGE, "too many arguments");
            if (!kpd_resolve_commit(argv[2])) error_print_die(ERR_USAGE, "'%s' is not a valid 'commit' suffix", argv[2]);
            commit_suffix = true;
            new_description_or_commit_message.p = argv[3];
        }
    }
    /*
    <description> and <status> and <action>=remove/done/undo and 'commit' and <description>
    */
    else if (argc == 5)
    {
        if (!kpd_resolve_status(&status, argv[1])) error_print_die(ERR_USAGE, "'%s' is not a valid status", argv[1]);
        if (!kpd_resolve_action(&action, argv[2])) error_print_die(ERR_USAGE, "'%s' is not a valid action", argv[2]);
        if (!(action == ACT_REMOVE || action == ACT_DONE || action == ACT_UNDO)) error_print_die(ERR_USAGE, "too many arguments");
        if (!kpd_resolve_commit(argv[3])) error_print_die(ERR_USAGE, "'%s' is not a valid 'commit' suffix", argv[3]);
        commit_suffix = true;
        new_description_or_commit_message.p = argv[4];
    }
    if (action == ACT_COMMIT) { action = ACT_NONE; commit_suffix = true; }

    /* Read -> ... */
    kpd_read_target(&entries, &path);
    mask = malloc(entries.size);
    if (mask == NULL) error_print_die(ERR_MALLOC, "malloc() failed");
    for (entry_i = entries.p, mask_i = mask; entry_i < entries.p + entries.size; entry_i++, mask_i++)
    {
        const bool string_match = string_find_case(entry_i->description, description_needle) != NULL;
        const bool status_match = (status == STA_ALL) ? true : ((status == STA_OPEN) ? (!entry_i->done) : (entry_i->done));
        const bool match = string_match && status_match;
        any_match |= match;
        *mask_i = match ? '\1' : '\0';
    }
    if (!any_match)
    {
        printf("No matches\n");
        free(mask);
        string_finalize(&path);
        entries_finalize(&entries);
        if (new_description_or_commit_message.capacity > 0) string_finalize(&new_description_or_commit_message);
        return ERR_OK;
    }

    if (action == ACT_DONE || action == ACT_UNDO || action == ACT_PRIORITY)
    {
        /* ... -> Modify entries -> Print modified entries -> ... */
        if (action == ACT_DONE || action == ACT_UNDO) changes = kpd_done_or_undo_f(&entries, mask, action == ACT_DONE);
        else changes = kpd_priority_f(&entries, mask, priority, priority_explicit);
        kpd_print_entries(&entries, description_needle, mask);
    }
    else if (action == ACT_REMOVE)
    {
        /* ... -> Print NOT modified entries -> Modify entries -> ... */
        kpd_print_entries(&entries, description_needle, mask);
        changes = kpd_remove_f(&entries, mask);
    }
    else if (action == ACT_EDIT)
    {
        /* ... -> [Description dialog] -> Modify entries -> Print modified entries -> ... */
        if (new_description_or_commit_message.p == NULL) kpd_edit_dialog(&entries, mask, &new_description_or_commit_message);
        changes = kpd_edit_f(&entries, mask, new_description_or_commit_message.p);
        kpd_print_entries(&entries, description_needle, mask);
    }
    else
    {
        /* ... -> Print NOT modified entries -> ... */
        kpd_print_entries(&entries, description_needle, mask);
    }

    /* ... -> [Commit dialog] -> [Write] -> [Commit] */
    if (commit_suffix && new_description_or_commit_message.p == NULL)
        kpd_commit_dialog(&entries, mask, &new_description_or_commit_message, (action == ACT_NONE) ? ACT_DONE : action);
    if (changes) kpd_write_target(&entries, &path);
    if (commit_suffix) kpd_invoke_git(path.p, new_description_or_commit_message.p);

    /* Cleanup */
    free(mask);
    string_finalize(&path);
    entries_finalize(&entries);
    if (new_description_or_commit_message.capacity > 0) string_finalize(&new_description_or_commit_message);
    return ERR_OK;
}

static int kpd_list(int argc, char **argv)
{
    enum Status status = STA_OPEN;
    enum Priority priority = PRI_MEDIUM;
    bool priority_explicit = false;
    struct EntryBuffer entries = ZERO_INIT;
    char *mask = NULL;

    /* Parse options */
    if (argc > 2) error_print_die(ERR_USAGE, "too many arguments");
    if (argc == 1)
    {
        if (kpd_resolve_status(&status, argv[0])) { /*do nothing*/ }
        else if (kpd_resolve_priority(&priority, argv[0])) priority_explicit = true;
        else error_print_die(ERR_USAGE, "'%s' is not a valid status or priority", argv[0]);
    }
    else if (argc == 2)
    {
        if (!kpd_resolve_status(&status, argv[0])) error_print_die(ERR_USAGE, "'%s' is not a valid status", argv[0]);
        if (!kpd_resolve_priority(&priority, argv[1])) error_print_die(ERR_USAGE, "'%s' is not a valid priority", argv[1]);
        priority_explicit = true;
    }

    /* Read -> Print entries */
    kpd_read_target(&entries, NULL);
    if (status != STA_ALL || priority_explicit)
    {
        const struct Entry *entry_i;
        char *mask_i;
        mask = malloc(entries.size);
        if (mask == NULL) error_print_die(ERR_MALLOC, "malloc() failed");
        for (entry_i = entries.p, mask_i = mask; entry_i < entries.p + entries.size; entry_i++, mask_i++)
        {
            const bool priority_match = !priority_explicit || entry_i->priority == priority;
            const bool status_match = (status == STA_ALL) ? true : ((status == STA_OPEN) ? (!entry_i->done) : (entry_i->done));
            *mask_i = (priority_match && status_match) ? '\1' : '\0';
        }
    }
    kpd_print_entries(&entries, NULL, mask);

    /* Cleanup */
    if (mask != NULL) free(mask);
    entries_finalize(&entries);
    return ERR_OK;
}

static int kpd_sort(int argc, char **argv)
{
    const struct Entry *entry_i;
    enum Status status = STA_OPEN;
    struct EntryBuffer entries = ZERO_INIT;
    char *mask, *mask_i;

    /* Parse options */
    if (argc > 2) error_print_die(ERR_USAGE, "too many arguments");
    if (argc == 1)
    {
        if (!kpd_resolve_status(&status, argv[0])) error_print_die(ERR_USAGE, "'%s' is not a valid status or priority", argv[0]);
    }

    /* Read -> Modify entries -> Print modified entries */
    kpd_read_target(&entries, NULL);
    entries_sort(&entries);
    mask = malloc(entries.size);
    if (mask == NULL) error_print_die(ERR_MALLOC, "malloc() failed");
    memset(mask, '\0', entries.size);
    for (entry_i = entries.p, mask_i = mask; entry_i < entries.p + entries.size; entry_i++, mask_i++)
    {
        const bool status_match = (status == STA_ALL) ? true : ((status == STA_OPEN) ? (!entry_i->done) : (entry_i->done));
        if (status_match) *mask_i = '\1';
    }
    kpd_print_entries(&entries, NULL, mask);

    /* Cleanup */
    free(mask);
    entries_finalize(&entries);
    return ERR_OK;
}

static int kpd_next(int argc, char **argv)
{
    struct EntryBuffer entries = ZERO_INIT;
    size_t highest_index;

    /* Parse options */
    (void)argv;
    if (argc > 0) error_print_die(ERR_USAGE, "too many arguments");

    /* Read -> Print entries */
    kpd_read_target(&entries, NULL);
    if (!entries_highest_open(&highest_index, &entries)) printf("Nothing to do\n");
    else kpd_print_entry(&entries.p[highest_index], NULL, 0, 0);

    /* Cleanup */
    entries_finalize(&entries);
    return ERR_OK;
}

static int kpd_test(int argc, char **argv)
{
    /* Parse options */
    (void)argv;
    if (argc > 0) error_print_die(ERR_USAGE, "too many arguments");

    /* Read -> Print */
    kpd_read_target(NULL, NULL);
    printf("All correct\n");

    return ERR_OK;
}

static int kpd_help(int argc, char **argv)
{
    /* Parse options */
    (void)argv;
    if (argc > 0) error_print_die(ERR_USAGE, "too many options");

    /* Print */
    printf("Kyrylo's Personal Dispatcher, version " VERSION "\n");
    printf("\n");
    printf("Placeholders:\n");
    printf("  <number>      Entry number or comma-separated list, defaults to task with highest priority\n");
    printf("  <priority>    One of: low | medium | high | critical, defaults to 'medium'\n");
    printf("  <status>      One of: all | open | done, defaults to 'open'\n");
    printf("  <directory>   Directory to contain TODO.md, defaults to current directory\n");
    printf("  <description> Description of the task\n");
    printf("  <action>      Action to be performed on found entries, one of:\n");
    printf("                  <commit> | remove <commit> | done <commit> | undo <commit> |\n");
    printf("                  priority [<priority>]      | edit [<description>]\n");
    printf("  <commit>      'commit' suffix. Format: commit [<message>]\n");
    printf("                Use 'commit' suffix to:\n");
    printf("                  1. execute <action> and save TODO.md\n");
    printf("                  2. stage TODO.md\n");
    printf("                  3. call 'git commit' with a commit message\n");
    printf("                    (generated from <description> by default)\n");
    printf("\n");
    printf("Commands:\n");
    printf("  init      [<directory>]               Initialize kpd in a directory\n");
    printf("  add       <description> [<priority>]  Add task\n");
    printf("\n");
    printf("  priority  [<number>] [<priority>]     Set task priority\n");
    printf("  edit      [<number>] [<description>]  Edit or set task description\n");
    printf("  commit    [<number>] [<message>]      Perform git commit, see description of <commit>\n");
    printf("  remove    [<number>] [<commit>]       Remove task\n");
    printf("  done      [<number>] [<commit>]       Mark task as done\n");
    printf("  undo      [<number>] [<commit>]       Mark task as not done, defaults to last done task\n");
    printf("\n");
    printf("  list      [<status>] [<priority>]     List entries\n");
    printf("  sort      [<status>]                  List entries sorted by priority (default command)\n");
    printf("  next                                  Print next task\n");
    printf("  test                                  Check if TODO.md exists and has the correct format\n");
    printf("  find      <description>\n");
    printf("            [<status>] [<action>]       Find task by description and execute command\n");
    printf("\n");
    printf("  help    | --help    | -h              Print this help\n");
    printf("  version | --version | -v              Print version\n");
    printf("\n");
    printf("All keywords can be resolved by first letter\n");
    
    return ERR_OK;
}

static int kpd_version(int argc, char **argv)
{
    /* Parse options */
    (void)argv;
    if (argc > 0) error_print_die(ERR_USAGE, "too many options");

    /* Print */
    printf("Kyrylo's Personal Dispatcher, version " VERSION "\n");

    /* Cleanup */
    return ERR_OK;
}

int main(int argc, char **argv)
{
    const char *option_string;

    if (argc <= 1)
    {
        /* No arguments */
        return kpd_sort(0, NULL);
    }
    else if (argv[1][0] == '-')
    {
        /* Auxiliary arguments */
        if (argc != 2) error_print_die(ERR_USAGE, "too many options");
        option_string = argv[1];
        if (strcmp(option_string, "-h") == 0 || strcmp(option_string, "--help") == 0) return kpd_help(0, NULL);
        else if (strcmp(option_string, "-v") == 0 || strcmp(option_string, "--version") == 0) return kpd_version(0, NULL);
        else error_print_die(ERR_USAGE, "'%s' is not a valid option", option_string);
    }
    else
    {
        /* Main operation */
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
        Command *command;
        if (!string_resolve(&command_index, command_string, command_strings, sizeof(command_strings)/sizeof(*command_strings)))
            error_print_die(ERR_USAGE, "'%s' is not a valid command", command_string);
        command = commands[command_index];
        if (command != NULL) return command(argc - 2, argv + 2);
    }
    return 0;
}
