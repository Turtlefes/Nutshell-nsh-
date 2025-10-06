#include "terminal.h"
#include "globals.h"
#include "init.h"

#include <termios.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#include <errno.h>
#include <csignal>
#include <readline/history.h>

bool is_terminal_initialized = false;
bool is_raw_mode = false;
struct termios original_termios;
struct termios current_termios;
struct termios shell_tmodes;   // simpan mode terminal shell

void setup_terminal()
{
    if (!isatty(STDIN_FILENO))
    {
        return;
    }

    if (tcgetattr(STDIN_FILENO, &original_termios) == -1)
    {
        perror("tcgetattr");
        return;
    }

    current_termios = original_termios;
    atexit(restore_terminal_mode);
    is_terminal_initialized = true;

    // PASTIKAN SHELL MENJADI PROCESS GROUP FOREGROUND
    shell_pgid = getpid();
    if (setpgid(shell_pgid, shell_pgid) < 0)
    {
        perror("setpgid shell");
        exit(1);
    }
    tcsetpgrp(STDIN_FILENO, shell_pgid);
}

void restore_terminal_mode()
{
    if (is_terminal_initialized && isatty(STDIN_FILENO))
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
        is_raw_mode = false;
    }
}

void set_raw_mode()
{
    if (!is_terminal_initialized || !isatty(STDIN_FILENO))
        return;
    if (is_raw_mode)
        return;

    current_termios = original_termios;
    current_termios.c_lflag &= ~(ICANON | ECHO | ISIG);
    current_termios.c_cc[VMIN] = 1;
    current_termios.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &current_termios) == -1)
    {
        perror("tcsetattr raw mode");
        return;
    }
    is_raw_mode = true;
}

void set_cooked_mode()
{
    if (!is_terminal_initialized || !isatty(STDIN_FILENO))
        return;
    if (!is_raw_mode)
        return;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &original_termios) == -1)
    {
        perror("tcsetattr cooked mode");
        return;
    }
    is_raw_mode = false;
}

void safe_set_cooked_mode()
{
    if (is_terminal_initialized && is_raw_mode)
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
        is_raw_mode = false;
    }
}

void safe_set_raw_mode()
{
    if (is_terminal_initialized && !is_raw_mode)
    {
        set_raw_mode();
    }
}

// Versi default: pakai STDIN_FILENO
void give_terminal_to(pid_t pgid) {
    if (!isatty(STDIN_FILENO)) {
        return; // Bukan terminal
    }

    // Coba assign terminal sampai berhasil atau error bukan EINTR
    while (tcsetpgrp(STDIN_FILENO, pgid) == -1) {
        if (errno == EINTR) {
            continue; // retry kalau ke-interrupt
        } else if (errno == ENOTTY) {
            return; // stdin bukan terminal
        } else if (errno == EINVAL) {
            return; // pgid bukan pgrp valid
        } else {
            perror("tcsetpgrp");
            return;
        }
    }
}

// Versi bisa pilih file descriptor
void give_terminal_to_fd(int fd, pid_t pgid) {
    if (!isatty(fd)) {
        return;
    }

    while (tcsetpgrp(fd, pgid) == -1) {
        if (errno == EINTR) {
            continue;
        } else if (errno == ENOTTY) {
            return;
        } else if (errno == EINVAL) {
            return;
        } else {
            perror("tcsetpgrp");
            return;
        }
    }
}

// FUNGSI UNTUK PROSES ANAK MENGAMBIL KONTROL TERMINAL
void take_terminal_to(pid_t pgid)
{
    if (isatty(STDIN_FILENO))
    {
        // Set process group untuk child, lalu berikan kontrol terminal ke process group ini
        if (setpgid(0, pgid) == -1 && errno != EACCES)
        {
            perror("setpgid child");
        }
        if (tcsetpgrp(STDIN_FILENO, pgid) == -1)
        {
            perror("tcsetpgrp child");
        }
    }
}

// [perbaiki reset_terminal]
void reset_terminal()
{
    if (is_terminal_initialized && isatty(STDIN_FILENO))
    {
        // Kembalikan ke mode original
        tcsetattr(STDIN_FILENO, TCSANOW, &original_termios);
        is_raw_mode = false;
        
        // Berikan kontrol terminal kembali ke shell
        give_terminal_to(shell_pgid);
        
        // Bersihkan input yang tertunda
        tcflush(STDIN_FILENO, TCIFLUSH);
    }
}

// [perbaiki safe_exit_terminal]
void safe_exit_terminal()
{
    reset_terminal();
}

void exit_shell(int exit_code)
{
    cleanup_session_manager();
    // Comprehensive shell exit function
    safe_exit_terminal();
    
    // Save history
    write_history(ns_HISTORY_FILE.c_str());
    
    exit(exit_code);
}

void clear_pending_input()
{
    if (is_terminal_initialized && isatty(STDIN_FILENO))
    {
        tcflush(STDIN_FILENO, TCIFLUSH);
    }
}