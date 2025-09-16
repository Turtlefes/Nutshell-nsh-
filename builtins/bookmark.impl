void handle_builtin_bookmark(const std::vector<std::string> &tokens)
{
    if (tokens.size() == 1 || (tokens.size() > 1 && tokens[1] == "list"))
    {
        std::ifstream bookmark_file(ns_BOOKMARK_FILE);
        if (bookmark_file.is_open())
        {
            std::string line;
            while (std::getline(bookmark_file, line))
            {
                size_t space_pos = line.find(' ');
                if (space_pos != std::string::npos)
                {
                    std::cout << std::setw(15) << std::left << line.substr(0, space_pos) << " -> " << line.substr(space_pos + 1) << std::endl;
                }
            }
        }
        last_exit_code = 0;
        return;
    }

    const auto usage = []() {
        std::cerr << "Usage: bookmark <command> [args]\n"
                  << "Commands:\n"
                  << "  list              List all bookmarks\n"
                  << "  add <name> <path> Add a new bookmark\n"
                  << "  remove <name>     Remove a bookmark\n";
    };

    if (tokens[1] == "add")
    {
        if (tokens.size() != 4)
        {
            usage();
            last_exit_code = 1;
            return;
        }
        std::ofstream bookmark_file(ns_BOOKMARK_FILE, std::ios::app);
        if (bookmark_file.is_open())
        {
            bookmark_file << tokens[2] << " " << fs::absolute(expand_tilde(tokens[3])).string() << std::endl;
            std::cout << "Bookmark '" << tokens[2] << "' added." << std::endl;
        }
        last_exit_code = 0;
    }
    else if (tokens[1] == "remove")
    {
        if (tokens.size() != 3)
        {
            usage();
            last_exit_code = 1;
            return;
        }
        fs::path tmp_path = ns_BOOKMARK_FILE.string() + ".tmp";
        std::ifstream in_file(ns_BOOKMARK_FILE);
        std::ofstream out_file(tmp_path);
        std::string line;
        bool found = false;

        if (!in_file.is_open())
        {
            std::cerr << "nsh: no bookmarks to remove" << std::endl;
            last_exit_code = 1;
            return;
        }

        while (std::getline(in_file, line))
        {
            if (line.rfind(tokens[2] + " ", 0) != 0)
                out_file << line << std::endl;
            else
                found = true;
        }
        in_file.close();
        out_file.close();

        if (found)
        {
            fs::rename(tmp_path, ns_BOOKMARK_FILE);
            std::cout << "Bookmark '" << tokens[2] << "' removed." << std::endl;
        }
        else
        {
            fs::remove(tmp_path);
            std::cerr << "Bookmark not found: " << tokens[2] << std::endl;
            last_exit_code = 1;
        }
    }
    else
    {
        usage();
        last_exit_code = 1;
    }
}