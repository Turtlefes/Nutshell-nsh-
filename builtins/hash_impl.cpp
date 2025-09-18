#include <iomanip>
#include <algorithm>
#include <vector>
#include <map>
#include <iostream>
#include <filesystem>
void handle_builtin_hash(const std::vector<std::string> &tokens)
{
    if (tokens.size() > 1 && (tokens[1] == "--help" || tokens[1] == "-h"))
    {
        std::cout << "hash: hash [-lr] [-p pathname] [-dt] [name ...]\n"
                  << "    Remember or display program locations.\n\n"
                  << "    Determine and remember the full pathname of each command NAME.  If\n"
                  << "    no arguments are given, information about remembered commands is displayed.\n\n"
                  << "    Options:\n"
                  << "      -d\t\tforget the remembered location of each NAME\n"
                  << "      -l\t\tdisplay in a format that may be reused as input\n"
                  << "      -p pathname\tuse PATHNAME as the full pathname of NAME\n"
                  << "      -r\t\tforget all remembered locations\n"
                  << "      -t\t\tprint the remembered location of each NAME, preceding\n"
                  << "\t\t\teach location with the corresponding NAME if multiple\n"
                  << "\t\t\tNAMEs are given\n"
                  << "      -s\t\tshow summary information (total commands and hits)\n"
                  << "      -v\t\tverbose output\n"
                  << "      --help\tshow this help message\n\n"
                  << "    Arguments:\n"
                  << "      NAME\tEach NAME is searched for in $PATH and added to the list\n"
                  << "\t\tof remembered commands.\n\n"
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
    bool verbose = false;
    bool show_summary = false;
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
            
            if (token == "-v") {
                verbose = true;
                continue;
            }
            if (token == "-s") {
                show_summary = true;
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
                    case 'v': verbose = true; break;
                    case 's': show_summary = true; break;
                    case 'p': 
                        if (j + 1 < token.size())
                        {
                            custom_path = token.substr(j + 1);
                            j = token.size();
                        }
                        else if (i + 1 < tokens.size())
                        {
                            custom_path = tokens[++i];
                        }
                        else
                        {
                            std::cerr << "hash: -p: requires pathname argument" << std::endl;
                            last_exit_code = 1;
                            return;
                        }
                        break;
                    default:
                        std::cerr << "hash: " << token << ": invalid option" << std::endl;
                        std::cerr << "hash: Try 'hash --help' for more information." << std::endl;
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
        if (verbose) {
            std::cout << "hash: hash table emptied (" << binary_hash_loc.size() << " entries removed)" << std::endl;
        } else {
            std::cout << "hash: hash table emptied" << std::endl;
        }
        binary_hash_loc.clear();
        last_exit_code = 0;
        return;
    }

    // Handle -d option (forget specific locations)
    if (forget_location)
    {
        if (names.empty())
        {
            std::cerr << "hash: -d: requires name argument" << std::endl;
            last_exit_code = 1;
            return;
        }

        bool all_found = true;
        for (const auto &name : names)
        {
            auto it = binary_hash_loc.find(name);
            if (it != binary_hash_loc.end())
            {
                if (verbose) {
                    std::cout << "hash: " << name << ": removed from hash table (was: " 
                              << it->second.path << ")" << std::endl;
                } else {
                    std::cout << "hash: " << name << ": removed from hash table" << std::endl;
                }
                binary_hash_loc.erase(it);
            }
            else
            {
                std::cerr << "hash: " << name << ": not found" << std::endl;
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
            std::cerr << "hash: -p: requires exactly one name argument" << std::endl;
            last_exit_code = 1;
            return;
        }

        fs::path path(custom_path);
        if (!fs::exists(path))
        {
            std::cerr << "hash: " << custom_path << ": no such file or directory" << std::endl;
            last_exit_code = 1;
            return;
        }

        if (!fs::is_regular_file(path))
        {
            std::cerr << "hash: " << custom_path << ": not a regular file" << std::endl;
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
        binary_hash_loc[names[0]] = {abs_path, names[0], 0};
        
        if (verbose) {
            std::cout << "hash: added " << names[0] << " = " << abs_path << " (custom path)" << std::endl;
        } else {
            std::cout << names[0] << " = " << abs_path << std::endl;
        }
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
            for (const auto &[cmd, info] : binary_hash_loc)
            {
                std::cout << "hash -p " << info.path << " " << cmd << std::endl;
            }
        }
        else if (terse_format)
        {
            for (const auto &[cmd, info] : binary_hash_loc)
            {
                std::cout << cmd << "\t" << info.path << std::endl;
            }
        }
        else
        {
            // Format seperti bash
            std::cout << "hits\tcommand" << std::endl;
            
            std::vector<std::pair<std::string, binary_hash_info>> sorted_entries;
            for (const auto &entry : binary_hash_loc)
            {
                sorted_entries.push_back(entry);
            }
            
            // Sort by hits descending, then by command name
            std::sort(sorted_entries.begin(), sorted_entries.end(),
                [](const auto &a, const auto &b) {
                    if (a.second.hits != b.second.hits) {
                        return a.second.hits > b.second.hits;
                    }
                    return a.first < b.first;
                });
            
            for (const auto &[cmd, info] : sorted_entries)
            {
                std::cout << std::setw(4) << info.hits << "\t" << info.path << std::endl;
            }
        }

        // Show summary jika diminta atau jika tidak ada format khusus
        if (show_summary || (!list_format && !terse_format))
        {
            size_t total_hits = 0;
            for (const auto &[cmd, info] : binary_hash_loc)
            {
                total_hits += info.hits;
            }
            std::cout << binary_hash_loc.size() << " command(s), " << total_hits << " total hit(s)" << std::endl;
        }

        last_exit_code = 0;
        return;
    }

    // Handle names provided - search for them and add to hash table
    bool all_found = true;
    int added_count = 0;
    int found_count = 0;

    for (const auto &name : names)
    {
        auto it = binary_hash_loc.find(name);
        if (it != binary_hash_loc.end())
        {
            found_count++;
            if (terse_format)
            {
                std::cout << name << "\t" << it->second.path << std::endl;
            }
            else if (verbose)
            {
                std::cout << "hash: found " << name << " = " << it->second.path 
                          << " (hits: " << it->second.hits << ")" << std::endl;
            }
            continue;
        }

        // Search for the command in PATH
        std::string binary_path = find_binary(name);
        if (!binary_path.empty())
        {
            binary_hash_loc[name] = {binary_path, name, 0};
            added_count++;
            
            if (terse_format)
            {
                std::cout << name << "\t" << binary_path << std::endl;
            }
            else if (verbose)
            {
                std::cout << "hash: added " << name << " = " << binary_path << std::endl;
            }
            else
            {
                std::cout << name << " = " << binary_path << std::endl;
            }
        }
        else
        {
            std::cerr << "hash: " << name << ": command not found" << std::endl;
            all_found = false;
        }
    }

    // Show operation summary in verbose mode
    if (verbose && !names.empty())
    {
        if (added_count > 0) {
            std::cout << "hash: added " << added_count << " command(s) to hash table" << std::endl;
        }
        if (found_count > 0) {
            std::cout << "hash: found " << found_count << " command(s) already in hash table" << std::endl;
        }
    }

    last_exit_code = all_found ? 0 : 1;
}