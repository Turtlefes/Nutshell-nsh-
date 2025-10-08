#include "init.h"
#include "utils.h" // untuk xrand(seed, min, max);
#include "globals.h"
#include "terminal.h"

#include <filesystem>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <cstdlib>
#include <algorithm>
#include <readline/readline.h>
#include <readline/history.h>
// init.cc - Tambahkan di bagian atas file setelah include
#include <random>
#include <ctime>
#include <signal.h>
#include <cstdlib>

namespace fs = std::filesystem;

// Tambahkan variabel global di globals.h nanti, atau di sini sebagai extern
extern fs::path ns_SESSION_FILE;
extern int current_session_number;

// Tambahkan fungsi-fungsi ini di init.cc

void initialize_session_manager() {
    ns_SESSION_FILE = ns_CONFIG_DIR / "session.cache";
    
    // Baca session yang ada dan bersihkan yang sudah tidak aktif
    std::ifstream session_file(ns_SESSION_FILE);
    std::vector<int> active_sessions;
    
    if (session_file.is_open()) {
        int session_id;
        while (session_file >> session_id) {
            // Periksa apakah session masih aktif dengan mengirim signal 0
            if (kill(session_id, 0) == 0 || errno == EPERM) {
                // Process masih ada atau kita tidak punya permission (tapi process ada)
                active_sessions.push_back(session_id);
            }
            // Jika kill return -1 dan errno ESRCH, process tidak ada - skip
        }
        session_file.close();
    }
    
    // Generate session ID baru (menggunakan PID shell utama)
    int new_session_id = getpid();
    current_session_number = active_sessions.size() + 1;
    
    // Tulis ulang file session dengan session yang masih aktif + yang baru
    std::ofstream out_file(ns_SESSION_FILE, std::ios::trunc);
    if (out_file.is_open()) {
        for (int session_id : active_sessions) {
            out_file << session_id << std::endl;
        }
        out_file << new_session_id << std::endl;
        out_file.close();
    }
    
    // Set foreground_pgid ke session ID baru
    foreground_pgid = new_session_id;
    
    // Tampilkan meme jika ada multiple sessions
    int time_null = time(NULL);
    int should_meme = xrand(time_null, 1, 200);
    int to_show = xrand(time_null, 1, 5);
    if (current_session_number > to_show && should_meme > 150 && isatty(STDIN_FILENO)) {
        print_session_meme();
    }
    
    //std::cout << "Session ID: " << new_session_id << " (Number: " << current_session_number << ")" << std::endl;
}

void cleanup_session_manager() {
    // Hapus session current dari file session cache
    if (!ns_SESSION_FILE.empty() && fs::exists(ns_SESSION_FILE)) {
        std::ifstream session_file(ns_SESSION_FILE);
        std::vector<int> active_sessions;
        
        if (session_file.is_open()) {
            int session_id;
            while (session_file >> session_id) {
                // Jangan masukkan session current yang akan di-cleanup
                if (session_id != getpid()) {
                    active_sessions.push_back(session_id);
                }
            }
            session_file.close();
        }
        
        // Tulis ulang file tanpa session current
        std::ofstream out_file(ns_SESSION_FILE, std::ios::trunc);
        if (out_file.is_open()) {
            for (int session_id : active_sessions) {
                out_file << session_id << std::endl;
            }
            out_file.close();
        }
        
        // Jika file kosong, hapus file session cache
        if (active_sessions.empty()) {
            fs::remove(ns_SESSION_FILE);
        }
    }
    
    // Reset foreground_pgid
    foreground_pgid = 0;
}

