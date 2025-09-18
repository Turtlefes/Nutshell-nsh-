void handle_builtin_hash(const std::vector<std::string> &tokens)
{
    if (tokens.size() > 1 && (tokens[1] == "--help" || tokens[1] == "-h"))
    {
        std::cout << "hash: hash [-lr] [-p pathname] [-dt] [name ...]\n"
                  << "    Remember or display program locations.\n\n"
                  << "    Determine and remember the full pathname of each command NAME.  If\n"
                  << "    no arguments are given, information about remembered commands is displayed.\n\n"
                  << "    Options:\n"
                  << "      -d        forget the remembered location of each NAME\n"
                  << "      -l        display in a format that may be reused as input\n"
                  << "      -p pathname       use PATHNAME as the full pathname of NAME\n"
                  << "      -r        forget all remembered locations\n"
                  << "      -t        print the remembered location of each NAME, preceding\n"
                  << "                each location with the corresponding NAME if multiple\n"
                  << "                NAMEs are given\n"
                  << "    Arguments:\n"
                  << "      NAME      Each NAME is searched for in $PATH and added to the list\n"
                  << "                of remembered commands.\n\n"
                  << "    Exit Status:\n"
                  << "    Returns success unless NAME is not found or an invalid option is given.\n";
        last_exit_code = 0;
        return;
    }

    // Parse options
    bool list_format = false;
    bool forget_location = false;
    bool forget_all = false;
    bool terse_format = false;
    std::string custom_path;
    std::vector<std::string> names;
    bool parsing_options = true;

    for (size_t i = 1; i < tokens.size(); ++i)
    {
        const std::string &token = tokens[i];

        if (parsing_options && token.size() > 1 && token[0] == '-')
        {
            if (token == "--")
            {
                parsing_options = false;
                continue;
            }
            
            for (size_t j = 1; j < token.size(); ++j)
            {
                switch (token[j])
                {
                    case 'l': list_format = true; break;
                    case 'd': forget_location = true; break;
                    case 'r': forget_all = true; break;
                    case 't': terse_format = true; break;
                    case 'p': 
                        if (j + 1 < token.size())
                        {
                            // -p followed by path in same token (-ppath)
                            custom_path = token.substr(j + 1);
                            j = token.size(); // break out of inner loop
                        }
                        else if (i + 1 < tokens.size())
                        {
                            custom_path = tokens[++i];
                        }
                        else
                        {
                            std::cerr << "hash: -p requires a pathname argument" << std::endl;
                            last_exit_code = 1;
                            return;
                        }
                        break;
                    default:
                        std::cerr << "hash: invalid option: -" << token[j] << std::endl;
                        std::cerr << "hash: use 'hash --help' for more information." << std::endl;
                        last_exit_code = 1;
                        return;
                }
            }
        }
        else
        {
            parsing_options = false;
            names.push_back(token);
        }
    }

    // Handle -r option (forget all remembered locations)
    if (forget_all)
    {
        binary_hash_loc.clear();
        std::cout << "hash: hash table emptied" << std::endl;
        last_exit_code = 0;
        return;
    }

    // Handle -d option (forget specific locations)
    if (forget_location)
    {
        if (names.empty())
        {
            std::cerr << "hash: -d requires at least one name argument" << std::endl;
            last_exit_code = 1;
            return;
        }

        bool all_found = true;
        for (const auto &name : names)
        {
            auto it = binary_hash_loc.find(name);
            if (it != binary_hash_loc.end())
            {
                binary_hash_loc.erase(it);
                std::cout << "hash: " << name << ": removed from hash table" << std::endl;
            }
            else
            {
                std::cerr << "hash: " << name << ": not found in hash table" << std::endl;
                all_found = false;
            }
        }
        last_exit_code = all_found ? 0 : 1;
        return;
    }

    // Handle -p option (add custom path)
    if (!custom_path.empty())
    {
        if (names.size() != 1)
        {
            std::cerr << "hash: -p requires exactly one name argument" << std::endl;
            last_exit_code = 1;
            return;
        }

        // Validate the custom path
        fs::path path(custom_path);
        if (!fs::exists(path) || !fs::is_regular_file(path))
        {
            std::cerr << "hash: " << custom_path << ": invalid path" << std::endl;
            last_exit_code = 1;
            return;
        }

        if (access(path.c_str(), X_OK) != 0)
        {
            std::cerr << "hash: " << custom_path << ": permission denied" << std::endl;
            last_exit_code = 1;
            return;
        }

        std::string abs_path = fs::absolute(path).string();
        binary_hash_loc[names[0]] = abs_path;
        std::cout << "hash: " << names[0] << " = " << abs_path << std::endl;
        last_exit_code = 0;
        return;
    }

    // If no names provided, display all remembered commands
    if (names.empty())
    {
        if (binary_hash_loc.empty())
        {
            std::cout << "hash: hash table empty" << std::endl;
            last_exit_code = 0;
            return;
        }

        if (list_format)
        {
            for (const auto &[cmd, path] : binary_hash_loc)
            {
                std::cout << "builtin hash -p " << path << " " << cmd << std::endl;
            }
        }
        else if (terse_format)
        {
            for (const auto &[cmd, path] : binary_hash_loc)
            {
                std::cout << path << std::endl;
            }
        }
        else
        {
            // Simple format: command = path
            for (const auto &[cmd, path] : binary_hash_loc)
            {
                std::cout << cmd << " = " << path << std::endl;
            }
        }
        last_exit_code = 0;
        return;
    }

    // Handle names provided - search for them and add to hash table
    bool all_found = true;

    for (const auto &name : names)
    {
        // Skip if already in hash table (unless we're using terse format)
        if (binary_hash_loc.find(name) != binary_hash_loc.end())
        {
            if (terse_format)
            {
                std::cout << name << "\t" << binary_hash_loc[name] << std::endl;
            }
            else
            {
                std::cout << name << " = " << binary_hash_loc[name] << std::endl;
            }
            continue;
        }

        // Search for the command in PATH
        std::string binary_path = find_binary(name);
        if (!binary_path.empty())
        {
            // Pastikan path disimpan di hash table
            binary_hash_loc[name] = binary_path;
            if (terse_format)
            {
                std::cout << name << "\t" << binary_path << std::endl;
            }
            else
            {
                std::cout << name << " = " << binary_path << std::endl;
            }
        }
        else
        {
            std::cerr << "hash: " << name << ": not found" << std::endl;
            all_found = false;
        }
    }

    last_exit_code = all_found ? 0 : 1;
}