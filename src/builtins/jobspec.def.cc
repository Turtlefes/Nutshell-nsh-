// builtins/jobs_impl.cc

#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <csignal>
#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <unordered_set>

#include <unistd.h>
#include <termios.h>

#include "execution.h" // validate_and_cleanup_jobs

namespace fs = std::filesystem;

// Struktur untuk menyimpan informasi job (lokal atau eksternal)
struct DisplayJobInfo {
    int job_id;
    pid_t pgid;
    pid_t shell_pid;
    std::string command;
    JobStatus status;
    int term_status;
    struct rusage usage;
    struct timeval start_tv;
    bool is_current_session;
    std::string session_display_name;
};

/**
 * @brief Memeriksa apakah job masih aktif (process group masih ada)
 */
bool is_job_alive(pid_t pgid) {
    // Kirim sinyal 0 untuk memeriksa apakah process group masih ada
    if (kill(-pgid, 0) == 0) {
        return true;
    }
    return errno != ESRCH;
}

/**
 * @brief Memperbarui status job dari sistem process
 */
void update_job_status_from_system(DisplayJobInfo& job) {
    if (!is_job_alive(job.pgid)) {
        job.status = JobStatus::DONE;
        return;
    }

    // Periksa status process group dengan lebih akurat
    int status;
    pid_t result = waitpid(-job.pgid, &status, WNOHANG | WUNTRACED | WCONTINUED);
    
    if (result > 0) {
        if (WIFEXITED(status)) {
            job.status = (WEXITSTATUS(status) == 0) ? JobStatus::DONE : JobStatus::EXITED;
            job.term_status = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            job.status = JobStatus::SIGNALED;
            job.term_status = WTERMSIG(status);
        } else if (WIFSTOPPED(status)) {
            job.status = JobStatus::STOPPED;
            job.term_status = WSTOPSIG(status);
        } else if (WIFCONTINUED(status)) {
            job.status = JobStatus::RUNNING;
        }
    } else if (result == 0) {
        // Process masih berjalan - cek apakah realmente running atau stopped
        // Untuk job yang di-stop, biasanya waitpid dengan WUNTRACED akan mengembalikan status stopped
        if (job.status != JobStatus::STOPPED) {
            job.status = JobStatus::RUNNING;
        }
    }
}

/**
 * @brief Membaca status job dari file kontrol.
 */
DisplayJobInfo read_job_controle_file(const fs::path& file_path, int temp_job_id, pid_t current_shell_pid) {
    DisplayJobInfo info = {};
    info.job_id = temp_job_id;
    info.status = JobStatus::UNKNOWN;
    
    std::ifstream ifs(file_path);
    std::string line;
    while (std::getline(ifs, line)) {
        size_t eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;
        
        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);
        
        try {
            if (key == "PGID") info.pgid = std::stoi(value);
            else if (key == "SHELL_PID") {
                info.shell_pid = std::stoi(value);
                info.is_current_session = (info.shell_pid == current_shell_pid);
            }
            else if (key == "COMMAND") info.command = value;
            else if (key == "STATUS") info.status = static_cast<JobStatus>(std::stoi(value));
            else if (key == "TERM_STATUS") info.term_status = std::stoi(value);
            else if (key == "RUTIME_SEC") info.usage.ru_utime.tv_sec = std::stoi(value);
            else if (key == "RUTIME_USEC") info.usage.ru_utime.tv_usec = std::stoi(value);
            else if (key == "RSTIME_SEC") info.usage.ru_stime.tv_sec = std::stoi(value);
            else if (key == "RSTIME_USEC") info.usage.ru_stime.tv_usec = std::stoi(value);
            else if (key == "START_TIME_SEC") info.start_tv.tv_sec = std::stoi(value);
            else if (key == "START_TIME_USEC") info.start_tv.tv_usec = std::stoi(value);
        } catch (const std::exception& e) {
            std::cerr << "nsh: jobs: error parsing control file: " << e.what() << std::endl;
        }
    }
    
    if (info.is_current_session) {
        info.session_display_name = "current";
    } else {
        info.session_display_name = std::to_string(info.shell_pid);
    }

    return info;
}

/**
 * @brief Menentukan job current dan previous berdasarkan status dan waktu
 */
