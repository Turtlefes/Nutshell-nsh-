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
                auto [var_name, value] = parse_env_assignment(token.text);
                current_simple_cmd.env_vars[var_name] = value;
                current_simple_cmd.exported_vars.insert(var_name);
                break;
            }
            case TokenType::WORD:
            case TokenType::STRING:
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

bool Parser::needs_continuation(const std::string& line) const {
    if (line.empty()) return false;
    
    std::string trimmed_line = rtrim(line);
    if (trimmed_line.empty()) return false;
    
    // Check for backslash continuation (odd number of backslashes at end)
    size_t backslash_count = 0;
    for (auto it = trimmed_line.rbegin(); it != trimmed_line.rend() && *it == '\\'; ++it) {
        backslash_count++;
    }
    
    // Jika jumlah backslash ganjil, maka ini continuation
    if (backslash_count % 2 == 1) {
        return true;
    }
    
    // Check for operators that require continuation
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

// Function to clean continuation line is no longer needed because
// the main logic in get_multiline_input is now correct.
// We keep it here but it's not called anymore.
std::string Parser::clean_continuation_line(std::string line) const {
    std::string trimmed_line = rtrim(line);
    if (trimmed_line.empty()) return line;
    
    // Tokenize to understand what to remove
    std::vector<Token> tokens = tokenize(trimmed_line);
    if (tokens.empty()) return line;
    
    const Token& last_token = tokens.back();
    
    // Handle backslash continuation
    if (last_token.type == TokenType::WORD && !last_token.text.empty() && last_token.text.back() == '\\') {
        // Hitung jumlah backslash di akhir
        size_t backslash_count = 0;
        for (auto it = last_token.text.rbegin(); it != last_token.text.rend(); ++it) {
            if (*it == '\\') backslash_count++;
            else break;
        }
        
        // Jika jumlah backslash ganjil, hapus yang terakhir
        if (backslash_count % 2 == 1) {
            // Cari posisi backslash terakhir
            size_t pos = line.find_last_of('\\');
            if (pos != std::string::npos) {
                line = line.substr(0, pos);
                line = rtrim(line);
                line += " ";
            }
        }
    } 
    // Handle operator continuation
    else if (last_token.type == TokenType::PIPE ||
             last_token.type == TokenType::AND_IF ||
             last_token.type == TokenType::OR_IF ||
             last_token.type == TokenType::LESS ||
             last_token.type == TokenType::GREAT ||
             last_token.type == TokenType::DGREAT ||
             last_token.type == TokenType::LESSLESS ||
             last_token.type == TokenType::LESSLESSLESS) {
        
        // Find the position of the operator at the end
        size_t op_length = last_token.text.length();
        size_t pos = line.length();
        
        // Work backwards to find the operator
        while (pos > 0 && std::isspace(line[pos - 1])) {
            pos--;
        }
        
        if (pos >= op_length) {
            // Remove the operator
            line = line.substr(0, pos - op_length);
            line = rtrim(line);
            line += " ";
        }
    }
    
    return line;
}

// Jangan lupa update operators_to_clean untuk konsistensi:
const std::vector<std::string> operators_to_clean = {
    "\\", "|", "&&", "||", "<", ">", ">>", "<<", "<<<"
};

// Helper function untuk wrap prompt dengan escape sequences
std::string wrap_prompt_for_readline(const std::string& prompt) {
    // Cari escape sequences dan wrap dengan \001 dan \002
    std::string wrapped;
    bool in_escape = false;
    
    for (size_t i = 0; i < prompt.length(); i++) {
        if (prompt[i] == '\033' && prompt.substr(i, 2) == "\033[") {
            wrapped += "\001"; // Start non-printable
            in_escape = true;
        }
        
        wrapped += prompt[i];
        
        if (in_escape && prompt[i] == 'm') {
            wrapped += "\002"; // End non-printable
            in_escape = false;
        }
    }
    
    return wrapped;
}

// Tambahkan function untuk safe output selama input
void safe_output_during_input(const std::string& message) {
    rl_save_prompt();
    rl_replace_line("", 0);
    rl_redisplay();
    
    std::cout << "\r" << message << std::endl;
    
    rl_restore_prompt();
    rl_redisplay();
}


std::string Parser::get_multiline_input(const std::string& initial_prompt) {
    std::string full_input;
    std::string current_prompt = initial_prompt;
    bool continuation = false;
    std::vector<std::string> continuation_lines;

    std::string wrapped_prompt = wrap_prompt_for_readline(current_prompt);

    do {
        safe_set_cooked_mode();
        rl_save_prompt();
        
        char* line_read = readline(wrapped_prompt.c_str());
        safe_set_raw_mode();

        if (line_read == nullptr) {
            if (full_input.empty()) {
                std::cout << "exit" << std::endl;
                save_history();
                exit_shell(last_exit_code);
            } else {
                break;
            }
        }

        std::string line(line_read);
        free(line_read);

        if (!line.empty() || continuation) {
            // Handle backslash continuation - remove trailing backslash
            if (continuation && !line.empty() && line.back() == '\\') {
                line = line.substr(0, line.length() - 1);
                line = rtrim(line);
            }
            
            continuation_lines.push_back(line);
            full_input += line;

            // Check for continuation
            continuation = needs_continuation(line);
            
            if (continuation) {
                current_prompt = "> ";
                wrapped_prompt = wrap_prompt_for_readline(current_prompt);
            } else {
                full_input += " "; // Add space between continued lines
            }
        } else if (line.empty() && !continuation) {
            full_input = "";
        }

        rl_reset_line_state();
        
    } while (continuation);
    
    // Trim trailing space
    if (!full_input.empty() && full_input.back() == ' ') {
        full_input.pop_back();
    }
    
    if (!full_input.empty()) {
        std::string history_entry;
        for (size_t i = 0; i < continuation_lines.size(); i++) {
            if (i > 0) {
                history_entry += " ";
            }
            
            // Untuk baris continuation, hapus backslash di akhir
            std::string cleaned_line = continuation_lines[i];
            if (i < continuation_lines.size() - 1 && !cleaned_line.empty() && cleaned_line.back() == '\\') {
                cleaned_line = cleaned_line.substr(0, cleaned_line.length() - 1);
                cleaned_line = rtrim(cleaned_line);
            }
            
            history_entry += cleaned_line;
        }
        
        // Clean individual lines from history
        for (const auto& line : continuation_lines) {
            if (!line.empty()) {
                HIST_ENTRY** hist_list = history_list();
                if (hist_list) {
                    for (int i = 0; hist_list[i]; i++) {
                        if (hist_list[i]->line == line) {
                            remove_history(i);
                            break;
                        }
                    }
                }
            }
        }
        
        add_history(history_entry.c_str());
    }

    return full_input;
}