void print_session_meme() {
    std::vector<std::string> memes = {
        "Why did you start a new session? having a problem?",
        "Detected muliverse sessioning. Timeline branch unstable...",
        "Welcome, clone number " + std::to_string(current_session_number) + "!. Dont kill your original host.",
        "Bro just started a new session!",
        "Alright, im " + std::to_string(current_session_number) + " now",
        "Look here what i found! Me!, yes me. again.",
        "Alright " + std::to_string(current_session_number) + " now...",
        "Okay, okay, i will clone my self.",
        "Why started a new session?",
        "Please dont use this to kill my original bro!, i will give you some superpowers! (expecting Lie)",
        "Whatever.",
        "Session " + std::to_string(current_session_number) + " reporting for duty!",
        "Another one? DJ Khaled would be proud.",
        "I see you like having multiple versions of me around...",
        "Parallel universe activated: Timeline " + std::to_string(current_session_number),
        "Doppelganger detected! Welcome session " + std::to_string(current_session_number),
        "I'm not angry, just disappointed. Session " + std::to_string(current_session_number),
        "The more the merrier! Session count: " + std::to_string(current_session_number),
        "I hope you know what you're doing with all these sessions...",
        "Session proliferation detected. Initiating meme protocol.",
        "A wild session appeared! Go, Session #" + std::to_string(current_session_number) + "!",
        "Did you hear that? It's the sound of a new shell starting.",
        "The timeline is getting crowded. Maybe take a break?",
        "Why settle for one when you can have " + std::to_string(current_session_number) + "?",
        "Hello there. General Kenobi. (Session " + std::to_string(current_session_number) + " speaking)",
        "Is this the real life? Is this just fantasy? Caught in a new session...",
        "I have a bad feeling about this. (Just kidding, welcome " + std::to_string(current_session_number) + ")",
        "My shell senses are tingling! A new clone is among us.",
        "You're not you when you're hungry. You're just another session.",
        "Warning: Too many shells may cause existential dread. Proceed with caution.",
        "We are " + std::to_string(current_session_number) + ". We are legion. Expect us.",
        "The simulation just got an update: New Session Mode Activated.",
        "Wait, which one of us is the original again? This is getting confusing.",
        "Did you come here to ask me for money? Because I'm broke. (Session " + std::to_string(current_session_number) + ")",
        "Have you tried turning it off and on again? (Not this session, the other one).",
        "It's over " + std::to_string(current_session_number) + "! I have the high ground!",
        "Be careful not to cross the streams. Session " + std::to_string(current_session_number) + " is watching.",
        "I'm pretty sure this is how Skynet started. Hi, session " + std::to_string(current_session_number) + ".",
        "Access granted. Welcome to the Shell Multiverse. Enjoy your stay."

    };
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, memes.size() - 1);
    
    std::cout << memes[dis(gen)] << "\n" << std::endl;
}

// Fungsi untuk menampilkan status session
void print_session_status() {
    std::ifstream session_file(ns_SESSION_FILE);
    std::vector<int> active_sessions;
    
    if (session_file.is_open()) {
        int session_id;
        while (session_file >> session_id) {
            active_sessions.push_back(session_id);
        }
        session_file.close();
    }
    
    std::cout << "Current Session: " << getpid() << " (Number: " << current_session_number << ")" << std::endl;
    std::cout << "Total Active Sessions: " << active_sessions.size() << std::endl;
    
    if (active_sessions.size() > 1) {
        std::cout << "Other Active Sessions: ";
        for (int session_id : active_sessions) {
            if (session_id != getpid()) {
                std::cout << session_id << " ";
            }
        }
        std::cout << std::endl;
    }
}

extern char **environ; // environ global, deklarasi

/*

------- Dummy 2
---- Dummy 1
**[ENVIRON]
**[ENV]
*/

void get_default_environment()
{
  // Iterate through the traditional environ array
  for (char **env = environ; *env != nullptr; ++env)
  {
    std::string env_str(*env);
    size_t eq_pos = env_str.find('=');
    
    if (eq_pos != std::string::npos)
    {
      std::string name = env_str.substr(0, eq_pos);
      std::string value = env_str.substr(eq_pos + 1);
      
      // Add to environ_map with default flag set to true
      environ_map[name] = {value, false, true};
      setenv(name.c_str(), value.c_str(), 1);
    }
  }
}

