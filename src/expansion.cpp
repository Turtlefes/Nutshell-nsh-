#include "expansion.h"
#include "execution.h"
#include "globals.h"
#include "parser.h"
#include "utils.h"

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <pwd.h>
#include <algorithm>
#include <cstring>
#include <stack>
#include <cmath>
#include <functional>
#include <map>
#include <array> // ADD: For safer buffer handling

// ... (fungsi cleanup_pattern, match_wildcard, expand_wildcard, expand_tilde tetap sama) ...
std::string cleanup_pattern(const std::string &pattern)
{
    if (pattern.empty())
        return "";
    std::string cleaned_pattern;
    cleaned_pattern += pattern[0];
    for (size_t i = 1; i < pattern.length(); ++i)
    {
        if (pattern[i] == '*' && cleaned_pattern.back() == '*')
            continue;
        cleaned_pattern += pattern[i];
    }
    return cleaned_pattern;
}

bool match_wildcard(const char *pattern, const char *text)
{
    while (*pattern)
    {
        if (*pattern == '?')
        {
            if (!*text)
                return false;
            pattern++;
            text++;
        }
        else if (*pattern == '*')
        {
            while (*text)
            {
                if (match_wildcard(pattern + 1, text))
                    return true;
                text++;
            }
            return match_wildcard(pattern + 1, text);
        }
        else
        {
            if (*pattern != *text)
                return false;
            pattern++;
            text++;
        }
    }
    return !*text;
}

std::vector<std::string> expand_wildcard(const std::string &pattern)
{
    fs::path p(pattern);
    fs::path dir = p.has_parent_path() && !p.parent_path().empty() ? p.parent_path() : LOGICAL_PWD;
    std::string filename_pattern = p.filename().string();
    if (filename_pattern.find_first_of("*?") == std::string::npos && fs::exists(p))
        return {pattern};

    std::string cleaned_pattern = cleanup_pattern(filename_pattern);
    std::vector<std::string> matches;
    try
    {
        if (!fs::exists(dir) || !fs::is_directory(dir))
            return {pattern};
        for (const auto &entry : fs::directory_iterator(dir))
        {
            std::string filename = entry.path().filename().string();
            if (filename[0] == '.' && (cleaned_pattern.empty() || cleaned_pattern[0] != '.'))
                continue;
            if (match_wildcard(cleaned_pattern.c_str(), filename.c_str()))
            {
                fs::path match_path = p.has_parent_path() ? (p.parent_path() / filename) : fs::path(filename);
                matches.push_back(match_path.string());
            }
        }
    }
    catch (const fs::filesystem_error &)
    {
        return {pattern};
    }
    if (matches.empty())
        return {pattern};
    std::sort(matches.begin(), matches.end());
    return matches;
}


std::string expand_tilde(const std::string &path)
{
    if (path.empty() || path[0] != '~')
        return path;
    if (path.length() == 1 || path[1] == '/')
        return HOME_DIR.string() + path.substr(1);

    size_t slash_pos = path.find('/');
    std::string user = path.substr(1, slash_pos == std::string::npos ? std::string::npos : slash_pos - 1);
    struct passwd *pw = getpwnam(user.c_str());
    if (pw)
    {
        std::string home_path = pw->pw_dir;
        if (slash_pos != std::string::npos)
            home_path += path.substr(slash_pos);
        return home_path;
    }
    return path;
}

