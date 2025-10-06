#include <all.h>
//asu
#include <iostream>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <readline/readline.h>

#include "terminal.h"
#include "execution.h"  // Add this include
#include "globals.h"    // Make sure this is included

#include <termios.h>

#include <string>
#include <vector>
#include <csignal> // Pastikan ini sudah termasuk

volatile sig_atomic_t current_signal = 0;
volatile sig_atomic_t sigchld_flag = 0;

// Definisikan struct untuk pasangan Nama Sinyal dan Nilai Sinyal
struct SignalInfo {
    std::string name; // Nama sinyal (misalnya "SIGINT")
    int number;       // Nilai sinyal (misalnya SIGINT)
};

// Variabel terpusat berisi daftar sinyal yang relevan
const std::vector<SignalInfo> what_signal = {
    {"SIGCHLD", SIGCHLD},
    {"SIGTSTP", SIGTSTP},
    {"SIGINT",  SIGINT},
    {"SIGQUIT", SIGQUIT},
    {"SIGTERM", SIGTERM},
    {"SIGHUP",  SIGHUP},
    {"SIGCONT", SIGCONT},
    {"SIGPIPE", SIGPIPE},
    {"SIGTTIN", SIGTTIN},
    {"SIGTTOU", SIGTTOU},
    {"SIGUSR1", SIGUSR1},
    {"SIGUSR2", SIGUSR2},
    // Tambahkan sinyal lain jika diperlukan (misalnya SIGWINCH, SIGURG)
};

/**
 * @brief Menunggu sebuah job (process group) di foreground hingga statusnya berubah.
 * * Fungsi ini akan memblokir eksekusi hingga sebuah proses dalam process group `pgid`
 * berhenti (stopped), selesai (terminated), atau diinterupsi oleh sinyal.
 * Fungsi ini juga memastikan kontrol terminal dikembalikan ke shell setelahnya.
 * * @param pgid Process Group ID dari job yang akan ditunggu.
 * @return Status integer yang dikembalikan oleh `waitpid`.
 */
int wait_for_job(pid_t pgid) {
    int status = 0;
    pid_t result;

    do {
        // Tunggu proses dalam process group -pgid
        // dengan opsi WUNTRACED untuk mendeteksi proses berhenti (Ctrl+Z)
        result = waitpid(-pgid, &status, WUNTRACED);
        
        // Cek jika proses berhenti (WIFSTOPPED) atau selesai (WIFEXITED/WIFSIGNALED)
        if (result > 0) {
            if (WIFSTOPPED(status) || WIFSIGNALED(status) || WIFEXITED(status)) {
                return status; // Langsung kembalikan status
            }
        }
    } while (result == -1 && errno == EINTR);

    // Jika waitpid gagal karena alasan lain
    if (result == -1) {
        perror("waitpid");
        return -1;
    }

    return status;
}



void clear_readline_input() {
    rl_replace_line("", 0);
    rl_crlf();
    rl_on_new_line();
    input_redisplay();
    
    // Force flush untuk memastikan output tertulis
    std::cout.flush();
}


void sigtstp_handler(int signum) {
    (void)signum;
    current_signal = signum;
    
    if (foreground_pgid == 0 || foreground_pgid == shell_pgid) {
        clear_readline_input();
        return;
    }

    if (kill(-foreground_pgid, SIGTSTP) < 0) {
        perror("kill (SIGTSTP)");
    } else {
        int status;
        // Tunggu hingga proses benar-benar stopped
        waitpid(-foreground_pgid, &status, WUNTRACED);
        clear_readline_input();

        // UPDATE STATUS JOB DI MEMORY - PERBAIKAN: Pastikan status di-update
        for (auto& [id, job] : jobs) {
            if (job.pgid == foreground_pgid) {
                job.status = JobStatus::STOPPED;
                job.term_status = WSTOPSIG(status);
                write_job_controle_file(job); // Update file kontrol
                std::cout << "\n[" << id << "]+ Stopped\t" << job.command << std::endl;
                break;
            }
        }
        
        // Reset foreground_pgid setelah job di-stop
        foreground_pgid = 0;
    }
}

void sigint_handler(int signum) {
    (void)signum;
    current_signal = signum;
    // Set global interrupt flag
    received_sigint = 1;
    
    if (foreground_pgid == 0 || foreground_pgid == shell_pgid) {
        // Dalam shell utama atau tidak ada foreground process
        clear_readline_input();
        return;
    }
    
    // Ada foreground process - kirim SIGINT ke process group tersebut
    if (kill(-foreground_pgid, SIGINT) < 0) {
        perror("kill (SIGINT)");
    }
}

void sigcont_handler(int signum) {
    (void)signum;
    current_signal = signum;
    setup_terminal();
    safe_set_raw_mode();
    tcflush(STDIN_FILENO, TCIFLUSH);

    if (isatty(STDIN_FILENO)) {
        rl_forced_update_display();
    }

    for (auto& [id, job] : jobs) {
        if (job.status == JobStatus::STOPPED) {
            kill(-job.pgid, SIGCONT);
            job.status = JobStatus::RUNNING;
        }
    }
}

void sigquit_handler(int signum) {
    (void)signum;
    current_signal = signum;
    if (foreground_pgid != 0) {
        if (kill(-foreground_pgid, SIGQUIT) < 0) {
            perror("kill (SIGQUIT)");
        }
        foreground_pgid = 0;
    } else {
        std::cout << "\nQuit (core dumped)" << std::endl;
        save_history();
        exit_shell(128 + SIGQUIT);
    }
}

void sigterm_handler(int signum) {
    (void)signum;
    current_signal = signum;
    if (foreground_pgid != 0) {
        if (kill(-foreground_pgid, SIGTERM) < 0) {
            perror("kill (SIGTERM)");
        }
    }
    std::cout << "\nReceived SIGTERM, exiting..." << std::endl;
    save_history();
    exit_shell(128 + SIGTERM);
}

