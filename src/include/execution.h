#ifndef EXECUTION_H
#define EXECUTION_H

#include "command.h"
#include "globals.h"
#include <string>
#include <vector>

// Tell the compiler that this global variable is defined in another file.
extern std::vector<std::pair<int, Job>> finished_jobs;

bool is_builtin(const std::string &command);
int execute_builtin(const SimpleCommand &cmd);
int execute_job(const ParsedCommand &cmd_group);
int execute_command_list(const std::vector<ParsedCommand> &commands);
void check_child_status();
void write_job_controle_file(const Job& job);
void validate_and_cleanup_jobs();
int add_job_to_list(pid_t pgid, const std::string& command, JobStatus status, bool update_current = true);

std::string find_binary(const std::string &cmd);

#endif // EXECUTION_H
