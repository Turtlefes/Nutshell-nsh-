#include "expansion.h"
#include "execution.h"
#include "globals.h"
#include "parser.h"

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

// ... (functions cleanup_pattern, match_wildcard, expand_wildcard, expand_tilde, execute_subshell_command remain the same) ...
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

std::string execute_subshell_command(const std::string &cmd) {
    // Buat pipe untuk stdout dan pipe terpisah untuk stderr
    int stdout_pipe[2], stderr_pipe[2];
    if (pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1) {
        perror("pipe");
        return "";
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        return "";
    }
    
    if (pid == 0) { // Child process
        // Tutup read end of pipes
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        
        // Redirect stdout ke write end of stdout pipe
        dup2(stdout_pipe[1], STDOUT_FILENO);
        close(stdout_pipe[1]);
        
        // Redirect stderr ke write end of stderr pipe
        dup2(stderr_pipe[1], STDERR_FILENO);
        close(stderr_pipe[1]);
        
        // Eksekusi command menggunakan parser dan executor internal
        Parser parser;
        try {
            auto commands = parser.parse(cmd);
            if (!commands.empty()) {
                exit(execute_command_list(commands));
            }
        } catch (const std::exception &e) {
            std::cerr << "nsh: " << e.what() << std::endl;
            exit(1);
        }
        
        exit(0);
    } else { // Parent process
        // Tutup write end of pipes
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);
        
        // Baca output stdout dari child dengan buffer dinamis
        std::string result;
        size_t buffer_size = 4096;
        char* buffer = static_cast<char*>(safe_malloc(buffer_size));
        
        if (!buffer) {
            close(stdout_pipe[0]);
            close(stderr_pipe[0]);
            return "";
        }
        
        ssize_t bytes_read;
        size_t total_read = 0;
        
        while ((bytes_read = read(stdout_pipe[0], buffer + total_read, buffer_size - total_read - 1)) > 0) {
            total_read += bytes_read;
            
            if (total_read >= buffer_size - 1) {
                buffer_size *= 2;
                char* new_buffer = static_cast<char*>(safe_realloc(buffer, buffer_size));
                if (!new_buffer) {
                    free(buffer);
                    close(stdout_pipe[0]);
                    close(stderr_pipe[0]);
                    return "";
                }
                buffer = new_buffer;
            }
        }
        
        if (total_read > 0) {
            buffer[total_read] = '\0';
            result = buffer;
        }
        
        free(buffer);
        close(stdout_pipe[0]);
        
        // Baca stderr dengan cara yang sama
        buffer_size = 4096;
        buffer = static_cast<char*>(safe_malloc(buffer_size));
        if (!buffer) {
            close(stderr_pipe[0]);
            return result;
        }
        
        total_read = 0;
        std::string error_output;
        
        while ((bytes_read = read(stderr_pipe[0], buffer + total_read, buffer_size - total_read - 1)) > 0) {
            total_read += bytes_read;
            
            if (total_read >= buffer_size - 1) {
                buffer_size *= 2;
                char* new_buffer = static_cast<char*>(safe_realloc(buffer, buffer_size));
                if (!new_buffer) {
                    free(buffer);
                    close(stderr_pipe[0]);
                    return result;
                }
                buffer = new_buffer;
            }
        }
        
        if (total_read > 0) {
            buffer[total_read] = '\0';
            error_output = buffer;
        }
        
        free(buffer);
        close(stderr_pipe[0]);
        
        // Tampilkan error output langsung ke terminal jika ada
        if (!error_output.empty()) {
            std::cerr << error_output;
            std::cerr.flush();
        }
        
        // Tunggu child process selesai
        int status;
        waitpid(pid, &status, 0);
        
        // Hapus newline terakhir jika ada
        if (!result.empty() && result.back() == '\n') {
            result.pop_back();
        }
        
        return result;
    }
}


// ... (arithmetic evaluation helper functions: is_operator, get_precedence, etc. remain the same) ...
bool is_operator(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || 
           c == '^' || c == '&' || c == '|' || c == '<' || c == '>' || c == '=';
}

int get_precedence(const std::string& op) {
    static const std::map<std::string, int> precedence = {
        {"||", 1}, {"&&", 2},
        {"|", 3}, {"^", 4}, {"&", 5},
        {"==", 6}, {"!=", 6},
        {"<", 7}, {"<=", 7}, {">", 7}, {">=", 7},
        {"<<", 8}, {">>", 8},
        {"+", 9}, {"-", 9},
        {"*", 10}, {"/", 10}, {"%", 10}
    };
    
    auto it = precedence.find(op);
    return it != precedence.end() ? it->second : 0;
}

bool is_right_associative(const std::string& op) {
    return op == "^" || op == "=";
}

