#ifndef PARSER_H
#define PARSER_H

#include <string>
#include <vector>
#include <map>
#include <set>
#include "command.h"  // Include command.h to use SimpleCommand and ParsedCommand

// Tipe-tipe token
enum class TokenType
{
    WORD,
    STRING,
    PIPE,
    AND_IF,
    OR_IF,
    SEMICOLON,
    LESS,
    GREAT,
    DGREAT,
    LESSLESS,
    LESSLESSLESS,
    AMPERSAND,
    LPAREN,
    RPAREN,
    ASSIGNMENT_WORD,
    UNKNOWN,
    END_OF_FILE,
    BACKSLASH
};

// Struktur untuk merepresentasikan sebuah token
struct Token
{
    TokenType type;
    std::string text;
};

class Parser
{
public:
    std::vector<ParsedCommand> parse(const std::string &input);
    // Method untuk mendapatkan input multiline
    std::string get_multiline_input(const std::string& initial_prompt);
    // Helper function untuk mendeteksi apakah baris memerlukan end_of_file_in
    bool needs_end_of_file_in(const std::string& line) const;
    std::vector<Token> tokenize(const std::string &input) const;

private:

    
    // Helper function untuk membersihkan end_of_file_in backslash
    
    void expand_aliases(std::vector<Token> &tokens);
};

#endif // PARSER_H