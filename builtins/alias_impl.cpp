void handle_builtin_alias(const std::vector<std::string> &tokens)
{
    if (tokens.size() == 1)
    {
        // Tampilkan semua alias
        for (const auto &[key, value] : aliases)
        {
            std::cout << "alias " << key << "='" << value << "'" << std::endl;
        }
        last_exit_code = 0;
        return;
    }

    // Gunakan parser untuk menangani argumen yang kompleks
    Parser parser;
    std::vector<std::string> processed_args;
    
    // Gabungkan semua token kecuali perintah "alias" itu sendiri
    std::string args_str;
    for (size_t i = 1; i < tokens.size(); ++i)
    {
        if (i > 1) args_str += " ";
        args_str += tokens[i];
    }
    
    // Parse argumen menggunakan parser yang sama
    try
    {
        auto parsed_commands = parser.parse(args_str);
        if (!parsed_commands.empty() && !parsed_commands[0].pipeline.empty())
        {
            const auto& first_cmd = parsed_commands[0].pipeline[0];
            
            // Ekstrak token dari hasil parsing
            for (const auto& token : first_cmd.tokens)
            {
                processed_args.push_back(token);
            }
        }
        else
        {
            // Fallback: gunakan token asli jika parsing gagal
            for (size_t i = 1; i < tokens.size(); ++i)
            {
                processed_args.push_back(tokens[i]);
            }
        }
    }
    catch (const std::exception& e)
    {
        // Jika parsing error, gunakan token asli
        for (size_t i = 1; i < tokens.size(); ++i)
        {
            processed_args.push_back(tokens[i]);
        }
    }

    // Proses setiap argumen yang sudah di-parsed
    for (const auto& arg : processed_args)
    {
        size_t eq_pos = arg.find('=');
        
        if (eq_pos == std::string::npos)
        {
            // Mode tampilkan: alias name
            auto it = aliases.find(arg);
            if (it != aliases.end())
            {
                std::cout << "alias " << it->first << "='" << it->second << "'" << std::endl;
                last_exit_code = 0;
            }
            else
            {
                std::cerr << "nsh: alias: " << arg << ": not found" << std::endl;
                last_exit_code = 1;
            }
        }
        else
        {
            // Mode set: name=value
            std::string key = arg.substr(0, eq_pos);
            std::string value = arg.substr(eq_pos + 1);
            
            // Hapus quotes jika ada (sesuai dengan behavior shell standar)
            if (value.size() >= 2 && 
                ((value.front() == '\'' && value.back() == '\'') || 
                 (value.front() == '"' && value.back() == '"')))
            {
                value = value.substr(1, value.length() - 2);
            }
            
            // Set alias
            aliases[key] = value;
            last_exit_code = 0;
        }
    }
    
    save_aliases();
}