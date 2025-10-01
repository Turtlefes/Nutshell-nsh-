#include <all.h>

#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <csignal>
#include <cstdlib>
#include <utils.h>
#include <filesystem>
#include <unistd.h>
#include <unordered_map>
#include <sstream>
#include <fstream>

#include "input.h" // untuk PS0

namespace fs = std::filesystem;

// This would normally be in a header file (like globals.h)
// For this demonstration, we define it her.

// The job_status_to_string from jobs_impl.cc would be in a shared utility file.
// We redeclare a simple version here for check_child_status.
std::string job_status_to_string_simple(JobStatus status) { return status == JobStatus::RUNNING ? "Running" : "Stopped"; }

std::vector<std::pair<int, Job>> finished_jobs;

/**
 * @brief Checks for status changes in child processes without blocking.
 *
 * Uses wait4() to reap terminated children and gather resource usage.
 * Finished jobs are moved to a separate queue to be reported to the user
 * before the next prompt.
 */
void check_child_status()
{
    int status;
    pid_t pid;
    struct rusage usage;

    while ((pid = wait4(-1, &status, WNOHANG | WUNTRACED | WCONTINUED, &usage)) > 0)
    {
        for (auto it = jobs.begin(); it != jobs.end(); )
        {
            if (it->second.pgid == pid || it->second.pgid == getpgid(pid))
            {
                if (WIFEXITED(status) || WIFSIGNALED(status))
                {
                    // Job has finished
                    it->second.usage = usage;
                    if (WIFEXITED(status)) {
                        int exit_code = WEXITSTATUS(status);
                        it->second.status = (exit_code == 0) ? JobStatus::DONE : JobStatus::EXITED;
                        it->second.term_status = exit_code;
                    } else { // WIFSIGNALED
                        it->second.status = JobStatus::SIGNALED;
                        it->second.term_status = WTERMSIG(status);
                    }
                }

                else if (WIFSTOPPED(status))
                {
                    // Job has stopped
                    if (isatty(STDIN_FILENO))
                        std::cout << "\n[" << it->first << "]+ Stopped\t" << it->second.command << std::endl;
                    it->second.status = JobStatus::STOPPED;
                    
                    // PERBAIKAN: Update job tracking ketika job di-stop
                    if (it->first != current_job_id) {
                        previous_job_id = current_job_id;
                        current_job_id = it->first;
                    }
                    
                    ++it;
                }
                else if (WIFCONTINUED(status))
                {
                    // Job has continued
                    it->second.status = JobStatus::RUNNING;
                    
                    // PERBAIKAN: Update job tracking ketika job di-continue
                    if (it->first != current_job_id) {
                        previous_job_id = current_job_id;
                        current_job_id = it->first;
                    }
                    
                    ++it;
                }
                else
                {
                    ++it;
                }
            }
            else
            {
                ++it;
            }
        }
    }
}

