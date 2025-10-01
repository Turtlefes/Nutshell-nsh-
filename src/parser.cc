#include "parser.h"
#include "globals.h"
#include "expansion.h"

#include "terminal.h" // untuk safe_set_cooked_mode dan safe_set_raw_mode
#include "globals.h"  // untuk last_exit_code, exit_shell, dll.
#include "init.h"     // untuk save_history
#include <utils.h>
#include <readline/readline.h>
#include <readline/history.h>

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <cctype>
#include <algorithm> // Untuk std::find

#include <cstdlib>
#include <sstream>
// Fungsi untuk mengubah string menjadi TokenType
TokenType get_token_type(const std::string &s)
{
    if (s == "|") return TokenType::PIPE;
    if (s == "&&") return TokenType::AND_IF;
    if (s == "||") return TokenType::OR_IF;
    if (s == ";") return TokenType::SEMICOLON;
    if (s == "<") return TokenType::LESS;
    if (s == ">") return TokenType::GREAT;
    if (s == ">>") return TokenType::DGREAT;
    if (s == "<<") return TokenType::LESSLESS;
    if (s == "<<<") return TokenType::LESSLESSLESS;
    if (s == "&") return TokenType::AMPERSAND;
    if (s == "(") return TokenType::LPAREN;
    if (s == ")") return TokenType::RPAREN;
    if (s == "\\") return TokenType::BACKSLASH;
    if (s == "=") return TokenType::ASSIGNMENT_WORD;
    return TokenType::WORD;
}

