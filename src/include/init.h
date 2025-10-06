#ifndef INIT_H
#define INIT_H

#include <filesystem>

void initialize_environment();
void load_configuration();
void save_history();
void load_history();
void save_aliases();
void load_aliases();

// init.h - Tambahkan deklarasi ini
void initialize_session_manager();
void cleanup_session_manager();
void print_session_meme();

extern std::filesystem::path HOME_DIR;

#endif // INIT_H
