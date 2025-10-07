void handle_builtin_pwd(const std::vector<std::string> &tokens) {
  bool physical = false;
  for (size_t i = 1; i < tokens.size(); i++) {
    if (tokens[i] == "-P")
      physical = true;
    else if (tokens[i] == "-L")
      physical = false;
    else if (tokens[i] == "--help") {
      std::cout << "pwd: pwd [-L|-P]\n    Print the name of the current "
                   "working directory.\n\n"
                << "    -L    print the value of $PWD if it names the current "
                   "working directory\n"
                << "    -P    print the physical directory, without any "
                   "symbolic links\n";
      last_exit_code = 0;
      return;
    } else {
      std::cerr << "pwd: invalid option -- '" << tokens[i].substr(1, 1) << "'"
                << std::endl;
      last_exit_code = 1;
      return;
    }
  }

  if (physical) {
    try {
      safe_print(fs::current_path().string());
    } catch (const fs::filesystem_error &e) {
      std::cerr << "pwd: " << e.what() << std::endl;
      last_exit_code = 1;
    }
  } else {
    safe_print(LOGICAL_PWD.string());
  }
  last_exit_code = 0;
}