void determine_current_and_previous_jobs(std::vector<DisplayJobInfo>& jobs) {
    current_job_id = 0;
    previous_job_id = 0;
    
    // Filter hanya job dari session current yang running/stopped
    std::vector<DisplayJobInfo> eligible_jobs;
    for (const auto& job : jobs) {
        if (job.is_current_session && 
            (job.status == JobStatus::RUNNING || job.status == JobStatus::STOPPED)) {
            eligible_jobs.push_back(job);
        }
    }
    
    if (eligible_jobs.empty()) {
        return;
    }
    
    // Urutkan berdasarkan job_id (terbaru pertama)
    std::sort(eligible_jobs.begin(), eligible_jobs.end(),
              [](const DisplayJobInfo& a, const DisplayJobInfo& b) {
                  return a.job_id > b.job_id;
              });
    
    // Job terbaru adalah current job
    current_job_id = eligible_jobs[0].job_id;
    
    // Job kedua terbaru adalah previous job
    if (eligible_jobs.size() >= 2) {
        previous_job_id = eligible_jobs[1].job_id;
    }
}

/**
 * @brief Mendapatkan semua job yang aktif di sistem (cross-session) tanpa duplikasi
 */
std::vector<DisplayJobInfo> get_all_active_jobs(pid_t current_shell_pid) {
    std::vector<DisplayJobInfo> all_jobs;
    std::unordered_set<pid_t> seen_pgids;
    int next_display_id = 1;

    // 1. Process Local Jobs (from 'jobs' map)
    for (const auto& [id, job] : jobs) {
        if (seen_pgids.count(job.pgid)) {
            continue;
        }
        seen_pgids.insert(job.pgid);

        DisplayJobInfo info = {};
        info.job_id = id;
        info.pgid = job.pgid;
        info.shell_pid = current_shell_pid;
        info.command = job.command;
        info.status = job.status;
        info.term_status = job.term_status;
        info.usage = job.usage;
        info.start_tv = job.start_tv;
        info.is_current_session = true;
        info.session_display_name = "current";

        update_job_status_from_system(info);
        
        all_jobs.push_back(info);
    }

    // 2. Process External Jobs (from control files) - Cross Session
    fs::path job_dir = ns_CONFIG_DIR / "jobs";
    if (fs::exists(job_dir)) {
        for (const auto& entry : fs::directory_iterator(job_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".controle") {
                DisplayJobInfo info = read_job_controle_file(entry.path(), next_display_id++, current_shell_pid);
                
                if (seen_pgids.count(info.pgid)) {
                    continue;
                }
                seen_pgids.insert(info.pgid);
                
                update_job_status_from_system(info);
                
                if (info.status != JobStatus::DONE && info.status != JobStatus::EXITED && info.status != JobStatus::SIGNALED) {
                    all_jobs.push_back(info);
                }
            }
        }
    }

    // Tentukan current dan previous jobs
    determine_current_and_previous_jobs(all_jobs);

    return all_jobs;
}

/**
 * @brief Menghitung CPU Usage Percentage.
 */
std::string format_cpu_percentage(const struct rusage& usage, const struct timeval& start_tv) {
    double cpu_time = (double)usage.ru_utime.tv_sec + (double)usage.ru_utime.tv_usec / 1e6 +
                      (double)usage.ru_stime.tv_sec + (double)usage.ru_stime.tv_usec / 1e6;

    struct timeval now;
    gettimeofday(&now, nullptr);
    double wall_time = (double)now.tv_sec + (double)now.tv_usec / 1e6 -
                       ((double)start_tv.tv_sec + (double)start_tv.tv_usec / 1e6);

    if (wall_time <= 0.0) return "0.0%";

    double cpu_percent = (cpu_time / wall_time) * 100.0;
    
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1) << cpu_percent << "%";
    return ss.str();
}

/**
 * @brief Mengkonversi JobStatus ke karakter state ala ps dengan navigasi +/-
 */
std::string get_ps_like_state(JobStatus status, int job_id) {
    std::string state;
    
    switch (status) {
        case JobStatus::RUNNING: state = "Running"; break;
        case JobStatus::STOPPED: state = "Stopped"; break;
        case JobStatus::DONE: state = "Done"; break;
        case JobStatus::EXITED: state = "Exited"; break;
        case JobStatus::SIGNALED: state = "Signaled"; break;
        default: state = "Unknown"; break;
    }
    
    // Tambahkan navigasi + dan - seperti di bash
    if (job_id == current_job_id) {
        state += " +";
    } else if (job_id == previous_job_id) {
        state += " -";
    }
    
    return state;
}

/**
 * @brief Mengkonversi JobStatus ke karakter state singkat ala ps command
 */
