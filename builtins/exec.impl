void handle_builtin_exec(const std::vector<std::string> &tokens)
{
    if (tokens.size() == 1)
    {
        // exec tanpa argumen - tidak melakukan apa-apa
        last_exit_code = 0;
        return;
    }

    if (tokens[1] == "--help" || tokens[1] == "-h")
    {
        std::cout << "exec: exec [-cl] [-a name] [command [arguments ...]] [redirection ...]\n"
                  << "    Replace the shell with the given command.\n\n"
                  << "    Execute COMMAND, replacing this shell with the specified program.\n"
                  << "    ARGUMENTS become the arguments to COMMAND.  If COMMAND is not specified,\n"
                  << "    any redirections take effect in the current shell.\n\n"
                  << "    Options:\n"
                  << "      -a name   pass NAME as the zeroth argument to COMMAND\n"
                  << "      -c        execute COMMAND with an empty environment\n"
                  << "      -l        place a dash in the zeroth argument to COMMAND\n"
                  << "      -v        verbose mode: print command before executing\n\n"
                  << "    If COMMAND cannot be executed, the shell exits with non-zero status.\n\n"
                  << "    Exit Status:\n"
                  << "    Returns success unless COMMAND is not found or an error occurs.\n";
        last_exit_code = 0;
        return;
    }

    // Parse options
    bool empty_env = false;
    bool login_shell = false;
    bool verbose_mode = false;
    std::string zeroth_arg;
    std::vector<std::string> command_args;
    std::vector<std::string> redirections;
    bool parsing_options = true;
    bool has_command = false;

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
            else if (token == "-c")
            {
                empty_env = true;
            }
            else if (token == "-l")
            {
                login_shell = true;
            }
            else if (token == "-v")
            {
                verbose_mode = true;
            }
            else if (token == "-a")
            {
                if (i + 1 < tokens.size())
                {
                    zeroth_arg = tokens[++i];
                }
                else
                {
                    std::cerr << "nsh: exec: -a requires an argument" << std::endl;
                    last_exit_code = 1;
                    return;
                }
            }
            else
            {
                std::cerr << "nsh: exec: invalid option: " << token << std::endl;
                last_exit_code = 2;
                return;
            }
        }
        else
        {
            parsing_options = false;
            
            // Check for redirection operators
            if (token == "<" || token == ">" || token == ">>" || token == "2>" || 
                token == "2>>" || token == "&>" || token == "&>>")
            {
                // Handle redirections
                if (i + 1 < tokens.size())
                {
                    redirections.push_back(token);
                    redirections.push_back(tokens[++i]);
                }
                else
                {
                    std::cerr << "nsh: exec: syntax error: missing file for redirection" << std::endl;
                    last_exit_code = 2;
                    return;
                }
            }
            else
            {
                command_args.push_back(token);
                has_command = true;
            }
        }
    }

    // Handle case where no command is provided but redirections exist
    if (!has_command && !redirections.empty())
    {
        // Apply redirections to current shell
        for (size_t i = 0; i < redirections.size(); i += 2)
        {
            const std::string &op = redirections[i];
            const std::string &file = redirections[i + 1];
            
            try
            {
                if (op == "<")
                {
                    int fd = open(file.c_str(), O_RDONLY);
                    if (fd == -1)
                    {
                        std::cerr << "nsh: exec: cannot open " << file << ": " << strerror(errno) << std::endl;
                        last_exit_code = 1;
                        return;
                    }
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }
                else if (op == ">")
                {
                    int fd = open(file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    if (fd == -1)
                    {
                        std::cerr << "nsh: exec: cannot create " << file << ": " << strerror(errno) << std::endl;
                        last_exit_code = 1;
                        return;
                    }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }
                else if (op == ">>")
                {
                    int fd = open(file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
                    if (fd == -1)
                    {
                        std::cerr << "nsh: exec: cannot append to " << file << ": " << strerror(errno) << std::endl;
                        last_exit_code = 1;
                        return;
                    }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }
                else if (op == "2>")
                {
                    int fd = open(file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    if (fd == -1)
                    {
                        std::cerr << "nsh: exec: cannot create " << file << ": " << strerror(errno) << std::endl;
                        last_exit_code = 1;
                        return;
                    }
                    dup2(fd, STDERR_FILENO);
                    close(fd);
                }
                else if (op == "2>>")
                {
                    int fd = open(file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
                    if (fd == -1)
                    {
                        std::cerr << "nsh: exec: cannot append to " << file << ": " << strerror(errno) << std::endl;
                        last_exit_code = 1;
                        return;
                    }
                    dup2(fd, STDERR_FILENO);
                    close(fd);
                }
                else if (op == "&>" || op == "&>>")
                {
                    int flags = (op == "&>") ? (O_WRONLY | O_CREAT | O_TRUNC) : (O_WRONLY | O_CREAT | O_APPEND);
                    int fd = open(file.c_str(), flags, 0666);
                    if (fd == -1)
                    {
                        std::cerr << "nsh: exec: cannot " << (op == "&>" ? "create" : "append to") 
                                  << " " << file << ": " << strerror(errno) << std::endl;
                        last_exit_code = 1;
                        return;
                    }
                    dup2(fd, STDOUT_FILENO);
                    dup2(fd, STDERR_FILENO);
                    close(fd);
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "nsh: exec: redirection error: " << e.what() << std::endl;
                last_exit_code = 1;
                return;
            }
        }
        
        last_exit_code = 0;
        return;
    }

    if (!has_command)
    {
        std::cerr << "nsh: exec: command required" << std::endl;
        last_exit_code = 1;
        return;
    }

    // Alokasi argv aman
    char** argv = static_cast<char**>(safe_malloc((command_args.size() + 2) * sizeof(char*)));
    if (!argv)
    {
        std::cerr << "nsh: exec: memory allocation failed" << std::endl;
        last_exit_code = 1;
        return;
    }

    try
    {
        size_t arg_index = 0;

        if (login_shell)
        {
            argv[arg_index++] = safe_strdup("-");
        }
        else if (!zeroth_arg.empty())
        {
            argv[arg_index++] = safe_strdup(zeroth_arg.c_str());
        }
        else
        {
            argv[arg_index++] = safe_strdup(command_args[0].c_str());
        }

        for (size_t i = 0; i < command_args.size(); ++i)
        {
            argv[arg_index++] = safe_strdup(command_args[i].c_str());
        }
        argv[arg_index] = nullptr;

        // Verbose mode: print the command
        if (verbose_mode)
        {
            std::cout << "exec: ";
            for (size_t i = 0; argv[i] != nullptr; ++i)
            {
                std::cout << argv[i] << " ";
            }
            std::cout << std::endl;
        }

        // Handle redirections before exec
        for (size_t i = 0; i < redirections.size(); i += 2)
        {
            const std::string &op = redirections[i];
            const std::string &file = redirections[i + 1];
            
            if (op == "<")
            {
                int fd = open(file.c_str(), O_RDONLY);
                if (fd == -1)
                {
                    std::cerr << "nsh: exec: cannot open " << file << ": " << strerror(errno) << std::endl;
                    throw std::runtime_error("redirection failed");
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }
            else if (op == ">")
            {
                int fd = open(file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fd == -1)
                {
                    std::cerr << "nsh: exec: cannot create " << file << ": " << strerror(errno) << std::endl;
                    throw std::runtime_error("redirection failed");
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            else if (op == ">>")
            {
                int fd = open(file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
                if (fd == -1)
                {
                    std::cerr << "nsh: exec: cannot append to " << file << ": " << strerror(errno) << std::endl;
                    throw std::runtime_error("redirection failed");
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }
            else if (op == "2>")
            {
                int fd = open(file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
                if (fd == -1)
                {
                    std::cerr << "nsh: exec: cannot create " << file << ": " << strerror(errno) << std::endl;
                    throw std::runtime_error("redirection failed");
                }
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
            else if (op == "2>>")
            {
                int fd = open(file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0666);
                if (fd == -1)
                {
                    std::cerr << "nsh: exec: cannot append to " << file << ": " << strerror(errno) << std::endl;
                    throw std::runtime_error("redirection failed");
                }
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
            else if (op == "&>" || op == "&>>")
            {
                int flags = (op == "&>") ? (O_WRONLY | O_CREAT | O_TRUNC) : (O_WRONLY | O_CREAT | O_APPEND);
                int fd = open(file.c_str(), flags, 0666);
                if (fd == -1)
                {
                    std::cerr << "nsh: exec: cannot " << (op == "&>" ? "create" : "append to") 
                              << " " << file << ": " << strerror(errno) << std::endl;
                    throw std::runtime_error("redirection failed");
                }
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
        }

        // Eksekusi program
        if (empty_env)
        {
            char *empty_envp[] = {nullptr};
            execve(argv[0], argv, empty_envp);
        }
        else
        {
            execvp(argv[0], argv);
            execv(argv[0], argv); // Fallback to execv if execvp fails
        }

        // Jika exec kembali, berarti terjadi error
        std::cerr << "nsh: exec: " << command_args[0] << ": " << strerror(errno) << std::endl;
        last_exit_code = 1;
    }
    catch (const std::bad_alloc &)
    {
        std::cerr << "nsh: exec: memory allocation failed for arguments" << std::endl;
        last_exit_code = 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "nsh: exec: " << e.what() << std::endl;
        last_exit_code = 1;
    }

    // Cleanup
    for (size_t i = 0; argv[i] != nullptr; ++i)
        free(argv[i]);
    free(argv);
}