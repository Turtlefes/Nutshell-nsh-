#ifndef EXPANSION_H
#define EXPANSION_H

#include <string>
#include <utility>
#include <vector>

bool is_env_assignment(const std::string &token);
std::pair<std::string, std::string>
parse_env_assignment(const std::string &token);
std::string expand_tilde(const std::string &path);
std::string expand_argument(const std::string &token);
std::string execute_subshell_command(const std::string &cmd);
void apply_expansions_and_wildcards(std::vector<std::string> &tokens);

#endif // EXPANSION_H