std::string get_ps_short_state(JobStatus status, pid_t pgid) {
    std::string state;
    
    pid_t fg_pgid = isatty(STDIN_FILENO) ? tcgetpgrp(STDIN_FILENO) : 0;
    
    switch (status) {
        case JobStatus::RUNNING: 
            state = "R"; 
            break;
        case JobStatus::STOPPED: 
            state = "T"; 
            break;
        case JobStatus::DONE: 
            state = "D"; 
            break;
        case JobStatus::EXITED: 
            state = "X"; 
            break;
        case JobStatus::SIGNALED: 
            state = "K"; 
            break;
        default: 
            state = "?"; 
            break;
    }
    
    // Tambahkan indikator foreground/background seperti ps
    if (fg_pgid == pgid) {
        state += "+";  // Foreground process group
    } else {
        state += " ";  // Background process group
    }
    
    return state;
}

/**
 * @brief Mengubah jobspec (%N, %%, %+, %-) menjadi Process Group ID (PGID) - Cross Session
 */
int jobspec_to_pgid(const std::string& jobspec, pid_t& pgid_out) {
    if (jobspec.empty() || jobspec[0] != '%') {
        return 1;
    }

    std::string spec_value = jobspec.substr(1);
    pid_t current_shell_pid = getpid();
    auto all_jobs = get_all_active_jobs(current_shell_pid);

    // Job ID Khusus (%, +, -) - Cross Session
    if (spec_value == "%" || spec_value == "+") {
        if (current_job_id != 0) {
            for (const auto& job : all_jobs) {
                if (job.job_id == current_job_id) {
                    pgid_out = job.pgid;
                    return 0;
                }
            }
        }
        std::cerr << "nsh: jobspec: No current job" << std::endl;
        return 1;
    } else if (spec_value == "-") {
        if (previous_job_id != 0) {
            for (const auto& job : all_jobs) {
                if (job.job_id == previous_job_id) {
                    pgid_out = job.pgid;
                    return 0;
                }
            }
        }
        std::cerr << "nsh: jobspec: No previous job" << std::endl;
        return 1;
    }

    // Job ID Numerik (%N) - Cross Session
    try {
        int job_id = std::stoi(spec_value);
        for (const auto& job : all_jobs) {
            if (job.job_id == job_id) {
                pgid_out = job.pgid;
                return 0;
            }
        }
    } catch (const std::exception&) {
        // Lanjut ke pencarian nama
    }

    // Job Nama/Prefix (%string) - Cross Session
    std::vector<DisplayJobInfo> matching_jobs;
    for (const auto& job : all_jobs) {
        if (job.command.find(spec_value) != std::string::npos) {
            matching_jobs.push_back(job);
        }
    }

    if (matching_jobs.size() == 1) {
        pgid_out = matching_jobs[0].pgid;
        return 0;
    } else if (matching_jobs.size() > 1) {
        std::cerr << "nsh: jobspec: ambiguous job name: " << jobspec << std::endl;
        return 1;
    }

    std::cerr << "nsh: job not found: " << jobspec << std::endl;
    return 1;
}

/**
 * @brief Converts a JobStatus enum to its string representation.
 */
std::string job_status_to_string(JobStatus status, int term_status = 0) {
    switch (status) {
        case JobStatus::RUNNING: return "Running";
        case JobStatus::STOPPED: return "Stopped";
        case JobStatus::DONE: return "Done";
        case JobStatus::EXITED: 
            if (term_status == 0) {
                return "Done";
            } else {
                return "Exited " + std::to_string(term_status);
            }
        case JobStatus::SIGNALED:
            if (const char* sig_str = strsignal(term_status)) {
                return sig_str;
            }
            return "Terminated";
        default: return "Unknown";
    }
}

/**
 * @brief Formats CPU time from a rusage struct into a readable string
 */
std::string format_cpu_time(const struct rusage& usage) {
    double user_time = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6;
    double sys_time = usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6;
    double total_time = user_time + sys_time;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << total_time << "s";
    return ss.str();
}

void show_jobs_help() {
    std::cout << "Usage: jobs [options]\n"
              << "Displays the status of jobs with navigation indicators.\n\n"
              << "Options:\n"
              << "  -l          Display process IDs and detailed information in ps-like format.\n"
              << "  -p          Display process IDs only.\n"
              << "  -r          Restrict output to running jobs.\n"
              << "  -s          Restrict output to stopped jobs.\n"
              << "  --help      Show this help message.\n\n"
              << "Navigation indicators:\n"
              << "  +           Current job (referenced by %% or %%+)\n"
              << "  -           Previous job (referenced by %%-) \n";
}