std::vector<Token> Parser::tokenize(const std::string &input) const
{
    std::vector<Token> tokens;
    
    if (input.empty()) return tokens;
    
    std::string current_token;
    char in_quote = 0;
    bool escaped = false;
    bool is_assignment = true;
    bool in_assignment_word = false;
    bool in_arithmetic = false;
    bool in_command_subst = false;
    int paren_count = 0;
    int brace_count = 0;
    
    // Safety check untuk input sangat pendek
    if (input.length() < 2 && (input == "'" || input == "\"")) {
        tokens.push_back({TokenType::STRING, input});
        return tokens;
    }

    for (size_t i = 0; i < input.length(); ++i)
    {
        // PERBAIKAN: Pastikan i tidak melebihi batas
        if (i >= input.length()) break;
        
        char c = input[i];

        // Handle escape sequences
        if (escaped)
        {
            // PERBAIKAN: Pastikan tidak ada buffer overflow
            if (in_quote == '"') {
                // Dalam double quotes, hanya karakter tertentu yang perlu di-escape
                if (c == '$' || c == '`' || c == '"' || c == '\\' || c == '\n') {
                    current_token += c;
                } else {
                    current_token += '\\';
                    current_token += c;
                }
            } else {
                current_token += c;
            }
            escaped = false;
            continue;
        }

        if (c == '\\' && in_quote != '\'')
        {
            escaped = true;
            continue;
        }

        // Handle arithmetic expansion $((..)) dan $[...]
        if (!in_quote && !in_arithmetic && c == '$')
        {
            // PERBAIKAN: Tambahkan bounds checking yang lebih ketat
            if (i + 2 < input.length() && input[i+1] == '(' && input[i+2] == '(')
            {
                // Arithmetic expansion: $((...))
                if (!current_token.empty())
                {
                    tokens.push_back({is_assignment && is_env_assignment(current_token) ? 
                                    TokenType::ASSIGNMENT_WORD : TokenType::WORD, current_token});
                    current_token.clear();
                }
                in_arithmetic = true;
                paren_count = 1;
                current_token = "$((";
                i += 2; // Skip the "$((" part

                // PERBAIKAN: Tambahkan bounds checking dalam loop
                while (i < input.length() && paren_count > 0)
                {
                    i++;
                    if (i >= input.length()) break; // Safety check
                    
                    current_token += input[i];
                    if (input[i] == '(') paren_count++;
                    else if (input[i] == ')') paren_count--;
                    
                    // PERBAIKAN: Pastikan i+1 tidak melebihi batas
                    if (paren_count == 0 && i + 1 < input.length() && input[i+1] == ')')
                    {
                        current_token += ')';
                        i++;
                        break;
                    }
                }

                if (paren_count == 0) {
                    tokens.push_back({TokenType::WORD, current_token});
                } else {
                    std::cerr << "nsh: syntax error: unclosed arithmetic expansion\n";
                    return {};
                }
                current_token.clear();
                in_arithmetic = false;
                continue;
            }
            else if (i + 1 < input.length() && input[i+1] == '[')
            {
                // Legacy arithmetic expansion: $[...]
                if (!current_token.empty())
                {
                    tokens.push_back({is_assignment && is_env_assignment(current_token) ? 
                                    TokenType::ASSIGNMENT_WORD : TokenType::WORD, current_token});
                    current_token.clear();
                }
                current_token = "$[";
                i++; // Skip the '$'

                // PERBAIKAN: Tambahkan bounds checking
                while (i + 1 < input.length() && input[i] != ']')
                {
                    i++;
                    if (i >= input.length()) break;
                    current_token += input[i];
                }

                if (i < input.length() && input[i] == ']') {
                    tokens.push_back({TokenType::WORD, current_token});
                } else {
                    std::cerr << "nsh: syntax error: unclosed legacy arithmetic expansion\n";
                    return {};
                }
                current_token.clear();
                continue;
            }
        }

        // Handle command substitution $(...) dan `...`
        if (!in_quote && !in_arithmetic && c == '$' && i + 1 < input.length() && input[i+1] == '(')
        {
            if (!current_token.empty())
            {
                tokens.push_back({is_assignment && is_env_assignment(current_token) ? 
                                TokenType::ASSIGNMENT_WORD : TokenType::WORD, current_token});
                current_token.clear();
            }
            in_command_subst = true;
            paren_count = 1;
            current_token = "$(";
            i++; // Skip the '$'

            // PERBAIKAN: Tambahkan bounds checking
            while (i < input.length() && paren_count > 0)
            {
                i++;
                if (i >= input.length()) break;
                
                current_token += input[i];
                if (input[i] == '(') paren_count++;
                else if (input[i] == ')') paren_count--;
            }

            if (paren_count == 0) {
                tokens.push_back({TokenType::WORD, current_token});
            } else {
                std::cerr << "nsh: syntax error: unclosed command substitution\n";
                return {};
            }
            current_token.clear();
            in_command_subst = false;
            continue;
        }
        else if (!in_quote && !in_arithmetic && !in_command_subst && c == '`')
        {
            // Backtick command substitution: `command`
            if (!current_token.empty())
            {
                tokens.push_back({is_assignment && is_env_assignment(current_token) ? 
                                TokenType::ASSIGNMENT_WORD : TokenType::WORD, current_token});
                current_token.clear();
            }
            current_token += c;
            i++;

            // PERBAIKAN: Tambahkan bounds checking
            while (i < input.length() && input[i] != '`')
            {
                if (input[i] == '\\' && i + 1 < input.length()) {
                    // Escape dalam backticks
                    current_token += input[i];
                    i++;
                    if (i < input.length()) {
                        current_token += input[i];
                    }
                } else {
                    current_token += input[i];
                }
                i++;
                if (i >= input.length()) break;
            }
            if (i < input.length()) {
                current_token += input[i]; // Tambahkan closing backtick
            } else {
                std::cerr << "nsh: syntax error: unclosed backtick substitution\n";
                return {};
            }

            tokens.push_back({TokenType::WORD, current_token});
            current_token.clear();
            continue;
        }

        // Handle parameter expansion ${...}
        if (!in_quote && c == '$' && i + 1 < input.length() && input[i+1] == '{')
        {
            if (!current_token.empty())
            {
                tokens.push_back({is_assignment && is_env_assignment(current_token) ? 
                                TokenType::ASSIGNMENT_WORD : TokenType::WORD, current_token});
                current_token.clear();
            }
            brace_count = 1;
            current_token = "${";
            i++; // Skip the '$'

            // PERBAIKAN: Tambahkan bounds checking
            while (i < input.length() && brace_count > 0)
            {
                i++;
                if (i >= input.length()) break;
                
                current_token += input[i];
                if (input[i] == '{') brace_count++;
                else if (input[i] == '}') brace_count--;
            }

            if (brace_count == 0) {
                tokens.push_back({TokenType::WORD, current_token});
            } else {
                std::cerr << "nsh: syntax error: unclosed parameter expansion\n";
                return {};
            }
            current_token.clear();
            continue;
        }

        // Penanganan quote dengan bounds checking
        if (in_quote)
        {
            current_token += c;
            if (c == in_quote)
            {
                // Check if this is an escaped quote within the same quote type
                if (i > 0 && input[i-1] == '\\' && in_quote == '"') {
                    // In double quotes, escaped quote doesn't end the quote
                    continue;
                }
                
                in_quote = 0;
                if (!in_assignment_word)
                {
                    if (!current_token.empty()) {
                        tokens.push_back({TokenType::STRING, current_token});
                    }
                    current_token.clear();
                }
            }
            continue;
        }

        // Memulai quote baru dengan bounds checking
        if (c == '\'' || c == '"')
        {
            if (!current_token.empty() && !in_assignment_word)
            {
                tokens.push_back({is_assignment && is_env_assignment(current_token) ? 
                                TokenType::ASSIGNMENT_WORD : TokenType::WORD, current_token});
                current_token.clear();
            }
            in_quote = c;
            current_token += c;
            
            // Special case: empty quotes - handle immediately
            if (i + 1 < input.length() && input[i+1] == c) {
                current_token += c;
                i++;
                in_quote = 0;
                if (!in_assignment_word) {
                    if (!current_token.empty()) {
                        tokens.push_back({TokenType::STRING, current_token});
                    }
                    current_token.clear();
                }
            }
        }
        else if (isspace(c))
        {
            if (!current_token.empty())
            {
                tokens.push_back({is_assignment && is_env_assignment(current_token) ? 
                                TokenType::ASSIGNMENT_WORD : TokenType::WORD, current_token});
                current_token.clear();
            }
            in_assignment_word = false;
            is_assignment = true;
        }
        else if (std::string(";&|<>()").find(c) != std::string::npos)
        {
            if (!current_token.empty())
            {
                tokens.push_back({is_assignment && is_env_assignment(current_token) ? 
                                TokenType::ASSIGNMENT_WORD : TokenType::WORD, current_token});
                current_token.clear();
            }
            in_assignment_word = false;

            std::string op(1, c);
            // PERBAIKAN: Tambahkan bounds checking untuk operator multi-karakter
            if (i + 1 < input.length())
            {
                if (c == '|' && input[i + 1] == '|') { 
                    op = "||"; 
                    i++; 
                }
                else if (c == '&' && input[i + 1] == '&') { 
                    op = "&&"; 
                    i++; 
                }
                else if (c == '>' && input[i + 1] == '>') { 
                    op = ">>"; 
                    i++; 
                }
                else if (c == '<' && input[i + 1] == '<') 
                { 
                    op = "<<"; 
                    i++; 
                    if (i + 1 < input.length() && input[i + 1] == '<') { 
                        op = "<<<"; 
                        i++; 
                    }
                }
                else if (c == '(' && input[i + 1] == '(') { 
                    op = "(("; 
                    i++; 
                }
            }

            tokens.push_back({get_token_type(op), op});
            
            // Reset assignment context setelah operator tertentu
            if (op == ";" || op == "&" || op == "|" || op == "||" || op == "&&" || 
                op == "(" || op == ")" || op == "((") {
                is_assignment = true;
            } else {
                is_assignment = false;
            }
        }
        else if (c == '=')
        {
            current_token += c;
            if (is_assignment && !in_assignment_word && is_env_assignment(current_token))
            {
                in_assignment_word = true;
            }
            else
            {
                is_assignment = false;
            }
        }
        else if (c == '#')
        {
            // Comments
            if (!current_token.empty())
            {
                tokens.push_back({is_assignment && is_env_assignment(current_token) ? 
                                TokenType::ASSIGNMENT_WORD : TokenType::WORD, current_token});
                current_token.clear();
            }
            break; // Skip sisa baris
        }
        else
        {
            current_token += c;
            // Jika kita sedang dalam konteks assignment dan menemukan karakter non-identifier,
            // maka ini bukan assignment word yang valid
            if (in_assignment_word && !isalnum(c) && c != '_' && c != '=') {
                in_assignment_word = false;
                is_assignment = false;
            }
        }
    }

    // Penanganan error
    if (in_quote)
    {
        std::cerr << "nsh: syntax error: unclosed quote `" << in_quote << "'\n";
        return {};
    }
    if (in_arithmetic || in_command_subst || brace_count > 0 || paren_count > 0)
    {
        std::cerr << "nsh: syntax error: unclosed substitution\n";
        return {};
    }
    
    // Handle token terakhir
    if (!current_token.empty()) {
        TokenType type = TokenType::WORD;
        if (is_assignment && is_env_assignment(current_token)) {
            type = TokenType::ASSIGNMENT_WORD;
        }
        tokens.push_back({type, current_token});
    }

    return tokens;
}

