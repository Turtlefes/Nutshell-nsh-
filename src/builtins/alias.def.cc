#include "builtins.h"
#include "globals.h"
#include "utils.h"
#include "expansion.h"
#include "init.h"
#include "parser.h"

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>

void handle_builtin_alias(const std::vector<std::string>& tokens) {
    if (tokens.size() == 1) {
        // Menampilkan semua alias yang ada
        if (aliases.empty()) {
            std::cout << "No aliases defined." << std::endl;
            last_exit_code = 0;
            return;
        }
        
        // Cari panjang nama alias terpanjang untuk formatting
        size_t max_name_length = 0;
        for (const auto& [name, value] : aliases) {
            if (name.length() > max_name_length) {
                max_name_length = name.length();
            }
        }
        
        // Tampilkan semua alias dengan formatting rapi
        for (const auto& [name, value] : aliases) {
            std::cout << "alias " << std::left << std::setw(max_name_length) << name 
                      << "='" << value << "'" << std::endl;
        }
        last_exit_code = 0;
        return;
    }
    
    // Kasus: Menampilkan alias tertentu (alias name1 name2...)
    // Cek jika tidak ada token dengan tanda '=' setelah token pertama
    bool has_assignment = false;
    for (size_t i = 1; i < tokens.size(); ++i) {
        if (tokens[i].find('=') != std::string::npos) {
            has_assignment = true;
            break;
        }
    }
    
    if (!has_assignment) {
        for (size_t i = 1; i < tokens.size(); ++i) {
            const std::string& alias_name = tokens[i];
            auto it = aliases.find(alias_name);
            if (it != aliases.end()) {
                std::cout << "alias " << it->first << "='" << it->second << "'" << std::endl;
                last_exit_code = 0;
            } else {
                std::cerr << "nsh: alias: " << alias_name << ": not found" << std::endl;
                last_exit_code = 1;
            }
        }
        return;
    }
    
    // Kasus: Mendefinisikan alias baru
    // Gabungkan semua token setelah "alias" menjadi satu string
    std::string full_cmd;
    for (size_t i = 1; i < tokens.size(); ++i) {
        if (i > 1) full_cmd += " ";
        full_cmd += tokens[i];
    }
    
    // Cari posisi '=' pertama yang tidak dalam quotes
    size_t equal_pos = std::string::npos;
    bool in_quote = false;
    char quote_char = 0;
    bool escaped = false;
    
    for (size_t i = 0; i < full_cmd.length(); ++i) {
        char c = full_cmd[i];
        
        if (escaped) {
            escaped = false;
            continue;
        }
        
        if (c == '\\') {
            escaped = true;
            continue;
        }
        
        if (in_quote) {
            if (c == quote_char) {
                in_quote = false;
            }
            continue;
        }
        
        if (c == '\'' || c == '"') {
            in_quote = true;
            quote_char = c;
            continue;
        }
        
        if (c == '=') {
            equal_pos = i;
            break;
        }
    }
    
    // Jika tidak ditemukan '=' yang tidak dalam quotes
    if (equal_pos == std::string::npos) {
        std::cerr << "nsh: alias: syntax error: missing '='" << std::endl;
        last_exit_code = 1;
        return;
    }
    
    // Ekstrak nama alias dan value
    std::string alias_name = full_cmd.substr(0, equal_pos);
    std::string alias_value = full_cmd.substr(equal_pos + 1);
    
    // Trim spasi dari nama alias
    alias_name = trim(alias_name);
    
    // Hapus quotes luar dari value jika ada
    alias_value = trim(alias_value);
    if (alias_value.size() >= 2) {
        if ((alias_value.front() == '\'' && alias_value.back() == '\'') ||
            (alias_value.front() == '"' && alias_value.back() == '"')) {
            alias_value = alias_value.substr(1, alias_value.length() - 2);
        }
    }
    
    // Validasi nama alias
    if (alias_name.empty()) {
        std::cerr << "nsh: alias: empty alias name" << std::endl;
        last_exit_code = 1;
        return;
    }
    
    // Cek karakter valid dalam nama alias (hanya alphanumeric dan underscore)
    for (char c : alias_name) {
        if (!isalnum(c) && c != '_') {
            std::cerr << "nsh: alias: invalid alias name: " << alias_name << std::endl;
            last_exit_code = 1;
            return;
        }
    }
    
    // Simpan alias
    aliases[alias_name] = alias_value;
    last_exit_code = 0;
    save_aliases();
    
    // Tampilkan konfirmasi
    std::cout << "alias " << alias_name << "='" << alias_value << "'" << std::endl;
}