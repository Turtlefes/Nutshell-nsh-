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
#include <utils.h>
#include <filesystem>
#include <unistd.h>
#include <unordered_map>

namespace fs = std::filesystem;

std::string job_status_to_string(JobStatus status) { return status == JobStatus::RUNNING ? "Running" : "Stopped"; }

void check_child_status()
{
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0)
    {
        for (auto it = jobs.begin(); it != jobs.end();)
        {
            if (it->second.pgid == pid || it->second.pgid == getpgid(pid))
            {
                if (WIFEXITED(status) || WIFSIGNALED(status))
                {
                    if (isatty(STDIN_FILENO))
                        std::cout << "\n[" << it->first << "]+ Done\t" << it->second.command << std::endl;
                    it = jobs.erase(it);
                }
                else if (WIFSTOPPED(status))
                {
                    if (isatty(STDIN_FILENO))
                        std::cout << "\n[" << it->first << "]+ Stopped\t" << it->second.command << std::endl;
                    it->second.status = JobStatus::STOPPED;
                    ++it;
                }
                else if (WIFCONTINUED(status))
                {
                    it->second.status = JobStatus::RUNNING;
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



/*
std::string find_binary(const std::string &cmd) {
    const char* path_env = std::getenv("PATH");
    if (!path_env) return "";

    std::string path_str(path_env);
    size_t start = 0, end;

    while ((end = path_str.find(':', start)) != std::string::npos) {
        std::string dir = path_str.substr(start, end - start);
        fs::path p = fs::path(dir) / cmd;
        if (fs::exists(p) && fs::is_regular_file(p) &&
            ((fs::status(p).permissions() & fs::perms::owner_exec) != fs::perms::none)) {
            return p.string();
        }
        start = end + 1;
    }

    // cek direktori terakhir setelah ':'
    std::string dir = path_str.substr(start);
    fs::path p = fs::path(dir) / cmd;
    if (fs::exists(p) && fs::is_regular_file(p) &&
        ((fs::status(p).permissions() & fs::perms::owner_exec) != fs::perms::none)) {
        return p.string();
    }

    return ""; // tidak ketemu
}
*/


std::string find_binary(const std::string &cmd)
{
    // Cek di hash table terlebih dahulu
    auto is_hashed = binary_hash_loc.find(cmd);
    if (is_hashed != binary_hash_loc.end()) {
        // Verify the cached path still exists and is executable
        if (fs::exists(is_hashed->second) && fs::is_regular_file(is_hashed->second) && 
            access(is_hashed->second.c_str(), X_OK) == 0) {
            return is_hashed->second;
        } else {
            // Remove invalid entry from hash table
            binary_hash_loc.erase(is_hashed);
        }
    }
    
    const char* path_env = std::getenv("PATH");
    if (!path_env) return "";
    
    std::string path_str = path_env;
    size_t start = 0, end;
    
    while ((end = path_str.find(':', start)) != std::string::npos) {
        std::string dir = path_str.substr(start, end - start);
        if (dir.empty()) {
            start = end + 1;
            continue;
        }
        
        fs::path binary_path = fs::path(dir) / cmd;
        if (fs::exists(binary_path) && fs::is_regular_file(binary_path) && 
            access(binary_path.c_str(), X_OK) == 0) {
            std::string abs_path = fs::absolute(binary_path).string();
            binary_hash_loc[cmd] = abs_path; // Add to hash table
            return abs_path;
        }
        start = end + 1;
    }
    
    // Cek direktori terakhir
    std::string dir = path_str.substr(start);
    if (!dir.empty()) {
        fs::path binary_path = fs::path(dir) / cmd;
        if (fs::exists(binary_path) && fs::is_regular_file(binary_path) && 
            access(binary_path.c_str(), X_OK) == 0) {
            std::string abs_path = fs::absolute(binary_path).string();
            binary_hash_loc[cmd] = abs_path; // Add to hash table
            
            return abs_path;
        }
    }
    
    return "";
}

void launch_process(pid_t pgid, const SimpleCommand &cmd, bool foreground, const std::string& original_cmd_name = "") {
    pid_t pid = getpid();
    if (pgid == 0)
        pgid = pid;
    setpgid(pid, pgid);

    if (foreground)
        tcsetpgrp(STDIN_FILENO, pgid);

    restore_terminal_mode();

    // Reset sinyal
    for (int sig = 1; sig < NSIG; sig++) {
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

    char** argv = static_cast<char**>(safe_malloc((cmd.tokens.size() + 1) * sizeof(char*)));
    if (!argv) {
        exit(1);
    }
    
    try {
        // Gunakan nama asli command untuk argv[0]
        if (!original_cmd_name.empty()) {
            argv[0] = safe_strdup(original_cmd_name.c_str());
        } else {
            argv[0] = safe_strdup(cmd.tokens[0].c_str());
        }
        
        // Token lainnya tetap
        for (size_t i = 1; i < cmd.tokens.size(); ++i) {
            argv[i] = safe_strdup(cmd.tokens[i].c_str());
        }
        argv[cmd.tokens.size()] = nullptr;

        for (const auto &[var_name, value] : cmd.env_vars) {
            if (cmd.exported_vars.find(var_name) != cmd.exported_vars.end()) {
                setenv(var_name.c_str(), value.c_str(), 1);
            }
        }

        // Eksekusi dengan path absolut, tapi argv[0] berisi nama asli
        execv(cmd.tokens[0].c_str(), argv);
        
        // Error handling
        std::cerr << "nsh: " << cmd.tokens[0] << ": " << strerror(errno) << std::endl;
        exit(126);

    } catch (const std::bad_alloc&) {
        for (size_t i = 0; i < cmd.tokens.size() && argv[i]; ++i) {
            free(argv[i]);
        }
        free(argv);
        exit(1);
    }
}

bool is_builtin(const std::string &command)
{
    static const std::set<std::string> builtins = {
        "exit", "cd", "alias", "unalias", "history", "pwd",
        "jobs", "fg", "bg", "clear", "export", "bookmark", "exec", "unset", "hash"};
    return builtins.count(command);
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
    else if (tokens[0] == "clear")
    {
        clear_screen(); // ini dari utils cok!
        last_exit_code = 0;
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
        for (const auto &[id, job] : jobs)
        {
            std::cout << "[" << id << "]\t"
                      << job_status_to_string(job.status) << "\t"
                      << job.command << std::endl;
        }
        last_exit_code = 0;
    }
    else if (tokens[0] == "fg" || tokens[0] == "bg")
    {
        if (tokens.size() < 2)
        {
            std::cerr << "nsh: " << tokens[0] << ": job ID required" << std::endl;
            last_exit_code = 1;
        }
        else
        {
            try
            {
                int job_id = std::stoi(tokens[1].substr(tokens[1][0] == '%' ? 1 : 0));
                if (jobs.count(job_id))
                {
                    pid_t pgid = jobs[job_id].pgid;
                    if (kill(-pgid, SIGCONT) < 0)
                    {
                        perror("kill (SIGCONT)");
                        last_exit_code = 1;
                    }
                    else
                    {
                        jobs[job_id].status = JobStatus::RUNNING;
                        if (tokens[0] == "fg")
                        {
                            foreground_pgid = pgid;
                            int status = wait_for_job(pgid);
                            if (WIFSTOPPED(status))
                            {
                                jobs[job_id].status = JobStatus::STOPPED;
                                std::cout << "\n[" << job_id << "]+ Stopped\t"
                                          << jobs[job_id].command << std::endl;
                            }
                            else
                                jobs.erase(job_id);
                            foreground_pgid = 0;
                            last_exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
                        }
                        else
                        {
                            std::cout << "[" << job_id << "]\t" << jobs[job_id].command << " &" << std::endl;
                            last_exit_code = 0;
                        }
                    }
                }
                else
                {
                    std::cerr << "nsh: " << tokens[0] << ": job not found: " << job_id << std::endl;
                    last_exit_code = 1;
                }
            }
            catch (...)
            {
                std::cerr << "nsh: " << tokens[0] << ": invalid job ID: " << tokens[1] << std::endl;
                last_exit_code = 1;
            }
        }
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

    // Built-in handling tetap sama
    if (cmd_group.pipeline.size() == 1 && !cmd_group.pipeline[0].tokens.empty() && is_builtin(cmd_group.pipeline[0].tokens[0]))
    {
        const auto &simple_cmd = cmd_group.pipeline[0];
        int stdin_backup = dup(STDIN_FILENO), stdout_backup = dup(STDOUT_FILENO);
        handle_redirection(simple_cmd);
        int code = execute_builtin(simple_cmd);
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

    // Lakukan path resolution di parent
    for (auto &simple_cmd : pipeline_with_paths) {
        if (simple_cmd.tokens.empty() || is_builtin(simple_cmd.tokens[0])) {
            original_cmd_names.push_back(""); // Kosong untuk builtin
            continue;
        }
        
        // Simpan nama command asli sebelum diganti
        std::string original_name = simple_cmd.tokens[0];
        std::string binary_path = find_binary(original_name);
        
        if (binary_path.empty()) {
            std::cerr << "nsh: " << original_name << ": command not found" << std::endl;
            return 127; // Kode exit standar untuk command not found
        }
        
        // Ganti nama command dengan path absolutnya untuk execv
        simple_cmd.tokens[0] = binary_path;
        original_cmd_names.push_back(original_name); // Simpan nama asli
    }

    for (size_t i = 0; i < pipeline_with_paths.size(); ++i)
    {
        const auto &simple_cmd = pipeline_with_paths[i];
        const std::string &original_name = original_cmd_names[i];
        bool is_last = (i == pipeline_with_paths.size() - 1);

        if (!is_last)
        {
            if (pipe(pipe_fd) < 0)
            {
                perror("pipe");
                return 1;
            }
        }

        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
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
            // Teruskan nama command asli ke launch_process
            launch_process(pgid, simple_cmd, !cmd_group.background, original_name);
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
    
    // Pastikan menutup file descriptor terakhir
    if (in_fd != STDIN_FILENO)
        close(in_fd);

    // Sisa fungsi (job control, background/foreground) tetap sama
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
        jobs[next_job_id] = {pgid, command_str, JobStatus::RUNNING};
        std::cout << "[" << next_job_id << "] " << pgid << std::endl;
        next_job_id++;
        return 0;
    }
    else
    {
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
            jobs[next_job_id] = {pgid, command_str, JobStatus::STOPPED};
            std::cout << "\n[" << next_job_id << "]+ Stopped\t" << command_str << std::endl;
            next_job_id++;
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
    for (size_t i = 0; i < commands.size(); ++i)
    {
        const auto &cmd_group = commands[i];
        if (i > 0)
        {
            const auto &prev_op = commands[i - 1].next_operator;
            if ((prev_op == ParsedCommand::Operator::AND && current_exit_code != 0) ||
                (prev_op == ParsedCommand::Operator::OR && current_exit_code == 0))
                continue;
        }
        if (cmd_group.pipeline.empty())
            continue;
        if (cmd_group.pipeline.size() == 1 && cmd_group.pipeline[0].tokens.empty() && !cmd_group.pipeline[0].env_vars.empty())
        {
            for (const auto &[var, val] : cmd_group.pipeline[0].env_vars)
                setenv(var.c_str(), val.c_str(), 1);
            current_exit_code = 0;
            continue;
        }
        current_exit_code = execute_job(cmd_group);
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
