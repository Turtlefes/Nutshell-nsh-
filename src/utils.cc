#include "utils.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <sys/ioctl.h>
#include <unistd.h>
// utils.cc
#include <readline/history.h>
#include <readline/readline.h>

void clear_history_list() { clear_history(); }

int get_terminal_width() {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    const char *columns = getenv("COLUMNS");
    if (columns) {
      return std::max(10, std::atoi(columns));
    }
    return 80;
  }
  return ws.ws_col;
}

void safe_print(const std::string &text) {
  int width = get_terminal_width();
  std::stringstream ss(text);
  std::string line;

  while (std::getline(ss, line)) {
    if (line.length() > (size_t)width) {
      std::cout << line.substr(0, width - 3) << "..." << std::endl;
    } else {
      std::cout << line << std::endl;
    }
  }
}

std::string trim(const std::string &str) {
  size_t start = str.find_first_not_of(" \t\n\r");
  if (start == std::string::npos)
    return "";

  size_t end = str.find_last_not_of(" \t\n\r");
  return str.substr(start, end - start + 1);
}

std::string rtrim(const std::string &s) {
  std::string result = s;
  result.erase(std::find_if(result.rbegin(), result.rend(),
                            [](unsigned char ch) { return !std::isspace(ch); })
                   .base(),
               result.end());
  return result;
}

bool ends_with_EOF_IN_operator(const std::string &line) {
  std::string trimmed_line = rtrim(line);
  if (trimmed_line.empty())
    return false;
  if (trimmed_line.back() == '\\')
    return true;
  if (trimmed_line.length() >= 2) {
    std::string last_two = trimmed_line.substr(trimmed_line.length() - 2);
    if (last_two == "&&" || last_two == "||")
      return true;
  }
  if (trimmed_line.back() == '|')
    return true;
  return false;
}

size_t visible_width(const std::string &s) {
  bool in_escape = false;
  size_t width = 0;

  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '\033') {
      in_escape = true;
      continue;
    }

    if (in_escape) {
      if (s[i] == 'm') {
        in_escape = false;
      } else if (s[i] == '[') {
        while (i < s.size() && !isalpha(s[i]))
          i++;
        if (i < s.size() && isalpha(s[i]))
          in_escape = false;
      }
      continue;
    }

    if ((s[i] & 0xC0) == 0x80) {
      continue;
    }

    if (static_cast<unsigned char>(s[i]) < 0x80) {
      width++;
    } else {
      width++;
      while (i + 1 < s.size() && (s[i + 1] & 0xC0) == 0x80) {
        i++;
      }
    }
  }
  return width;
}

unsigned int xrand(unsigned int seed, int min, int max) {
  unsigned int results = 1103515245U * seed + 12345U;
  results = (results % (max - min + 1) + min);
  return results;
}

void clear_screen() {
  // 1️⃣ Cek TTY
  if (!isatty(STDOUT_FILENO)) {
    std::cout << std::endl;
    return;
  }

  // 2️⃣ Ambil PATH environment
  const char *path_cstr = std::getenv("PATH");
  std::string path_env;
  if (!path_cstr)
    path_env = ""; // PATH null → kosong
  else
    path_env = path_cstr;

  // 3️⃣ Cek PATH tidak kosong
  if (!path_env.empty()) {
    size_t start = 0, end;
    while ((end = path_env.find(':', start)) != std::string::npos) {
      std::string dir = path_env.substr(start, end - start);
      std::string fullpath = dir + "/clear";
      if (access(fullpath.c_str(), X_OK) == 0) { // ada & executable
        std::system(fullpath.c_str());
        return;
      }
      start = end + 1;
    }
    // Cek sisa path terakhir
    std::string dir = path_env.substr(start);
    std::string fullpath = dir + "/clear";
    if (!dir.empty() && access(fullpath.c_str(), X_OK) == 0) {
      std::system(fullpath.c_str());
      return;
    }
  }

  // 4️⃣ Coba TERM dan tput clear
  const char *term = std::getenv("TERM");
  if (term && std::system("tput clear 2>/dev/null") == 0) {
    return;
  }

  // 5️⃣ Fallback ANSI escape
  std::cout << "\033[2J\033[H";
  std::cout.flush();

  // 6️⃣ Extra fallback: newline sebanyak rows terminal
  struct winsize w{};
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_row > 0) {
    for (int i = 0; i < w.ws_row; ++i) {
      std::cout << "\n";
    }
    std::cout << "\033[H"; // balikkan cursor ke atas
    std::cout.flush();
  } else {
    // fallback terakhir
    std::cout << std::string(50, '\n');
  }
}

void input_redisplay() { rl_redisplay(); }

// In utils.cc
bool is_string_numeric(const std::string &s) {
  if (s.empty())
    return false;
  for (char c : s) {
    if (!std::isdigit(c)) {
      return false;
    }
  }
  return true;
}
