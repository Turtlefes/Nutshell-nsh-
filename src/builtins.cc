// [110102]

#include "builtins.h"
#include "globals.h"
#include "utils.h"
#include "expansion.h"
#include "init.h"
#include "parser.h"
#include "execution.h"
#include "signals.h"
#include "terminal.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <unordered_map>

#include <sys/wait.h>
#include <cstring>
#include <cerrno>
#include <algorithm>

#include <readline/history.h>


#include "builtins/cd.def.cc"
#include "builtins/pwd.def.cc"
#include "builtins/alias.def.cc"
#include "builtins/unalias.def.cc"
#include "builtins/export.def.cc"
#include "builtins/bookmark.def.cc"
#include "builtins/history.def.cc"
#include "builtins/exec.def.cc"
#include "builtins/unset.def.cc"
#include "builtins/hash.def.cc"
#include "builtins/jobspec.def.cc"