void load_history_append()
{
    std::ifstream history_file(ns_HISTORY_FILE);
    if (history_file.is_open())
    {
        std::string line;
        while (std::getline(history_file, line))
        {
            if (std::find(command_history.begin(), command_history.end(), line) == command_history.end())
            {
                command_history.push_back(line);
            }
        }
        history_index = command_history.size();
    }
}

void load_history_replace()
{
    command_history.clear();
    std::ifstream history_file(ns_HISTORY_FILE);
    if (history_file.is_open())
    {
        std::string line;
        while (std::getline(history_file, line))
        {
            command_history.push_back(line);
        }
        history_index = command_history.size();
    }
}

void save_history_append()
{
    std::ofstream history_file(ns_HISTORY_FILE, std::ios::app);
    if (history_file.is_open())
    {
        for (const auto &cmd : command_history)
        {
            history_file << cmd << std::endl;
        }
    }
}

void save_history_replace()
{
    save_history();
}

void handle_builtin_history(const std::vector<std::string> &tokens)
{
    if (tokens.size() > 1 && (tokens[1] == "--help" || tokens[1] == "-h"))
    {
        std::cout << "history: history [-c] [-d offset] [n] or history -anrw [filename] or history -ps arg [arg...]\n"
                  << "    Display or manipulate the history list.\n\n"
                  << "    Options:\n"
                  << "      -c        clear the history list by deleting all of the entries\n"
                  << "      -d offset delete the history entry at position OFFSET. Negative\n"
                  << "                offsets count back from the end of the history list\n"
                  << "      -a        append history lines from this session to the history file\n"
                  << "      -n        read all history lines not already read from the history file\n"
                  << "                and append them to the history list\n"
                  << "      -r        read the history file and append the contents to the history\n"
                  << "                list\n"
                  << "      -w        write the current history to the history file\n"
                  << "      -p        perform history expansion on each ARG and display the result\n"
                  << "                without storing it in the history list\n"
                  << "      -s        append the ARGs to the history list as a single entry\n\n"
                  << "    If FILENAME is given, it is used as the history file. Otherwise\n"
                  << "    if $HISTFILE has a value, that is used, else ~/.nsh/history.\n\n"
                  << "    Exit Status:\n"
                  << "    Returns success unless an invalid option is given or an error occurs.\n";
        last_exit_code = 0;
        return;
    }

    bool clear_history = false;
    bool delete_entry = false;
    int delete_offset = 0;
    bool append_to_file = false;
    bool read_new = false;
    bool read_file = false;
    bool write_file = false;
    bool perform_expansion = false;
    bool store_single = false;
    std::vector<std::string> args;

    for (size_t i = 1; i < tokens.size(); ++i)
    {
        const std::string &token = tokens[i];

        if (token == "-c")
        {
            clear_history = true;
        }
        else if (token == "-d")
        {
            delete_entry = true;
            if (i + 1 < tokens.size())
            {
                try
                {
                    delete_offset = std::stoi(tokens[++i]);
                }
                catch (...)
                {
                    std::cerr << "history: invalid offset: " << tokens[i] << std::endl;
                    last_exit_code = 1;
                    return;
                }
            }
            else
            {
                std::cerr << "history: -d requires an offset argument" << std::endl;
                last_exit_code = 1;
                return;
            }
        }
        else if (token == "-a")
        {
            append_to_file = true;
        }
        else if (token == "-n")
        {
            read_new = true;
        }
        else if (token == "-r")
        {
            read_file = true;
        }
        else if (token == "-w")
        {
            write_file = true;
        }
        else if (token == "-p")
        {
            perform_expansion = true;
        }
        else if (token == "-s")
        {
            store_single = true;
        }
        else if (token[0] == '-')
        {
            std::cerr << "history: invalid option: " << token << std::endl;
            std::cerr << "history: use 'history --help' for more information." << std::endl;
            last_exit_code = 1;
            return;
        }
        else
        {
            args.push_back(token);
        }
    }

    if (clear_history)
    {
        clear_history_list();
        std::cout << "History cleared" << std::endl;
    }

    if (delete_entry)
    {
        HISTORY_STATE *hist_state = history_get_history_state();
        int actual_index;
        
        if (delete_offset < 0)
        {
            actual_index = history_length + delete_offset;
        }
        else
        {
            actual_index = delete_offset - 1;
        }

        if (actual_index >= 0 && actual_index < history_length)
        {
            HIST_ENTRY *entry = remove_history(actual_index);
            if (entry)
            {
                free(entry->line);
                free(entry);
                std::cout << "Deleted history entry " << (actual_index + 1) << std::endl;
            }
        }
        else
        {
            std::cerr << "history: offset out of range: " << delete_offset << std::endl;
            last_exit_code = 1;
        }
        free(hist_state);
    }

    if (append_to_file)
    {
        write_history(ns_HISTORY_FILE.c_str());
    }

    if (read_new)
    {
        read_history(ns_HISTORY_FILE.c_str());
    }

    if (read_file)
    {
        read_history(ns_HISTORY_FILE.c_str());
    }

    if (write_file)
    {
        write_history(ns_HISTORY_FILE.c_str());
    }

    if (perform_expansion)
    {
        for (const auto &arg : args)
        {
            char *expanded = nullptr;
            int result = history_expand((char *)arg.c_str(), &expanded);
            
            if (result < 0 || expanded == nullptr)
            {
                std::cerr << "history: expansion failed: " << arg << std::endl;
                last_exit_code = 1;
            }
            else
            {
                if (result)
                {
                    std::cout << expanded << std::endl;
                }
                else
                {
                    std::cout << arg << std::endl;
                }
                free(expanded);
            }
        }
    }

    if (store_single)
    {
        if (!args.empty())
        {
            std::string single_entry;
            for (const auto &arg : args)
            {
                single_entry += arg + " ";
            }
            if (!single_entry.empty())
            {
                single_entry.pop_back();
            }
            add_history(single_entry.c_str());
        }
    }

    if (!clear_history && !delete_entry && !append_to_file && !read_new &&
        !read_file && !write_file && !perform_expansion && !store_single)
    {
        int show_count = -1;
        if (!args.empty())
        {
            try
            {
                show_count = std::stoi(args[0]);
            }
            catch (...)
            {
                // Jika bukan angka, tampilkan semua
                show_count = -1;
            }
        }

        HISTORY_STATE *hist_state = history_get_history_state();
        int start_index = 0;
        
        if (show_count > 0 && show_count < history_length)
        {
            start_index = history_length - show_count;
        }

        for (int i = start_index; i < history_length; ++i)
        {
            HIST_ENTRY *entry = history_get(i + history_base);
            if (entry)
            {
                std::cout << " " << (i + 1) << "  " << entry->line << std::endl;
            }
        }
        free(hist_state);
    }

    last_exit_code = 0;
}