// FIX: Refactored to use modern C++ for safer memory and buffer handling
std::string execute_subshell_command(const std::string &cmd) {
    int stdout_pipe[2], stderr_pipe[2];
    if (pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1) {
        perror("pipe");
        return "";
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        close(stdout_pipe[0]); close(stdout_pipe[1]);
        close(stderr_pipe[0]); close(stderr_pipe[1]);
        return "";
    }
    
    if (pid == 0) { // Child process
        close(stdout_pipe[0]); close(stderr_pipe[0]);
        dup2(stdout_pipe[1], STDOUT_FILENO); close(stdout_pipe[1]);
        dup2(stderr_pipe[1], STDERR_FILENO); close(stderr_pipe[1]);
        
        Parser parser;
        try {
            auto commands = parser.parse(cmd);
            exit(commands.empty() ? 0 : execute_command_list(commands));
        } catch (const std::exception &e) {
            std::cerr << "nsh: " << e.what() << std::endl;
            exit(1);
        }
        exit(0);
    } else { // Parent process
        close(stdout_pipe[1]); close(stderr_pipe[1]);
        
        std::string result, error_output;
        std::array<char, 4096> buffer;
        ssize_t bytes_read;

        // Read stdout from child
        while ((bytes_read = read(stdout_pipe[0], buffer.data(), buffer.size())) > 0) {
            result.append(buffer.data(), bytes_read);
        }
        close(stdout_pipe[0]);

        // Read stderr from child
        while ((bytes_read = read(stderr_pipe[0], buffer.data(), buffer.size())) > 0) {
            error_output.append(buffer.data(), bytes_read);
        }
        close(stderr_pipe[0]);
        
        // Display error output directly
        if (!error_output.empty()) {
            std::cerr << error_output << std::flush;
        }
        
        waitpid(pid, nullptr, 0);
        
        // Remove trailing newline
        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }
        
        return result;
    }
}


bool is_operator(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || 
           c == '^' || c == '&' || c == '|' || c == '<' || c == '>' || c == '=' ||
           c == '!' || c == '~';
}

// FIX: Adjusted precedence to better match Bash (e.g., `**` > unary `-` > `*`)
// ADD: Added unary operators `u+`, `u-` for internal processing
int get_precedence(const std::string& op) {
    static const std::map<std::string, int> precedence = {
        {"||", 1}, {"&&", 2},
        {"|", 3}, {"^", 4}, {"&", 5},
        {"==", 6}, {"!=", 6},
        {"<", 7}, {"<=", 7}, {">", 7}, {">=", 7},
        {"<<", 8}, {">>", 8},
        {"+", 9}, {"-", 9},
        {"*", 10}, {"/", 10}, {"%", 10},
        {"u+", 11}, {"u-", 11}, {"!", 11}, {"~", 11}, // Unary operators
        {"**", 12}  // Exponentiation has highest precedence
    };
    
    auto it = precedence.find(op);
    return it != precedence.end() ? it->second : 0;
}

// ADD: Unary operators are right-associative
bool is_right_associative(const std::string& op) {
    return op == "**" || op == "u+" || op == "u-" || op == "!" || op == "~";
}

// FIX: Tokenizer now recognizes hex literals (e.g., 0xff)
std::vector<std::string> tokenize_arithmetic(const std::string& expr) {
    std::vector<std::string> tokens;
    for (size_t i = 0; i < expr.length(); ++i) {
        if (isspace(expr[i])) continue;

        if (isdigit(expr[i])) {
            std::string num;
            if (expr[i] == '0' && i + 1 < expr.length() && (expr[i+1] == 'x' || expr[i+1] == 'X')) {
                num += "0x";
                i += 2;
                while (i < expr.length() && isxdigit(expr[i])) {
                    num += expr[i++];
                }
                i--;
            } else {
                while (i < expr.length() && isdigit(expr[i])) {
                    num += expr[i++];
                }
                i--;
            }
            tokens.push_back(num);
        } else if (isalpha(expr[i]) || expr[i] == '_') {
            std::string var;
            while (i < expr.length() && (isalnum(expr[i]) || expr[i] == '_')) {
                var += expr[i++];
            }
            i--;
            tokens.push_back(var);
        } else if (is_operator(expr[i])) {
            std::string op(1, expr[i]);
            if (i + 1 < expr.length()) {
                std::string two_char_op = op + expr[i+1];
                if (get_precedence(two_char_op) > 0 || two_char_op == "!=") {
                     op = two_char_op;
                     i++;
                }
            }
            tokens.push_back(op);
        } else if (expr[i] == '(' || expr[i] == ')') {
            tokens.push_back(std::string(1, expr[i]));
        }
    }
    return tokens;
}