void sighup_handler(int signum) {
    (void)signum;
    current_signal = signum;
    std::cout << "\nSIGHUP received, exiting..." << std::endl;
    save_history();
    exit_shell(128 + SIGHUP);
}

void sigusr1_handler(int signum) {
    (void)signum;
    current_signal = signum;
    std::cout << "\nReceived SIGUSR1" << std::endl;
}

void sigusr2_handler(int signum) {
    (void)signum;
    current_signal = signum;
    std::cout << "\nReceived SIGUSR2" << std::endl;
}
/*
void sigwinch_handler(int signum) {
    (void)signum;
    if (isatty(STDIN_FILENO)) {
        rl_resize_terminal();
    }
}
*/
void sigpipe_handler(int signum) {
    (void)signum;
    current_signal = signum;
}

void sigchld_handler(int signum) {
    (void)signum;
    int status;
    sigchld_flag = 1;
    current_signal = signum;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        for (auto it = jobs.begin(); it != jobs.end(); ) {
            if (it->second.pgid == pid || it->second.pgid == getpgid(pid)) {
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    // Job completed
                    it->second.status = WIFEXITED(status) ? JobStatus::EXITED : JobStatus::SIGNALED;
                    it->second.term_status = WIFEXITED(status) ? WEXITSTATUS(status) : WTERMSIG(status);
                    
                    write_job_controle_file(it->second);
                    finished_jobs.push_back(*it);
                    it = jobs.erase(it);
                } else if (WIFSTOPPED(status)) {
                    // Job stopped - PERBAIKAN: Update status ke STOPPED
                    it->second.status = JobStatus::STOPPED;
                    it->second.term_status = WSTOPSIG(status);
                    write_job_controle_file(it->second);
                    ++it;
                } else if (WIFCONTINUED(status)) {
                    // Job continued
                    it->second.status = JobStatus::RUNNING;
                    write_job_controle_file(it->second);
                    ++it;
                } else {
                    ++it;
                }
            } else {
                ++it;
            }
        }
    }
}

/**
 * @brief Menonaktifkan sinyal dengan mengaturnya agar diabaikan (SIG_IGN)
 * menggunakan variabel terpusat what_signal.
 * @param signal Nama sinyal dalam bentuk string (misalnya, "SIGINT").
 * @return 0 jika berhasil, -1 jika gagal.
 */
int disable_signal(std::string signal) {
    // 1. Cari sinyal di variabel what_signal
    int signal_number = -1;
    for (const auto& info : what_signal) {
        if (info.name == signal) {
            signal_number = info.number;
            break;
        }
    }

    if (signal_number == -1) {
        // Sinyal tidak ditemukan
        return -1;
    }

    // 2. Terapkan SIG_IGN menggunakan nilai sinyal yang ditemukan
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = SIG_IGN; // Mengabaikan sinyal

    return sigaction(signal_number, &sa, NULL);
}

/**
 * @brief Mengembalikan handler sinyal ke perilaku default (SIG_DFL)
 * menggunakan variabel terpusat what_signal.
 * @param signal Nama sinyal dalam bentuk string (misalnya, "SIGINT").
 * @return 0 jika berhasil, -1 jika gagal.
 */
int restore_signal(std::string signal) {
    // 1. Cari sinyal di variabel what_signal
    int signal_number = -1;
    for (const auto& info : what_signal) {
        if (info.name == signal) {
            signal_number = info.number;
            break;
        }
    }

    if (signal_number == -1) {
        // Sinyal tidak ditemukan
        return -1;
    }

    // 2. Terapkan SIG_DFL menggunakan nilai sinyal yang ditemukan
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = SIG_DFL; // Mengembalikan ke perilaku default

    return sigaction(signal_number, &sa, NULL);
}

/**
 * @brief Mendapatkan sinyal yang sedang diterima
 * @return Nilai sinyal yang sedang aktif, 0 jika tidak ada sinyal
 */
int get_current_signal() {
    return current_signal;
}

/**
 * @brief Reset variabel current_signal ke 0
 */
void reset_current_signal() {
    current_signal = 0;
}

/**
 * @brief Mendapatkan nama sinyal dari nilai numerik
 * @param signum Nilai sinyal
 * @return Nama sinyal sebagai string, "UNKNOWN" jika tidak dikenali
 */
std::string get_signal_name(int signum) {
    for (const auto& info : what_signal) {
        if (info.number == signum) {
            return info.name;
        }
    }
    return "UNKNOWN";
}

void setup_signals() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NODEFER; // Tambahkan SA_NODEFER untuk menghindari rekursi

    sa.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &sa, NULL);

    sa.sa_handler = sigtstp_handler;
    sigaction(SIGTSTP, &sa, NULL);

    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = sigquit_handler;
    sigaction(SIGQUIT, &sa, NULL);

    sa.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &sa, NULL);

    sa.sa_handler = sighup_handler;
    sigaction(SIGHUP, &sa, NULL);

    sa.sa_handler = sigcont_handler;
    sigaction(SIGCONT, &sa, NULL);
    /*
    sa.sa_handler = sigwinch_handler;
    sigaction(SIGWINCH, &sa, NULL);
    */
    sa.sa_handler = sigusr1_handler;
    sigaction(SIGUSR1, &sa, NULL);

    sa.sa_handler = sigusr2_handler;
    sigaction(SIGUSR2, &sa, NULL);

    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
    sigaction(SIGTTIN, &sa, NULL);
    sigaction(SIGTTOU, &sa, NULL);
    sigaction(SIGURG, &sa, NULL);
}
