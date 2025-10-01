#include "globals.h"
#include "terminal.h"
#include "signals.h"
#include "init.h"
#include "parser.h"
#include "execution.h"
#include "utils.h"
#include "input.h"

#include <iostream>
#include <string>
#include <vector>
#include <exception>
#include <termios.h>
#include <fstream>
#include <filesystem>
#include <csignal> // Added for strsignal
#include <iomanip> // Added for std::left, std::setw

namespace fs = std::filesystem;

// Sertakan header Readline
#include <readline/readline.h>
#include <readline/history.h>

// Tell the compiler that this global variable is defined in another file.
extern std::vector<std::pair<int, Job>> finished_jobs;

// Deklarasi fungsi untuk menangani argumen command line
void handle_command_line_args(int argc, char* argv[]);
void execute_script_file(const std::string& filename);
void run_interactive_shell();
void process_rcfile();
void run_subshell_command(const std::string& command);

// Forward declaration for jobs status
void report_finished_jobs();
std::string job_status_to_string(JobStatus status, int term_status);

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
        
        // cek apakah stdin adalah terminal
        if (!isatty(STDIN_FILENO)) 
        {
            // Mode non-interaktif: baca dari stdin (pipe atau redirect)
            Parser parser;
            std::string line;
            while (std::getline(std::cin, line)) {
                if (line.empty() || line.find_first_not_of(" \t") == std::string::npos) {
                    continue;
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
            return last_exit_code;
        }
        
        // kalau stdin = TTY, baru jalanin interactive shell
        run_interactive_shell();
        
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
    
    // Setup signal handling untuk interrupt script
    struct sigaction old_sigint_action;
    struct sigaction script_sigint_action;
    
    // Simpan handler SIGINT lama
    sigaction(SIGINT, NULL, &old_sigint_action);
    
    // Set handler khusus untuk script execution
    script_sigint_action.sa_handler = [](int sig) {
        last_exit_code = 130; // 128 + SIGINT
        EOF_IN_interrupt = 1; // Pastikan flag interrupt di-set
        throw std::runtime_error("Script execution interrupted");
    };
    sigemptyset(&script_sigint_action.sa_mask);
    script_sigint_action.sa_flags = 0;
    sigaction(SIGINT, &script_sigint_action, NULL);
    
    bool script_interrupted = false;
    
    try {
        if (has_nsh_shebang) {
            // Skip the shebang line, process the rest line by line
            std::string line;
            while (std::getline(file, line)) {
                if (line.empty() || line.find_first_not_of(" \t") == std::string::npos) {
                    continue; // Skip empty lines
                }
                
                // Check for Ctrl+C interruption before processing each line
                if (EOF_IN_interrupt) {
                    last_exit_code = 130;
                    script_interrupted = true;
                    break;
                }
                
                try {
                    auto commands = parser.parse(line);
                    if (!commands.empty()) {
                        last_exit_code = execute_command_list(commands);
                        
                        // Jika command mengatur exit code yang menunjukkan interrupt
                        if (last_exit_code == 130 || EOF_IN_interrupt) {
                            script_interrupted = true;
                            break;
                        }
                    }
                } catch (const std::exception &e) {
                    std::cerr << "nsh: " << e.what() << std::endl;
                    last_exit_code = 1;
                    // Continue dengan line berikutnya meskipun ada error
                }
                
                // Reset interrupt flag setelah memproses setiap baris
                // TIDAK reset di sini karena kita ingin interrupt tetap berlaku
                // EOF_IN_interrupt = 0; // HAPUS BARIS INI
            }
        } else {
            // No nsh shebang, process the entire file including first line
            std::string line = first_line;
            if (!line.empty() && line.find_first_not_of(" \t") != std::string::npos) {
                // Check for interruption
                if (EOF_IN_interrupt) {
                    last_exit_code = 130;
                    script_interrupted = true;
                } else {
                    try {
                        auto commands = parser.parse(line);
                        if (!commands.empty()) {
                            last_exit_code = execute_command_list(commands);
                            if (last_exit_code == 130 || EOF_IN_interrupt) {
                                script_interrupted = true;
                            }
                        }
                    } catch (const std::exception &e) {
                        std::cerr << "nsh: " << e.what() << std::endl;
                        last_exit_code = 1;
                    }
                }
                // TIDAK reset interrupt flag di sini
                // EOF_IN_interrupt = 0; // HAPUS BARIS INI
            }
            
            if (!script_interrupted) {
                while (std::getline(file, line)) {
                    if (line.empty() || line.find_first_not_of(" \t") == std::string::npos) {
                        continue; // Skip empty lines
                    }
                    
                    // Check for interruption
                    if (EOF_IN_interrupt) {
                        last_exit_code = 130;
                        script_interrupted = true;
                        break;
                    }
                    
                    try {
                        auto commands = parser.parse(line);
                        if (!commands.empty()) {
                            last_exit_code = execute_command_list(commands);
                            if (last_exit_code == 130 || EOF_IN_interrupt) {
                                script_interrupted = true;
                                break;
                            }
                        }
                    } catch (const std::exception &e) {
                        std::cerr << "nsh: " << e.what() << std::endl;
                        last_exit_code = 1;
                    }
                    
                    // TIDAK reset interrupt flag di sini
                    // EOF_IN_interrupt = 0; // HAPUS BARIS INI
                }
            }
        }
    } catch (const std::runtime_error& e) {
        // Ditangani oleh signal handler
        script_interrupted = true;
        last_exit_code = 130;
    } catch (...) {
        // Handle other exceptions
        script_interrupted = true;
        last_exit_code = 1;
    }
    
    // Restore original SIGINT handler
    sigaction(SIGINT, &old_sigint_action, NULL);
    
    // Clear interrupt flag HANYA JIKA script tidak diinterrupt
    if (!script_interrupted) {
        EOF_IN_interrupt = 0;
    }
    
    if (script_interrupted) {
        last_exit_code = 130;
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

void report_finished_jobs() {
    if (!isatty(STDIN_FILENO) || finished_jobs.empty()) {
        return;
    }

    // Sort finished jobs by job ID for ordered output
    std::sort(finished_jobs.begin(), finished_jobs.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    for (const auto& finished_job_pair : finished_jobs) {
        const auto& job = finished_job_pair.second;
        const auto& id = finished_job_pair.first;

        std::cout << "[" << id << "]+" << "\t"
                  << std::left << std::setw(10) << job_status_to_string(job.status, job.term_status) << "\t"
                  << job.command << std::endl;
    }
    finished_jobs.clear();
}

void run_interactive_shell() {
    // Gunakan readline history
    using_history();
    read_history(ns_HISTORY_FILE.c_str());
    if (isatty(STDIN_FILENO))
      process_rcfile();

    Parser parser;

    while (true) {
        
        if (tcgetpgrp(STDIN_FILENO) != shell_pgid) {
            give_terminal_to(shell_pgid);
            continue;
        }
        
        safe_set_raw_mode();
        //tcflush(STDIN_FILENO, TCIFLUSH);
        
        //std::cout << "\r\033[K";
        //std::cout.flush();
        
        EOF_IN_interrupt = 0;

        std::string main_prompt = get_prompt_string();
        
        // Gunakan parser untuk mendapatkan input multiline
        std::string full_input = get_multiline_input(main_prompt);
        
        if (!dont_execute_first)
        {
            try {
                auto commands = parser.parse(full_input);
                
                // Perubahan di sini: Hanya laporkan finished_jobs jika ada perintah yang dijalankan
                if (!commands.empty()) {
                    last_exit_code = execute_command_list(commands);
                    // Pindahkan report_finished_jobs() ke sini
                    report_finished_jobs(); 
                } else {
                    // Perintah kosong (user hanya tekan enter tanpa command)
                    // Kita masih ingin melaporkan status job di sini
                    // karena menekan Enter tanpa perintah adalah tindakan
                    // 'konfirmasi prompt' yang kamu maksud.
                    report_finished_jobs();
                }
                
            } catch (const std::exception &e) {
                std::cerr << "nsh: " << e.what() << std::endl;
                last_exit_code = 1;
                // Meskipun ada error, melaporkan finished jobs masih masuk akal
                report_finished_jobs();
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


