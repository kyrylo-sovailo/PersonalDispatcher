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

Usage: kpd [COMMAND [OPTIONS]]

Placeholders:
  <number>       Entry number, defaults to first entry with highest priority
  <priority>     One of: low | medium | high | critical, defaults to 'medium'
  <directory>    Directory to contain TODO.md, defaults to current directory
  <description>  Description of the entry
  <action>       Action to be performed on found entries, one of:
                   clear [commit] | done [commit] | priority <priority> | edit

Commands:
  init     [<directory>]               Initialize kpd in a directory
  add      [<priority>] <description>  Add entry

  priority [<number>] <priority>       Set entry priority
  edit     [<number>]                  Edit entry description
  clear    [<number>] [commit]         Clear entry
  done     [<number>] [commit]         Mark entry as done

  find     <description> [<action>]    Find entry by description and execute command
  list     [all|done] [<priority>]     List entries
  sort     [all|done]                  List entries sorted by priority (default command)
  next                                 Print next entry
  test                                 Check if TODO.md exists and has the correct format

  help    | --help    | -h             Print this help
  version | --version | -v             Print version

All commands can be resolved by first letter

Use 'commit' suffix to:
  1. add changes to 
  2. stage TODO.md
  3. call 'git commit' with an automatic commit message