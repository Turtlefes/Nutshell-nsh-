#include "expansion.h"
#include "globals.h"
#include "init.h"   // for save_history, exit_shell
#include "parser.h" // for tokenize, needs_EOF_IN
#include "terminal.h"
#include "utils.h" // for rtrim
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <limits.h>
#include <pwd.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <unistd.h>

// --- helper fungsi ---
std::string get_username() {
  const char *login = getenv("USER");
  if (login)
    return login;
  struct passwd *pw = getpwuid(geteuid());
  return pw ? pw->pw_name : "unknown";
}

std::string get_hostname(bool full = false) {
  char buf[HOST_NAME_MAX + 1];
  if (gethostname(buf, sizeof(buf)) == 0) {
    if (!full) {
      std::string h(buf);
      size_t dot = h.find('.');
      if (dot != std::string::npos)
        return h.substr(0, dot);
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

std::string get_ps0() {
  std::string ps0_string;
  const char *ps0_env = getenv("PS0");
  if (ps0_env) {
    ps0_string = ps0_env;
  } else {
    ps0_string = "";
  }
  return ps0_string;
}

std::string get_prompt_string() {
  //
  std::string ps1_string;
  const char *ps1_env = getenv("PS1");
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
      case 'u':
        result_prompt += get_username();
        break;
      case 'h':
        result_prompt += get_hostname(false);
        break;
      case 'H':
        result_prompt += get_hostname(true);
        break;
      case 'w':
        result_prompt += shorten_path(LOGICAL_PWD);
        break;
      case 'W': {
        std::string pwd = LOGICAL_PWD;
        size_t pos = pwd.find_last_of('/');
        result_prompt += (pos == std::string::npos) ? pwd : pwd.substr(pos + 1);
        break;
      }
      case '$':
        result_prompt += (geteuid() == 0) ? "#" : "$";
        break;
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
      case 'n':
        result_prompt += '\n';
        break;
      case 's':
        result_prompt += ext_shell_name;
        break;
      case 'v':
        result_prompt += shell_version;
        break;
      case 'V':
        result_prompt += shell_version_long;
        break;
      case '!':
        result_prompt += std::to_string(history_number);
        break;
      case '#':
        result_prompt += std::to_string(command_number);
        break;
      case 'p':
        result_prompt += std::to_string(getpid());
        break;
      case 'U':
        result_prompt += std::to_string(getuid());
        break;
      case 'g':
        result_prompt += std::to_string(getgid());
        break;
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
    else if (c == '$' && i + 1 < ps1_string.length() &&
             ps1_string[i + 1] == '(') {
      in_command_substitution = true;
      i++;
      continue;
    }
    // This block handles ANSI escape sequences starting with `\e[`
    else if (c == '\033' && i + 1 < ps1_string.length() &&
             ps1_string[i + 1] == '[') {
      result_prompt +=
          "\001\033"; // Start of non-printable sequence for readline
      in_non_printable_escape = true;
      result_prompt += ps1_string[++i]; // Append the '['
      continue;
    }
    // This block handles the end of an ANSI escape sequence
    else if (in_non_printable_escape && c == 'm') {
      result_prompt += c;
      result_prompt += "\002"; // End of non-printable sequence
      in_non_printable_escape = false;
    } else {
      result_prompt += c;
    }
  }

  return result_prompt;
}

// Helper function untuk wrap prompt dengan escape sequences
std::string wrap_prompt_for_readline(const std::string &prompt) {
  // Cari escape sequences dan wrap dengan \001 dan \002
  std::string wrapped;
  bool in_escape = false;

  for (size_t i = 0; i < prompt.length(); i++) {
    if (prompt[i] == '\033' && prompt.substr(i, 2) == "\033[") {
      wrapped += "\001"; // Start non-printable
      in_escape = true;
    }

    wrapped += prompt[i];

    if (in_escape && prompt[i] == 'm') {
      wrapped += "\002"; // End non-printable
      in_escape = false;
    }
  }

  return wrapped;
}

std::string get_multiline_input(const std::string &initial_prompt) {
  Parser parser;

  std::string full_input;
  std::string current_prompt = initial_prompt;
  bool EOF_IN = false;
  std::vector<std::string> EOF_IN_lines;
  size_t EOF_IN_position = 0;
  bool EOF_IN_by_operator = false;
  bool EOF_IN_by_backslash = false;

  bool show_expanded_history = false;
  std::string expanded_input;

  do {
    safe_set_cooked_mode();
    rl_save_prompt();

    std::string wrapped_prompt = wrap_prompt_for_readline(current_prompt);
    char *line_read = readline(wrapped_prompt.c_str());
    safe_set_raw_mode();

    if (received_sigint && EOF_IN) {
      EOF_IN = false;
      current_prompt = initial_prompt;
      received_sigint = 0;
      full_input.clear();
      break;
    }

    if (line_read == nullptr) {
      if (full_input.empty() && isatty(STDIN_FILENO)) {
        std::cout << "exit" << std::endl;
        exit_shell(last_exit_code);
      } else {
        if (EOF_IN) {
          std::cerr << "\nnsh: unexpected end of file" << std::endl;
          full_input.clear();
          break;
        } else {
          break;
        }
      }
    }

    std::string line(line_read);
    free(line_read);

    // PERBAIKAN: History expansion hanya untuk baris pertama atau baris baru
    if (!line.empty() && !EOF_IN) {
      std::string temp_line = line;
      if (parser.expand_history(temp_line)) {
        show_expanded_history = true;
        expanded_input = temp_line;

        // Tampilkan hasil expansion untuk konfirmasi user
        dont_execute_first = 1;
        rl_replace_line(expanded_input.c_str(), 1);
        input_redisplay();
        dont_execute_first = 0;

        // PERBAIKAN: Jangan langsung assign, biarkan user edit dulu
        // line = temp_line; // Jangan assign di sini

        // Simpan expanded input untuk digunakan nanti
        expanded_input = temp_line;
      } else {
        // Reset jika tidak ada expansion
        show_expanded_history = false;
        expanded_input.clear();
      }
    }

    if (EOF_IN && line.empty()) {
      EOF_IN = false;
      break;
    }

    if (!line.empty() || EOF_IN) {
      if (EOF_IN) {
        if (EOF_IN_by_operator) {
          if (!full_input.empty() && full_input.back() != ' ') {
            full_input += " ";
          }
          full_input += line;
        } else if (EOF_IN_by_backslash) {
          if (EOF_IN_position < full_input.length()) {
            full_input = full_input.substr(0, EOF_IN_position);
            full_input += line;
          }
        }
      } else {
        // PERBAIKAN: Gunakan expanded input jika ada
        if (show_expanded_history && !expanded_input.empty()) {
          full_input = expanded_input;
        } else {
          full_input = line;
        }
      }

      EOF_IN_lines.push_back(line);

      EOF_IN = parser.needs_EOF_IN(full_input); // PERBAIKAN: Gunakan full_input
      EOF_IN_position = 0;
      EOF_IN_by_operator = false;
      EOF_IN_by_backslash = false;

      if (EOF_IN) {
        std::vector<Token> tokens =
            parser.tokenize(full_input); // PERBAIKAN: Gunakan full_input
        if (!tokens.empty()) {
          const Token &last_token = tokens.back();
          EOF_IN_by_operator = (last_token.type == TokenType::PIPE ||
                                last_token.type == TokenType::AND_IF ||
                                last_token.type == TokenType::OR_IF ||
                                last_token.type == TokenType::LESS ||
                                last_token.type == TokenType::GREAT ||
                                last_token.type == TokenType::DGREAT ||
                                last_token.type == TokenType::LESSLESS ||
                                last_token.type == TokenType::LESSLESSLESS);
        }

        std::string trimmed_line =
            rtrim(full_input); // PERBAIKAN: Gunakan full_input
        if (!trimmed_line.empty() && trimmed_line.back() == '\\') {
          size_t backslash_count = 0;
          for (auto it = trimmed_line.rbegin();
               it != trimmed_line.rend() && *it == '\\'; ++it) {
            backslash_count++;
          }
          if (backslash_count % 2 == 1) {
            EOF_IN_by_backslash = true;
            EOF_IN_by_operator = false;
          }
        }

        if (EOF_IN_by_operator) {
          EOF_IN_position = full_input.length();
        } else if (EOF_IN_by_backslash) {
          EOF_IN_position = full_input.length();
          size_t backslash_count = 0;
          for (auto it = full_input.rbegin();
               it != full_input.rend() && *it == '\\'; ++it) {
            backslash_count++;
          }
          if (backslash_count > 0) {
            EOF_IN_position = full_input.length() - backslash_count;
          }
        }

        current_prompt = "> ";
      }
    } else if (line.empty() && !EOF_IN) {
      full_input = "";
      break;
    }

    rl_reset_line_state();

  } while (EOF_IN && !received_sigint);

  // PERBAIKAN: Gunakan expanded input di akhir jika ada
  if (show_expanded_history && !expanded_input.empty()) {
    full_input = expanded_input;
  }

  if (!full_input.empty()) {
    std::string history_entry = full_input;

    // Clean up individual lines from history
    for (const auto &line : EOF_IN_lines) {
      if (!line.empty()) {
        HIST_ENTRY **hist_list = history_list();
        if (hist_list) {
          for (int i = 0; hist_list[i]; i++) {
            if (std::string(hist_list[i]->line) == line) {
              remove_history(i);
              break;
            }
          }
        }
      }
    }

    // Clean up trailing backslashes
    std::string trimmed_line = rtrim(full_input);
    if (!trimmed_line.empty() && trimmed_line.back() == '\\') {
      size_t backslash_start = trimmed_line.length() - 1;
      while (backslash_start > 0 && trimmed_line[backslash_start - 1] == '\\') {
        backslash_start--;
      }
      if ((trimmed_line.length() - backslash_start) % 2 == 1) {
        full_input = full_input.substr(
            0, full_input.length() -
                   (full_input.length() - trimmed_line.length()) - 1);
      }
    }

    // Add to history
    add_history(full_input.c_str());
  }

  return full_input;
}