// FIX: Shunting-yard algorithm now correctly identifies and handles unary operators
std::vector<std::string> infix_to_postfix(const std::vector<std::string>& tokens) {
    std::vector<std::string> output;
    std::stack<std::string> op_stack;
    bool expect_operand = true;

    for (const auto& token : tokens) {
        if (token == "(") {
            op_stack.push(token);
            expect_operand = true;
        } else if (token == ")") {
            while (!op_stack.empty() && op_stack.top() != "(") {
                output.push_back(op_stack.top());
                op_stack.pop();
            }
            if (!op_stack.empty()) op_stack.pop(); // Pop '('
            expect_operand = false;
        } else if (get_precedence(token) > 0) {
            std::string op_to_push = token;
            if (expect_operand && (token == "+" || token == "-")) { // Handle unary +/-
                op_to_push = "u" + token;
            }

            while (!op_stack.empty() && op_stack.top() != "(" &&
                   (get_precedence(op_stack.top()) > get_precedence(op_to_push) ||
                   (get_precedence(op_stack.top()) == get_precedence(op_to_push) && 
                    !is_right_associative(op_to_push)))) {
                output.push_back(op_stack.top());
                op_stack.pop();
            }
            op_stack.push(op_to_push);
            expect_operand = true;
        } else { // Operand (number or variable)
            output.push_back(token);
            expect_operand = false;
        }
    }
    
    while (!op_stack.empty()) {
        output.push_back(op_stack.top());
        op_stack.pop();
    }
    return output;
}

// FIX: Now uses `std::stol` with base 0 for auto-detection of hex/octal
// ADD: Handles unary operator tokens `u+` and `u-`
long evaluate_postfix(const std::vector<std::string>& postfix) {
    std::stack<long> stack;
    
    for (const auto& token : postfix) {
        if (get_precedence(token) > 0) {
            // Handle unary operators
            if (token == "u+" || token == "u-" || token == "!" || token == "~") {
                 if (stack.empty()) throw std::runtime_error("Invalid expression: not enough operands for unary op");
                 long a = stack.top(); stack.pop();
                 if (token == "u+") stack.push(a);
                 else if (token == "u-") stack.push(-a);
                 else if (token == "!") stack.push(!a);
                 else if (token == "~") stack.push(~a);
                 continue;
            }

            if (stack.size() < 2) throw std::runtime_error("Invalid expression: not enough operands");
            long b = stack.top(); stack.pop();
            long a = stack.top(); stack.pop();
            
            if (token == "+") stack.push(a + b);
            else if (token == "-") stack.push(a - b);
            else if (token == "*") stack.push(a * b);
            else if (token == "/") { if (b == 0) throw std::runtime_error("Division by zero"); stack.push(a / b); }
            // NOTE: C++ '%' behavior for negative numbers matches Bash.
            // The sign of `a % b` is the sign of `a`. This is correct.
            else if (token == "%") { if (b == 0) throw std::runtime_error("Modulo by zero"); stack.push(a % b); }
            else if (token == "**") {
                if (b < 0) { // Bash-like integer exponentiation
                    stack.push(a == 1 ? 1 : (a == -1 ? (b % 2 == 0 ? 1 : -1) : 0));
                } else {
                    long res = 1; for (long i = 0; i < b; ++i) res *= a; stack.push(res);
                }
            }
            else if (token == "^") stack.push(a ^ b);
            else if (token == "&") stack.push(a & b);
            else if (token == "|") stack.push(a | b);
            else if (token == "<<") stack.push(a << b);
            else if (token == ">>") stack.push(a >> b);
            else if (token == "<") stack.push(a < b);
            else if (token == "<=") stack.push(a <= b);
            else if (token == ">") stack.push(a > b);
            else if (token == ">=") stack.push(a >= b);
            else if (token == "==") stack.push(a == b);
            else if (token == "!=") stack.push(a != b);
            else if (token == "&&") stack.push(a && b);
            else if (token == "||") stack.push(a || b);

        } else { // Operand
            try {
                // Use std::stol with base 0 to auto-detect hex (0x) and octal (0)
                stack.push(std::stol(token, nullptr, 0));
            } catch (...) {
                const char* val = getenv(token.c_str());
                try {
                    stack.push(val ? std::stol(val, nullptr, 0) : 0);
                } catch (...) {
                    stack.push(0); // Variable is not a valid number
                }
            }
        }
    }
    
    if (stack.size() != 1) throw std::runtime_error("Invalid expression");
    return stack.top();
}


