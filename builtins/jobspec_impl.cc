// builtins/jobs_impl.cc

#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <csignal>
#include <sys/time.h>
#include <sys/resource.h>

/**
 * @brief Converts a JobStatus enum to its string representation.
 * @param status The job status enum.
 * @param term_status The termination signal or exit code, used for SIGNALED status.
 * @return A string describing the status.
 */
std::string job_status_to_string(JobStatus status, int term_status = 0) {
    switch (status) {
        case JobStatus::RUNNING: return "Running";
        case JobStatus::STOPPED: return "Stopped";
        case JobStatus::DONE: return "Done";
        case JobStatus::EXITED: return "Exited " + std::to_string(term_status); // Menampilkan kode keluar
        case JobStatus::SIGNALED:
            if (const char* sig_str = strsignal(term_status)) {
                return sig_str;
            }
            return "Terminated";
        default: return "Unknown";
    }
}


/**
 * @brief Formats CPU time from a rusage struct into a readable string (e.g., "0.05s").
 * @param usage The rusage struct containing user and system time.
 * @return A formatted string of the total CPU time.
 */
std::string format_cpu_time(const struct rusage& usage) {
    double user_time = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1e6;
    double sys_time = usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1e6;
    double total_time = user_time + sys_time;
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << total_time << "s";
    return ss.str();
}

/**
 * @brief Handles the 'jobs' builtin command with support for options.
 *
 * Provides options to display running, stopped, or all jobs, and to show
 * detailed information like PGID and CPU usage.
 *
 * @param tokens The command tokens, including 'jobs' and its arguments.
 */

/**
 * @brief Mengubah jobspec (%N, %%, %+, %-) menjadi Process Group ID (PGID).
 * @param jobspec String jobspec (misalnya "%1", "%%").
 * @param pgid_out PGID yang ditemukan (akan diisi).
 * @return 0 jika sukses, 1 jika job tidak ditemukan atau error.
 */
int jobspec_to_pgid(const std::string& jobspec, pid_t& pgid_out) {
    if (jobspec.empty() || jobspec[0] != '%') {
        // Ini seharusnya sudah ditangani oleh pemanggil jika argumen adalah PID
        return 1;
    }

    // --- Job ID Khusus (%, +, -) ---
    std::string spec_value = jobspec.substr(1);

    // PERBAIKAN: Pastikan current_job_id valid sebelum digunakan
    if (spec_value == "%" || spec_value == "+") {
        // Jika current_job_id tidak valid, cari job yang paling baru
        if (current_job_id == 0 || jobs.find(current_job_id) == jobs.end()) {
            current_job_id = find_most_recent_job();
        }
        
        if (current_job_id > 0) {
            auto it = jobs.find(current_job_id);
            if (it != jobs.end()) {
                pgid_out = it->second.pgid;
                return 0;
            }
        }
        std::cerr << "nsh: jobspec: No current job" << std::endl;
        return 1;
    } else if (spec_value == "-") {
        // Jika previous_job_id tidak valid, cari job kedua paling baru
        if (previous_job_id == 0 || jobs.find(previous_job_id) == jobs.end()) {
            previous_job_id = find_second_most_recent_job();
        }
        
        if (previous_job_id > 0) {
            auto it = jobs.find(previous_job_id);
            if (it != jobs.end()) {
                pgid_out = it->second.pgid;
                return 0;
            }
        }
        std::cerr << "nsh: jobspec: No previous job" << std::endl;
        return 1;
    }

    // --- Job ID Numerik (%N) ---
    try {
        int job_id = std::stoi(spec_value);
        if (job_id > 0) {
            auto it = jobs.find(job_id);
            if (it != jobs.end()) {
                pgid_out = it->second.pgid;
                return 0;
            }
            // Jangan tampilkan error jika ini hanya mencoba parse job ID
            // std::cerr << "nsh: kill: job not found: " << jobspec << std::endl;
            // return 1;
        }
    } catch (const std::exception&) {
        // Jika bukan angka, lanjut ke pencarian nama
    }

    // --- Job Nama/Prefix (%string) ---
    // Mencari job berdasarkan prefix atau nama lengkap.
    std::vector<int> matching_jobs;
    for (const auto& pair : jobs) {
        // Asumsi 'command' di Job struct adalah string lengkap perintah.
        // Cek apakah 'command' dimulai dengan spec_value.
        // TODO: Pertimbangkan case-insensitivity jika diperlukan.
        if (pair.second.command.rfind(spec_value, 0) == 0) {
            matching_jobs.push_back(pair.first);
        }
    }

    if (matching_jobs.size() == 1) {
        // Tepat satu job cocok (match).
        auto it = jobs.find(matching_jobs[0]);
        if (it != jobs.end()) { // Seharusnya selalu benar
            pgid_out = it->second.pgid;
            return 0;
        }
    } else if (matching_jobs.size() > 1) {
        // Job ambigu (lebih dari satu job cocok).
        std::cerr << "nsh: jobspec: ambiguous job name: " << jobspec << std::endl;
        return 1;
    }

    // Fallback jika tidak ditemukan (baik ID numerik maupun nama/prefix)
    std::cerr << "nsh: job not found: " << jobspec << std::endl;
    return 1;
}
 