void handle_here_document(const std::string &delimiter)
{
    std::string line;
    safe_set_cooked_mode();
    int temp_fd = open("heredoc.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (temp_fd == -1)
    {
        perror("heredoc temp file");
        exit(1);
    }

    while (true)
    {
        std::cout << "> ";
        std::flush(std::cout);
        if (!std::getline(std::cin, line))
        {
            std::cerr << "\nnsh: warning: here-document delimited by end-of-file (wanted `" << delimiter << "')" << std::endl;
            break;
        }
        if (line == delimiter)
            break;
        write(temp_fd, line.c_str(), line.length());
        write(temp_fd, "\n", 1);
    }
    close(temp_fd);
    safe_set_raw_mode();
}

void handle_redirection(const SimpleCommand &cmd)
{
    if (!cmd.here_string_content.empty())
    {
        int pipe_fd[2];
        pipe(pipe_fd);
        write(pipe_fd[1], cmd.here_string_content.c_str(), cmd.here_string_content.length());
        write(pipe_fd[1], "\n", 1);
        close(pipe_fd[1]);
        dup2(pipe_fd[0], STDIN_FILENO);
        close(pipe_fd[0]);
    }
    else if (!cmd.here_doc_delimiter.empty())
    {
        handle_here_document(cmd.here_doc_delimiter);
        int fd_in = open("heredoc.tmp", O_RDONLY);
        if (fd_in == -1)
        {
            perror("nsh: heredoc");
            exit(1);
        }
        dup2(fd_in, STDIN_FILENO);
        close(fd_in);
        unlink("heredoc.tmp");
    }
    else if (!cmd.stdin_file.empty())
    {
        int fd_in = open(expand_tilde(cmd.stdin_file).c_str(), O_RDONLY);
        if (fd_in == -1)
        {
            perror(("nsh: " + cmd.stdin_file).c_str());
            exit(1);
        }
        dup2(fd_in, STDIN_FILENO);
        close(fd_in);
    }

    if (!cmd.stdout_file.empty())
    {
        int flags = O_WRONLY | O_CREAT | (cmd.append_stdout ? O_APPEND : O_TRUNC);
        int fd_out = open(expand_tilde(cmd.stdout_file).c_str(), flags, 0666);
        if (fd_out == -1)
        {
            perror(("nsh: " + cmd.stdout_file).c_str());
            exit(1);
        }
        dup2(fd_out, STDOUT_FILENO);
        close(fd_out);
    }
}

std::string find_binary(const std::string &cmd)
{
    // Cek jika command adalah path relatif atau absolut
    if (cmd.find('/') != std::string::npos)
    {
        fs::path cmd_path(cmd);

        // Jika path relatif (seperti ./script), ubah ke path absolut
        if (cmd_path.is_relative())
        {
            cmd_path = fs::absolute(cmd_path);
        }

        // Verifikasi file exists dan executable
        if (fs::exists(cmd_path) && fs::is_regular_file(cmd_path) &&
            access(cmd_path.c_str(), X_OK) == 0)
        {
            return cmd_path.string();
        }

        return ""; // Tidak ditemukan atau tidak executable
    }

    // Cek di hash table terlebih dahulu untuk command non-path
    auto is_hashed = binary_hash_loc.find(cmd);
    if (is_hashed != binary_hash_loc.end())
    {
        // Verify the cached path still exists and is executable
        if (fs::exists(is_hashed->second.path) && fs::is_regular_file(is_hashed->second.path) &&
            access(is_hashed->second.path.c_str(), X_OK) == 0)
        {
            // TAMBAH HIT COUNT DI SINI
            binary_hash_loc[cmd].hits += 1;
            return is_hashed->second.path;
        }
        else
        {
            // Remove invalid entry from hash table
            std::cerr << "nsh: hash invalid: " << cmd << " entry not found" << std::endl;
            binary_hash_loc.erase(is_hashed);
            return "";
        }
    }

    const char *path_env = std::getenv("PATH");
    if (!path_env)
        return "";

    std::string path_str = path_env;
    size_t start = 0, end;

    while ((end = path_str.find(':', start)) != std::string::npos)
    {
        std::string dir = path_str.substr(start, end - start);
        if (dir.empty())
        {
            start = end + 1;
            continue;
        }

        fs::path binary_path = fs::path(dir) / cmd;
        if (fs::exists(binary_path) && fs::is_regular_file(binary_path) &&
            access(binary_path.c_str(), X_OK) == 0)
        {
            std::string abs_path = fs::absolute(binary_path).string();
            // TAMBAH KE HASH TABLE DENGAN HIT COUNT 1
            binary_hash_loc[cmd] = {abs_path, cmd, 1};
            return abs_path;
        }
        start = end + 1;
    }

    // Cek direktori terakhir
    std::string dir = path_str.substr(start);
    if (!dir.empty())
    {
        fs::path binary_path = fs::path(dir) / cmd;
        if (fs::exists(binary_path) && fs::is_regular_file(binary_path) &&
            access(binary_path.c_str(), X_OK) == 0)
        {
            std::string abs_path = fs::absolute(binary_path).string();
            // TAMBAH KE HASH TABLE DENGAN HIT COUNT 1
            binary_hash_loc[cmd] = {abs_path, cmd, 1};
            return abs_path;
        }
    }

    return "";
}

std::vector<char*> build_envp() {
    std::vector<char*> envp;
    
    for (const auto& pair : environ_map) {
        if (pair.second.is_exported && !pair.second.value.empty()) {
            std::string env_entry = pair.first + '=' + pair.second.value;
            char* dup_str = safe_strdup(env_entry.c_str());
            if (!dup_str) {
                // Cleanup already allocated strings before returning
                for (char* env : envp) {
                    free(env);
                }
                std::cerr << "nsh: Memory allocation failed for environment" << std::endl;
                return {nullptr};
            }
            envp.push_back(dup_str);
        }
    }
    envp.push_back(nullptr);
    return envp;
}

void launch_process(pid_t pgid, const SimpleCommand &cmd, bool foreground, const std::string &original_cmd_name = "")
{
    // Check if this is a builtin command in a child process
    if (!cmd.tokens.empty() && is_builtin(cmd.tokens[0]))
    {
        // Builtin in pipeline - execute and exit
        handle_redirection(cmd);
        int exit_code = execute_builtin(cmd);
        exit(exit_code);
    }

    pid_t pid = getpid();
    if (pgid == 0)
        pgid = pid;
    setpgid(pid, pgid);

    if (foreground)
        tcsetpgrp(STDIN_FILENO, pgid);

    restore_terminal_mode();

    // Reset sinyal
    for (int sig = 1; sig < NSIG; sig++)
    {
        if (sig == SIGKILL || sig == SIGSTOP)
            continue;
        signal(sig, SIG_DFL);
    }
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGTTIN, SIG_DFL);
    signal(SIGTTOU, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);

    handle_redirection(cmd);

    char **argv = static_cast<char **>(safe_malloc((cmd.tokens.size() + 1) * sizeof(char *)));
    if (!argv)
    {
        std::cerr << "nsh: Memory allocation failed" << std::endl;
        exit(1);
    }

    try
    {
        // Gunakan nama asli command untuk argv[0]
        if (!original_cmd_name.empty())
        {
            argv[0] = safe_strdup(original_cmd_name.c_str());
        }
        else
        {
            // Untuk path, gunakan basename sebagai argv[0]
            if (cmd.tokens[0].find('/') != std::string::npos)
            {
                fs::path cmd_path(cmd.tokens[0]);
                argv[0] = safe_strdup(cmd_path.filename().c_str());
            }
            else
            {
                argv[0] = safe_strdup(cmd.tokens[0].c_str());
            }
        }

        // Token lainnya tetap
        for (size_t i = 1; i < cmd.tokens.size(); ++i)
        {
            argv[i] = safe_strdup(cmd.tokens[i].c_str());
        }
        argv[cmd.tokens.size()] = nullptr;

        for (const auto &[var_name, value] : cmd.env_vars)
        {
            if (cmd.exported_vars.find(var_name) != cmd.exported_vars.end())
            {
                setenv(var_name.c_str(), value.c_str(), 1);
            }
        }
        
        std::vector<char*> envp = build_envp();
        
        /*
        for (int i = 0; argv[i] != nullptr; i++)
        {
          std::cout << "arg$" << i << ": " << argv[i] << std::endl;
        }
        */
        
        std::string PS0_display = get_ps0();
        if (!PS0_display.empty())
        {
          std::cout << PS0_display << "\n";
        }
        // Eksekusi dengan path absolut
        execve(cmd.tokens[0].c_str(), argv, envp.data());
        
        // Safe memory, cleanup
        for (char* env : envp) {
          free(env);
        }

        // Error handling jika execv gagal
        const std::string &error_cmd_name = original_cmd_name.empty() ? cmd.tokens[0] : original_cmd_name;
        bool is_hashed = binary_hash_loc.count(error_cmd_name);

        switch (errno)
        {
        case EACCES:
            if (is_hashed)
                std::cerr << "nsh: hash corrupted: " << error_cmd_name << ": Permission denied" << std::endl;
            else
                std::cerr << "nsh: " << error_cmd_name << ": Permission denied" << std::endl;
            exit(126);
        case ENOENT:
            std::cerr << "nsh: " << error_cmd_name << ": No such file or directory" << std::endl;
            exit(127);
        case ENOEXEC:
            if (is_hashed)
                std::cerr << "nsh: hash corrupted: " << error_cmd_name << ": Exec format error" << std::endl;
            else
                std::cerr << "nsh: " << error_cmd_name << ": Exec format error" << std::endl;
            exit(126);
        case EISDIR:
            if (is_hashed)
                std::cerr << "nsh: hash corrupted: " << error_cmd_name << ": Is a directory" << std::endl;
            else
                std::cerr << "nsh: " << error_cmd_name << ": Is a directory" << std::endl;
            exit(126);
        case ENOTDIR:
            std::cerr << "nsh: " << error_cmd_name << ": Not a directory" << std::endl;
            exit(126);
        case E2BIG:
            std::cerr << "nsh: " << error_cmd_name << ": Argument list too long" << std::endl;
            exit(126);
        case ELOOP:
            std::cerr << "nsh: " << error_cmd_name << ": Too many levels of symbolic links" << std::endl;
            exit(126);
        default:
            std::cerr << "nsh: " << error_cmd_name << ": " << strerror(errno) << std::endl;
            exit(126);
        }
    }
    catch (const std::bad_alloc &)
    {
        for (size_t i = 0; i < cmd.tokens.size() && argv[i]; ++i)
        {
            free(argv[i]);
        }
        free(argv);
        std::cerr << "nsh: Memory allocation failed" << std::endl;
        exit(1);
    }
}

