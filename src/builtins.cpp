#include "builtins.h"
#include "globals.h"
#include "utils.h"
#include "expansion.h"
#include "init.h"
#include "parser.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <cstdlib>

#include <sys/wait.h>
#include <cstring>
#include <cerrno>
#include <algorithm>

#include <readline/history.h>


#include "../builtins/cd.impl"
#include "../builtins/pwd.impl"
#include "../builtins/alias.impl"
#include "../builtins/unalias.impl"
#include "../builtins/export.impl"
#include "../builtins/bookmark.impl"
#include "../builtins/history.impl"
#include "../builtins/exec.impl"
#include "../builtins/unset.impl"