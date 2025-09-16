#include <iostream>
#include <string>
#include <ctime>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <limits.h>
#include <cstdlib>
#include <sstream>

#include "expansion.h"
#include "globals.h"

// --- helper fungsi ---
std::string get_username() {
    const char *login = getenv("USER");
    if (login) return login;
    struct passwd *pw = getpwuid(geteuid());
    return pw ? pw->pw_name : "unknown";
}

std::string get_hostname(bool full = false) {
    char buf[HOST_NAME_MAX + 1];
    if (gethostname(buf, sizeof(buf)) == 0) {
        if (!full) {
            std::string h(buf);
            size_t dot = h.find('.');
            if (dot != std::string::npos) return h.substr(0, dot);
            return h;
        }
        return buf;
    }
    return "host";
}

std::string shorten_path(const fs::path &p) {
    std::string path_str = p.string();
    std::string home_str = HOME_DIR.string();

    // Ganti home directory dengan '~'
    if (path_str.rfind(home_str, 0) == 0) {
        path_str = "~" + path_str.substr(home_str.length());
    }
    
    // Pecah path menjadi komponen-komponen
    std::vector<std::string> components;
    std::string temp;
    std::stringstream ss(path_str);
    while (getline(ss, temp, fs::path::preferred_separator)) {
        if (!temp.empty()) {
            components.push_back(temp);
        }
    }
    
    // Jika komponen lebih dari 3, perpendek
    if (components.size() > 3) {
        return ".../" + components[components.size() - 2] + "/" + components.back();
    }
    
    return path_str;
}

std::string get_prompt_string(bool continuation) {
    if (continuation) return "> ";

    std::string ps1_string;
    const char* ps1_env = getenv("PS1");
    if (ps1_env) {
        ps1_string = ps1_env;
    } else {
        ps1_string = "\\e[0;32m\\w \\e[0m$ ";
    }

    std::string result_prompt;
    bool in_non_printable_escape = false;
    bool in_command_substitution = false;
    std::string command_buffer;

    for (size_t i = 0; i < ps1_string.length(); ++i) {
        char c = ps1_string[i];

        // This block handles command substitution like $(command)
        if (in_command_substitution) {
            if (c == ')') {
                in_command_substitution = false;
                std::string command_result = execute_subshell_command(command_buffer);
                result_prompt += command_result;
                command_buffer.clear();
            } else {
                command_buffer += c;
            }
            continue;
        }

        // This block handles backslash escapes
        if (c == '\\' && i + 1 < ps1_string.length()) {
            char next_c = ps1_string[++i];
            switch (next_c) {
                case 'u': result_prompt += get_username(); break;
                case 'h': result_prompt += get_hostname(false); break;
                case 'H': result_prompt += get_hostname(true); break;
                case 'w': result_prompt += shorten_path(LOGICAL_PWD); break;
                case 'W': {
                    std::string pwd = LOGICAL_PWD;
                    size_t pos = pwd.find_last_of('/');
                    result_prompt += (pos == std::string::npos) ? pwd : pwd.substr(pos+1);
                    break;
                }
                case '$': result_prompt += (geteuid() == 0) ? "#" : "$"; break;
                case 'd': {
                    char buf[64];
                    time_t t = time(nullptr);
                    strftime(buf, sizeof(buf), "%a %b %d", localtime(&t));
                    result_prompt += buf;
                    break;
                }
                case 't': {
                    char buf[64];
                    time_t t = time(nullptr);
                    strftime(buf, sizeof(buf), "%H:%M:%S", localtime(&t));
                    result_prompt += buf;
                    break;
                }
                case 'T': {
                    char buf[64];
                    time_t t = time(nullptr);
                    strftime(buf, sizeof(buf), "%I:%M:%S", localtime(&t));
                    result_prompt += buf;
                    break;
                }
                case 'A': {
                    char buf[64];
                    time_t t = time(nullptr);
                    strftime(buf, sizeof(buf), "%H:%M", localtime(&t));
                    result_prompt += buf;
                    break;
                }
                case '@': {
                    char buf[64];
                    time_t t = time(nullptr);
                    strftime(buf, sizeof(buf), "%I:%M%p", localtime(&t));
                    result_prompt += buf;
                    break;
                }
                case 'n': result_prompt += '\n'; break;
                case 's': result_prompt += ext_shell_name; break;
                case 'v': result_prompt += shell_version; break;
                case 'V': result_prompt += shell_version_long; break;
                case '!': result_prompt += std::to_string(history_number); break;
                case '#': result_prompt += std::to_string(command_number); break;
                case 'p': result_prompt += std::to_string(getpid()); break;
                case 'U': result_prompt += std::to_string(getuid()); break;
                case 'g': result_prompt += std::to_string(getgid()); break;
                case 'D': {
                    if (i + 1 < ps1_string.length() && ps1_string[i + 1] == '{') {
                        size_t end = ps1_string.find('}', i + 2);
                        if (end != std::string::npos) {
                            std::string fmt = ps1_string.substr(i + 2, end - (i + 2));
                            char buf[128];
                            time_t t = time(nullptr);
                            strftime(buf, sizeof(buf), fmt.c_str(), localtime(&t));
                            result_prompt += buf;
                            i = end;
                        }
                    }
                    break;
                }
                // ANSI escape codes and non-printable sequences
                case '[':
                    result_prompt += "\001";
                    break;
                case ']':
                    result_prompt += "\002";
                    break;
                case 'e':
                case 'E':
                    result_prompt += "\033";
                    break;
                default:
                    result_prompt += '\\';
                    result_prompt += next_c;
                    break;
            }
        }
        // This block handles command substitution in the form of $(...)
        else if (c == '$' && i + 1 < ps1_string.length() && ps1_string[i + 1] == '(') {
            in_command_substitution = true;
            i++;
            continue;
        }
        // This block handles ANSI escape sequences starting with `\e[`
        else if (c == '\033' && i + 1 < ps1_string.length() && ps1_string[i + 1] == '[') {
            result_prompt += "\001\033"; // Start of non-printable sequence for readline
            in_non_printable_escape = true;
            result_prompt += ps1_string[++i]; // Append the '['
            continue;
        }
        // This block handles the end of an ANSI escape sequence
        else if (in_non_printable_escape && c == 'm') {
            result_prompt += c;
            result_prompt += "\002"; // End of non-printable sequence
            in_non_printable_escape = false;
        }
        else {
            result_prompt += c;
        }
    }

    return result_prompt;
}