void handle_builtin_jobs(const std::vector<std::string>& tokens) {
    bool list_details = false;
    bool list_pgid_only = false;
    bool running_only = false;
    bool stopped_only = false;

    for (size_t i = 1; i < tokens.size(); ++i) {
        if (tokens[i] == "-l") {
            list_details = true;
        } else if (tokens[i] == "-p") {
            list_pgid_only = true;
        } else if (tokens[i] == "-r") {
            running_only = true;
        } else if (tokens[i] == "-s") {
            stopped_only = true;
        } else if (tokens[i] == "--help") {
            show_jobs_help();
            return;
        } else {
            std::cerr << "nsh: jobs: invalid option -- '" << tokens[i] << "'" << std::endl;
            show_jobs_help();
            return;
        }
    }

    pid_t current_shell_pid = getpid();
    auto all_jobs = get_all_active_jobs(current_shell_pid);

    // Filter jobs berdasarkan opsi
    std::vector<DisplayJobInfo> filtered_jobs;
    for (const auto& job : all_jobs) {
        if (running_only && job.status != JobStatus::RUNNING) continue;
        if (stopped_only && job.status != JobStatus::STOPPED) continue;
        filtered_jobs.push_back(job);
    }

    // Urutkan jobs: session current dulu, lalu external, kemudian by job_id
    std::sort(filtered_jobs.begin(), filtered_jobs.end(), 
             [](const DisplayJobInfo& a, const DisplayJobInfo& b) {
                 if (a.is_current_session != b.is_current_session) 
                     return a.is_current_session;
                 return a.job_id < b.job_id;
             });

    if (list_pgid_only) {
        for (const auto& job : filtered_jobs) {
            std::cout << job.pgid << std::endl;
        }
        return;
    }

    // Format output ps-like untuk opsi -l
    if (list_details) {
        std::cout << std::left << std::setw(8) << "JOBID" 
                  << std::setw(8) << "STAT"
                  << std::setw(10) << "PGID"
                  << std::setw(12) << "SESSION"
                  << std::setw(12) << "CPU Time" 
                  << std::setw(8) << "CPU%"
                  << "COMMAND" << std::endl;
        
        for (const auto& job : filtered_jobs) {
            std::string job_id_str = "[" + std::to_string(job.job_id) + "]";
            if (job.job_id == current_job_id) {
                job_id_str += "+";
            } else if (job.job_id == previous_job_id) {
                job_id_str += "-";
            }
            
            std::cout << std::left << std::setw(8) << job_id_str;
            std::cout << std::setw(8) << get_ps_short_state(job.status, job.pgid);
            std::cout << std::setw(10) << job.pgid;
            std::cout << std::setw(12) << job.session_display_name;
            std::cout << std::setw(12) << format_cpu_time(job.usage);
            
            if (job.status == JobStatus::RUNNING) {
                std::cout << std::setw(8) << format_cpu_percentage(job.usage, job.start_tv);
            } else {
                std::cout << std::setw(8) << "0.0%";
            }
            std::cout << job.command << std::endl;
        }
        return;
    }

    // Format default tanpa opsi -l (tampilkan dengan navigasi +/-)
    for (const auto& job : filtered_jobs) {
        std::string job_id_str = "[" + std::to_string(job.job_id) + "]";
        if (job.job_id == current_job_id) {
            job_id_str += "+";
        } else if (job.job_id == previous_job_id) {
            job_id_str += "-";
        }
        
        std::cout << job_id_str
                  << "\t" << job_status_to_string(job.status, job.term_status)
                  << "\t\t" << job.command;
        if (!job.is_current_session) {
            std::cout << " (session: " << job.session_display_name << ")";
        }
        std::cout << std::endl;
    }
}

// ... (fungsi-fungsi lainnya seperti kill, bg, fg tetap sama)