void handle_builtin_jobs(const std::vector<std::string>& tokens) {
    bool list_details = false;   // -l: Show detailed info
    bool list_pgid_only = false; // -p: Show only the process group ID
    bool running_only = false;   // -r: Show only running jobs
    bool stopped_only = false;   // -s: Show only stopped jobs
    std::vector<int> specific_jobs;

    // --- Argument Parsing ---
    for (size_t i = 1; i < tokens.size(); ++i) {
        const std::string& arg = tokens[i];
        if (arg.rfind('-', 0) == 0 && arg.length() > 1) {
            for (size_t j = 1; j < arg.length(); ++j) {
                switch (arg[j]) {
                    case 'l': list_details = true; break;
                    case 'p': list_pgid_only = true; list_details = false; break;
                    case 'r': running_only = true; stopped_only = false; break;
                    case 's': stopped_only = true; running_only = false; break;
                    default:
                        std::cerr << "nsh: jobs: invalid option: -" << arg[j] << std::endl;
                        last_exit_code = 1;
                        return;
                }
            }
        } else {
            try {
                specific_jobs.push_back(std::stoi(arg.substr(arg[0] == '%' ? 1 : 0)));
            } catch (...) {
                std::cerr << "nsh: jobs: invalid job specification: " << arg << std::endl;
                last_exit_code = 1;
                return;
            }
        }
    }

    last_exit_code = 0;

    // --- Display Logic ---
    if (list_pgid_only) {
        for (const auto& [id, job] : jobs) {
            if (running_only && job.status != JobStatus::RUNNING) continue;
            if (stopped_only && job.status != JobStatus::STOPPED) continue;
            std::cout << job.pgid << std::endl;
        }
        return;
    }

    if (list_details) {
        std::cout << "Job\tGroup\tCPU\tState\t\tCommand" << std::endl;
    }

    std::vector<int> jobs_to_display;
    if (specific_jobs.empty()) {
        for(const auto& pair : jobs) {
            jobs_to_display.push_back(pair.first);
        }
    } else {
        jobs_to_display = specific_jobs;
    }

    bool found_any_job = false;
    for (int job_id : jobs_to_display) {
        auto it = jobs.find(job_id);
        if (it == jobs.end()) {
            std::cerr << "nsh: jobs: no such job: " << job_id << std::endl;
            last_exit_code = 1;
            continue;
        }
        found_any_job = true;

        const auto& job = it->second;
        if (running_only && job.status != JobStatus::RUNNING) continue;
        if (stopped_only && job.status != JobStatus::STOPPED) continue;

        std::cout << "[" << job_id << "]+" << "\t";
        if (list_details) {
            std::cout << job.pgid << "\t"
                      << format_cpu_time(job.usage) << "\t"
                      << std::left << std::setw(10) << job_status_to_string(job.status, job.term_status) << "\t";
        } else {
            std::cout << std::left << std::setw(10) << job_status_to_string(job.status, job.term_status) << "\t";
        }
        std::cout << job.command << std::endl;
    }
}

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

void handle_builtin_bg(const std::vector<std::string>& tokens) {
    std::string jobspec = (tokens.size() > 1) ? tokens[1] : "%+";
    pid_t pgid_out = 0;

    if (jobspec_to_pgid(jobspec, pgid_out) != 0) {
        // jobspec_to_pgid sudah menampilkan pesan error
        last_exit_code = 1;
        return;
    }
    
    // Cari Job berdasarkan PGID
    int job_id = -1;
    Job* job_ptr = nullptr;
    for (auto& [id, job] : jobs) { // Gunakan referensi untuk memodifikasi job
        if (job.pgid == pgid_out) {
            job_id = id;
            job_ptr = &job;
            break;
        }
    }

    if (job_ptr) {
        if (job_ptr->status == JobStatus::RUNNING) {
            std::cerr << "nsh: bg: job " << job_id << " already in background" << std::endl;
            last_exit_code = 0; // Bash return 0 di sini
            return;
        }

        // Kirim SIGCONT untuk melanjutkan job
        if (kill(-pgid_out, SIGCONT) < 0) {
            perror("nsh: bg: kill (SIGCONT)");
            last_exit_code = 1;
        } else {
            job_ptr->status = JobStatus::RUNNING;
            std::cout << "[" << job_id << "]+ " << job_ptr->command << " &" << std::endl;
            last_exit_code = 0;
        }
    } else {
        // Seharusnya ditangani oleh jobspec_to_pgid, tapi sebagai fallback
        std::cerr << "nsh: bg: job not found: " << jobspec << std::endl;
        last_exit_code = 1;
    }
}