std::string evaluate_arithmetic(const std::string& expr) {
    if (expr.empty()) return "0";
    try {
        auto tokens = tokenize_arithmetic(expr);
        auto postfix = infix_to_postfix(tokens);
        long result = evaluate_postfix(postfix);
        return std::to_string(result);
    } catch (const std::exception& e) {
        std::cerr << "nsh: arithmetic error: " << expr << ": " << e.what() << std::endl;
        return "0"; // Bash returns 0 on error within $((...))
    }
}


// ... (sisa file: expand_argument, apply_expansions_and_wildcards, etc. tetap sama) ...
std::string expand_argument(const std::string &token)
{
    std::string result;
    result.reserve(token.length());
    bool in_single_quote = false;
    bool in_double_quote = false;
    bool escaped = false;
    
    // ADD: Variabel untuk menghasilkan angka random yang konsisten dalam satu ekspansi
    static unsigned int random_seed = static_cast<unsigned int>(time(nullptr)) + getpid();

    for (size_t i = 0; i < token.length(); ++i)
    {
        char c = token[i];

        if (escaped)
        {
            if (in_double_quote) {
                if (c == '$' || c == '`' || c == '"' || c == '\\' || c == '\n') {
                    result += c;
                } else {
                    result += '\\';
                    result += c;
                }
            } else if (!in_single_quote) {
                result += c;
            } else {
                result += '\\';
                result += c;
            }
            escaped = false;
            continue;
        }

        if (c == '\\' && !in_single_quote)
        {
            escaped = true;
            continue;
        }

        if (c == '\'' && !in_double_quote)
        {
            in_single_quote = !in_single_quote;
            continue;
        }
        if (c == '"' && !in_single_quote)
        {
            in_double_quote = !in_double_quote;
            continue;
        }

        if (in_single_quote)
        {
            result += c;
            continue;
        }

        if (c == '$' && !in_single_quote)
        {
            size_t start = i + 1;
            if (start >= token.length())
            {
                result += '$';
                continue;
            }

            // ADD: Handle $RANDOM khusus
            if (token.compare(start, 5, "RANDOM") == 0 && 
                (start + 5 >= token.length() || !isalnum(token[start + 5]) && token[start + 5] != '_'))
            {
                // Generate random number between 0 and 32767 (matching Bash behavior)
                random_seed = xrand(random_seed, 0, 32767);
                result += std::to_string(random_seed);
                i = start + 5;
            }
            else if (token.compare(start, 3, "UID") == 0 && 
                (start + 3 >= token.length() || !isalnum(token[start + 3]) && token[start +3] != '_'))
            {
              uid_t uid = getuid();
              result += std::to_string(uid);
              i = start + 3;
            }
            else if (token.compare(start, 4, "EUID") == 0 && 
                (start + 4 >= token.length() || !isalnum(token[start + 4]) && token[start + 4] != '_'))
            {
              uid_t euid = geteuid();
              result += std::to_string(euid);
              i = start + 4;
            }
            else if (token[start] == '{')
            {
                size_t end = token.find('}', start);
                if (end != std::string::npos)
                {
                    std::string var_name = token.substr(start + 1, end - start - 1);
                    // ADD: Handle ${RANDOM} khusus
                    if (var_name == "RANDOM") {
                        random_seed = xrand(random_seed, 0, 32767);
                        result += std::to_string(random_seed);
                    } else {
                        const char *val = getenv(var_name.c_str());
                        if (val) result += val;
                    }
                    i = end;
                }
                else
                {
                    result += "${";
                    i = start;
                }
            }
            else if (token[start] == '(')
            {
                if (start + 1 < token.length() && token[start + 1] == '(') // Arithmetic
                {
                    int paren_level = 1;
                    size_t end = start + 2;
                    while (end < token.length() -1) {
                        if (token[end] == '(') paren_level++;
                        else if (token[end] == ')') paren_level--;
                        
                        if (paren_level == 0) break;
                        end++;
                    }

                    if (end < token.length() - 1 && token[end] == ')' && token[end + 1] == ')')
                    {
                        std::string expr = token.substr(start + 2, end - (start + 2));
                        result += evaluate_arithmetic(expr);
                        i = end + 1;
                    }
                    else
                    {
                        result += "$((";
                        i = start + 1;
                    }
                }
                else // Subshell
                {
                    int paren_level = 1;
                    size_t end = start + 1;
                    while (end < token.length()) {
                        if (token[end] == '(') paren_level++;
                        else if (token[end] == ')') paren_level--;
                        
                        if (paren_level == 0) break;
                        end++;
                    }

                    if (end < token.length() && token[end] == ')')
                    {
                        result += execute_subshell_command(token.substr(start + 1, end - start - 1));
                        i = end;
                    }
                    else
                    {
                        result += "$(";
                        i = start;
                    }
                }
            }
            else if (isalpha(token[start]) || token[start] == '_')
            {
                size_t end = start;
                while (end < token.length() && (isalnum(token[end]) || token[end] == '_'))
                    end++;
                std::string var_name = token.substr(start, end - start);
                
                // ADD: Handle $RANDOM khusus
                if (var_name == "RANDOM") {
                    random_seed = xrand(random_seed, 0, 32767);
                    result += std::to_string(random_seed);
                } else {
                    const char *val = getenv(var_name.c_str());
                    if (val) result += val;
                }
                i = end - 1;
            }
            else if (isdigit(token[start]))
            {
                i = start;
            }
            else if (token[start] == '?')
            {
                result += std::to_string(last_exit_code);
                i++;
            }
            else if (token[start] == '$')
            {
                result += std::to_string(getpid());
                i++;
            }
            else if (token[start] == '!')
            {
                if (!jobs.empty())
                    result += std::to_string(jobs.rbegin()->second.pgid);
                else
                    result += "0";
                i++;
            }
            else
            {
                result += '$';
            }
        }
        else if (c == '`' && !in_single_quote)
        {
            size_t end = token.find('`', i + 1);
            if (end != std::string::npos)
            {
                result += execute_subshell_command(token.substr(i + 1, end - i - 1));
                i = end;
            }
            else
            {
                result += '`';
            }
        }
        else
        {
            result += c;
        }
    }
    return result;
}

