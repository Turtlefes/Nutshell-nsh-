#ifndef EXECUTION_H
#define EXECUTION_H

#include "command.h"
#include "globals.h"
#include <string>
#include <vector>

// Tell the compiler that this global variable is defined in another file.
extern std::vector<std::pair<int, Job>> finished_jobs;
std::vector<char*> build_envp(); // important for global child environment 
bool is_builtin(const std::string &command);
int execute_builtin(const SimpleCommand &cmd);
std::string find_binary(const std::string &cmd);
int execute_job(const ParsedCommand &cmd_group, bool use_env);
int execute_command_list(const std::vector<ParsedCommand> &commands, bool use_env = true);
void check_child_status();
void write_job_controle_file(const Job& job);
void validate_and_cleanup_jobs();
int add_job_to_list(pid_t pgid, const std::string& command, JobStatus status, bool update_current = true);

std::string find_binary(const std::string &cmd);

#endif // EXECUTION_H
