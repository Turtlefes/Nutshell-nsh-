#include "globals.h"
#include "terminal.h"
#include "signals.h"
#include "init.h"
#include "parser.h"
#include "execution.h"
#include "utils.h"
#include "prompt.h"

#include <iostream>
#include <string>
#include <vector>
#include <exception>
#include <termios.h>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

// Sertakan header Readline
#include <readline/readline.h>
#include <readline/history.h>

// Deklarasi fungsi untuk menangani argumen command line
void handle_command_line_args(int argc, char* argv[]);
void execute_script_file(const std::string& filename);
void run_interactive_shell();
void process_rcfile();
void run_subshell_command(const std::string& command);

int main(int argc, char* argv[]) {
    try {
        setup_terminal();
        setup_signals();
        initialize_environment();
        load_configuration();
        
        // Handle command line arguments
        if (argc > 1) {
            handle_command_line_args(argc, argv);
        }
        
        run_interactive_shell(); // to start the shell
        
        write_history(ns_HISTORY_FILE.c_str());
        restore_terminal_mode();
        return last_exit_code;
        
    } catch (const std::bad_alloc&) {
        std::cerr << "nsh: fatal error: out of memory" << std::endl;
        restore_terminal_mode();
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "nsh: fatal error: " << e.what() << std::endl;
        restore_terminal_mode();
        return 1;
    } catch (...) {
        std::cerr << "nsh: fatal error: unknown exception" << std::endl;
        restore_terminal_mode();
        return 1;
    }
}

void handle_command_line_args(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc);
    
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "-c" || args[i] == "--command") {
            if (i + 1 < args.size()) {
                std::string command = args[i + 1];
                run_subshell_command(command);
                exit_shell(last_exit_code); // Keluar setelah menjalankan command
            } else {
                std::cerr << "nsh: option requires an argument -- '" << args[i] << "'" << std::endl;
                exit_shell(1);
            }
        }
        else if (args[i] == "--help" || args[i] == "-h") {
            std::cout << "Turtlefes Nutshell, version " << shell_version_long << std::endl;
            std::cout << "Usage: " << argv[0] << " [OPTIONS] [FILE]\n\n"
                      << "Options:\n"
                      << "  -c, --command COMMAND  Execute COMMAND and exit\n"
                      << "  -h, --help             Show this help message\n"
                      << "  -v, --version          Show version information\n\n"
                      << "If FILE is provided, execute commands from FILE\n";
            exit_shell(0);
        }
        else if (args[i] == "--version" || args[i] == "-v") {
            std::cout << "Turtlefes Nutshell, version " << shell_version_long << 
            "\n" << COPYRIGHT << "\n" << LICENSE << "\n\nThis is free software; you are free to change and redistribute it." << "\nThere is NO WARRANTY, to the extent permitted by law." << std::endl;
            exit_shell(0);
        }
        else if (args[i][0] != '-') {
            // Ini kemungkinan nama file script
            execute_script_file(args[i]);
            exit_shell(last_exit_code);
        }
        else {
            std::cerr << "nsh: invalid option -- '" << args[i] << "'" << std::endl;
            std::cerr << "Try 'nsh --help' for more information." << std::endl;
            exit_shell(1);
        }
    }
}