// ... (kode setelahnya tetap sama) ...

void apply_expansions_and_wildcards(std::vector<std::string> &tokens)
{
    if (tokens.empty())
        return;
    
    std::vector<std::string> new_tokens;
    
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        const auto &token = tokens[i];
        
        if (i == 0 && is_env_assignment(token))
        {
            size_t eq_pos = token.find('=');
            std::string var_name = token.substr(0, eq_pos);
            std::string value = token.substr(eq_pos + 1);
            
            std::string expanded_value = expand_argument(expand_tilde(value));
            new_tokens.push_back(var_name + "=" + expanded_value);
            continue;
        }

        std::string expanded_arg = expand_argument(expand_tilde(token));

        if (expanded_arg.find_first_of("*?") != std::string::npos && 
            !is_env_assignment(expanded_arg))
        {
            std::vector<std::string> expanded_wildcard = expand_wildcard(expanded_arg);
            new_tokens.insert(new_tokens.end(), expanded_wildcard.begin(), expanded_wildcard.end());
        }
        else
        {
            new_tokens.push_back(expanded_arg);
        }
    }
    
    tokens = new_tokens;
}

bool is_env_assignment(const std::string &token)
{
    size_t eq_pos = token.find('=');
    if (eq_pos == std::string::npos || eq_pos == 0)
        return false;
    for (size_t i = 0; i < eq_pos; i++)
    {
        if (!isalnum(token[i]) && token[i] != '_')
            return false;
    }
    return true;
}

std::pair<std::string, std::string> parse_env_assignment(const std::string &token)
{
    size_t eq_pos = token.find('=');
    if (eq_pos == std::string::npos || eq_pos == 0) {
        return {"", ""};
    }
    
    std::string var_name = token.substr(0, eq_pos);
    std::string value = token.substr(eq_pos + 1);

    std::string expanded_value = expand_argument(value);
    
    return {var_name, expanded_value};
}
