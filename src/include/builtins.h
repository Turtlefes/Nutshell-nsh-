#ifndef BUILTINS_H
#define BUILTINS_H

#include <vector>
#include <string>

void handle_builtin_hash(const std::vector<std::string> &tokens);
void handle_builtin_cd(const std::vector<std::string> &t);
void handle_builtin_pwd(const std::vector<std::string> &tokens);
void handle_builtin_alias(const std::vector<std::string> &tokens);
void handle_builtin_unalias(const std::vector<std::string> &tokens);
void handle_builtin_export(const std::vector<std::string> &tokens);
void handle_builtin_bookmark(const std::vector<std::string> &tokens);
void handle_builtin_history(const std::vector<std::string> &tokens);
void handle_builtin_exec(const std::vector<std::string> &tokens);
void handle_builtin_unset(const std::vector<std::string> &tokens);
void handle_builtin_jobs(const std::vector<std::string>& tokens);
void handle_builtin_kill(const std::vector<std::string> &tokens);
void handle_builtin_bg(const std::vector<std::string> &tokens);
void handle_builtin_fg(const std::vector<std::string> &tokens);

#endif // BUILTINS_H
