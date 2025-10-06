#include "execution.h"
#include "terminal.h"

#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <iostream>
#include <vector>
#include <string>

// Asumsi fungsi-fungsi berikut dideklarasikan di tempat lain (misalnya, execution.h)
// dan dapat diakses oleh file ini.
// extern int last_exit_code;
// std::vector<char*> build_envp();
// std::string find_binary(const std::string& command);
// void restore_terminal_mode();
// void exit_shell(int code);


void handle_builtin_exec(const std::vector<std::string> &tokens)
{
    // Cek jika exec dipanggil tanpa argumen (kecuali "exec" itu sendiri)
    if (tokens.size() == 1)
    {
        // exec tanpa argumen - tidak melakukan apa-apa, keluar dengan sukses.
        last_exit_code = 0;
        return;
    }

    // Handle opsi bantuan
    if (tokens[1] == "--help" || tokens[1] == "-h")
    {
        std::cout << "exec: exec [-clv] [-a name] [command [arguments ...]] [redirection ...]\n"
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

    // Variabel untuk parsing
    bool empty_env = false;
    bool login_shell = false;
    bool verbose_mode = false;
    std::string zeroth_arg;
    std::vector<std::string> command_args;
    std::vector<std::string> redirections;
    size_t i = 1; // Indeks untuk iterasi manual melalui token

    // Fase 1: Parsing Opsi
    while (i < tokens.size() && !tokens[i].empty() && tokens[i][0] == '-')
    {
        const std::string &token = tokens[i];

        if (token == "--")
        {
            i++; // Lewati "--"
            break; // Hentikan parsing opsi
        }
        
        // Handle -a yang membutuhkan argumen terpisah
        if (token == "-a")
        {
            if (i + 1 < tokens.size())
            {
                zeroth_arg = tokens[i + 1];
                i += 2; // Konsumsi "-a" dan argumennya
                continue; // Lanjutkan parsing dari token berikutnya
            }
            else
            {
                std::cerr << "nsh: exec: -a requires an argument" << std::endl;
                last_exit_code = 2; // Error penggunaan
                return;
            }
        }
        
        // Handle opsi gabungan seperti -clv
        for (size_t j = 1; j < token.size(); ++j)
        {
            char opt_char = token[j];
            switch (opt_char)
            {
            case 'c':
                empty_env = true;
                break;
            case 'l':
                login_shell = true;
                break;
            case 'v':
                verbose_mode = true;
                break;
            default:
                std::cerr << "nsh: exec: invalid option: -" << opt_char << std::endl;
                last_exit_code = 2;
                return;
            }
        }
        i++; // Pindah ke token berikutnya
    }

    // Fase 2: Parsing Perintah dan Pengalihan dari token yang tersisa
    while (i < tokens.size())
    {
        const std::string &token = tokens[i];
        if (token == "<" || token == ">" || token == ">>" || token == "2>" ||
            token == "2>>" || token == "&>" || token == "&>>")
        {
            if (i + 1 < tokens.size())
            {
                redirections.push_back(token);
                redirections.push_back(tokens[i + 1]);
                i += 2;
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
            i++;
        }
    }

    // Fungsi untuk menangani pengalihan (untuk menghindari duplikasi kode)
    auto apply_redirections = [&]() -> bool {
        for (size_t k = 0; k < redirections.size(); k += 2)
        {
            const std::string &op = redirections[k];
            const std::string &file = redirections[k + 1];

            int flags = 0;
            int target_fd = -1;

            if (op == "<") { flags = O_RDONLY; target_fd = STDIN_FILENO; }
            else if (op == ">") { flags = O_WRONLY | O_CREAT | O_TRUNC; target_fd = STDOUT_FILENO; }
            else if (op == ">>") { flags = O_WRONLY | O_CREAT | O_APPEND; target_fd = STDOUT_FILENO; }
            else if (op == "2>") { flags = O_WRONLY | O_CREAT | O_TRUNC; target_fd = STDERR_FILENO; }
            else if (op == "2>>") { flags = O_WRONLY | O_CREAT | O_APPEND; target_fd = STDERR_FILENO; }
            else if (op == "&>" || op == "&>>") {
                flags = (op == "&>") ? (O_WRONLY | O_CREAT | O_TRUNC) : (O_WRONLY | O_CREAT | O_APPEND);
            }

            int fd = open(file.c_str(), flags, 0666);
            if (fd == -1)
            {
                std::cerr << "nsh: exec: " << file << ": " << strerror(errno) << std::endl;
                last_exit_code = 1;
                return false;
            }

            if (op == "&>" || op == "&>>") {
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
            } else {
                dup2(fd, target_fd);
            }
            close(fd);
        }
        return true;
    };

    // Handle kasus di mana tidak ada perintah yang diberikan
    if (command_args.empty())
    {
        // Jika hanya ada pengalihan, terapkan dan berhasil.
        if (!redirections.empty())
        {
            if (!apply_redirections()) return;
        }
        // Jika tidak ada perintah (misalnya, hanya "exec -c"), berhasil.
        last_exit_code = 0;
        return;
    }

    // Terapkan opsi -a atau -l. Opsi -a lebih diprioritaskan.
    if (!zeroth_arg.empty())
    {
        command_args[0] = zeroth_arg;
    }
    else if (login_shell)
    {
        command_args[0] = "-" + command_args[0];
    }

    // Mode verbose: cetak perintah sebelum dieksekusi
    if (verbose_mode)
    {
        for (size_t k = 0; k < command_args.size(); ++k)
        {
            std::cout << command_args[k] << (k == command_args.size() - 1 ? "" : " ");
        }
        std::cout << std::endl;
    }

    // Terapkan pengalihan sebelum eksekusi
    if (!apply_redirections())
    {
        return;
    }
    
    // Siapkan argumen untuk execve
    std::vector<char*> argv_vec;
    for (const std::string& arg : command_args) {
        argv_vec.push_back(const_cast<char*>(arg.c_str()));
    }
    argv_vec.push_back(nullptr);
    
    char* const* argv = argv_vec.data();
    char** envp = nullptr;
    std::vector<char*> envp_vec; // Deklarasikan di luar blok if agar tetap ada
    
    if (empty_env) {
        static char* empty_env_arr[] = { nullptr };
        envp = empty_env_arr;
    } else {
        // Asumsi build_envp() mengembalikan vector yang dikelola oleh shell
        envp_vec = build_envp();
        envp = envp_vec.data();
    }

    // Dapatkan path lengkap ke biner
    std::string binary_path = find_binary(command_args[0]);
    if (binary_path.empty()) {
        std::cerr << "nsh: exec: " << command_args[0] << ": command not found" << std::endl;
        last_exit_code = 127;
        exit_shell(last_exit_code);
    }
    
    restore_terminal_mode();

    // Eksekusi perintah menggunakan execve
    if (execve(binary_path.c_str(), argv, envp) == -1) {
        std::cerr << "nsh: exec: " << command_args[0] << ": " << strerror(errno) << std::endl;
        
        if (errno == ENOENT) {
            last_exit_code = 127; // Command not found
        } else if (errno == EACCES) {
            last_exit_code = 126; // Cannot execute
        } else {
            last_exit_code = 1; // General error
        }
    
        exit_shell(last_exit_code);
    }
    
    // Kode di sini tidak akan pernah dieksekusi jika execve berhasil
}