void Parser::expand_aliases(std::vector<Token> &tokens)
{
    if (tokens.empty())
        return;

    bool is_command_start = true;
    std::set<std::string> expansion_guard;

    for (size_t i = 0; i < tokens.size(); ++i)
    {
        auto &token = tokens[i];

        if (is_command_start && token.type == TokenType::WORD)
        {
            auto it = aliases.find(token.text);
            if (it != aliases.end() && expansion_guard.find(token.text) == expansion_guard.end())
            {
                expansion_guard.insert(token.text);

                // Parse alias value
                std::vector<Token> alias_tokens = tokenize(it->second);
                if (!alias_tokens.empty())
                {
                    tokens.erase(tokens.begin() + i);
                    tokens.insert(tokens.begin() + i, alias_tokens.begin(), alias_tokens.end());
                    i--; // Process the inserted tokens
                    continue;
                }
            }
            is_command_start = false;
        }

        // Reset command start at operators
        if (token.type == TokenType::AND_IF || token.type == TokenType::OR_IF || 
            token.type == TokenType::SEMICOLON || token.type == TokenType::PIPE ||
            token.type == TokenType::AMPERSAND)
        {
            is_command_start = true;
            expansion_guard.clear();
        }
    }
}






std::vector<ParsedCommand> Parser::parse(const std::string &input)
{
    std::vector<ParsedCommand> command_list;
    if (input.empty() || input.find_first_not_of(" \t\n") == std::string::npos)
        return command_list;
      
    std::vector<Token> tokens = tokenize(input);
    if (tokens.empty())
        return command_list;
    
    expand_aliases(tokens);

    command_list.emplace_back();
    SimpleCommand current_simple_cmd;
    bool expect_redirect_file = false;
    TokenType last_redirect_type = TokenType::WORD;
    
    bool command_word_found = false;

    for (size_t i = 0; i < tokens.size(); ++i)
    {
        const Token &token = tokens[i];

        if (expect_redirect_file)
        {
            if (token.type == TokenType::WORD || token.type == TokenType::STRING)
            {
                std::string filename = token.text;
                // Remove quotes if present
                if (filename.size() >= 2 && 
                   ((filename.front() == '\'' && filename.back() == '\'') ||
                    (filename.front() == '"' && filename.back() == '"')))
                {
                    filename = filename.substr(1, filename.size() - 2);
                }

                if (last_redirect_type == TokenType::LESS)
                    current_simple_cmd.stdin_file = filename;
                else if (last_redirect_type == TokenType::GREAT)
                {
                    current_simple_cmd.stdout_file = filename;
                    current_simple_cmd.append_stdout = false;
                }
                else if (last_redirect_type == TokenType::DGREAT)
                {
                    current_simple_cmd.stdout_file = filename;
                    current_simple_cmd.append_stdout = true;
                }
                else if (last_redirect_type == TokenType::LESSLESS)
                    current_simple_cmd.here_doc_delimiter = filename;
                else if (last_redirect_type == TokenType::LESSLESSLESS)
                    current_simple_cmd.here_string_content = filename;
                
                expect_redirect_file = false;
            }
            else
            {
                std::cerr << "nsh: syntax error: expected filename after redirect" << std::endl;
                return {};
            }
            continue;
        }

        switch (token.type)
        {
            case TokenType::ASSIGNMENT_WORD:
            {
                if (!command_word_found)
                {
                    auto [var_name, value] = parse_env_assignment(token.text);
                    current_simple_cmd.env_vars[var_name] = value;
                    current_simple_cmd.exported_vars.insert(var_name);
                    
                    environ_map[var_name] = {value, false, false};
                }
                else
                {
                    current_simple_cmd.tokens.push_back(token.text);
                }
                break;
            }
            case TokenType::WORD:
            case TokenType::STRING:
                command_word_found = true;
                current_simple_cmd.tokens.push_back(token.text);
                break;
                
            case TokenType::PIPE:
                if (current_simple_cmd.tokens.empty() && current_simple_cmd.env_vars.empty())
                {
                    std::cerr << "nsh: syntax error near unexpected token `" << token.text << "'" << std::endl;
                    return {};
                }
                apply_expansions_and_wildcards(current_simple_cmd.tokens);
                command_list.back().pipeline.push_back(current_simple_cmd);
                current_simple_cmd = {};
                command_word_found = false;
                break;

            case TokenType::AND_IF:
            case TokenType::OR_IF:
            case TokenType::SEMICOLON:
                if (current_simple_cmd.tokens.empty() && current_simple_cmd.env_vars.empty())
                {
                    std::cerr << "nsh: syntax error near unexpected token `" << token.text << "'" << std::endl;
                    return {};
                }
                apply_expansions_and_wildcards(current_simple_cmd.tokens);
                command_list.back().pipeline.push_back(current_simple_cmd);
                current_simple_cmd = {};

                if (token.type == TokenType::AND_IF)
                    command_list.back().next_operator = ParsedCommand::Operator::AND;
                else if (token.type == TokenType::OR_IF)
                    command_list.back().next_operator = ParsedCommand::Operator::OR;
                else if (token.type == TokenType::SEMICOLON)
                    command_list.back().next_operator = ParsedCommand::Operator::SEQUENCE;

                command_list.emplace_back();
                command_word_found = false;
                break;

            case TokenType::LESS:
            case TokenType::GREAT:
            case TokenType::DGREAT:
            case TokenType::LESSLESS:
            case TokenType::LESSLESSLESS:
                expect_redirect_file = true;
                last_redirect_type = token.type;
                break;

            case TokenType::AMPERSAND:
                if (i == tokens.size() - 1)
                    command_list.back().background = true;
                else
                    current_simple_cmd.tokens.push_back(token.text);
                break;

            default:
                current_simple_cmd.tokens.push_back(token.text);
                break;
        }
    }

    if (!current_simple_cmd.tokens.empty() || !current_simple_cmd.env_vars.empty())
    {
        apply_expansions_and_wildcards(current_simple_cmd.tokens);
        command_list.back().pipeline.push_back(current_simple_cmd);
    }

    // Remove empty commands
    command_list.erase(
        std::remove_if(command_list.begin(), command_list.end(),
            [](const ParsedCommand &cmd) { return cmd.pipeline.empty(); }),
        command_list.end()
    );

    return command_list;
}

