# 🐚 Nutshell-nsh 2025

---

**Date:** 2025-09-18 → 2025-09-19  
**Version:** 0.3.8.46 → 0.3.8.54  

### 🐞 Bugs
- Escaping backslash on EOF_IN mode **[FIXED]**
- Cannot execute relative path **[FIXED]**
- Pipeline cannot execute builtin command **[FIXED]**
- Assignment variable like `VAR=/value` is considered as a path execution
- Alias assignment error **[FIXED – caused by tokenizer]**
- Bug when `cd` with `CDPATH` **[FIXED]**

### 🚧 To-be-changed / Coming
- Hash output **[CHANGED]**
- Hash hits count **[ADDED]**

### [+] Added
- Hash cache support  
- New builtin `hash`  
- Hash hits counter  
- Better error handling on execution  

### [*] Fixed
- `argv` parsing problems  
- Input auto-done after Ctrl+C  
- Double-quoted expansion couldn’t use command/arithmetic substitution  
- Segmentation fault when using quotes  
- Relative path execution fixed  
- Pipeline builtin execution fixed  
- `cd` with `CDPATH` fixed  
- EOF backslash escaping fixed  

### [>] Changed
- New `README.md`  
- Hash output  
- `CHANGELOG.md` updated  

---

**Date:** 2025-09-18 → 2025-09-23  
**Version:** 0.3.8.46 → 0.3.8.56 → 0.3.8.64  

### [+] Added
- Environment child process support  
- Pipelines like `echo ls | nsh`  
- Bookmark-based directory switching  
- Builtin `type` command  
- History expansion with `!`  

### [*] Fixed
- Alias assignment tokenizer bug  
- Child process signal output glitch  
- EOF interrupt issues  
- One-line variable initialization bug  
- File execution interruption (v2)  

---

**Date:** 2025-09-23 → 2025-10-04  
**Version:** 0.3.8.64 → 0.3.8.73  

### [+] Added
#### v0.3.8.70
- Extended redirection system  
- Cross-session job persistence  
- Puzzle math: `echo $((?+12=28))` → outputs `16` (supports complex ops)  
- Easter Egg #1 — Session memes  
#### v0.3.8.73
- New `-f, --file` options for `nsh`  
- Shell description info  
- Easter Egg #2 — Nuts crack animation  

### [*] Fixed
#### v0.3.8.70
- Job structure issues  
- Job status synchronization bugs  
#### v0.3.8.73
- Default variables overwritten on assignment  

### [-] Removed
- Interactive bookmark clearing question  

---

**Date:** 2025-10-04 → 2025-10-07  
**Version:** 0.3.8.73 → 0.3.8.76 (and upper)  

### [+] Added
- `HISTFILE` support — custom history file path  
- `HISTSIZE` support — configurable history size  

### [*] Fixed
- Replaced all `exit()` calls with `exit_shell()`  
- Fixed builtin `exec` causing stdin freeze  
- Fixed `exec -c` behavior  
- Redirection no longer misinterprets `&` as filename  
- History void bug (still under testing)  
- Foreground control (`fg`) bug fixed — signals, stdin freeze, and hang issues resolved  

---

**Date:** 2025-10-07  
**Version:** 0.3.8.81 (and upper)  

### [*] Fixed
- Expansion unintentionally disabled (now re-enabled)  
- Command-level expansion structure rewritten  
