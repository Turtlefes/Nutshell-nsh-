#include <all.h>

#include <iostream>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <readline/readline.h>

#include "terminal.h"

#include <termios.h>

int wait_for_job(pid_t pgid)
{
    int status = 0;
    pid_t pid;

    do
    {
        pid = waitpid(-pgid, &status, WUNTRACED);
        if (pid == -1 && errno != EINTR)
        {
            perror("waitpid");
            break;
        }
    } while (pid > 0 && !WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));

    tcsetpgrp(STDIN_FILENO, shell_pgid);
    return status;
}

// [perbaiki sigtstp_handler]
void sigtstp_handler(int signum) {
    (void)signum;
    
    // IGNORE COMPLETELY jika tidak ada foreground job
    if (foreground_pgid == 0 || foreground_pgid == shell_pgid) {
        // TAMBAHKAN: Clear input seperti Ctrl+C meskipun ignore
        rl_replace_line("", 0);   // kosongkan input
        rl_crlf();                // bikin newline
        rl_on_new_line();         // pindah ke baris baru
        
        rl_replace_line("", 0);   // pastikan buffer kosong
        rl_redisplay();           // redraw prompt
        return;
    }
    
    // Hanya proses job suspension
    if (kill(-foreground_pgid, SIGTSTP) < 0) {
        perror("kill (SIGTSTP)");
    } else {
        int status;
        waitpid(-foreground_pgid, &status, WUNTRACED);
        
        // TAMBAHKAN: Clear input setelah suspend job
        rl_replace_line("", 0);   // kosongkan input
        rl_crlf();                // bikin newline
        rl_on_new_line();         // pindah ke baris baru
        
        // tampilkan pesan job stopped
        for (const auto& [id, job] : jobs) {
            if (job.pgid == foreground_pgid) {
                std::cout << "\n[" << id << "]+ Stopped\t" << job.command << std::endl;
                break;
            }
        }
        
        rl_replace_line("", 0);   // pastikan buffer kosong
        rl_redisplay();           // redraw prompt
    }
}

// [perbaiki sigint_handler]
void sigint_handler(int signum) {
    (void)signum;
    if (foreground_pgid != 0) {
        if (kill(-foreground_pgid, SIGINT) < 0) {
            perror("kill (SIGINT)");
        }
    } else {
        rl_replace_line("", 0);   // kosongkan input
        rl_crlf();                // bikin newline
        rl_on_new_line();         // pindah ke baris baru
        
        rl_replace_line("", 0);   // pastikan buffer kosong
        rl_redisplay();           // redraw prompt
    }
}

// [perbaiki sigcont_handler]
void sigcont_handler(int signum) {
    (void)signum;
    // Restore terminal mode ketika shell dilanjutkan
    setup_terminal();
    safe_set_raw_mode();
    
    // Bersihkan input yang tertunda
    tcflush(STDIN_FILENO, TCIFLUSH);
    
    // Perbarui tampilan readline
    if (isatty(STDIN_FILENO)) {
        rl_forced_update_display();
    }
    
    // Kirim SIGCONT ke semua job yang stopped (jika perlu)
    for (auto& [id, job] : jobs) {
        if (job.status == JobStatus::STOPPED) {
            kill(-job.pgid, SIGCONT);
            job.status = JobStatus::RUNNING;
        }
    }
}

void sigquit_handler(int signum)
{
    (void)signum;
    if (foreground_pgid != 0)
    {
        if (kill(-foreground_pgid, SIGQUIT) < 0)
        {
            perror("kill (SIGQUIT)");
        }
        foreground_pgid = 0;
    }
    else
    {
        std::cout << "\nQuit (core dumped)" << std::endl;
        save_history();
        exit(128 + SIGQUIT);
    }
}

void sigterm_handler(int signum)
{
    (void)signum;
    if (foreground_pgid != 0)
    {
        if (kill(-foreground_pgid, SIGTERM) < 0)
        {
            perror("kill (SIGTERM)");
        }
    }
    std::cout << "\nReceived SIGTERM, exiting..." << std::endl;
    save_history();
    exit(128 + SIGTERM);
}

void sighup_handler(int signum)
{
    (void)signum;
    std::cout << "\nSIGHUP received, exiting..." << std::endl;
    save_history();
    exit(128 + SIGHUP);
}

void sigusr1_handler(int signum)
{
    (void)signum;
    std::cout << "\nReceived SIGUSR1" << std::endl;
}

void sigusr2_handler(int signum)
{
    (void)signum;
    std::cout << "\nReceived SIGUSR2" << std::endl;
}

void sigwinch_handler(int signum)
{
    (void)signum;
    if (isatty(STDIN_FILENO))
    {
        // Memberi tahu Readline bahwa ukuran terminal telah berubah
        rl_resize_terminal();
    }
}

void sigpipe_handler(int signum)
{
    (void)signum;
}

void sigchld_handler(int signum) {
    (void)signum;
    
    // Gunakan non-blocking wait untuk menghindari deadlock
    int status;
    pid_t pid;
    
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        // Critical section - lakukan dengan cepat
        for (auto it = jobs.begin(); it != jobs.end();) {
            if (it->second.pgid == pid || it->second.pgid == getpgid(pid)) {
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    if (isatty(STDIN_FILENO)) {
                        try {
                            std::cout << "\n[" << it->first << "]+ Done\t" << it->second.command << std::endl;
                        } catch (...) {
                            // Ignore output errors dalam signal handler
                        }
                    }
                    it = jobs.erase(it);
                } else if (WIFSTOPPED(status)) {
                    if (isatty(STDIN_FILENO)) {
                        try {
                            std::cout << "\n[" << it->first << "]+ Stopped\t" << it->second.command << std::endl;
                        } catch (...) {
                            // Ignore output errors
                        }
                    }
                    it->second.status = JobStatus::STOPPED;
                    ++it;
                } else if (WIFCONTINUED(status)) {
                    it->second.status = JobStatus::RUNNING;
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


void setup_signals() {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    // SIGCHLD - tangani proses anak yang berubah status
    sa.sa_handler = sigchld_handler;
    sigaction(SIGCHLD, &sa, NULL);
    
    // SIGTSTP - Ctrl+Z
    sa.sa_handler = sigtstp_handler;
    sigaction(SIGTSTP, &sa, NULL);
    
    // SIGINT - Ctrl+C
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);
    
    // SIGQUIT - Ctrl+\
    
    sa.sa_handler = sigquit_handler;
    sigaction(SIGQUIT, &sa, NULL);
    
    // SIGTERM - terminasi
    sa.sa_handler = sigterm_handler;
    sigaction(SIGTERM, &sa, NULL);
    
    // SIGHUP - hangup
    sa.sa_handler = sighup_handler;
    sigaction(SIGHUP, &sa, NULL);
    
    // SIGCONT - continue (setelah di-stop)
    sa.sa_handler = sigcont_handler;
    sigaction(SIGCONT, &sa, NULL);
    
    // SIGWINCH - window size change
    sa.sa_handler = sigwinch_handler;
    sigaction(SIGWINCH, &sa, NULL);
    
    // SIGUSR1 dan SIGUSR2
    sa.sa_handler = sigusr1_handler;
    sigaction(SIGUSR1, &sa, NULL);
    sa.sa_handler = sigusr2_handler;
    sigaction(SIGUSR2, &sa, NULL);

    // Abaikan sinyal-sinyal berikut
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
    sigaction(SIGTTIN, &sa, NULL);
    sigaction(SIGTTOU, &sa, NULL);
    sigaction(SIGURG, &sa, NULL);
}
