#include "globals.h"
#include "terminal.h"
#include "signals.h"
#include "init.h"
#include "parser.h"
#include "execution.h"
#include "utils.h" // xrand and others
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

#include <unistd.h>     // For usleep (Unix-like sleep for microseconds)
#include <cstdlib>      // For system("clear") or similar
#include <ctime>        // For seeding random number generator
#include <random>       // For better random text selection
#include <thread>       // For std::this_thread::sleep_for (alternative to usleep)
#include <chrono>       // For use with std::this_thread::sleep_for


namespace fs = std::filesystem;

// Sertakan header Readline
#include <readline/readline.h>
#include <readline/history.h>

// program nama
std::string program_name;

// Deklarasi fungsi untuk menangani argumen command line
void handle_command_line_args(int argc, char* argv[]);
void execute_script_file(const std::string& filename);
void run_interactive_shell();
void process_rcfile();
void run_subshell_command(const std::string& command);

// Forward declaration for jobs status
void report_finished_jobs();
std::string job_status_to_string(JobStatus status, int term_status);

void run_nutshell_easter_egg() {
    // Definisi frame animasi kacang pecah (5 baris per frame)
    const std::vector<std::string> nut_frames = {
        // Frame 1: Kulit tertutup rapat
        "      _     ",
        "     ( )    ",
        "    /   \\   ",
        "   |     |  ",
        "   \\_ _/   ",
        
        // Frame 2: Sedikit terbuka
        "      _ _   ",
        "     ( V )  ",
        "    / / \\ \\ ",
        "   | |   | |",
        "   \\_ _ _/  ",
        
        // Frame 3
        "      _   _ ",
        "     ( ) ( )",
        "    / /   \\ \\",
        "   | /     \\|",
        "   \\_ _ _ _/ ",
        
        // Frame 4
        "     /    \\ ",
        "    /      \\",
        "   (        )",
        "   |        |",
        "   \\_      _/ ",
        
        // Frame 5: Pembukaan yang cepat
        "    /\\   /\\ ",
        "   /  \\_/  \\",
        "  (        )",
        "  |        |",
        "  \\_      _/ ",
        
        // Frame 6
        "   /\\     /\\",
        "  /  \\___/  \\",
        " (          )",
        " |          |",
        " \\_        _/ ",
        
        // Frame 7
        "  /\\       /\\",
        " /  \\_____/  \\",
        "(            )",
        "|            |",
        "\\_          _/ ",

        // Frame 8
        " /\\         /\\",
        "/  \\_______/  \\",
        "(              )",
        "|              |",
        "\\_            _/ ",
        
        // Frame 9 (Puncak terbuka)
        "/\\___________/\\",
        "\\  _________  /",
        " (           ) ",
        "  |         |  ",
        "  \\_       _/  ",
        
        // Frame 10 (Mulai muncul kacang)
        "/\\___________/\\",
        "\\  _________  /",
        " (   o   o   ) ",
        "  |    v    |  ",
        "  \\_       _/  "
    };
    
    // Konfigurasi Kacang Hidup
    const std::string alive_nut_top =
        "\t\t/\\___________/\\ \n"
        "\t\t\\  _________  /\n";
    const std::string alive_nut_face = 
        "\t\t (   o   o   ) \n"
        "\t\t  |    v    |  \n"
        "\t\t  \\_       _/  ";

    // Teks random dari kacang hidup
    const std::vector<std::string> random_texts = {
        "Did you just crack my shell?",
        "Welcome to the nutshell!",
        "I'm more than just a nut.",
        "The kernel of truth is here.",
        "Shell-ebrate good times!",
        "Why so serious?",
        "C++ is my favorite flavor.",
        "Don't worry, be happy nut.",
        "Segmentation fault? Sounds tasty."
    };
    
    // Gunakan xrand untuk memilih teks random
    int rand_idx = xrand(time(NULL), 0, random_texts.size() - 1);
    std::string random_message = random_texts[rand_idx];
    
    int num_frames = nut_frames.size() / 5;
    auto delay = std::chrono::milliseconds(100); // 100ms delay per frame

    // Initial clear (Clear screen, move cursor home)
    std::cout << "\033[2J\033[H"; 
    
    // Animasi pembukaan (Frame 1 - 10)
    for (int i = 0; i < num_frames; ++i) {
        // Clear area dan posisikan kursor
        std::cout << "\033[2J\033[H"; 
        std::cout << "\n\n"; 
        for (int j = 0; j < 5; ++j) {
            std::cout << "\t\t" << nut_frames[i * 5 + j] << std::endl;
        }
        std::cout.flush();
        std::this_thread::sleep_for(delay);
    }
    
    // Frame berbicara - dengan clear screen yang tepat
    std::string speech_bubble = 
        "\t\t /" + std::string(random_message.length() + 2, '-') + "\\\n"
        "\t\t<  " + random_message + "  >\n"
        "\t\t \\" + std::string(random_message.length() + 2, '-') + "/\n";
        
    auto speak_delay = std::chrono::milliseconds(200);

    for (int i = 0; i < 5; ++i) { // Kurangi iterasi untuk menghindari penumpukan
        // Clear screen dan posisikan kursor di atas
        std::cout << "\033[2J\033[H"; 
        
        // Tampilkan gelembung bicara
        std::cout << speech_bubble << std::endl;
        
        // Tampilkan kacang hidup
        std::cout << alive_nut_top
                  << alive_nut_face << "\n";
                  
        std::cout.flush();
        std::this_thread::sleep_for(speak_delay); 
    }
    
    // Tampilkan frame terakhir lebih lama
    std::cout << "\033[2J\033[H"; 
    std::cout << speech_bubble << std::endl;
    std::cout << alive_nut_top
              << alive_nut_face << "\n";
              
    std::cout << "\n\n\t\t--- Crack succesfully ---\n" << std::endl;
}

