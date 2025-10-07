#ifndef PARSER_H
#define PARSER_H

#include "command.h" // Include command.h to use SimpleCommand and ParsedCommand
#include <map>
#include <set>
#include <string>
#include <vector>

// Tipe-tipe token
enum class TokenType {
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
struct Token {
  TokenType type;
  std::string text;
};

class Parser {
public:
  std::vector<ParsedCommand> parse(const std::string &input);
  // Method untuk mendapatkan input multiline
  // Helper function untuk mendeteksi apakah baris memerlukan EOF_IN
  bool expand_history(std::string &input);
  bool needs_EOF_IN(const std::string &line) const;
  std::vector<Token> tokenize(const std::string &input) const;

private:
  std::string get_history_by_number(int number);
  std::string get_history_by_pattern(const std::string &pattern);
  void expand_aliases(std::vector<Token> &tokens);
  std::string clean_EOF_IN_line(std::string line) const;
};

#endif // PARSER_H