// Fungsi untuk mengekstrak nomor history dari string
int extract_history_number(const std::string& str) {
    if (str.empty() || str[0] != '!') return -1;
    
    try {
        if (str.length() > 1) {
            return std::stoi(str.substr(1));
        }
    } catch (const std::exception& e) {
        return -1;
    }
    return -1;
}

// Fungsi untuk mendapatkan history berdasarkan nomor - PERBAIKAN
std::string Parser::get_history_by_number(int number) {
    HIST_ENTRY** hist_list = history_list();
    if (!hist_list) {
        std::cerr << "nsh: history: history list not available" << std::endl;
        return "";
    }
    
    int hist_length = history_length;
    if (hist_length == 0) {
        std::cerr << "nsh: history: no history entries" << std::endl;
        return "";
    }
    
    // Handle negative indexing
    if (number < 0) {
        number = hist_length + number;
    }
    
    // Validasi range
    if (number < 0 || number >= hist_length) {
        std::cerr << "nsh: history: history position " << number << " not found" << std::endl;
        return "";
    }
    
    return hist_list[number]->line;
}

// Fungsi untuk mendapatkan history berdasarkan string pattern - PERBAIKAN
std::string Parser::get_history_by_pattern(const std::string& pattern) {
    HIST_ENTRY** hist_list = history_list();
    if (!hist_list) {
        std::cerr << "nsh: history: history list not available" << std::endl;
        return "";
    }
    
    int hist_length = history_length;
    if (hist_length == 0) {
        std::cerr << "nsh: history: no history entries" << std::endl;
        return "";
    }
    
    // Search from most recent to oldest
    for (int i = hist_length - 1; i >= 0; i--) {
        std::string entry = hist_list[i]->line;
        
        // For !pattern (starts with pattern)
        if (!pattern.empty() && pattern[0] != '?' && 
            entry.length() >= pattern.length() &&
            entry.substr(0, pattern.length()) == pattern) {
            return entry;
        }
        
        // For !?pattern? (contains pattern)
        if (!pattern.empty() && pattern[0] == '?' && 
            entry.find(pattern.substr(1)) != std::string::npos) {
            return entry;
        }
    }
    
    std::cerr << "nsh: history: no matching history entry for `" << pattern << "'" << std::endl;
    return "";
}

