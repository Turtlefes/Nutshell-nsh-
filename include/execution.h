#ifndef EXECUTION_H
#define EXECUTION_H

#include "command.h"
#include <string>
#include <vector>

bool is_builtin(const std::string &command);
int execute_builtin(const SimpleCommand &cmd);
int execute_job(const ParsedCommand &cmd_group);
int execute_command_list(const std::vector<ParsedCommand> &commands);
void check_child_status();

std::string find_binary(const std::string &cmd);

#endif // EXECUTION_H