bool is_builtin(const std::string &command)
{
    static const std::set<std::string> builtins = {
        "exit", "cd", "alias", "unalias", "history", "pwd",
        "jobs", "fg", "bg", "kill", "export", "bookmark", "exec", "unset", "hash", "type"};
    return builtins.count(command);
}

void handle_builtin_type(const std::vector<std::string>& tokens) {
    if (tokens.size() < 2) {
        std::cerr << "nsh: type: usage: type [-a] name [name ...]" << std::endl;
        last_exit_code = 1;
        return;
    }

    bool find_all = false;
    std::vector<std::string> names_to_check;

    // Periksa opsi -a
    size_t start_index = 1;
    if (tokens[1] == "-a") {
        find_all = true;
        start_index = 2;
    }
    
    // Ambil nama-nama yang akan diperiksa
    for (size_t i = start_index; i < tokens.size(); ++i) {
        names_to_check.push_back(tokens[i]);
    }

    if (names_to_check.empty()) {
        std::cerr << "nsh: type: usage: type [-a] name [name ...]" << std::endl;
        last_exit_code = 1;
        return;
    }

    last_exit_code = 0; // Atur default exit code ke 0

    for (const auto& name : names_to_check) {
        bool found = false;

        // 1. Cek sebagai alias
        auto alias_it = aliases.find(name);
        if (alias_it != aliases.end()) {
            std::cout << name << " is aliased to `" << alias_it->second << "`" << std::endl;
            found = true;
            if (!find_all) continue;
        }

        // 2. Cek sebagai builtin
        if (is_builtin(name)) {
            std::cout << name << " is a shell builtin" << std::endl;
            found = true;
            if (!find_all) continue;
        }

        // 3. Cek sebagai variabel environment
        auto env_it = environ_map.find(name);
        if (env_it != environ_map.end()) {
            const auto& var_info = env_it->second;
            
            if (var_info.is_default && var_info.is_exported) {
                std::cout << name << " is a default and exported variable" << std::endl;
            } else if (var_info.is_default) {
                std::cout << name << " is a default variable" << std::endl;
            } else if (var_info.is_exported) {
                std::cout << name << " is an exported variable" << std::endl;
            } else {
                std::cout << name << " is a session variable" << std::endl;
            }
            found = true;
            if (!find_all) continue;
        }

        // 4. Cek sebagai bookmark
        std::ifstream bookmark_file(ns_BOOKMARK_FILE);
        if (bookmark_file.is_open()) {
            std::string line;
            while (std::getline(bookmark_file, line)) {
                size_t space_pos = line.find(' ');
                if (space_pos != std::string::npos) {
                    std::string bookmark_name = line.substr(0, space_pos);
                    std::string bookmark_path = line.substr(space_pos + 1);
                    
                    if (bookmark_name == name) {
                        std::cout << name << " is a bookmark to `" << bookmark_path << "`" << std::endl;
                        found = true;
                        bookmark_file.close();
                        if (!find_all) break;
                        continue; // Lanjut ke pencarian berikutnya jika -a
                    }
                }
            }
            bookmark_file.close();
            if (found && !find_all) continue;
        }

        // 5. Cek sebagai fungsi shell (tidak diimplementasikan di sini, tapi bisa ditambahkan)
        // Saat ini tidak ada kode untuk fungsi shell di file yang diberikan.

        // 6. Cek sebagai binary/program eksternal (menggunakan PATH)
        // Periksa apakah sudah ada di hash table
        auto hash_it = binary_hash_loc.find(name);
        if (hash_it != binary_hash_loc.end()) {
            std::cout << name << " is hashed (" << hash_it->second.path << ")" << std::endl;
            found = true;
            if (!find_all) continue;
        } else {
            // Jika tidak ada di hash, cari di PATH
            std::string path_env = get_env_var("PATH");
            if (!path_env.empty()) {
                std::stringstream ss(path_env);
                std::string path_dir;
                bool path_found = false;
                
                while (std::getline(ss, path_dir, ':')) {
                    if (path_dir.empty()) continue;
                    
                    fs::path binary_path = fs::path(path_dir) / name;
                    if (fs::exists(binary_path) && fs::is_regular_file(binary_path)) {
                        std::cout << name << " is " << binary_path.string() << std::endl;
                        found = true;
                        path_found = true;
                        if (!find_all) break;
                    }
                }
                if (path_found && !find_all) continue;
            }
        }

        // Jika tidak ditemukan
        if (!found) {
            std::cerr << "nsh: type: " << name << ": not found" << std::endl;
            last_exit_code = 1;
        }
    }
}