// --- Signal Mapping ---
// Daftar sinyal umum yang didukung
const std::unordered_map<std::string, int> signal_names = {
    {"HUP", SIGHUP}, {"INT", SIGINT}, {"QUIT", SIGQUIT}, {"ILL", SIGILL},
    {"ABRT", SIGABRT}, {"FPE", SIGFPE}, {"KILL", SIGKILL}, {"SEGV", SIGSEGV},
    {"PIPE", SIGPIPE}, {"ALRM", SIGALRM}, {"TERM", SIGTERM}, {"CHLD", SIGCHLD},
    {"CONT", SIGCONT}, {"STOP", SIGSTOP}, {"TSTP", SIGTSTP}, {"TTIN", SIGTTIN},
    {"TTOU", SIGTTOU}, {"USR1", SIGUSR1}, {"USR2", SIGUSR2}
    // Tambahkan sinyal lain sesuai kebutuhan OS Anda
};

/**
 * @brief Mendapatkan nomor sinyal dari nama sinyal (case-insensitive, menghilangkan awalan SIG).
 * @param sigspec Nama sinyal (misalnya "KILL", "SIGTERM", "term").
 * @return Nomor sinyal, atau 0 jika tidak ditemukan.
 */
int get_signal_by_name(const std::string& sigspec) {
    std::string upper_sigspec = sigspec;
    std::transform(upper_sigspec.begin(), upper_sigspec.end(), upper_sigspec.begin(), ::toupper);
    
    // Hapus awalan "SIG" jika ada
    if (upper_sigspec.rfind("SIG", 0) == 0) {
        upper_sigspec = upper_sigspec.substr(3);
    }

    auto it = signal_names.find(upper_sigspec);
    if (it != signal_names.end()) {
        return it->second;
    }
    return 0;
}

/**
 * @brief Display help information for kill command
 */
void show_kill_help() {
    std::cout << "Usage: kill [options] <pid>|<jobspec>...\n"
              << "Send signals to processes.\n\n"
              << "Options:\n"
              << "  -s <sig>    Specify signal by name (e.g., TERM, KILL)\n"
              << "  -n <num>    Specify signal by number (e.g., 9, 15)\n"
              << "  -<sig>      Specify signal by name or number (e.g., -TERM, -9)\n"
              << "  -h, --help  Show this help message\n\n"
              << "Signals can be specified by name (without SIG prefix) or number.\n"
              << "Targets can be process IDs (PID) or job specifications (%job).\n"
              << "Default signal is TERM (15).\n";
}

/**
 * @brief Implementasi built-in 'kill'.
 *
 * Sintaks: kill [-s sigspec | -n signum | -sigspec] pid | jobspec ...
 *
 * @param tokens Argumen perintah.
 */
