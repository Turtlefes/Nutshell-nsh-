//this file is a implementation of builtin cd command in the builtins.cpp

void handle_builtin_cd(const std::vector<std::string> &t)
{
    bool use_physical = false;
    bool check_exist_err = false;
    std::string path_arg_str;
    size_t first_arg_idx = 0;

    for (size_t i = 1; i < t.size(); ++i)
    {
        const std::string &token = t[i];
        if (token.rfind("-", 0) == 0 && token.length() > 1)
        {
            if (token == "-P")
            {
                use_physical = true;
            }
            else if (token == "-L")
            {
                use_physical = false;
            }
            else if (token == "-e")
            {
                check_exist_err = true;
            }
            else if (token == "-")
            {
                path_arg_str = "-";
                first_arg_idx = i;
                break;
            }
            else if (token == "--")
            {
                first_arg_idx = i + 1;
                break;
            }
            else
            {
                std::cerr << "nsh: cd: " << token << ": invalid option" << std::endl;
                std::cerr << "cd: usage: cd [-L|-P|-e] [dir]" << std::endl;
                last_exit_code = 2;
                return;
            }
            first_arg_idx = i + 1;
        }
        else
        {
            first_arg_idx = i;
            break;
        }
    }

    if (first_arg_idx > 0 && first_arg_idx < t.size())
    {
        path_arg_str = t[first_arg_idx];
    }
    else if (t.size() == 1)
    {
        path_arg_str.clear();
    }

    fs::path target_path;
    if (path_arg_str.empty())
    {
        target_path = HOME_DIR;
    }
    else if (path_arg_str == "-")
    {
        if (OLD_PWD.empty())
        {
            std::cerr << "nsh: cd: OLDPWD not set" << std::endl;
            last_exit_code = 1;
            return;
        }
        target_path = OLD_PWD;
        safe_print(target_path.string());
    }
    else
    {
        target_path = expand_tilde(path_arg_str);
    }

    if (!path_arg_str.empty() && path_arg_str[0] != '/' && path_arg_str[0] != '~' && path_arg_str != "-")
    {
        const char *cdpath = getenv("CDPATH");
        bool resolved = false;
        if (cdpath && cdpath[0] != '\0')
        {
            std::stringstream ss(cdpath);
            std::string path_item;
            while (std::getline(ss, path_item, ':'))
            {
                if (path_item.empty())
                    continue;
                fs::path test_path = fs::path(expand_tilde(path_item)) / path_arg_str;
                if (fs::exists(test_path) && fs::is_directory(test_path))
                {
                    target_path = test_path;
                    resolved = true;
                    break;
                }
            }
        }

        if (!resolved)
        {
            std::ifstream bookmark_file(ns_BOOKMARK_FILE);
            if (bookmark_file.is_open())
            {
                std::string line;
                while (std::getline(bookmark_file, line))
                {
                    size_t space_pos = line.find(' ');
                    if (space_pos != std::string::npos &&
                        line.substr(0, space_pos) == path_arg_str)
                    {
                        target_path = line.substr(space_pos + 1);
                        break;
                    }
                }
            }
        }
    }

    fs::path prev_logical = LOGICAL_PWD;
    try
    {
        fs::path new_logical_pwd;
        fs::path physical_path_to_change;

        if (use_physical)
        {
            physical_path_to_change = fs::canonical(
                target_path.is_absolute() ? target_path : LOGICAL_PWD / target_path);
            new_logical_pwd = physical_path_to_change;
        }
        else
        {
            fs::path temp_logical_path = (target_path.is_absolute()
                                              ? target_path
                                              : (LOGICAL_PWD / target_path));

            temp_logical_path = temp_logical_path.lexically_normal();

            if (!fs::exists(temp_logical_path))
            {
                std::cerr << "nsh: cd: " << path_arg_str << ": No such file or directory" << std::endl;
                last_exit_code = 1;
                return;
            }

            physical_path_to_change = temp_logical_path;
            new_logical_pwd = temp_logical_path;
        }

        if (!fs::is_directory(physical_path_to_change))
        {
            std::cerr << "nsh: cd: " << path_arg_str << ": Not a directory" << std::endl;
            last_exit_code = 1;
            return;
        }

        fs::current_path(physical_path_to_change);

        OLD_PWD = prev_logical;
        LOGICAL_PWD = new_logical_pwd;

        std::string pwd_str = LOGICAL_PWD.string();
        if (pwd_str.size() > 1 && pwd_str.back() == '/')
        {
            pwd_str.pop_back();
            LOGICAL_PWD = pwd_str;
        }

        setenv("PWD", LOGICAL_PWD.c_str(), 1);
        setenv("OLDPWD", OLD_PWD.c_str(), 1);
        last_exit_code = 0;
    }
    catch (const fs::filesystem_error &e)
    {
        std::cerr << "nsh: cd: " << path_arg_str << ": " << e.code().message() << std::endl;
        last_exit_code = 1;
    }
}