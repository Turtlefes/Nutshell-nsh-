void handle_builtin_unalias(const std::vector<std::string> &tokens)
{
    if (tokens.size() < 2)
    {
        std::cerr << "nsh: unalias: usage: unalias [-a] name [name ...]" << std::endl;
        last_exit_code = 1;
        return;
    }
    last_exit_code = 0;
    if (tokens[1] == "-a")
        aliases.clear();
    else
    {
        for (size_t i = 1; i < tokens.size(); ++i)
        {
            if (aliases.erase(tokens[i]) == 0)
            {
                std::cerr << "nsh: unalias: " << tokens[i] << ": not found" << std::endl;
                last_exit_code = 1;
            }
        }
    }
    save_aliases();
}