int execute_builtin(const SimpleCommand &cmd)
{
    std::map<std::string, std::string> original_env;

    for (const auto &[var_name, value] : cmd.env_vars)
    {
        const char *current_val = getenv(var_name.c_str());
        if (current_val)
            original_env[var_name] = current_val;
    }

    for (const auto &[var_name, value] : cmd.env_vars)
    {
        if (cmd.exported_vars.find(var_name) != cmd.exported_vars.end())
        {
            setenv(var_name.c_str(), value.c_str(), 1);
        }
    }

    const auto &tokens = cmd.tokens;
    if (tokens.empty())
        return 0;

    if (tokens[0] == "exit")
    {
        save_history();
        exit(tokens.size() > 1 ? std::stoi(tokens[1]) : 0);
    }
    else if (tokens[0] == "type")
    {
        handle_builtin_type(tokens);
    }
    else if (tokens[0] == "cd")
    {
        handle_builtin_cd(tokens);
    }
    else if (tokens[0] == "pwd")
    {
        handle_builtin_pwd(tokens);
    }
    else if (tokens[0] == "alias")
    {
        handle_builtin_alias(tokens);
    }
    else if (tokens[0] == "unalias")
    {
        handle_builtin_unalias(tokens);
    }
    else if (tokens[0] == "history")
    {
        handle_builtin_history(tokens);
    }
    else if (tokens[0] == "export")
    {
        handle_builtin_export(tokens);
    }
    else if (tokens[0] == "exec")
    {
      handle_builtin_exec(tokens);
    }
    else if (tokens[0] == "unset")
    {
      handle_builtin_unset(tokens);
    }
    else if (tokens[0] == "bookmark")
    {
        handle_builtin_bookmark(tokens);
    }
    else if (tokens[0] == "hash")
    {
        handle_builtin_hash(tokens);
    }
    else if (tokens[0] == "jobs")
    {
       handle_builtin_jobs(tokens);
    }
    else if (tokens[0] == "kill")
    {
       handle_builtin_kill(tokens);
    }
    else if (tokens[0] == "bg")
    {
       handle_builtin_bg(tokens);
    }
    else if (tokens[0] == "fg")
    {
       handle_builtin_fg(tokens);
    }
    
    for (const auto &[var_name, value] : cmd.env_vars)
    {
        if (original_env.count(var_name))
        {
            setenv(var_name.c_str(), original_env[var_name].c_str(), 1);
        }
        else
        {
            unsetenv(var_name.c_str());
        }
    }

    return last_exit_code;
}

