#include <all.h>
//asu
#include <iostream>
#include <csignal>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <readline/readline.h>

#include "terminal.h"

#include <termios.h>

int wait_for_job(pid_t pgid) {
    int status = 0;
    pid_t pid;

    do {
        pid = waitpid(-pgid, &status, WUNTRACED);
        if (pid == -1 && errno != EINTR) {
            perror("waitpid");
            break;
        }
    } while (pid > 0 && !WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSTOPPED(status));

    tcsetpgrp(STDIN_FILENO, shell_pgid);
    return status;
}

void clear_readline_input() {
    // Pastikan kursor berada di awal baris baru
    if (rl_end > 0) {
        std::cout << std::endl;
    }
    
    rl_replace_line("", 0);
    rl_crlf();
    rl_on_new_line();
    rl_redisplay();
    
    // Force flush untuk memastikan output tertulis
    std::cout.flush();
}


void sigtstp_handler(int signum) {
    (void)signum;

    if (foreground_pgid == 0 || foreground_pgid == shell_pgid) {
        clear_readline_input();
        return;
    }

    if (kill(-foreground_pgid, SIGTSTP) < 0) {
        perror("kill (SIGTSTP)");
    } else {
        int status;
        waitpid(-foreground_pgid, &status, WUNTRACED);
        clear_readline_input();

        for (const auto& [id, job] : jobs) {
            if (job.pgid == foreground_pgid) {
                std::cout << "\n[" << id << "]+ Stopped\t" << job.command << std::endl;
                break;
            }
        }
    }
}

void sigint_handler(int signum) {
    (void)signum;
    if (foreground_pgid != 0) {
        if (kill(-foreground_pgid, SIGINT) < 0) {
            perror("kill (SIGINT)");
        }
    } else {
        EOF_IN_interrupt = 1; // flag Eof interrupt untuk continuation
        clear_readline_input();
    }
}

void sigcont_handler(int signum) {
    (void)signum;
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
    if (foreground_pgid != 0) {
        if (kill(-foreground_pgid, SIGQUIT) < 0) {
            perror("kill (SIGQUIT)");
        }
        foreground_pgid = 0;
    } else {
        std::cout << "\nQuit (core dumped)" << std::endl;
        save_history();
        exit(128 + SIGQUIT);
    }
}

void sigterm_handler(int signum) {
    (void)signum;
    if (foreground_pgid != 0) {
        if (kill(-foreground_pgid, SIGTERM) < 0) {
            perror("kill (SIGTERM)");
        }
    }
    std::cout << "\nReceived SIGTERM, exiting..." << std::endl;
    save_history();
    exit(128 + SIGTERM);
}

void sighup_handler(int signum) {
    (void)signum;
    std::cout << "\nSIGHUP received, exiting..." << std::endl;
    save_history();
    exit(128 + SIGHUP);
}

void sigusr1_handler(int signum) {
    (void)signum;
    std::cout << "\nReceived SIGUSR1" << std::endl;
}

void sigusr2_handler(int signum) {
    (void)signum;
    std::cout << "\nReceived SIGUSR2" << std::endl;
}

void sigwinch_handler(int signum) {
    (void)signum;
    if (isatty(STDIN_FILENO)) {
        rl_resize_terminal();
    }
}

void sigpipe_handler(int signum) {
    (void)signum;
}

void sigchld_handler(int signum) {
    (void)signum;
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        for (auto it = jobs.begin(); it != jobs.end();) {
            if (it->second.pgid == pid || it->second.pgid == getpgid(pid)) {
                if (WIFEXITED(status) || WIFSIGNALED(status)) {
                    if (isatty(STDIN_FILENO)) {
                        std::cout << "\n[" << it->first << "]+ Done\t" << it->second.command << std::endl;
                    }
                    it = jobs.erase(it);
                } else if (WIFSTOPPED(status)) {
                    if (isatty(STDIN_FILENO)) {
                        std::cout << "\n[" << it->first << "]+ Stopped\t" << it->second.command << std::endl;
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

    sa.sa_handler = sigwinch_handler;
    sigaction(SIGWINCH, &sa, NULL);

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