// Fungsi utama untuk history expansion - PERBAIKAN
bool Parser::expand_history(std::string& input) {
    if (input.empty() || input.find('!') == std::string::npos) {
        return false;
    }
    
    std::string result;
    size_t pos = 0;
    bool expanded = false;
    bool in_single_quote = false;
    bool in_double_quote = false;
    
    while (pos < input.length()) {
        size_t bang_pos = input.find('!', pos);
        
        if (bang_pos == std::string::npos) {
            // No more ! found, append the rest
            result += input.substr(pos);
            break;
        }
        
        // Append text before !
        result += input.substr(pos, bang_pos - pos);
        
        // Check if we're in quotes
        for (size_t i = pos; i < bang_pos; i++) {
            if (input[i] == '\'' && (i == 0 || input[i-1] != '\\')) {
                in_single_quote = !in_single_quote;
            } else if (input[i] == '"' && (i == 0 || input[i-1] != '\\')) {
                in_double_quote = !in_double_quote;
            }
        }
        
        // Skip history expansion inside single quotes
        if (in_single_quote) {
            result += '!';
            pos = bang_pos + 1;
            continue;
        }
        
        // Handle escaped \!
        if (bang_pos > 0 && input[bang_pos - 1] == '\\') {
            // Remove the backslash and keep the !
            result.pop_back(); // Remove the backslash
            result += '!';
            pos = bang_pos + 1;
            continue;
        }
        
        // Check what follows the !
        if (bang_pos + 1 >= input.length()) {
            // ! at the end of input
            result += '!';
            break;
        }
        
        char next_char = input[bang_pos + 1];
        std::string replacement;
        bool expansion_found = false;
        
        if (next_char == '!') {
            // !! - previous command
            replacement = get_history_by_number(-1);
            pos = bang_pos + 2;
            expansion_found = true;
        } else if (isdigit(next_char) || (next_char == '-' && bang_pos + 2 < input.length() && isdigit(input[bang_pos + 2]))) {
            // !n or !-n - history number n
            size_t end_pos = bang_pos + 2;
            while (end_pos < input.length() && (isdigit(input[end_pos]) || input[end_pos] == '-')) {
                end_pos++;
            }
            
            std::string num_str = input.substr(bang_pos + 1, end_pos - bang_pos - 1);
            int hist_num = extract_history_number("!" + num_str);
            replacement = get_history_by_number(hist_num);
            pos = end_pos;
            expansion_found = true;
        } else if (isalpha(next_char) || next_char == '_') {
            // !string - most recent command starting with string
            size_t end_pos = bang_pos + 2;
            while (end_pos < input.length() && (isalnum(input[end_pos]) || input[end_pos] == '_' || input[end_pos] == '-')) {
                end_pos++;
            }
            
            std::string pattern = input.substr(bang_pos + 1, end_pos - bang_pos - 1);
            replacement = get_history_by_pattern(pattern);
            pos = end_pos;
            expansion_found = true;
        } else if (next_char == '?') {
            // !?string? - most recent command containing string
            size_t end_pos = bang_pos + 2;
            size_t question_end = input.find('?', end_pos);
            if (question_end != std::string::npos) {
                std::string pattern = input.substr(end_pos, question_end - end_pos);
                replacement = get_history_by_pattern(pattern);
                pos = question_end + 1;
                expansion_found = true;
            } else {
                // No closing ?, treat as regular !
                result += '!';
                pos = bang_pos + 1;
                continue;
            }
        }
        
        if (expansion_found && !replacement.empty()) {
            result += replacement;
            expanded = true;
            
            // Update quote state for the replacement text
            for (char c : replacement) {
                if (c == '\'' && (result.size() == 1 || result[result.size()-2] != '\\')) {
                    in_single_quote = !in_single_quote;
                } else if (c == '"' && (result.size() == 1 || result[result.size()-2] != '\\')) {
                    in_double_quote = !in_double_quote;
                }
            }
        } else {
            // Expansion failed or not found, keep the original
            if (expansion_found && replacement.empty()) {
                // Expansion was attempted but failed - keep the ! pattern
                size_t end_pos = pos;
                result += input.substr(bang_pos, end_pos - bang_pos);
            } else {
                // Not a valid expansion pattern, keep the !
                result += '!';
                pos = bang_pos + 1;
            }
        }
    }
    
    if (expanded) {
        input = result;
        return true;
    }
    
    return false;
}