int execute_job(const ParsedCommand &cmd_group)
{
    if (cmd_group.pipeline.empty())
        return 0;

    // Handle builtin commands in pipeline
    if (cmd_group.pipeline.size() == 1 &&
        !cmd_group.pipeline[0].tokens.empty() &&
        is_builtin(cmd_group.pipeline[0].tokens[0]))
    {

        const auto &simple_cmd = cmd_group.pipeline[0];

        // Save original file descriptors
        int stdin_backup = dup(STDIN_FILENO);
        int stdout_backup = dup(STDOUT_FILENO);

        // Apply redirection for the builtin
        handle_redirection(simple_cmd);

        // Execute the builtin
        int code = execute_builtin(simple_cmd);

        // Restore original file descriptors
        dup2(stdin_backup, STDIN_FILENO);
        dup2(stdout_backup, STDOUT_FILENO);
        close(stdin_backup);
        close(stdout_backup);

        return code;
    }

    int in_fd = STDIN_FILENO, pipe_fd[2];
    pid_t pgid = 0;
    std::vector<pid_t> pids;

    // Salin pipeline agar bisa dimodifikasi dan simpan nama command asli
    std::vector<SimpleCommand> pipeline_with_paths = cmd_group.pipeline;
    std::vector<std::string> original_cmd_names;

    // Lakukan path resolution di parent untuk memberikan error yang lebih baik
    for (auto &simple_cmd : pipeline_with_paths)
    {
        if (simple_cmd.tokens.empty() || is_builtin(simple_cmd.tokens[0]))
        {
            original_cmd_names.push_back(""); // Kosong untuk builtin
            continue;
        }

        std::string original_name = simple_cmd.tokens[0];
        std::string binary_path;

        // Jika command adalah path eksplisit (mengandung '/')
        if (original_name.find('/') != std::string::npos)
        {
            struct stat path_stat;
            if (stat(original_name.c_str(), &path_stat) != 0)
            {
                std::cerr << "nsh: " << original_name << ": " << strerror(errno) << std::endl;
                return (errno == ENOENT) ? 127 : 126;
            }

            if (S_ISDIR(path_stat.st_mode))
            {
                std::cerr << "nsh: " << original_name << ": Is a directory" << std::endl;
                return 126;
            }

            if (access(original_name.c_str(), X_OK) != 0)
            {
                if (errno == ELOOP)
                    std::cerr << "nsh: " << original_name << ": Too many levels of symbolic links" << std::endl;
                else
                    std::cerr << "nsh: " << original_name << ": Permission denied" << std::endl;
                return 126;
            }
            binary_path = fs::absolute(original_name).string();
        }
        else
        {
            // Jika bukan path, cari menggunakan find_binary (yang juga menangani hash)
            binary_path = find_binary(original_name);

            if (binary_path.empty())
            {
                std::cerr << "nsh: " << original_name << ": command not found" << std::endl;
                return 127;
            }
        }
        simple_cmd.tokens[0] = binary_path;
        original_cmd_names.push_back(original_name);
    }

    for (size_t i = 0; i < pipeline_with_paths.size(); ++i)
    {
        const auto &simple_cmd = pipeline_with_paths[i];
        const std::string &original_name = original_cmd_names[i];
        bool is_last = (i == pipeline_with_paths.size() - 1);

        if (!is_last) {
          if (pipe(pipe_fd) < 0) {
            // Cleanup resources sebelum return
            if (in_fd != STDIN_FILENO) close(in_fd);
              for (pid_t existing_pid : pids) {
              kill(existing_pid, SIGKILL);  // Cleanup any already forked processes
              }
        
            switch (errno) {
              case EPIPE:
                std::cerr << "nsh: pipe: broken pipe" << std::endl;
                break;
              case EMFILE:
                std::cerr << "nsh: pipe: too many open files" << std::endl;
                break;
              case ENFILE:
                std::cerr << "nsh: pipe: system file table overflow" << std::endl;
                break;
              case EFAULT:
                std::cerr << "nsh: pipe: invalid pipe buffer address" << std::endl;
                break;
              case ENOMEM:
                std::cerr << "nsh: pipe: cannot allocate memory" << std::endl;
                break;
              default:
                perror("nsh: pipe");
            }
            return 1;
          }
        }

        pid_t pid = fork();
        if (pid < 0)
        {
            if (errno == EAGAIN)
                std::cerr << "nsh: fork: Resource temporarily unavailable" << std::endl;
            else if (errno == ENOMEM)
                std::cerr << "nsh: fork: Cannot allocate memory" << std::endl;
            else
                perror("nsh: fork");
            return 1;
        }

        if (pid == 0)
        {
            if (in_fd != STDIN_FILENO)
            {
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }
            if (!is_last)
            {
                close(pipe_fd[0]);
                dup2(pipe_fd[1], STDOUT_FILENO);
                close(pipe_fd[1]);
            }

            if (!simple_cmd.tokens.empty() && is_builtin(simple_cmd.tokens[0]))
            {
                int exit_code = execute_builtin(simple_cmd);
                exit(exit_code);
            }
            else
            {
                launch_process(pgid, simple_cmd, !cmd_group.background, original_name);
            }
        }
        else
        {
            pids.push_back(pid);
            if (pgid == 0)
                pgid = pid;
            setpgid(pid, pgid);
            if (in_fd != STDIN_FILENO)
                close(in_fd);
            if (!is_last)
            {
                close(pipe_fd[1]);
                in_fd = pipe_fd[0];
            }
        }
    }

    if (in_fd != STDIN_FILENO)
        close(in_fd);

    if (cmd_group.background)
    {
        std::string command_str;
        for (const auto &sc : cmd_group.pipeline)
        {
            for (const auto &token : sc.tokens)
                command_str += token + " ";
            if (&sc != &cmd_group.pipeline.back())
                command_str += "| ";
        }
        
        // PERBAIKAN: Update job tracking
        previous_job_id = current_job_id;  // Simpan current job sebelumnya
        current_job_id = next_job_id;      // Set current job ke yang baru
        
        jobs[next_job_id] = {pgid, command_str, JobStatus::RUNNING};
        std::cout << "[" << next_job_id << "] " << pgid << std::endl;
        next_job_id++;
        
        // Update last launched job
        last_launched_job_id = current_job_id;
        
        return 0;
    }
    else
    {
        if (isatty(STDIN_FILENO)) {
            tcsetpgrp(STDIN_FILENO, pgid);
        }
        foreground_pgid = pgid;
        int status = 0;
        for (size_t i = 0; i < pids.size(); ++i)
        {
            int current_status;
            waitpid(pids[i], &current_status, WUNTRACED);
            if (i == pids.size() - 1)
                status = current_status;
        }
        if (WIFSTOPPED(status))
        {
            std::string command_str;
            for (const auto &sc : cmd_group.pipeline)
            {
                for (const auto &token : sc.tokens)
                    command_str += token + " ";
                if (&sc != &cmd_group.pipeline.back())
                    command_str += "| ";
            }
            
            // PERBAIKAN: Update job tracking untuk job yang stopped
            previous_job_id = current_job_id;
            current_job_id = next_job_id;
            
            jobs[next_job_id] = {pgid, command_str, JobStatus::STOPPED};
            std::cout << "\n[" << next_job_id << "]+ Stopped\t" << command_str << std::endl;
            next_job_id++;
            
            last_launched_job_id = current_job_id;
        }
        foreground_pgid = 0;
        tcsetpgrp(STDIN_FILENO, shell_pgid);
        if (WIFEXITED(status))
            return WEXITSTATUS(status);
        if (WIFSIGNALED(status))
            return WTERMSIG(status) + 128;
        return 1;
    }
}


