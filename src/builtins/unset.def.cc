void handle_builtin_unset(const std::vector<std::string> &tokens) {
  if (tokens.size() == 1 ||
      (tokens.size() > 1 && (tokens[1] == "--help" || tokens[1] == "-h"))) {
    std::cout << "unset: unset [-f] [-v] [-n] [name ...]\n"
              << "    Unset values and attributes of shell variables and "
                 "functions.\n\n"
              << "    For each NAME, remove the corresponding variable or "
                 "function.\n\n"
              << "    Options:\n"
              << "      -f    treat each NAME as a shell function\n"
              << "      -v    treat each NAME as a shell variable\n"
              << "      -n    treat each NAME as a name reference and unset "
                 "the variable itself\n"
              << "            rather than the variable it references\n\n"
              << "    Without options, unset first tries to unset a variable, "
                 "and if that fails,\n"
              << "    tries to unset a function.\n\n"
              << "    Some variables cannot be unset:\n"
              << "      BASHOPTS   IFS        PPID       SHELLOPTS  UID\n"
              << "      EUID       OPTARG     SECONDS    TERM\n"
              << "      FUNCNAME   OPTIND     SHELL      TIMEFORMAT\n"
              << "      GROUPS     OSTYPE     SHLVL      USER\n"
              << "      HISTCMD    PIPESTATUS SUDO_GID   USERNAME\n"
              << "      HOSTNAME   POSIXLY_CORRECT SUDO_UID\n"
              << "    Exit Status:\n"
              << "    Returns success unless an invalid option is given or a "
                 "NAME is read-only.\n";
    last_exit_code = 0;
    return;
  }

  bool unset_function = false;
  bool unset_variable = false;
  bool name_reference = false;
  std::vector<std::string> names_to_unset;

  // Daftar variabel yang tidak boleh di-unset (read-only)
  const std::vector<std::string> readonly_variables = {
      "BASHOPTS",        "EUID",     "FUNCNAME",   "GROUPS",
      "HISTCMD",         "HOSTNAME", "IFS",        "OPTARG",
      "OPTIND",          "OSTYPE",   "PIPESTATUS", "PPID",
      "POSIXLY_CORRECT", "SECONDS",  "SHELL",      "SHELLOPTS",
      "SHLVL",           "SUDO_GID", "SUDO_UID",   "TERM",
      "TIMEFORMAT",      "UID",      "USER",       "USERNAME"};

  // Parse options
  size_t i = 1;
  while (i < tokens.size()) {
    const std::string &token = tokens[i];

    if (token == "-f") {
      unset_function = true;
      i++;
    } else if (token == "-v") {
      unset_variable = true;
      i++;
    } else if (token == "-n") {
      name_reference = true;
      i++;
    } else if (token == "--") {
      i++;
      break;
    } else if (token[0] == '-') {
      std::cerr << "nsh: unset: invalid option: " << token << std::endl;
      std::cerr << "unset: usage: unset [-f] [-v] [-n] [name ...]" << std::endl;
      last_exit_code = 2;
      return;
    } else {
      break;
    }
  }

  // Kumpulkan nama-nama yang akan di-unset
  for (; i < tokens.size(); ++i) {
    names_to_unset.push_back(tokens[i]);
  }

  if (names_to_unset.empty()) {
    std::cerr << "nsh: unset: not enough arguments" << std::endl;
    std::cerr << "unset: usage: unset [-f] [-v] [-n] [name ...]" << std::endl;
    last_exit_code = 1;
    return;
  }

  // Validasi konflik opsi
  if ((unset_function && unset_variable) ||
      (name_reference && unset_function)) {
    std::cerr << "nsh: unset: cannot combine -f with -v or -n" << std::endl;
    last_exit_code = 2;
    return;
  }

  last_exit_code = 0;

  for (const auto &name : names_to_unset) {
    // Cek jika variabel read-only
    if (std::find(readonly_variables.begin(), readonly_variables.end(), name) !=
        readonly_variables.end()) {
      std::cerr << "nsh: unset: " << name << ": cannot unset: readonly variable"
                << std::endl;
      last_exit_code = 1;
      continue;
    }

    if (unset_function) {
      // Unset function
      // TODO: Implement fungsi shell jika ada
      std::cerr << "nsh: unset: -f: shell functions not yet implemented"
                << std::endl;
      last_exit_code = 1;
    } else if (unset_variable) {
      // Unset variable saja
      if (get_env_var(name) != nullptr) {
        unset_env_var(name);
      } else {
        std::cerr << "nsh: unset: " << name << ": cannot unset: not a variable"
                  << std::endl;
        last_exit_code = 1;
      }
    } else if (name_reference) {
      // Unset name reference
      // TODO: Implement name references jika ada
      std::cerr << "nsh: unset: -n: name references not yet implemented"
                << std::endl;
      last_exit_code = 1;
    } else {
      // Default: coba unset variable, lalu function, lalu alias
      bool unset_success = false;

      // Coba unset environment variable
      if (get_env_var(name) != nullptr) {
        unset_env_var(name);
        unset_success = true;
      }

      // Coba unset alias
      if (aliases.erase(name) > 0) {
        save_aliases();
        unset_success = true;
      }

      // TODO: Coba unset function jika ada

      if (!unset_success) {
        std::cerr << "nsh: unset: " << name << ": cannot unset: not found"
                  << std::endl;
        last_exit_code = 1;
      }
    }
  }
}