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


#include "../builtins/cd_impl.cc"
#include "../builtins/pwd_impl.cc"
#include "../builtins/alias_impl.cc"
#include "../builtins/unalias_impl.cc"
#include "../builtins/export_impl.cc"
#include "../builtins/bookmark_impl.cc"
#include "../builtins/history_impl.cc"
#include "../builtins/exec_impl.cc"
#include "../builtins/unset_impl.cc"
#include "../builtins/hash_impl.cc"
#include "../builtins/jobspec_impl.cc"