void handle_builtin_kill(const std::vector<std::string> &tokens) {
    // Handle help option
    if (tokens.size() == 2 && (tokens[1] == "-h" || tokens[1] == "--help")) {
        show_kill_help();
        last_exit_code = 0;
        return;
    }

    // kill harus memiliki setidaknya satu target
    if (tokens.size() < 2) {
        std::cerr << "nsh: kill: usage: kill [-s signal | -n number | -signal] pid | jobspec ...\n";
        std::cerr << "nsh: kill: use 'kill -h' for more information\n";
        last_exit_code = 2; // Exit code untuk kesalahan penggunaan
        return;
    }

    int signal_num = SIGTERM; // Default sinyal: SIGTERM (15)
    std::vector<std::string> targets;
    int error_count = 0;
    
    // --- Parsing Argumen ---
    for (size_t i = 1; i < tokens.size(); ++i) {
        const std::string& token = tokens[i];

        if (token == "-s") {
            // Opsi -s sigspec
            if (i + 1 < tokens.size()) {
                std::string sigspec = tokens[++i];
                signal_num = get_signal_by_name(sigspec);
                if (signal_num == 0) {
                    std::cerr << "nsh: kill: unknown signal: " << sigspec << std::endl;
                    last_exit_code = 1;
                    return;
                }
            } else {
                std::cerr << "nsh: kill: option -s requires a signal argument\n";
                last_exit_code = 2;
                return;
            }
        } else if (token == "-n") {
            // Opsi -n signum
            if (i + 1 < tokens.size()) {
                std::string signum_str = tokens[++i];
                try {
                    signal_num = std::stoi(signum_str);
                    if (signal_num <= 0 || signal_num >= NSIG) {
                        throw std::out_of_range("invalid signal number");
                    }
                } catch (const std::exception&) {
                    std::cerr << "nsh: kill: invalid signal number: " << signum_str << std::endl;
                    last_exit_code = 1;
                    return;
                }
            } else {
                std::cerr << "nsh: kill: option -n requires a signal number argument\n";
                last_exit_code = 2;
                return;
            }
        } else if (token.rfind('-') == 0 && token.length() > 1) {
            // Opsi -sigspec atau -signum
            std::string spec = token.substr(1);
            
            // Handle help option in the middle of arguments
            if (spec == "h" || spec == "-help") {
                show_kill_help();
                last_exit_code = 0;
                return;
            }
            
            // Coba sebagai nomor sinyal
            if (std::all_of(spec.begin(), spec.end(), ::isdigit)) {
                try {
                    int num = std::stoi(spec);
                    if (num <= 0 || num >= NSIG) {
                         throw std::out_of_range("invalid signal number");
                    }
                    signal_num = num;
                } catch (const std::exception&) {
                    std::cerr << "nsh: kill: invalid signal number: " << spec << std::endl;
                    last_exit_code = 1;
                    return;
                }
            } else {
                // Coba sebagai nama sinyal
                int sig = get_signal_by_name(spec);
                if (sig != 0) {
                    signal_num = sig;
                } else {
                    // Jika bukan sinyal, perlakukan sebagai target PID negatif
                    // kill -N mengirim sinyal default ke PGID N.
                    // Namun, POSIX mensyaratkan -N sebagai nomor sinyal.
                    // Kita akan tetap pada interpretasi sinyal jika tidak ada target yang ditentukan
                    // sebagai PID/PGID negatif secara eksplisit.
                    // Untuk kesederhanaan, jika bukan sinyal yang dikenal, ini adalah error
                    std::cerr << "nsh: kill: unknown signal or option: " << token << std::endl;
                    last_exit_code = 1;
                    return;
                }
            }
        } else {
            // Ini adalah PID atau jobspec
            targets.push_back(token);
        }
    }

    if (targets.empty()) {
        std::cerr << "nsh: kill: pid or jobspec must be specified\n";
        std::cerr << "nsh: kill: use 'kill -h' for more information\n";
        last_exit_code = 2;
        return;
    }

    // --- Pemrosesan Target dan Pengiriman Sinyal ---
    for (const std::string& target : targets) {
        int target_id = 0;
        int kill_result = -1;
        pid_t pgid_to_kill = 0;

        if (target[0] == '%') {
            // --- EKSPANSI JOBSPEC ---
            if (jobspec_to_pgid(target, pgid_to_kill) == 0) {
                // Jobspec ditemukan. Kirim sinyal ke PGID.
                target_id = -pgid_to_kill; // Untuk pesan output
                kill_result = kill(-pgid_to_kill, signal_num); 
            } else {
                // Jobspec tidak valid/tidak ditemukan/ambigu (pesan error sudah di dalam jobspec_to_pgid)
                error_count++;
                continue;
            }
        } else {
            // Target adalah PID atau PGID negatif (misalnya -123)
            try {
                target_id = std::stoi(target);
                
                if (target_id > 0) {
                    // PID positif - kirim ke process
                    kill_result = kill(target_id, signal_num);
                } else if (target_id < 0) {
                    // PID negatif - kirim ke process group
                    kill_result = kill(target_id, signal_num);
                } else {
                    // PID 0 tidak valid
                    std::cerr << "nsh: kill: invalid target: " << target << " (PID cannot be 0)" << std::endl;
                    error_count++;
                    continue;
                }
            } catch (const std::exception&) {
                std::cerr << "nsh: kill: invalid target argument: " << target << std::endl;
                error_count++;
                continue;
            }
        }

        // Penanganan hasil kill()
        if (kill_result == 0) {
            std::string target_desc = (target[0] == '%') ? 
                                      ("Job " + target + " (PGID " + std::to_string(std::abs(target_id)) + ")") :
                                      (target_id > 0 ? "Process " + target : "Process Group " + std::to_string(-target_id));
            std::cout << target_desc << " signaled with " << strsignal(signal_num) << std::endl;
        } else {
            // kill_result == -1
            std::cerr << "nsh: kill: failed to send signal " << strsignal(signal_num) << " to " << target << ": " << strerror(errno) << std::endl;
            error_count++;
        }
    }

    last_exit_code = (error_count > 0) ? 1 : 0;
}

/**
 * @brief Display help information for fg command
 */
void show_fg_help() {
    std::cout << "Usage: fg [jobspec]\n"
              << "Bring a background job to the foreground.\n\n"
              << "Arguments:\n"
              << "  jobspec    Job specification (e.g., %1, %+, %-)\n"
              << "             If not specified, uses the current job (%+).\n\n"
              << "The job will resume execution in the foreground with terminal control.\n"
              << "If the job is stopped, it will be sent a SIGCONT signal.\n";
}

