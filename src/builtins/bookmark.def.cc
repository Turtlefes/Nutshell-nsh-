void handle_builtin_bookmark(const std::vector<std::string> &tokens) {
  const auto usage = []() {
    std::cout << "bookmark: bookmark [name] [path]\n"
              << "    Manage directory bookmarks.\n\n"
              << "    Commands:\n"
              << "      bookmark                 List all bookmarks\n"
              << "      bookmark list            List all bookmarks\n"
              << "      bookmark add NAME PATH   Add a new bookmark\n"
              << "      bookmark remove NAME     Remove a bookmark\n"
              << "      bookmark rename OLD NEW  Rename a bookmark\n"
              << "      bookmark show NAME       Show path for a bookmark\n"
              << "      bookmark clear           Remove all bookmarks\n\n";
  };

  // No arguments or "list" - show all bookmarks
  if (tokens.size() == 1 || (tokens.size() == 2 && tokens[1] == "list")) {
    std::ifstream bookmark_file(ns_BOOKMARK_FILE);
    if (!bookmark_file.is_open()) {
      std::cout << "bookmark: no bookmarks defined\n";
      last_exit_code = 0;
      return;
    }

    std::string line;
    int count = 0;

    while (std::getline(bookmark_file, line)) {
      size_t space_pos = line.find(' ');
      if (space_pos != std::string::npos) {
        std::string name = line.substr(0, space_pos);
        std::string path = line.substr(space_pos + 1);

        // Check if path exists and is accessible
        bool path_exists = fs::exists(path) && fs::is_directory(path);
        std::string status = path_exists ? "" : " (invalid)";

        std::cout << name << " -> " << path << status << "\n";
        count++;
      }
    }
    bookmark_file.close();

    if (count == 0) {
      std::cout << "bookmark: no bookmarks defined\n";
    }

    last_exit_code = 0;
    return;
  }

  // Quick add: bookmark NAME PATH
  if (tokens.size() == 3 && tokens[1] != "add" && tokens[1] != "remove" &&
      tokens[1] != "rename" && tokens[1] != "show" && tokens[1] != "clear") {
    std::string name = tokens[1];
    std::string path = tokens[2];

    // Validate bookmark name
    if (name.empty() || name.find(' ') != std::string::npos) {
      std::cerr << "bookmark: name cannot be empty or contain spaces\n";
      last_exit_code = 1;
      return;
    }

    // Check if bookmark already exists
    std::ifstream check_file(ns_BOOKMARK_FILE);
    if (check_file.is_open()) {
      std::string line;
      while (std::getline(check_file, line)) {
        if (line.rfind(name + " ", 0) == 0) {
          std::cerr << "bookmark: '" << name << "' already exists\n";
          check_file.close();
          last_exit_code = 1;
          return;
        }
      }
      check_file.close();
    }

    // Add the bookmark
    std::ofstream bookmark_file(ns_BOOKMARK_FILE, std::ios::app);
    if (bookmark_file.is_open()) {
      std::string abs_path = fs::absolute(expand_tilde(path)).string();
      bookmark_file << name << " " << abs_path << "\n";
      std::cout << "bookmark: added '" << name << "' -> " << abs_path << "\n";
      last_exit_code = 0;
    } else {
      std::cerr << "bookmark: could not open bookmark file\n";
      last_exit_code = 1;
    }
    return;
  }

  // Handle specific commands
  if (tokens[1] == "add") {
    if (tokens.size() != 4) {
      std::cerr << "bookmark: usage: bookmark add <name> <path>\n";
      last_exit_code = 1;
      return;
    }

    std::string name = tokens[2];
    std::string path = tokens[3];

    // Validate bookmark name
    if (name.empty() || name.find(' ') != std::string::npos) {
      std::cerr << "bookmark: name cannot be empty or contain spaces\n";
      last_exit_code = 1;
      return;
    }

    // Check if bookmark already exists
    std::ifstream check_file(ns_BOOKMARK_FILE);
    if (check_file.is_open()) {
      std::string line;
      while (std::getline(check_file, line)) {
        if (line.rfind(name + " ", 0) == 0) {
          std::cerr << "bookmark: '" << name << "' already exists\n";
          check_file.close();
          last_exit_code = 1;
          return;
        }
      }
      check_file.close();
    }

    // Add the bookmark
    std::ofstream bookmark_file(ns_BOOKMARK_FILE, std::ios::app);
    if (bookmark_file.is_open()) {
      std::string abs_path = fs::absolute(expand_tilde(path)).string();
      bookmark_file << name << " " << abs_path << "\n";
      std::cout << "bookmark: added '" << name << "' -> " << abs_path << "\n";
      last_exit_code = 0;
    } else {
      std::cerr << "bookmark: could not open bookmark file\n";
      last_exit_code = 1;
    }
  } else if (tokens[1] == "remove") {
    if (tokens.size() != 3) {
      std::cerr << "bookmark: usage: bookmark remove <name>\n";
      last_exit_code = 1;
      return;
    }

    std::string name = tokens[2];
    fs::path tmp_path = ns_BOOKMARK_FILE.string() + ".tmp";
    std::ifstream in_file(ns_BOOKMARK_FILE);
    std::ofstream out_file(tmp_path);
    std::string line;
    bool found = false;

    if (!in_file.is_open()) {
      std::cerr << "bookmark: no bookmarks to remove\n";
      last_exit_code = 1;
      return;
    }

    while (std::getline(in_file, line)) {
      if (line.rfind(name + " ", 0) != 0)
        out_file << line << "\n";
      else
        found = true;
    }
    in_file.close();
    out_file.close();

    if (found) {
      fs::rename(tmp_path, ns_BOOKMARK_FILE);
      std::cout << "bookmark: removed '" << name << "'\n";
      last_exit_code = 0;
    } else {
      fs::remove(tmp_path);
      std::cerr << "bookmark: '" << name << "' not found\n";
      last_exit_code = 1;
    }
  } else if (tokens[1] == "rename") {
    if (tokens.size() != 4) {
      std::cerr << "bookmark: usage: bookmark rename <old-name> <new-name>\n";
      last_exit_code = 1;
      return;
    }

    std::string old_name = tokens[2];
    std::string new_name = tokens[3];

    // Validate new name
    if (new_name.empty() || new_name.find(' ') != std::string::npos) {
      std::cerr << "bookmark: new name cannot be empty or contain spaces\n";
      last_exit_code = 1;
      return;
    }

    fs::path tmp_path = ns_BOOKMARK_FILE.string() + ".tmp";
    std::ifstream in_file(ns_BOOKMARK_FILE);
    std::ofstream out_file(tmp_path);
    std::string line;
    bool found = false;
    bool new_name_exists = false;

    if (!in_file.is_open()) {
      std::cerr << "bookmark: no bookmarks to rename\n";
      last_exit_code = 1;
      return;
    }

    // Check if new name already exists
    while (std::getline(in_file, line)) {
      if (line.rfind(new_name + " ", 0) == 0) {
        new_name_exists = true;
        break;
      }
    }
    in_file.clear();
    in_file.seekg(0);

    if (new_name_exists) {
      std::cerr << "bookmark: '" << new_name << "' already exists\n";
      in_file.close();
      fs::remove(tmp_path);
      last_exit_code = 1;
      return;
    }

    // Perform rename
    while (std::getline(in_file, line)) {
      if (line.rfind(old_name + " ", 0) == 0) {
        out_file << new_name << " " << line.substr(old_name.length() + 1)
                 << "\n";
        found = true;
      } else {
        out_file << line << "\n";
      }
    }
    in_file.close();
    out_file.close();

    if (found) {
      fs::rename(tmp_path, ns_BOOKMARK_FILE);
      std::cout << "bookmark: renamed '" << old_name << "' to '" << new_name
                << "'\n";
      last_exit_code = 0;
    } else {
      fs::remove(tmp_path);
      std::cerr << "bookmark: '" << old_name << "' not found\n";
      last_exit_code = 1;
    }
  } else if (tokens[1] == "show") {
    if (tokens.size() != 3) {
      std::cerr << "bookmark: usage: bookmark show <name>\n";
      last_exit_code = 1;
      return;
    }

    std::string name = tokens[2];
    std::ifstream bookmark_file(ns_BOOKMARK_FILE);
    bool found = false;

    if (bookmark_file.is_open()) {
      std::string line;
      while (std::getline(bookmark_file, line)) {
        if (line.rfind(name + " ", 0) == 0) {
          std::string path = line.substr(name.length() + 1);
          std::cout << path << "\n";
          found = true;
          break;
        }
      }
      bookmark_file.close();
    }

    if (!found) {
      std::cerr << "bookmark: '" << name << "' not found\n";
      last_exit_code = 1;
    } else {
      last_exit_code = 0;
    }
  } else if (tokens[1] == "clear") {
    if (tokens.size() != 2) {
      std::cerr << "bookmark: usage: bookmark clear\n";
      last_exit_code = 1;
      return;
    }

    std::ofstream bookmark_file(ns_BOOKMARK_FILE, std::ios::trunc);
    if (bookmark_file.is_open()) {
      std::cout << "bookmark: all bookmarks cleared\n";
      last_exit_code = 0;
    } else {
      std::cerr << "bookmark: could not clear bookmarks\n";
      last_exit_code = 1;
    }
    last_exit_code = 0;
  } else if (tokens[1] == "help") {
    usage();
    last_exit_code = 0;
  } else {
    std::cerr << "bookmark: unknown command: " << tokens[1] << "\n";
    usage();
    last_exit_code = 1;
  }
}