#ifndef GLOBALS_H
#define GLOBALS_H

#include "platform.h"
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include <sys/resource.h>

namespace fs = std::filesystem;

// --- Shell Information ---
extern std::string program_name;
extern const char *const shell_version;
extern const char *const shell_version_long;
extern const char *const ext_shell_name;
extern const char *const release_date;
extern const char *const COPYRIGHT;
extern const char *const LICENSE;
extern std::string shell_desc;

// --- Terminal Colors ---
extern const char *const RESET;
extern const char *const BOLD;
extern const char *const BLUE;
extern const char *const CYAN;
extern const char *const GREEN;
extern const char *const DARK_GREEN;
extern const char *const RED;
extern const char *const YELLOW;

// --- Global Paths ---
extern fs::path HOME_DIR;
extern fs::path OLD_PWD;
extern fs::path LOGICAL_PWD;
extern fs::path ns_CONFIG_DIR;
extern fs::path ns_HISTORY_FILE;
extern fs::path ns_CONFIG_FILE;
extern fs::path ETCDIR;
extern fs::path ns_ALIAS_FILE;
extern fs::path ns_BOOKMARK_FILE;
extern fs::path ns_RC_FILE;

// --- Shell State ---
extern std::map<std::string, std::string> aliases;
extern std::vector<std::string> command_history;
extern size_t history_index;
extern int last_exit_code;
extern char **environ;
extern volatile sig_atomic_t received_sigint;
extern volatile int dont_execute_first;
struct binary_hash_info {
  std::string path;
  std::string command_name;
  size_t hits = 0;
};
extern std::unordered_map<std::string, binary_hash_info> binary_hash_loc;
// globals.h - Tambahkan di bagian variabel global
extern fs::path ns_SESSION_FILE;
extern int current_session_number;

// --- Environ management ---
void set_env_var(const std::string &name, const std::string &value,
                 bool is_exported = false);
void unset_env_var(const std::string &name);
const char *get_env_var(const std::string &name);
struct var_info {
  std::string value;
  bool is_exported;
  bool is_default;
};
extern std::unordered_map<std::string, var_info> environ_map;

// --- Job Control Structures ---
enum class JobStatus {
  RUNNING,  // Job sedang berjalan
  STOPPED,  // Job dihentikan (misalnya oleh Ctrl+Z)
  DONE,     // Job selesai dengan kode keluar 0
  EXITED,   // Job selesai dengan kode keluar non-0
  SIGNALED, // Job diakhiri oleh sinyal
  UNKNOWN   // Status tidak diketahui
};

struct Job {
  pid_t pgid;
  std::string command;
  JobStatus status;
  struct rusage usage;
  int term_status;              // Holds exit code or signal number
  pid_t shell_pid = 0;          // PID dari shell pemilik job (Session ID)
  struct timeval start_tv = {}; // Waktu mulai job (untuk CPU %)
};

extern std::map<int, Job> jobs;
extern int next_job_id;
extern volatile pid_t shell_pgid;
extern volatile pid_t shell_pid;
extern volatile pid_t foreground_pgid;

// Global variables to track the current and previous jobs
// Job tracking
extern int last_launched_job_id;
// Pastikan baris ini sudah ada:
extern int current_job_id;  // Job ID yang ditandai dengan '+' atau '%%'
extern int previous_job_id; // Job ID yang ditandai dengan '-'
// Fungsi helper untuk job tracking
int find_most_recent_job();
int find_second_most_recent_job();
bool is_job_active(int job_id);
int get_active_job_count();
void update_job_tracking(int finished_job_id);

// Fungsi untuk melakukan ekspansi jobspec ke PGID
int jobspec_to_pgid(const std::string &jobspec, pid_t &pgid_out);
// Returns: 0 on success, 1 on job not found/error.

// --- Extra counters (from errors) ---
extern int history_number;
extern int command_number;

// --- Safe memory allocation functions ---
inline void *safe_malloc(size_t size) {
  void *ptr = malloc(size);
  if (!ptr && size > 0)
    throw std::bad_alloc();
  return ptr;
}

inline void *safe_realloc(void *ptr, size_t size) {
  void *new_ptr = realloc(ptr, size);
  if (!new_ptr && size > 0) {
    free(ptr);
    throw std::bad_alloc();
  }
  return new_ptr;
}

inline void *safe_calloc(size_t num, size_t size) {
  void *ptr = calloc(num, size);
  if (!ptr && num > 0 && size > 0)
    throw std::bad_alloc();
  return ptr;
}

inline char *safe_strdup(const char *str) {
  if (!str)
    return nullptr;
  char *new_str = static_cast<char *>(malloc(strlen(str) + 1));
  if (!new_str)
    throw std::bad_alloc();
  strcpy(new_str, str);
  return new_str;
}

#endif // GLOBALS_H