/**
 * @brief Handles the 'fg' (foreground) builtin command.
 *
 * This function brings a job from the background or a stopped state to the
 * foreground. It gives terminal control to the job's process group and waits
 * for it to finish or stop.
 *
 * @param tokens The command tokens, including 'fg' and an optional jobspec.
 */
void handle_builtin_fg(const std::vector<std::string>& tokens) {
    pid_t pgid_to_find = 0;

    // Tentukan PGID dari jobspec atau job terbaru
    if (tokens.size() > 1) {
        if (jobspec_to_pgid(tokens[1], pgid_to_find) != 0) {
            last_exit_code = 1;
            return;
        }
    } else {
        int job_id = find_most_recent_job();
        if (job_id == 0) {
            std::cerr << "nsh: fg: No current job" << std::endl;
            last_exit_code = 1;
            return;
        }
        auto job_it = jobs.find(job_id);
        if (job_it != jobs.end()) {
            pgid_to_find = job_it->second.pgid;
        } else {
            std::cerr << "nsh: fg: No current job" << std::endl;
            last_exit_code = 1;
            return;
        }
    }

    // Cari job berdasarkan PGID
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
        std::cerr << "nsh: fg: job not found." << std::endl;
        last_exit_code = 1;
        return;
    }

    // Cetak perintah job
    std::cout << "Send job " << job_id_to_erase << " (" << job_ptr->command << ") Group: " << job_ptr->pgid << " to foreground" << std::endl;

    // ----- Step 1: Pindahkan terminal & atur signals -----
    if (isatty(STDIN_FILENO)) {
        foreground_pgid = job_ptr->pgid;
        
        // Disable signal shell sementara
        disable_signal("SIGINT");
        disable_signal("SIGTSTP");

        // Pindahkan terminal ke job
        give_terminal_to(job_ptr->pgid);
    }
    // ----- Step 2: Lanjutkan job STOPPED & tunggu -----
    if (kill(-job_ptr->pgid, SIGCONT) < 0) {
        perror("kill(SIGCONT)");
        if (isatty(STDIN_FILENO)) {
            give_terminal_to(shell_pgid);
            foreground_pgid = 0;
        }
        last_exit_code = 1;
        return;
    }

    // Tunggu job selesai atau berhenti
    int status = wait_for_job(job_ptr->pgid);
    //int status = waitpid(job_ptr->pgid, &status, WUNTRACED);

    // ----- Step 3: Kembalikan terminal ke shell dan restore handler -----
    if (isatty(STDIN_FILENO)) {
        give_terminal_to(shell_pgid);
        foreground_pgid = 0;

        // Restore handler shell
        restore_signal("SIGINT");
        restore_signal("SIGTSTP");
    }
    
    // ----- Step 4: Update status job -----
    if (WIFEXITED(status)) {
        job_ptr->status = JobStatus::DONE;
        job_ptr->term_status = WEXITSTATUS(status);
        last_exit_code = job_ptr->term_status;
    } else if (WIFSIGNALED(status)) {
        job_ptr->status = JobStatus::SIGNALED;
        job_ptr->term_status = WTERMSIG(status);
        last_exit_code = 128 + job_ptr->term_status;
    } else if (WIFSTOPPED(status)) {
        job_ptr->status = JobStatus::STOPPED;
        job_ptr->term_status = WSTOPSIG(status);
        std::cerr << "\n[" << job_id_to_erase << "]+ Stopped\t" << job_ptr->command << std::endl;
        last_exit_code = 128 + job_ptr->term_status;
    } else {
        std::cerr << "nsh: fg: unexpected status." << std::endl;
    }

    // Hapus job dari list hanya kalau DONE atau SIGNALED
    
    if (job_ptr->status == JobStatus::DONE || job_ptr->status == JobStatus::SIGNALED) {
        jobs.erase(job_id_to_erase);
    }
}



