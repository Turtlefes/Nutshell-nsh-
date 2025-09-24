# Nutshell-nsh

**Date:** 2025-09-18 -> 2025-09-19 **Version:** 0.3.8.46 -> 0.3.8.54

### bugs
- Escaping backslash on EOF_IN mode [FIXED]
- Cannot execute relative path [FIXED]
- Pipeline cannot execute builtin command [FIXED]
- Assignment variable like ```VAR=/value``` is considered as a path execution
- Alias Assignment
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



**Date:** 2025-09-18 -> 2025-09-19 **Version:** 0.3.8.46 -> 0.3.8.56

### [+] Added
- Environment child process