int main(int argc, char* argv[]) {
    program_name = argv[0];
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

void show_program_help()
{
  std::cout << "Turtlefes Nutshell, version " << shell_version_long << std::endl;
  std::cout << "Usage: " << program_name << " [OPTIONS] [FILE]\n\n"
            << shell_desc << "\n"
            << "\nOptions:\n"
            << std::left
            << std::setw(30) << "  -c, --command COMMAND" << "Execute COMMAND and exit\n"
            << std::setw(30) << "  -f, --file FILE" << "Execute commands from file targets\n"
            << std::setw(30) << "  -h, --help" << "Show this help message\n"
            << std::setw(30) << "  -v, --version" << "Show version information\n\n"
            << "If FILE is provided, execute commands from FILE\n"
            << "Crack open the shell wisely.\n";
}

void handle_command_line_args(int argc, char *argv[]) {
  std::vector<std::string> args(argv + 1, argv + argc);

  for (size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "crack" || args[i] == "crack open") {
      run_nutshell_easter_egg();
      exit_shell(last_exit_code);
    }
    if (args[i] == "-c" || args[i] == "--command") {
      if (i + 1 < args.size()) {
        std::string command = args[i + 1];
        run_subshell_command(command);
        exit_shell(last_exit_code); // Keluar setelah menjalankan command
      } else {
        std::cerr << "nsh: option requires an argument -- '" << args[i] << "'"
                  << std::endl;
        exit_shell(1);
      }
    } else if (args[i] == "--help" || args[i] == "-h") {
      show_program_help();
      exit_shell(last_exit_code);
    } else if (args[i] == "--version" || args[i] == "-v") {
      std::cout
          << "Turtlefes Nutshell, version " << shell_version_long << " (" << release_date << ")" << "\n"
          << COPYRIGHT << "\n"
          << LICENSE
          << "\n\nHomepage: https://github.com/Turtlefes/Nutshell-nsh-\n\nThis "
             "is free software; you are free to change and redistribute it."
          << "\nThere is NO WARRANTY, to the extent permitted by law."
          << std::endl;
      exit_shell(0);
    } else if (args[i] == "--file" || args[i] == "-f") {
      if (i + 1 < args.size()) {
        std::string file = args[i + 1];
        execute_script_file(file);
        exit_shell(last_exit_code);
      } else {
        std::cerr << "nsh: option requires target file -- '" << args[i] << "'"
                  << std::endl;
        exit_shell(1);
      }
    } else if (args[i][0] != '-') {
      // Ini kemungkinan nama file script
      execute_script_file(args[i]);
      exit_shell(last_exit_code);
    } else {
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
        received_sigint = 1; // Pastikan flag interrupt di-set
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
                if (received_sigint) {
                    last_exit_code = 130;
                    script_interrupted = true;
                    break;
                }
                
                try {
                    auto commands = parser.parse(line);
                    if (!commands.empty()) {
                        last_exit_code = execute_command_list(commands);
                        
                        // Jika command mengatur exit code yang menunjukkan interrupt
                        if (last_exit_code == 130 || received_sigint) {
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
                // received_sigint = 0; // HAPUS BARIS INI
            }
        } else {
            // No nsh shebang, process the entire file including first line
            std::string line = first_line;
            if (!line.empty() && line.find_first_not_of(" \t") != std::string::npos) {
                // Check for interruption
                if (received_sigint) {
                    last_exit_code = 130;
                    script_interrupted = true;
                } else {
                    try {
                        auto commands = parser.parse(line);
                        if (!commands.empty()) {
                            last_exit_code = execute_command_list(commands);
                            if (last_exit_code == 130 || received_sigint) {
                                script_interrupted = true;
                            }
                        }
                    } catch (const std::exception &e) {
                        std::cerr << "nsh: " << e.what() << std::endl;
                        last_exit_code = 1;
                    }
                }
                // TIDAK reset interrupt flag di sini
                // received_sigint = 0; // HAPUS BARIS INI
            }
            
            if (!script_interrupted) {
                while (std::getline(file, line)) {
                    if (line.empty() || line.find_first_not_of(" \t") == std::string::npos) {
                        continue; // Skip empty lines
                    }
                    
                    // Check for interruption
                    if (received_sigint) {
                        last_exit_code = 130;
                        script_interrupted = true;
                        break;
                    }
                    
                    try {
                        auto commands = parser.parse(line);
                        if (!commands.empty()) {
                            last_exit_code = execute_command_list(commands);
                            if (last_exit_code == 130 || received_sigint) {
                                script_interrupted = true;
                                break;
                            }
                        }
                    } catch (const std::exception &e) {
                        std::cerr << "nsh: " << e.what() << std::endl;
                        last_exit_code = 1;
                    }
                    
                    // TIDAK reset interrupt flag di sini
                    // received_sigint = 0; // HAPUS BARIS INI
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
        received_sigint = 0;
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
    if (isatty(STDIN_FILENO))
      process_rcfile();
    
    load_history();  
    
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
        
        received_sigint = 0;
        reset_current_signal();

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
            
            // simpan secara periodik
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


