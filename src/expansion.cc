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
#include <cmath>      // Diperlukan untuk std::abs, std::fmod, std::pow, std::trunc
#include <limits>     // Diperlukan untuk std::numeric_limits
#include <stdexcept>  // Diperlukan untuk std::runtime_error
#include <iomanip>    // Diperlukan untuk std::setprecision, std::fixed

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

double evaluate_postfix(const std::vector<std::string>& postfix); // forward declaration for tokenize_arithmetic

// FIX: Tokenizer now recognizes hex literals (e.g., 0xff) AND decimal numbers
std::vector<std::string> tokenize_arithmetic(const std::string& expr) {
    std::vector<std::string> tokens;
    for (size_t i = 0; i < expr.length(); ++i) {
        if (isspace(expr[i])) continue;

        if (isdigit(expr[i])) {
            std::string num;
            bool is_hex = false;
            bool is_decimal = false;
            
            // Check for hex prefix
            if (expr[i] == '0' && i + 1 < expr.length() && (expr[i+1] == 'x' || expr[i+1] == 'X')) {
                is_hex = true;
                num += "0x";
                i += 2;
                while (i < expr.length() && isxdigit(expr[i])) {
                    num += expr[i++];
                }
                i--;
            } else {
                // Handle decimal numbers (integer and floating point)
                while (i < expr.length() && (isdigit(expr[i]) || expr[i] == '.')) {
                    if (expr[i] == '.') {
                        if (is_decimal) break; // Second decimal point, stop
                        is_decimal = true;
                    }
                    num += expr[i++];
                }
                i--;
            }
            tokens.push_back(num);
        } else if (isalpha(expr[i]) || expr[i] == '_' || expr[i] == '?') {
            // ADD: Support for '?' in puzzles like ?+8=21
            std::string var;
            while (i < expr.length() && (isalnum(expr[i]) || expr[i] == '_' || expr[i] == '?')) {
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
        } else if (expr[i] == '=') {
            // Handle = operator for puzzles
            std::string op(1, expr[i]);
            if (i + 1 < expr.length() && expr[i+1] == '=') {
                op = "==";
                i++;
            }
            tokens.push_back(op);
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

// --- GANTI FUNGSI evaluate_postfix ---
// FIX: Menggunakan 'double' untuk mendukung angka desimal.
// ADD: Menambahkan error handling untuk operator integer-only dalam konteks desimal.
double evaluate_postfix(const std::vector<std::string>& postfix) {
    std::stack<double> stack;
    const double epsilon = 1e-9; // Toleransi untuk perbandingan floating-point

    auto is_whole_number = [&](double n) {
        return std::abs(n - std::trunc(n)) < epsilon;
    };

    for (const auto& token : postfix) {
        if (get_precedence(token) > 0) {
            // Handle unary operators
            if (token == "u+" || token == "u-" || token == "!" || token == "~") {
                 if (stack.empty()) throw std::runtime_error("Invalid expression: not enough operands for unary op");
                 double a = stack.top(); stack.pop();
                 if (token == "u+") stack.push(a);
                 else if (token == "u-") stack.push(-a);
                 else if (token == "!") stack.push(!a); // Logical NOT
                 else if (token == "~") { // Bitwise NOT
                    if (!is_whole_number(a)) throw std::runtime_error("Bitwise operator '~' requires an integer operand");
                    stack.push(~static_cast<long>(a));
                 }
                 continue;
            }

            if (stack.size() < 2) throw std::runtime_error("Invalid expression: not enough operands");
            double b = stack.top(); stack.pop();
            double a = stack.top(); stack.pop();
            
            if (token == "+") stack.push(a + b);
            else if (token == "-") stack.push(a - b);
            else if (token == "*") stack.push(a * b);
            else if (token == "/") { 
                if (std::abs(b) < epsilon) throw std::runtime_error("Division by zero"); 
                stack.push(a / b); 
            }
            else if (token == "%") { 
                if (std::abs(b) < epsilon) throw std::runtime_error("Modulo by zero");
                if (!is_whole_number(a) || !is_whole_number(b)) throw std::runtime_error("Modulo operator '%' requires integer operands");
                stack.push(static_cast<long>(a) % static_cast<long>(b)); 
            }
            else if (token == "**") {
                stack.push(std::pow(a, b));
            }
            // Bitwise operators, memerlukan integer
            else if (token == "^" || token == "&" || token == "|" || token == "<<" || token == ">>") {
                if (!is_whole_number(a) || !is_whole_number(b)) throw std::runtime_error("Bitwise operators require integer operands");
                long la = static_cast<long>(a);
                long lb = static_cast<long>(b);
                if (token == "^") stack.push(la ^ lb);
                else if (token == "&") stack.push(la & lb);
                else if (token == "|") stack.push(la | lb);
                else if (token == "<<") stack.push(la << lb);
                else if (token == ">>") stack.push(la >> lb);
            }
            // Logical/comparison operators
            else if (token == "<") stack.push(a < b);
            else if (token == "<=") stack.push(a <= b);
            else if (token == ">") stack.push(a > b);
            else if (token == ">=") stack.push(a >= b);
            else if (token == "==") stack.push(std::abs(a - b) < epsilon);
            else if (token == "!=") stack.push(std::abs(a - b) >= epsilon);
            else if (token == "&&") stack.push(a && b);
            else if (token == "||") stack.push(a || b);

        } else { // Operand
            if (token == "?") {
                // Dalam konteks evaluasi normal, '?' tidak memiliki nilai. 
                // Ini seharusnya hanya ditangani oleh solve_puzzle.
                throw std::runtime_error("Puzzle variable '?' encountered during direct evaluation");
            }
            
            try {
                stack.push(std::stod(token));
            } catch (...) {
                const char* val = get_env_var(token);
                try {
                    stack.push(val ? std::stod(val) : 0.0);
                } catch (...) {
                    stack.push(0.0); // Variabel tidak valid atau bukan angka
                }
            }
        }
    }
    
    if (stack.size() != 1) throw std::runtime_error("Invalid expression: stack should contain a single value at the end");
    return stack.top();
}


// --- GANTI FUNGSI solve_puzzle ---
// UPGRADE: Menggunakan metode aljabar linier (two-point form) untuk solusi instan.
// SUPPORT: Mendukung multiple '?' sebagai variabel yang sama.
// SUPPORT: Mendukung puzzle desimal.
// ADD: Error handling untuk kontradiksi dan identitas.
double solve_puzzle(const std::vector<std::string>& tokens) {
    size_t equal_pos = std::string::npos;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i] == "=" || tokens[i] == "==") {
            equal_pos = i;
            break;
        }
    }

    if (equal_pos == std::string::npos) {
        throw std::runtime_error("Invalid puzzle format: must contain '='");
    }

    // Fungsi helper untuk mengevaluasi ekspresi dengan mengganti '?' dengan nilai tertentu
    auto evaluate_with_value = [&](const std::vector<std::string>& expr_tokens, double value) -> double {
        std::vector<std::string> substituted_tokens;
        bool has_unsupported_op = false;
        for (const auto& token : expr_tokens) {
            if (token == "?") {
                substituted_tokens.push_back(std::to_string(value));
            } else {
                if (token == "%" || token == "^" || token == "&" || token == "|" || token == "<<" || token == ">>" || token == "~") {
                    has_unsupported_op = true;
                }
                substituted_tokens.push_back(token);
            }
        }
        if (has_unsupported_op) {
            throw std::runtime_error("Puzzles with bitwise or modulo operators are not supported");
        }
        auto postfix = infix_to_postfix(substituted_tokens);
        return evaluate_postfix(postfix);
    };

    std::vector<std::string> left_tokens(tokens.begin(), tokens.begin() + equal_pos);
    std::vector<std::string> right_tokens(tokens.begin() + equal_pos + 1, tokens.end());

    // Hitung persamaan f(x) = L(x) - R(x), di mana kita mencari x agar f(x) = 0
    // Asumsikan persamaan ini linear: f(x) = ax + b
    // Kita bisa menemukan 'a' dan 'b' dengan mengevaluasi pada dua titik, misal x=0 dan x=1.
    // f(0) = a*0 + b = b
    // f(1) = a*1 + b = a + b
    // Maka, a = f(1) - f(0) dan b = f(0)
    
    double f0, f1;
    try {
        double left0 = evaluate_with_value(left_tokens, 0.0);
        double right0 = evaluate_with_value(right_tokens, 0.0);
        f0 = left0 - right0; // Ini adalah 'b'

        double left1 = evaluate_with_value(left_tokens, 1.0);
        double right1 = evaluate_with_value(right_tokens, 1.0);
        f1 = left1 - right1; // Ini adalah 'a + b'
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to evaluate puzzle structure: ") + e.what());
    }
    
    double b = f0;
    double a = f1 - f0;
    const double epsilon = 1e-9;

    // Sekarang selesaikan ax + b = 0
    if (std::abs(a) < epsilon) { // Jika 'a' mendekati nol
        if (std::abs(b) < epsilon) { // Jika 'b' juga nol (0 = 0)
            throw std::runtime_error("Infinite solutions: equation is an identity");
        } else { // Jika 'b' bukan nol (misal: 5 = 0)
            throw std::runtime_error("No solution: equation is a contradiction");
        }
    }
    
    // Solusinya adalah x = -b / a
    return -b / a;
}


// --- GANTI FUNGSI evaluate_arithmetic ---
// FIX: Mengubah tipe hasil menjadi double dan memformat output string
std::string evaluate_arithmetic(const std::string& expr) {
    if (expr.empty()) return "0";
    try {
        auto tokens = tokenize_arithmetic(expr);
        
        bool has_question = false;
        bool has_equal = false;
        
        for (const auto& token : tokens) {
            if (token == "?") has_question = true;
            if (token == "=" || token == "==") has_equal = true;
        }
        
        double result;
        
        if (has_question && has_equal) {
            result = solve_puzzle(tokens);
        } else if (has_question) {
            throw std::runtime_error("Puzzle must contain both '?' and '='");
        } else {
            auto postfix = infix_to_postfix(tokens);
            result = evaluate_postfix(postfix);
        }
        
        // Format output: hapus .0 jika merupakan bilangan bulat
        std::ostringstream oss;
        if (std::abs(result - std::trunc(result)) < 1e-9) {
            oss << static_cast<long>(result);
        } else {
            oss << std::fixed << std::setprecision(10) << result;
            // Hapus trailing zeros
            std::string str = oss.str();
            str.erase(str.find_last_not_of('0') + 1, std::string::npos);
            if(str.back() == '.') {
                str.pop_back();
            }
            return str;
        }
        return oss.str();

    } catch (const std::exception& e) {
        std::cerr << "nsh: arithmetic error: " << expr << ": " << e.what() << std::endl;
        return "0"; // Bash returns 0 on error within $((...))
    }
}


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
                        const char *val = get_env_var(var_name);
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
                    const char *val = get_env_var(var_name);
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
