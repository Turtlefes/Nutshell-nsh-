#include "globals.h"

// --- Shell Information ---
const char *const shell_version   = "0.3.8";
const char *const shell_version_long = "0.3.8.56";
const char *const ext_shell_name  = "nsh";
const char *const release_date    = "2025";
const char *const COPYRIGHT       = "Copyright (c) 2025 Turtlefes. Bayu Setiawan";
const char *const LICENSE         = "License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>";

// --- Terminal Colors ---
const char *const RESET      = "\033[0m";
const char *const BOLD       = "\033[1m";
const char *const BLUE       = "\033[1;34m";
const char *const CYAN       = "\033[1;36m";
const char *const GREEN      = "\033[1;32m";
const char *const DARK_GREEN = "\033[0;32m";
const char *const RED        = "\033[1;31m";
const char *const YELLOW     = "\033[1;33m";

// --- Global Paths ---
fs::path HOME_DIR;
fs::path OLD_PWD;
fs::path LOGICAL_PWD;
fs::path ns_CONFIG_DIR;
fs::path ns_HISTORY_FILE;
fs::path ns_CONFIG_FILE;
fs::path ETCDIR;
fs::path ns_ALIAS_FILE;
fs::path ns_BOOKMARK_FILE;
fs::path ns_RC_FILE;

// --- Shell State ---
std::map<std::string, std::string> aliases;
std::vector<std::string> command_history;
size_t history_index = 0;
int last_exit_code = 0;
char **environ = nullptr;
volatile sig_atomic_t end_of_file_in_interrupt = 0;
std::unordered_map<std::string, binary_hash_info> binary_hash_loc;

// --- Environment management
void set_env_var(const std::string& name, const std::string& value, bool is_exported)
{
  // Check if this variable already exists as a default
  bool is_default = false;
  auto it = environ_map.find(name);
  if (it != environ_map.end()) {
    is_default = it->second.is_default;
  }
  
  environ_map[name] = {value, is_exported, is_default};
  
  if (is_exported)
  {
    // set traditional
    setenv(name.c_str(), value.c_str(), 1);
  }
  else
  {
    // Only set in environ_map, not in traditional
    unsetenv(name.c_str());
  }
}
void unset_env_var(const std::string& name)
{
  auto it = environ_map.find(name);
  if (it != environ_map.end()) {
    // Don't remove default variables, just clear their values
    if (it->second.is_default) {
      environ_map[name] = {"", false, true}; // Keep as default but empty
    } else {
      environ_map.erase(name);
    }
  }
  unsetenv(name.c_str());
}
const char* get_env_var(const std::string& name)
{
  auto it = environ_map.find(name);
  if (it != environ_map.end())
  {
    return it->second.value.c_str();
  }
  return getenv(name.c_str()); // fallback to traditional environ
}
std::unordered_map<std::string, var_info> environ_map;

// --- Job Control ---
std::map<int, Job> jobs;
int next_job_id = 1;
volatile pid_t shell_pgid = 0;
volatile pid_t foreground_pgid = 0;

// --- Extra counters ---
int history_number = 0;
int command_number = 0;