std::vector<std::string> tokenize_arithmetic(const std::string& expr) {
    std::vector<std::string> tokens;
    size_t i = 0;
    size_t n = expr.length();
    
    while (i < n) {
        if (isspace(expr[i])) {
            i++;
            continue;
        }
        
        if (isdigit(expr[i])) {
            std::string num;
            while (i < n && (isdigit(expr[i]) || expr[i] == '.')) {
                num += expr[i++];
            }
            tokens.push_back(num);
            continue;
        }
        
        if (isalpha(expr[i]) || expr[i] == '_') {
            std::string var;
            while (i < n && (isalnum(expr[i]) || expr[i] == '_')) {
                var += expr[i++];
            }
            tokens.push_back(var);
            continue;
        }
        
        if (is_operator(expr[i])) {
            std::string op;
            op += expr[i++];
            
            if (i < n && is_operator(expr[i])) {
                std::string potential_op = op + expr[i];
                if (potential_op == "<<" || potential_op == ">>" || 
                    potential_op == "<=" || potential_op == ">=" ||
                    potential_op == "==" || potential_op == "!=" ||
                    potential_op == "&&" || potential_op == "||") {
                    op = potential_op;
                    i++;
                }
            }
            tokens.push_back(op);
            continue;
        }
        
        if (expr[i] == '(' || expr[i] == ')') {
            tokens.push_back(std::string(1, expr[i++]));
            continue;
        }
        
        if (expr[i] == '!' || expr[i] == '~') {
            tokens.push_back(std::string(1, expr[i++]));
            continue;
        }
        
        i++;
    }
    
    return tokens;
}

std::vector<std::string> infix_to_postfix(const std::vector<std::string>& tokens) {
    std::vector<std::string> output;
    std::stack<std::string> op_stack;
    
    for (const auto& token : tokens) {
        if (token == "(") {
            op_stack.push(token);
        } else if (token == ")") {
            while (!op_stack.empty() && op_stack.top() != "(") {
                output.push_back(op_stack.top());
                op_stack.pop();
            }
            if (!op_stack.empty() && op_stack.top() == "(") {
                op_stack.pop();
            }
        } else if (get_precedence(token) > 0) {
            while (!op_stack.empty() && op_stack.top() != "(" &&
                   (get_precedence(op_stack.top()) > get_precedence(token) ||
                   (get_precedence(op_stack.top()) == get_precedence(token) && 
                    !is_right_associative(token)))) {
                output.push_back(op_stack.top());
                op_stack.pop();
            }
            op_stack.push(token);
        } else {
            output.push_back(token);
        }
    }
    
    while (!op_stack.empty()) {
        output.push_back(op_stack.top());
        op_stack.pop();
    }
    
    return output;
}

long evaluate_postfix(const std::vector<std::string>& postfix) {
    std::stack<long> stack;
    
    for (const auto& token : postfix) {
        if (get_precedence(token) > 0) {
            if (stack.size() < 2) {
                throw std::runtime_error("Invalid expression: not enough operands");
            }
            
            long b = stack.top(); stack.pop();
            long a = stack.top(); stack.pop();
            
            if (token == "+") stack.push(a + b);
            else if (token == "-") stack.push(a - b);
            else if (token == "*") stack.push(a * b);
            else if (token == "/") {
                if (b == 0) throw std::runtime_error("Division by zero");
                stack.push(a / b);
            }
            else if (token == "%") stack.push(a % b);
            else if (token == "^") stack.push(static_cast<long>(pow(a, b)));
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
        } else if (token == "!") {
            if (stack.empty()) throw std::runtime_error("Invalid expression: not enough operands for !");
            long a = stack.top(); stack.pop();
            stack.push(!a);
        } else if (token == "~") {
            if (stack.empty()) throw std::runtime_error("Invalid expression: not enough operands for ~");
            long a = stack.top(); stack.pop();
            stack.push(~a);
        } else {
            if (isdigit(token[0]) || (token.length() > 1 && token[0] == '-')) {
                try {
                    stack.push(std::stol(token));
                } catch (...) {
                    throw std::runtime_error("Invalid number: " + token);
                }
            } else {
                const char* val = getenv(token.c_str());
                if (val) {
                    try {
                        stack.push(std::stol(val));
                    } catch (...) {
                        stack.push(0); 
                    }
                } else {
                    stack.push(0);
                }
            }
        }
    }
    
    if (stack.size() != 1) {
        throw std::runtime_error("Invalid expression");
    }
    
    return stack.top();
}

std::string evaluate_arithmetic(const std::string& expr) {
    try {
        auto tokens = tokenize_arithmetic(expr);
        auto postfix = infix_to_postfix(tokens);
        long result = evaluate_postfix(postfix);
        return std::to_string(result);
    } catch (const std::exception& e) {
        std::cerr << "nsh: Arithmetic expansion error: " << e.what() << std::endl;
        return "0";
    }
}

