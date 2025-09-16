#include "init.h"
#include "globals.h"

#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <cstdlib>
#include <algorithm>
#include <readline/readline.h>
#include <readline/history.h>

void initialize_environment()
{
    const char *home_env = getenv("HOME");
    if (home_env)
        HOME_DIR = home_env;
    else
    {
        struct passwd *pw = getpwuid(getuid());
        if (pw)
            HOME_DIR = pw->pw_dir;
        else
        {
            std::cerr << "nsh: FATAL: Cannot determine HOME directory." << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    const char *pwd_env = getenv("PWD");
    LOGICAL_PWD = (pwd_env && fs::exists(pwd_env)) ? fs::path(pwd_env).lexically_normal() : fs::current_path();
    setenv("PWD", LOGICAL_PWD.c_str(), 1);

    ns_CONFIG_DIR = HOME_DIR / ".nsh";
    ns_CONFIG_FILE = ns_CONFIG_DIR / "config.rc";
    ns_HISTORY_FILE = ns_CONFIG_DIR / "history";
    ns_RC_FILE = ns_CONFIG_DIR / "nsrc";
    ns_ALIAS_FILE = ns_CONFIG_DIR / "alias";
    ns_BOOKMARK_FILE = ns_CONFIG_DIR / "bookmarks";
    ETCDIR = ns_CONFIG_DIR;

    try
    {
        if (!fs::exists(ns_CONFIG_DIR))
            fs::create_directory(ns_CONFIG_DIR);
    }
    catch (const fs::filesystem_error &e)
    {
        std::cerr << "nsh: FATAL: Failed to create config directory: " << e.what() << std::endl;
        exit(EXIT_FAILURE);
    }
}

void save_aliases()
{
    std::ofstream alias_file(ns_ALIAS_FILE, std::ios::trunc);
    if (!alias_file.is_open())
        return;
    for (const auto &[key, value] : aliases)
    {
        alias_file << key << "='" << value << "'" << std::endl;
    }
}

void load_aliases()
{
    std::ifstream alias_file(ns_ALIAS_FILE);
    if (!alias_file.is_open())
        return;
    std::string line;
    while (std::getline(alias_file, line))
    {
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos)
        {
            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);
            if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'')
            {
                value = value.substr(1, value.length() - 2);
            }
            aliases[key] = value;
        }
    }
}

void load_configuration()
{
    load_aliases();
}

void load_history() {
    command_history.clear();
    std::ifstream history_file(ns_HISTORY_FILE);
    if (history_file.is_open()) {
        std::string line;
        while (std::getline(history_file, line)) {
            command_history.push_back(line);
            // Selalu tambahkan ke GNU Readline history
            add_history(line.c_str());
        }
        history_index = command_history.size();
    }
}

void save_history() {
    std::ofstream history_file(ns_HISTORY_FILE);
    if (history_file.is_open()) {
        for (const auto &cmd : command_history) {
            history_file << cmd << std::endl;
        }
    }
    // Selalu simpan GNU Readline history
    write_history(ns_HISTORY_FILE.c_str());
}