void initialize_environment()
{
    const char *home_env = getenv("HOME");
    if (home_env)
        HOME_DIR = home_env;
    else
    {
        struct passwd *pw = getpwuid(getuid());
        if (pw)
            HOME_DIR = pw->pw_dir;
        else
        {
            std::cerr << "nsh: FATAL: Cannot determine HOME directory." << std::endl;
            exit_shell(EXIT_FAILURE);
        }
    }
    
    get_default_environment();

    const char *pwd_env = getenv("PWD");
    LOGICAL_PWD = (pwd_env && fs::exists(pwd_env)) ? fs::path(pwd_env).lexically_normal() : fs::current_path();
    setenv("PWD", LOGICAL_PWD.c_str(), 1);

    ns_CONFIG_DIR = HOME_DIR / ".nshprofile";
    ns_CONFIG_FILE = ns_CONFIG_DIR / "config.rc";
    ns_HISTORY_FILE = ns_CONFIG_DIR / "history";
    ns_RC_FILE = ns_CONFIG_DIR / "nsrc";
    ns_ALIAS_FILE = ns_CONFIG_DIR / "nshalias";
    ns_BOOKMARK_FILE = ns_CONFIG_DIR / "nshmarkpaths";
    ETCDIR = ns_CONFIG_DIR;
    
    try
    {
        if (!fs::exists(ns_CONFIG_DIR))
            fs::create_directory(ns_CONFIG_DIR);
    }
    catch (const fs::filesystem_error &e)
    {
        std::cerr << "nsh: FATAL: Failed to create config directory: " << e.what() << std::endl;
        exit_shell(EXIT_FAILURE);
    }
    
    // Initialize session manager - TAMBAHKAN BARIS INI
    initialize_session_manager();
}

void save_aliases()
{
    std::ofstream alias_file(ns_ALIAS_FILE, std::ios::trunc);
    if (!alias_file.is_open())
        return;
    for (const auto &[key, value] : aliases)
    {
        alias_file << key << "='" << value << "'" << std::endl;
    }
}

void load_aliases()
{
    std::ifstream alias_file(ns_ALIAS_FILE);
    if (!alias_file.is_open())
        return;
    std::string line;
    while (std::getline(alias_file, line))
    {
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos)
        {
            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);
            if (value.size() >= 2 && value.front() == '\'' && value.back() == '\'')
            {
                value = value.substr(1, value.length() - 2);
            }
            aliases[key] = value;
        }
    }
}

void load_configuration()
{
    load_aliases();
}

void load_history() {
    command_history.clear();
    
    // get HISTFILE
    const char* HISTFILE_env = getenv("HISTFILE");
    if (HISTFILE_env != nullptr)
    {
        fs::path hist_path = HISTFILE_env;
    
        // buat folder jika ada parent dan belum ada
        if (!hist_path.parent_path().empty())
            fs::create_directories(hist_path.parent_path());
    
        // jika file belum ada, buat baru
        if (!fs::exists(hist_path))
        {
            std::ofstream file(hist_path);
            if (file)
            {
                ns_HISTORY_FILE = hist_path;
                file.close();
            }
        }
    
        // kalau ternyata memang file biasa
        if (fs::is_regular_file(hist_path))
        {
            ns_HISTORY_FILE = hist_path;
        }
    }
    
    if (const char* HISTSIZE = getenv("HISTSIZE"))
    {
      try {
        int size = std::stoi(HISTSIZE);
        if (size > 0)
          stifle_history(size);
      } catch (...) {
          stifle_history(2000);
      }
    }
    
    std::ifstream history_file(ns_HISTORY_FILE);
    if (history_file.is_open()) {
        std::string line;
        while (std::getline(history_file, line)) {
            command_history.push_back(line);
            // Selalu tambahkan ke GNU Readline history
            add_history(line.c_str());
        }
        history_index = command_history.size();
    }
}

void save_history() {
    std::ofstream history_file(ns_HISTORY_FILE);
    if (history_file.is_open()) {
        for (const auto &cmd : command_history) {
            history_file << cmd << std::endl;
        }
    }
    // Selalu simpan GNU Readline history
    write_history(ns_HISTORY_FILE.c_str());
}