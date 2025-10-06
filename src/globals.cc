#include <iostream>
#include "globals.h"
#include "execution.h"

//#
// --- Shell Information ---
const char *const shell_version   = "0.3.8";
const char *const shell_version_long = "0.3.8.70";
const char *const ext_shell_name  = "nsh";
const char *const release_date    = "2025";
const char *const COPYRIGHT       = "Copyright (c) 2025 Turtlefes. Bayu Setiawan";
const char *const LICENSE         = "License GPLv3+: GNU GPL version 3 or later <https://gnu.org/licenses/gpl.html>";
std::string shell_desc = "Nutshell (nsh) - Comes packed with features (and bugs). You get the standard UNIX tools, plus some... interesting additions you won't find anywhere else. Consider the bugs as bonus content.";

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
volatile sig_atomic_t received_sigint = 0;
volatile int dont_execute_first = 0; // dont execute command if == 1;
std::unordered_map<std::string, binary_hash_info> binary_hash_loc;
// globals.cc - Tambahkan definisi
fs::path ns_SESSION_FILE;
int current_session_number = 1;

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
  
  /*
  if (is_exported || is_default)
  {
    // set traditional
    setenv(name.c_str(), value.c_str(), 1);
  }
  else
  {
    // Only set in environ_map, not in traditional
    unsetenv(name.c_str());
  }
  */
  
  // we set it traditionally, because envp uses environ_map, so this is okay
  setenv(name.c_str(), value.c_str(), 1);
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
// globals.cc (Tambahkan di dekat deklarasi variabel jobs)
// ...
std::map<int, Job> jobs;
int next_job_id = 1; 
volatile pid_t shell_pgid = 0;
volatile pid_t foreground_pgid = 0;  // Sekarang digunakan untuk session management
volatile pid_t shell_pid = 0; // NEW: PID dari shell nsh saat init.cc

// Job tracking
int last_launched_job_id = 0;
int current_job_id = 0;
int previous_job_id = 0;

/**
 * @brief Finds the most recent job (highest job ID)
 */
int find_most_recent_job() {
    if (jobs.empty()) return 0;
    
    int max_id = 0;
    for (const auto& [id, job] : jobs) {
        if (id > max_id) {
            max_id = id;
        }
    }
    return max_id;
}

/**
 * @brief Finds the second most recent job
 */
int find_second_most_recent_job() {
    if (jobs.size() < 2) return 0;
    
    int max1 = 0, max2 = 0;
    for (const auto& [id, job] : jobs) {
        if (id > max1) {
            max2 = max1;
            max1 = id;
        } else if (id > max2) {
            max2 = id;
        }
    }
    return max2;
}

/**
 * @brief Validates if a job is still active
 */
bool is_job_active(int job_id) {
    auto it = jobs.find(job_id);
    if (it == jobs.end()) return false;
    
    // Check if process group still exists
    if (kill(-it->second.pgid, 0) == -1 && errno == ESRCH) {
        // Process group doesn't exist, remove the job
        jobs.erase(it);
        return false;
    }
    
    return true;
}

/**
 * @brief Gets active job count
 */
int get_active_job_count() {
    // Clean up stale jobs first
    validate_and_cleanup_jobs();
    return jobs.size();
}


// Fungsi untuk memperbarui job tracking ketika job selesai
void update_job_tracking(int finished_job_id) {
    if (finished_job_id == current_job_id) {
        previous_job_id = current_job_id;
        current_job_id = find_most_recent_job();
    } else if (finished_job_id == previous_job_id) {
        previous_job_id = find_second_most_recent_job();
    }
}


// --- Extra counters ---
int history_number = 0;
int command_number = 0;