/**
 * @brief Display help information for bg command
 */
void show_bg_help() {
    std::cout << "Usage: bg [jobspec]\n"
              << "Resume a stopped job in the background.\n\n"
              << "Arguments:\n"
              << "  jobspec    Job specification (e.g., %1, %+, %-)\n"
              << "             If not specified, uses the current job (%+).\n\n"
              << "The job will resume execution in the background.\n"
              << "A SIGCONT signal will be sent to the job's process group.\n";
}

void handle_builtin_bg(const std::vector<std::string>& tokens) {
    // Handle help option
    if (tokens.size() == 2 && (tokens[1] == "-h" || tokens[1] == "--help")) {
        show_bg_help();
        last_exit_code = 0;
        return;
    }

    std::string jobspec = (tokens.size() > 1) ? tokens[1] : "%+";
    pid_t pgid_out = 0;

    if (jobspec_to_pgid(jobspec, pgid_out) != 0) {
        // jobspec_to_pgid sudah menampilkan pesan error
        last_exit_code = 1;
        return;
    }
    
    // Cari Job berdasarkan PGID dari semua session
    pid_t current_shell_pid = getpid();
    auto all_jobs = get_all_active_jobs(current_shell_pid);
    
    const DisplayJobInfo* job_info = nullptr;
    for (const auto& job : all_jobs) {
        if (job.pgid == pgid_out) {
            job_info = &job;
            break;
        }
    }

    if (!job_info) {
        std::cerr << "nsh: bg: job not found: " << jobspec << std::endl;
        last_exit_code = 1;
        return;
    }

    // Cek jika job dari session lain
    if (!job_info->is_current_session) {
        std::cerr << "nsh: bg: cannot enforce session background control (" 
                  << job_info->session_display_name << ") from this session" << std::endl;
        last_exit_code = 1;
        return;
    }

    // Cari job di local jobs map untuk modifikasi
    Job* job_ptr = nullptr;
    for (auto& [id, job] : jobs) {
        if (job.pgid == pgid_out) {
            job_ptr = &job;
            break;
        }
    }

    if (job_ptr) {
        if (job_ptr->status == JobStatus::RUNNING) {
            std::cerr << "nsh: bg: job " << job_info->job_id << " already in background" << std::endl;
            last_exit_code = 0; // Bash return 0 di sini
            return;
        }

        // Kirim SIGCONT untuk melanjutkan job
        if (kill(-pgid_out, SIGCONT) < 0) {
            perror("nsh: bg: kill (SIGCONT)");
            last_exit_code = 1;
        } else {
            job_ptr->status = JobStatus::RUNNING;
            std::cout << "[" << job_info->job_id << "]+ " << job_ptr->command << " &" << std::endl;
            last_exit_code = 0;
        }
    } else {
        std::cerr << "nsh: bg: job not found in current session: " << jobspec << std::endl;
        last_exit_code = 1;
    }
}

