# Environment variables nutshell-nsh

### Overview
**Nutshell** uses various environment variables to control shell behavior, configuration, and special features. This document describes the supported variables and how they affect shell operation.

### How to use
You can just use **$** to get environment variables, example:
```nsh
$ echo $RANDOM
12671
```

### Default variables
- **USER** Current username
- **HOME** Home directories
- **PWD** Current working directories
- **OLDPWD** Previous working directories
- **PATH** To search up binary files/program files
- **PS0** Before command prompt string
- **PS1** Primary prompt string

### Nutshell special variables
- **RANDOM** Random integrer value 0 to 32767 (like bash)
- **UID** Current User ID
- **EUID** Effective User ID
- **HISTFILE** Where to save your history file (Warning: can be in your current path)
- **HISTSIZE** Your history max size
- **$** Current Shell PID
- **!** Last job PGID
- **?** Last exit code

### Escape Sequences for PS1

- **\u** – Username
- **\h** – Hostname (short)
- **\H** – Hostname (full)
- **\w** – Current directory (with ~ for HOME)
- **\W** – Basename of the current directory
- **$** – # for root, $ for regular user
- **\d** – Date in the format "Mon Jan 01"
- **\t** – Time in 24-hour format "HH:MM:SS"
- **\T** – Time in 12-hour format "HH:MM:SS"
- **\A** – Time in 24-hour format "HH:MM"
- **\@** – Time in 12-hour format "HH:MMam/pm"
- **\n** – Newline
- **\s** – Shell name (e.g., "nsh")
- **\v** – Shell version (short)
- **\V** – Shell version (long)
- **\!** – History number
- **\#** – Command number
- **\p** – PID of the shell
- **\U** – User ID
- **\g** – Group ID
- **\D{format}** – Custom date format (using strftime)
- **\e or \E** – Escape character
- **\[ and \[** – Delimiters for non-printable sequences
