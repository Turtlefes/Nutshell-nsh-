#ifndef UTILS_H
#define UTILS_H

#include <string>

int get_terminal_width();
void safe_print(const std::string &text);
//std::string rtrim(std::string s);
bool ends_with_EOF_IN_operator(const std::string &line);
size_t visible_width(const std::string &s);
void clear_screen();
// utils.h
void clear_history_list();
std::string rtrim(const std::string& s);
std::string trim(const std::string& s);
unsigned int xrand(unsigned int seed, int min, int max);
void input_redisplay();
// In utils.h
bool is_string_numeric(const std::string& s);

#endif // UTILS_H
