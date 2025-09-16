#ifndef COMMAND_H
#define COMMAND_H

#include <vector>
#include <string>
#include <map>
#include <set>

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
};

// Struktur untuk menyimpan satu baris perintah lengkap, yang bisa berupa pipeline
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