int execute_command_list(const std::vector<ParsedCommand> &commands) 
{
    if (commands.empty())
        return 0;
    int current_exit_code = 0;
    for (size_t i = 0; i < commands.size(); ++i) {
        // Check for interrupt before executing each command
        if (EOF_IN_interrupt) {
            return 130; // SIGINT exit code
        }
        
        const auto &cmd_group = commands[i];
        if (i > 0) {
            const auto &prev_op = commands[i - 1].next_operator;
            if ((prev_op == ParsedCommand::Operator::AND && current_exit_code != 0) ||
                (prev_op == ParsedCommand::Operator::OR && current_exit_code == 0))
                continue;
        }
        last_exit_code = current_exit_code;
        if (cmd_group.pipeline.empty())
            continue;
        if (cmd_group.pipeline.size() == 1 && cmd_group.pipeline[0].tokens.empty() && !cmd_group.pipeline[0].env_vars.empty()) {
            for (const auto &[var, val] : cmd_group.pipeline[0].env_vars)
                setenv(var.c_str(), val.c_str(), 1);
            current_exit_code = 0;
            continue;
        }
        current_exit_code = execute_job(cmd_group);
        
        // Check for interrupt after executing each command
        if (EOF_IN_interrupt) {
            return 130;
        }
    }
    return current_exit_code;
}

int execute_subshell_direct(const std::string& command) {
    Parser parser;
    try {
        auto commands = parser.parse(command);
        if (!commands.empty()) {
            return execute_command_list(commands);
        }
    } catch (const std::exception &e) {
        std::cerr << "nsh: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}