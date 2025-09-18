// [1102]

#include "builtins.h"
#include "globals.h"
#include "utils.h"
#include "expansion.h"
#include "init.h"
#include "parser.h"
#include "execution.h"

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


#include "../builtins/cd_impl.cpp"
#include "../builtins/pwd_impl.cpp"
#include "../builtins/alias_impl.cpp"
#include "../builtins/unalias_impl.cpp"
#include "../builtins/export_impl.cpp"
#include "../builtins/bookmark_impl.cpp"
#include "../builtins/history_impl.cpp"
#include "../builtins/exec_impl.cpp"
#include "../builtins/unset_impl.cpp"
#include "../builtins/hash_impl.cpp"