void execute_script_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "nsh: cannot open file: " << filename << std::endl;
        last_exit_code = 1;
        return;
    }
    
    // Check for shebang line
    std::string first_line;
    std::getline(file, first_line);
    
    // If shebang points to nsh, ignore it and process the rest
    bool has_nsh_shebang = (first_line.find("#!/bin/nsh") != std::string::npos ||
                             first_line.find("#!/usr/bin/nsh") != std::string::npos);
    
    Parser parser;
    
    if (has_nsh_shebang) {
        // Skip the shebang line, process the rest line by line
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line.find_first_not_of(" \t") == std::string::npos) {
                continue; // Skip empty lines
            }
            
            try {
                auto commands = parser.parse(line);
                if (!commands.empty()) {
                    last_exit_code = execute_command_list(commands);
                }
            } catch (const std::exception &e) {
                std::cerr << "nsh: " << e.what() << std::endl;
                last_exit_code = 1;
            }
        }
    } else {
        // No nsh shebang, process the entire file including first line
        // But process line by line for bash compatibility
        std::string line = first_line;
        if (!line.empty() && line.find_first_not_of(" \t") != std::string::npos) {
            try {
                auto commands = parser.parse(line);
                if (!commands.empty()) {
                    last_exit_code = execute_command_list(commands);
                }
            } catch (const std::exception &e) {
                std::cerr << "nsh: " << e.what() << std::endl;
                last_exit_code = 1;
            }
        }
        
        while (std::getline(file, line)) {
            if (line.empty() || line.find_first_not_of(" \t") == std::string::npos) {
                continue; // Skip empty lines
            }
            
            try {
                auto commands = parser.parse(line);
                if (!commands.empty()) {
                    last_exit_code = execute_command_list(commands);
                }
            } catch (const std::exception &e) {
                std::cerr << "nsh: " << e.what() << std::endl;
                last_exit_code = 1;
            }
        }
    }
}

void process_rcfile()
{
    fs::path rcpath = HOME_DIR / ".nshrc";
    if (fs::exists(rcpath) && fs::is_regular_file(rcpath))
    {
      std::ifstream file(rcpath);
      if (file.is_open()) {
        Parser parser;
        std::string line;
       
        while (std::getline(file, line))
        {
          if (line.empty() || line.find_first_not_of(" \t") == std::string::npos)
            continue;
          try {
            auto commands = parser.parse(line);
            if (!commands.empty()) // id command not empty, jika command tidak kosong maka
            {
              last_exit_code = execute_command_list(commands); // maka exit code menyesuaikan return fungsi execute_command_list, soalnya fungsi itu bertipe int dan bisa return
            }
          } catch (const std::exception& e)
          {
            std::cerr << "error while processing rcfile: " << e.what() << std::endl;
          }
        }
      } else
      {
        // We ignore this, because to keep user comfort (sorry if my english bad)
      }
   }
    
}

void run_interactive_shell() {
    // Gunakan readline history
    using_history();
    read_history(ns_HISTORY_FILE.c_str());
    if (isatty(STDIN_FILENO))
      process_rcfile();

    Parser parser;

    while (true) {
        check_child_status();
        
        if (tcgetpgrp(STDIN_FILENO) != shell_pgid) {
            give_terminal_to(shell_pgid);
            continue;
        }
        
        safe_set_raw_mode();
        //tcflush(STDIN_FILENO, TCIFLUSH);
        
        //std::cout << "\r\033[K";
        //std::cout.flush();

        std::string main_prompt = get_prompt_string();
        
        // Gunakan parser untuk mendapatkan input multiline
        std::string full_input = parser.get_multiline_input(main_prompt);
        
        try {
            auto commands = parser.parse(full_input);
            if (!commands.empty()) {
                last_exit_code = execute_command_list(commands);
            }
        } catch (const std::exception &e) {
            std::cerr << "nsh: " << e.what() << std::endl;
            last_exit_code = 1;
        }
        
        if (last_exit_code >= 128) {
          std::cout << "\n";
          std::cout.flush();
        }
        
        // Simpan history secara periodik
        if (command_history.size() % 10 == 0) {
            write_history(ns_HISTORY_FILE.c_str());
        }
    }
}

void run_subshell_command(const std::string& command) {
    Parser parser;
    try {
        auto commands = parser.parse(command);
        if (!commands.empty()) {
            last_exit_code = execute_command_list(commands);
        }
    } catch (const std::exception &e) {
        std::cerr << "nsh: " << e.what() << std::endl;
        last_exit_code = 1;
    }
}