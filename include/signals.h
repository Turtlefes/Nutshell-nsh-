#ifndef SIGNALS_H
#define SIGNALS_H

void setup_signals();
void suspend_shell();
void suspend_current_job();
int wait_for_job(pid_t pgid);

int disable_signal(std::string signal);
int restore_signal(std::string signal);

// Deklarasi handler agar bisa diakses jika perlu (misal untuk setup)
void sigchld_handler(int signum);
void sigtstp_handler(int signum);
void sigint_handler(int signum);
void sigquit_handler(int signum);
void sigterm_handler(int signum);
void sighup_handler(int signum);
void sigusr1_handler(int signum);
void sigusr2_handler(int signum);
void sigwinch_handler(int signum);
void sigpipe_handler(int signum);
void sigcont_handler(int signum);

#endif // SIGNALS_H