std::string expand_argument(const std::string &token)
{
    std::string result;
    result.reserve(token.length());
    bool in_single_quote = false;
    bool in_double_quote = false;

    for (size_t i = 0; i < token.length(); ++i)
    {
        char c = token[i];

        if (c == '\\' && !in_single_quote)
        {
            if (i + 1 < token.length())
            {
                if (in_double_quote)
                {
                    if (strchr("$`\"\\", token[i + 1]))
                    {
                        result += token[++i];
                    }
                    else
                    {
                        result += c;
                    }
                }
                else
                {
                    result += token[++i];
                }
            }
            else
            {
                result += c;
            }
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

        if (c == '$')
        {
            size_t start = i + 1;
            if (start >= token.length())
            {
                result += '$';
                continue;
            }

            if (token[start] == '{')
            {
                size_t end = token.find('}', start);
                if (end != std::string::npos)
                {
                    std::string full_spec = token.substr(start + 1, end - start - 1);
                    std::string var_name = full_spec;
                    
                    size_t slash_pos = full_spec.find('/');
                    if (slash_pos != std::string::npos) {
                        var_name = full_spec.substr(0, slash_pos);
                        std::string rest = full_spec.substr(slash_pos + 1);
                        size_t second_slash_pos = rest.find('/');
                        if (second_slash_pos != std::string::npos) {
                            std::string pattern = rest.substr(0, second_slash_pos);
                            std::string replacement = rest.substr(second_slash_pos + 1);
                            const char* val = getenv(var_name.c_str());
                            if (val) {
                                std::string s_val(val);
                                size_t pos = s_val.find(pattern);
                                if (pos != std::string::npos) {
                                    s_val.replace(pos, pattern.length(), replacement);
                                }
                                result += s_val;
                            }
                        } else {
                             const char *val = getenv(full_spec.c_str());
                             if (val) result += val;
                        }
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
                const char *val = getenv(var_name.c_str());
                if (val)
                    result += val;
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
        else if (c == '`')
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

// ... (remaining functions apply_expansions_and_wildcards, is_env_assignment, parse_env_assignment remain the same) ...
void apply_expansions_and_wildcards(std::vector<std::string> &tokens)
{
    if (tokens.empty())
        return;
    std::vector<std::string> new_tokens;
    for (size_t i = 0; i < tokens.size(); ++i)
    {
        const auto &token = tokens[i];
        if (is_env_assignment(token) && i == 0)
        {
            // For environment assignments, we need to split on '=' and only expand the value
            size_t eq_pos = token.find('=');
            std::string var_name = token.substr(0, eq_pos);
            std::string value = token.substr(eq_pos + 1);
            
            // Expand only the value part
            std::string expanded_value = expand_argument(expand_tilde(value));
            
            // Reconstruct the assignment
            new_tokens.push_back(var_name + "=" + expanded_value);
            continue;
        }

        std::string expanded_arg = expand_argument(expand_tilde(token));

        if (expanded_arg.find_first_of("*?") != std::string::npos)
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
    std::string var_name = token.substr(0, eq_pos);
    std::string value = token.substr(eq_pos + 1);

    // For assignment values, we need to preserve backslashes literally
    // but still handle quotes and other special characters appropriately
    std::string temp_value = value;
    
    // Remove surrounding quotes but preserve backslashes within
    if (temp_value.length() >= 2 && 
        ((temp_value.front() == '\'' && temp_value.back() == '\'') ||
         (temp_value.front() == '"' && temp_value.back() == '"')))
    {
        temp_value = temp_value.substr(1, temp_value.length() - 2);
        
        // For quoted strings, we need to handle escaped characters properly
        std::string processed_value;
        bool escaped = false;
        
        for (char c : temp_value) {
            if (escaped) {
                processed_value += c;
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
                processed_value += c; // Keep the backslash
            } else {
                processed_value += c;
            }
        }
        
        value = processed_value;
    }
    else
    {
        // For unquoted values, we still need to preserve backslashes
        // but perform basic expansion (like tilde expansion)
        value = expand_tilde(temp_value);
        
        // But we need to prevent backslash removal in the expansion process
        // So we'll handle this specially
        std::string preserved_value;
        bool escaped = false;
        
        for (char c : value) {
            if (escaped) {
                preserved_value += c;
                escaped = false;
            } else if (c == '\\') {
                escaped = true;
                preserved_value += c; // Keep the backslash
            } else {
                preserved_value += c;
            }
        }
        
        value = preserved_value;
    }
    
    return {var_name, value};
}
