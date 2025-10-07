void handle_builtin_export(const std::vector<std::string> &tokens) {
  if (tokens.size() > 1 && (tokens[1] == "--help" || tokens[1] == "-h")) {
    std::cout
        << "export: export [-fn] [name[=value] ...] or export -p\n"
        << "    Set export attribute for shell variables.\n\n"
        << "    Marks each NAME for automatic export to the environment of\n"
        << "    subsequently executed commands.  If VALUE is supplied, assign\n"
        << "    VALUE before exporting.\n\n"
        << "    Options:\n"
        << "      -f    refer to shell functions\n"
        << "      -n    remove the export property from each NAME\n"
        << "      -p    display a list of all exported variables and "
           "functions\n\n"
        << "    Exit Status:\n"
        << "    Returns success unless an invalid option is given or NAME is "
           "invalid.\n";
    last_exit_code = 0;
    return;
  }

  bool remove_export = false;
  bool show_exported = false;
  bool function_flag = false;
  std::vector<std::string> names_to_export;
  std::vector<std::string> names_to_unexport;

  for (size_t i = 1; i < tokens.size(); ++i) {
    const std::string &token = tokens[i];

    if (token == "-n") {
      remove_export = true;
    } else if (token == "-p") {
      show_exported = true;
    } else if (token == "-f") {
      function_flag = true;
      std::cerr << "nsh: export: -f: shell functions not yet implemented"
                << std::endl;
      last_exit_code = 1;
      return;
    } else if (token == "--") {
      for (size_t j = i + 1; j < tokens.size(); ++j) {
        if (remove_export) {
          names_to_unexport.push_back(tokens[j]);
        } else {
          names_to_export.push_back(tokens[j]);
        }
      }
      break;
    } else if (token[0] == '-') {
      std::cerr << "nsh: export: invalid option: " << token << std::endl;
      std::cerr << "export: usage: export [-fn] [name[=value] ...] or export -p"
                << std::endl;
      last_exit_code = 2;
      return;
    } else {
      if (remove_export) {
        names_to_unexport.push_back(token);
      } else {
        names_to_export.push_back(token);
      }
    }
  }

  if (show_exported) {
    for (char **env = environ; *env; ++env) {
      std::cout << "export " << *env << std::endl;
    }
    last_exit_code = 0;
    return;
  }

  if (remove_export) {
    for (const auto &name : names_to_unexport) {
      if (getenv(name.c_str())) {
        unsetenv(name.c_str());
      } else {
        std::cerr << "nsh: export: " << name << ": not an exported variable"
                  << std::endl;
        last_exit_code = 1;
      }
    }
    last_exit_code = 0;
    return;
  }

  if (names_to_export.empty()) {
    for (char **env = environ; *env; ++env) {
      std::cout << "export " << *env << std::endl;
    }
    last_exit_code = 0;
    return;
  }

  for (const auto &token : names_to_export) {
    size_t eq_pos = token.find('=');

    if (eq_pos != std::string::npos) {
      auto [var_name, value] = parse_env_assignment(token);
      set_env_var(var_name, value, true);
    } else {
      const char *value = getenv(token.c_str());
      if (value) {
        set_env_var(token, value, true);
      } else {
        std::cerr << "nsh: export: " << token << " variable not found"
                  << std::endl;
        last_exit_code = 1;
      }
    }
  }
  last_exit_code = 0;
}
