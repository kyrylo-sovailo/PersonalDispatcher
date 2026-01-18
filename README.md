# Welcome to Kyrylo's Personal Dispatcher

An utility to manage TODO.md. There are many like it, but this one is mine.

The main feature of KPD is that TODO.md searches for TODO.md in the current directory and its parents (in contrast to taskwarrior and todo.txt). It makes it friendly to git and cmake.

### Build

```
mkdir build
cd build
cmake ..
cmake --build .
./kpd
```

KPD requires GNU readline.

### Usage

```
Usage: kpd [<command> <option>*]

Placeholders:
  <number>      Entry number or comma-separated list, defaults to task with highest priority
  <priority>    One of: low | medium | high | critical, defaults to 'medium'
  <status>      One of: all | open | done, defaults to 'open'
  <directory>   Directory to contain TODO.md, defaults to current directory
  <description> Description of the task
  <action>      Action to be performed on found entries, one of:
                  <commit> | remove <commit> | done <commit> | undo <commit> |
                  priority <priority> | edit [<description>]
  <commit>      'commit' suffix. Format: commit [<message>]
                Use 'commit' suffix to:
                  1. execute <action> and update TODO.md
                  2. stage TODO.md
                  3. call 'git commit' with a commit message
                    (generated from <description> by default)

Commands:
  init      [<directory>]               Initialize kpd in a directory
  add       <description> [<priority>]  Add task

  priority  [<number>] [<priority>]     Set task priority
  edit      [<number>] [<description>]  Edit or set task description
  commit    [<number>] [<message>]      Perform git commit, see description of <commit>
  remove    [<number>] [<commit>]       Remove task
  done      [<number>] [<commit>]       Mark task as done
  undo      [<number>] [<commit>]       Mark task as not done, defaults to last done task

  list      [<status>] [<priority>]     List entries
  sort      [<status>]                  List entries sorted by priority (default command)
  next                                  Print next task
  test                                  Check if TODO.md exists and has the correct format
  find      <description>
            [<status>] [<action>]       Find task by description and execute command

  help    | --help    | -h              Print this help
  version | --version | -v              Print version

All keywords can be resolved by first letter
```