bool Parser::needs_EOF_IN(const std::string& line) const {
    if (line.empty()) return false;
    
    std::string trimmed_line = rtrim(line);
    if (trimmed_line.empty()) return false;
    
    // Check for backslash EOF_IN - only if backslash is the last non-whitespace character
    // and it's not part of an escape sequence
    if (!trimmed_line.empty() && trimmed_line.back() == '\\') {
        // Count the number of consecutive backslashes at the end
        size_t backslash_count = 0;
        for (auto it = trimmed_line.rbegin(); it != trimmed_line.rend() && *it == '\\'; ++it) {
            backslash_count++;
        }
        
        // If odd number of backslashes, then it's a true EOF_IN (not escaped)
        if (backslash_count % 2 == 1) {
            return true;
        }
    }
    
    // Check for operators that require EOF_IN
    std::vector<Token> tokens = tokenize(trimmed_line);
    if (tokens.empty()) return false;
    
    const Token& last_token = tokens.back();
    
    switch (last_token.type) {
        case TokenType::PIPE:
        case TokenType::AND_IF:
        case TokenType::OR_IF:
        case TokenType::LESS:
        case TokenType::GREAT:
        case TokenType::DGREAT:
        case TokenType::LESSLESS:
        case TokenType::LESSLESSLESS:
            return true;
        default:
            return false;
    }
}
