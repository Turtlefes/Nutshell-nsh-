# Nutshell-nsh 2025

**Date:** 2025-09-18 -> 2025-09-19 **Version:** 0.3.8.46 -> 0.3.8.54

### bugs
- Escaping backslash on EOF_IN mode [FIXED]
- Cannot execute relative path [FIXED]
- Pipeline cannot execute builtin command [FIXED]
- Assignment variable like ```VAR=/value``` is considered as a path execution
- Alias Assignment [FIXED, caused by tokenizer]
- Bug when cd when having a CDPATH [FIXED]

### To-be-changed/Coming
- Hash output [CHANGED]
- Hash hits count [ADDED]

### [+] Added
- Added hash cache support
- New builtin hash
- Hash hits count
- More error handling on execution

### [*] Fixed
- Argv problems with command
- Fix problem auto done input when using ctrl+c
- Double quote expansion can't use command substitution or arithmetic expansion
- Segmentation fault when using quotes
- Fix cannot execute relative path
- Pipeline cannot execute builtin command
- Bug when cd when having a CDPATH
- Escaping backslash on EOF_IN mode

### [>] Changed
- new README.md
- Hash output
- CHANGELOG.md



**Date:** 2025-09-18 -> 2025-09-23 **Version:** 0.3.8.46 -> 0.3.8.56 -> 0.3.8.64

### [+] Added
- Environment child process
- Support for pipelines execution like ``echo ls | nsh``
- Bookmark Change directory
- Builtin 'type'
- History expansion "!"

### [*] Fixed
- Alias Assignment [FIXED, caused by tokenizer]
- Signal output on child process looking weird
- EOF interrupt
- Variable Assignment in one line can't be initialized
- Cannot interrupt a file execution V2

**Date:** 2025-09-23 -> 2025-10-4 **Version:** 0.3.8.64 -> 0.3.8.73

### [+] Added
#### 0.3.8.70
- More redirection
- Cross session jobs
- Puzzle math like ```echo $((?+12=28)) # output 16``` and support for complex operation
- Easter egg #1 | session easter egg/memes
#### 0.3.8.73
- New options "-f, --file" in nsh
- Shell description
- Easter egg #2 | Nuts crack animation easter egg/memes

### [*] Fixed
#### 0.3.8.70
- Jobs structure
- Jobs status structure
- Incorrect jobs status bugs
#### 0.3.8.73
- Default variable get overwritted when assigning a variable

### [-] Removed
- Bookmark interactive question to clear

**Date:** 2025-10-4 -> 2025-10-6 **Version:** 0.3.8.73, 0.3.8.76 (and upper)

### [+] Added
- HISTFILE support, you can customize where to save your history files
- HISTSIZE support, you can customize your max size history

### [*] Fixed
- Replaced exit() with exit_shell() in all code that uses
- Fixed builtin exec causes stdin fo freeze
- Fixed builtin exec -c not working properly
- Fixed redirection expecting file descriptor "&" as a file targets
- Weird History void (still testing out, but fixed)
- Fixed fg bugs: Cannot send a signal, stdin freezing, and making the foreground stuck, now fixed (im so happy with this)