void handle_builtin_fg(const std::vector<std::string>& tokens) {
    // Handle help option
    if (tokens.size() == 2 && (tokens[1] == "-h" || tokens[1] == "--help")) {
        show_fg_help();
        last_exit_code = 0;
        return;
    }

    pid_t pgid_to_find = 0;
    pid_t current_shell_pid = getpid();

    // Tentukan PGID dari jobspec atau job terbaru
    if (tokens.size() > 1) {
        if (jobspec_to_pgid(tokens[1], pgid_to_find) != 0) {
            last_exit_code = 1;
            return;
        }
    } else {
        // Gunakan current job (%+)
        auto all_jobs = get_all_active_jobs(current_shell_pid);
        if (current_job_id == 0) {
            std::cerr << "nsh: fg: No current job" << std::endl;
            last_exit_code = 1;
            return;
        }
        
        bool found = false;
        for (const auto& job : all_jobs) {
            if (job.job_id == current_job_id) {
                pgid_to_find = job.pgid;
                found = true;
                break;
            }
        }
        
        if (!found) {
            std::cerr << "nsh: fg: No current job" << std::endl;
            last_exit_code = 1;
            return;
        }
    }

    // Cari job dari semua session
    auto all_jobs = get_all_active_jobs(current_shell_pid);
    const DisplayJobInfo* job_info = nullptr;
    for (const auto& job : all_jobs) {
        if (job.pgid == pgid_to_find) {
            job_info = &job;
            break;
        }
    }

    if (!job_info) {
        std::cerr << "nsh: fg: job not found." << std::endl;
        last_exit_code = 1;
        return;
    }

    // Cek jika job dari session lain
    if (!job_info->is_current_session) {
        std::cerr << "nsh: fg: can't bring process (" << job_info->pgid 
                  << ") from session " << job_info->session_display_name 
                  << " into current session foreground" << std::endl;
        last_exit_code = 1;
        return;
    }

    // Cari job di local jobs map untuk modifikasi
    Job* job_ptr = nullptr;
    int job_id_to_erase = -1;
    for (auto& [id, job] : jobs) {
        if (job.pgid == pgid_to_find) {
            job_ptr = &job;
            job_id_to_erase = id;
            break;
        }
    }

    if (!job_ptr) {
        std::cerr << "nsh: fg: job not found in current session." << std::endl;
        last_exit_code = 1;
        return;
    }

    // Cetak perintah job
    std::cout << job_ptr->command << std::endl;
    
    // Setup signal handling
    sigset_t mask_all, prev_mask, mask_sigchld;
    sigfillset(&mask_all);
    sigemptyset(&mask_sigchld);
    sigaddset(&mask_sigchld, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask_sigchld, &prev_mask);

    // Kirim SIGCONT jika job stopped
    if (job_ptr->status == JobStatus::STOPPED) {
        if (kill(-job_ptr->pgid, SIGCONT) < 0) {
            perror("nsh: fg: kill(SIGCONT)");
            if (isatty(STDIN_FILENO)) {
                give_terminal_to(shell_pgid);
                sigprocmask(SIG_SETMASK, &prev_mask, NULL);
                foreground_pgid = 0;
            }
            last_exit_code = 1;
            return;
        }
    }
    
    // Simpan terminal settings shell
    tcgetattr(STDIN_FILENO, &shell_tmodes);
    
    // Berikan terminal control ke job
    if (tcsetpgrp(STDIN_FILENO, job_ptr->pgid) < 0) {
        perror("nsh: fg: tcsetpgrp");
        // Tetap lanjutkan tetapi warning
    }
    
    // Tandai job sebagai running di foreground
    job_ptr->status = JobStatus::RUNNING;
    foreground_pgid = job_ptr->pgid;

    // Restore signal handling dan unblock SIGCHLD
    sigprocmask(SIG_SETMASK, &prev_mask, NULL);    

    // Tunggu job selesai atau berhenti
    int status;
    pid_t w;
    bool job_completed = false;
    
    do {
        w = waitpid(-job_ptr->pgid, &status, WUNTRACED | WCONTINUED);
        if (w == -1) {
            if (errno == EINTR) continue;
            perror("nsh: fg: waitpid");
            break;
        }

        if (WIFSTOPPED(status)) {
            job_ptr->status = JobStatus::STOPPED;
            job_ptr->term_status = WSTOPSIG(status);
            std::cout << "\n[" << job_id_to_erase << "]+ Stopped\t" << job_ptr->command << std::endl;
            break;
        } else if (WIFCONTINUED(status)) {
            job_ptr->status = JobStatus::RUNNING;
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            job_completed = true;
            break;
        }
    } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));

    // Kembalikan terminal control ke shell
    foreground_pgid = 0;
    if (tcsetpgrp(STDIN_FILENO, shell_pgid) < 0) {
        perror("nsh: fg: tcsetpgrp: restore shell");
    }
    tcsetattr(STDIN_FILENO, TCSADRAIN, &shell_tmodes);
    
    // Update status job dan exit code
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code == 0) {
            job_ptr->status = JobStatus::DONE;
        } else {
            job_ptr->status = JobStatus::EXITED;
        }
        job_ptr->term_status = exit_code;
        last_exit_code = job_ptr->term_status;
    } else if (WIFSIGNALED(status)) {
        job_ptr->status = JobStatus::SIGNALED;
        job_ptr->term_status = WTERMSIG(status);
        last_exit_code = 128 + job_ptr->term_status;
        std::cout << "\n[" << job_id_to_erase << "]+ Terminated\t" << job_ptr->command << std::endl;
    } else if (WIFSTOPPED(status)) {
        // Already handled in the loop
        last_exit_code = 128 + job_ptr->term_status;
    }

    // Hapus job dari list hanya kalau completed (DONE atau SIGNALED)
    if (job_completed) {
        jobs.erase(job_id_to_erase);
    }
}
