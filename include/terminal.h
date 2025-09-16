#ifndef TERMINAL_H
#define TERMINAL_H

#include <sys/types.h>

void setup_terminal();
void restore_terminal_mode();
void set_raw_mode();
void set_cooked_mode();
void safe_set_cooked_mode();
void safe_set_raw_mode();
void exit_shell(int exit_code);
void reset_terminal();


void give_terminal_to(pid_t pgid);

void clear_pending_input();

#endif // TERMINAL_H
