#ifndef COMMAND_H
#define COMMAND_H

#include <vector>
#include <string>
#include <map>
#include <set>

// Definisikan tipe-tipe redirection yang mungkin
enum class RedirectionType {
    NONE,
    REDIR_IN,          // < file
    REDIR_OUT,         // > file
    REDIR_OUT_APPEND,  // >> file
    HERE_DOC,          // << DELIMITER
    HERE_STRING,       // <<< string
    DUPLICATE_OUT,     // >&fd
    DUPLICATE_IN,      // <&fd
    CLOSE_FD,          // >&- atau <&-
    REDIR_OUT_ERR,     // &> file atau >& file
    REDIR_OUT_ERR_APPEND // &>> file
};

// Struct untuk menyimpan detail satu operasi redirection
struct Redirection {
    RedirectionType type = RedirectionType::NONE;
    int source_fd = -1;       // FD yang akan di-redirect (e.g., 0, 1, 2)
    std::string target_file;  // Nama file target
    std::string delimiter;    // Untuk here-doc
    std::string content;      // Untuk here-string
    int target_fd = -1;       // FD target untuk duplikasi
};

// Struktur untuk menyimpan satu perintah sederhana (misalnya, `ls -l`)
struct SimpleCommand
{
    std::vector<std::string> tokens;
    std::string stdin_file;
    std::string stdout_file;
    bool append_stdout = false;
    std::string here_doc_delimiter;
    std::string here_string_content;
    std::map<std::string, std::string> env_vars;
    std::set<std::string> exported_vars;
    std::vector<Redirection> redirections;
};

// Struktur untuk menyimpan satu baris perintah lengkap, yang bisa berupa pipeline
// Dalam parser.h, tambahkan ke struct ParsedCommand
struct ParsedCommand
{
    std::vector<SimpleCommand> pipeline;
    enum class Operator
    {
        NONE,
        AND,
        OR,
        SEQUENCE
    };
    Operator next_operator = Operator::NONE;
    bool background = false;
};

#endif // COMMAND_H
