#ifndef PROMPT_H
#define PROMPT_H

#include <string>

std::string get_prompt_string();
std::string get_ps0();
std::string get_multiline_input(const std::string& initial_prompt);

#endif // PROMPT_H
