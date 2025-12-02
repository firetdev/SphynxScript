#pragma once

#include <stack>
#include <map>
#include <sstream>
#include <stdexcept>
#include <cmath> // For std::fmod and std::floor

/**
 * @brief Holds the result of an evaluation.
 * The 'type' string can be "int", "float", "bool", "string", or "error".
 */
struct EvalResult {
    std::string value;
    std::string type;

    EvalResult(std::string v = "", std::string t = "empty") : value(std::move(v)), type(std::move(t)) {}

    // Helper conversion functions
    float asFloat() const { return std::stof(value); }
    long long asInt() const { return std::stoll(value); }
    bool asBool() const { return value == "true"; }
    std::string asString() const {
        // Return contents *without* the quotes
        if (type == "string" && value.length() >= 2) {
            return value.substr(1, value.length() - 2);
        }
        return value;
    }
};

class Evaluator {
public:
    /**
     * @brief Main function to evaluate a full expression string.
     * @param expression The infix expression (e.g., "5 * (3 + 2)").
     * @return An EvalResult containing the final value or an error.
     */
    EvalResult evaluate(const std::string& expression) {
        try {
            // 1. Tokenize the input string
            std::vector<std::string> tokens = tokenize(expression);
            
            // 2. Convert from infix to postfix (Shunting-Yard)
            std::vector<std::string> postfix = shuntingYard(tokens);
            
            // 3. Evaluate the postfix expression
            return evaluatePostfix(postfix);
        } catch (const std::exception& e) {
            // Catch any syntax or runtime errors
            return EvalResult(e.what(), "error");
        }
    }

private:
    // Helper function for implicit type conversion
    EvalResult coerceToNumber(const EvalResult& result) {
        if (result.type == "string") {
            try {
                // Remove surrounding quotes for conversion
                std::string raw_value = result.asString();
                
                // Check if the string *only* contains a number
                if (isNumber(raw_value)) {
                    // Use the existing number type detection
                    std::string new_type = (raw_value.find('.') == std::string::npos) ? "int" : "float";
                    return EvalResult(raw_value, new_type);
                }
            } catch (const std::exception& e) {
                // Failed to convert, just return the original string result
            }
        }
        return result; // Return the original result if not a string or conversion fails
    }

    // --- Type Detection Helpers ---
    static bool isBool(const std::string& s) {
        return s == "true" || s == "false";
    }

    static bool isString(const std::string& s) {
        return s.length() >= 2 && s.front() == '"' && s.back() == '"';
    }

    // A more robust number checker
    static bool isNumber(const std::string& s) {
        try {
            size_t pos;
            std::stof(s, &pos);
            // Ensure the *entire* string was parsed as a number
            return pos == s.length();
        } catch (...) {
            return false;
        }
    }

    static std::string getTokenType(const std::string& token) {
        if (isBool(token)) return "bool";
        if (isString(token)) return "string";
        if (isNumber(token)) {
            // It's an int if it's a number and has no decimal point
            if (token.find('.') == std::string::npos) return "int";
            return "float";
        }
        return "operator";
    }

    // --- 1. Tokenizer ---
    std::vector<std::string> tokenize(const std::string& expression) {
        std::vector<std::string> tokens;
        std::string current_token;

        for (size_t i = 0; i < expression.length(); ++i) {
            char c = expression[i];

            if (isspace(c)) {
                continue;
            } 
            // Single-char operators
            else if (std::string("()*/%").find(c) != std::string::npos) {
                tokens.push_back(std::string(1, c));
            }
            // Handle + and - (special for unary)
            else if (c == '+' || c == '-') {
                // Check if it's unary:
                // 1. It's the first token.
                // 2. It follows an operator or an open parenthesis.
                bool isUnary = (tokens.empty() || 
                               std::string("(=!<>|&+-*/%").find(tokens.back().back()) != std::string::npos ||
                               tokens.back() == "(");

                if (isUnary && i + 1 < expression.length() && (isdigit(expression[i+1]) || expression[i+1] == '.')) {
                    // It's a unary sign attached to a number
                    current_token += c;
                    i++;
                    while (i < expression.length() && (isdigit(expression[i]) || expression[i] == '.')) {
                        current_token += expression[i];
                        i++;
                    }
                    i--; // Rewind one char
                    tokens.push_back(current_token);
                    current_token = "";
                } else {
                    // It's a binary operator
                    tokens.push_back(std::string(1, c));
                }
            }
            // Multi-char operators: ==, !=, <=, >=, &&, ||
            else if (std::string("=!<>|&").find(c) != std::string::npos) {
                std::string op(1, c);
                if (i + 1 < expression.length()) {
                    char next_c = expression[i+1];
                    if (c == '=' && next_c == '=') { op = "=="; i++; }
                    else if (c == '!' && next_c == '=') { op = "!="; i++; }
                    else if (c == '<' && next_c == '=') { op = "<="; i++; }
                    else if (c == '>' && next_c == '=') { op = ">="; i++; }
                    else if (c == '&' && next_c == '&') { op = "&&"; i++; }
                    else if (c == '|' && next_c == '|') { op = "||"; i++; }
                }
                tokens.push_back(op);
            }
            // Numbers (that didn't start with +/-)
            else if (isdigit(c) || c == '.') {
                current_token += c;
                while (i + 1 < expression.length() && (isdigit(expression[i+1]) || expression[i+1] == '.')) {
                    current_token += expression[i+1];
                    i++;
                }
                tokens.push_back(current_token);
                current_token = "";
            }
            // String literals
            else if (c == '"') {
                current_token += c; // Add opening quote
                i++;
                while (i < expression.length() && expression[i] != '"') {
                    if (expression[i] == '\\' && i + 1 < expression.length()) {
                        // Handle escaped characters (e.g., \")
                        current_token += expression[i+1];
                        i++;
                    } else {
                        current_token += expression[i];
                    }
                    i++;
                }
                if (i == expression.length()) {
                    throw std::runtime_error("Syntax Error: Unterminated string");
                }
                current_token += '"'; // Add closing quote
                tokens.push_back(current_token);
                current_token = "";
            }
            // Booleans
            else if (isalpha(c)) {
                current_token += c;
                while (i + 1 < expression.length() && isalpha(expression[i+1])) {
                    current_token += expression[i+1];
                    i++;
                }
                if (current_token == "true" || current_token == "false") {
                    tokens.push_back(current_token);
                } else {
                    throw std::runtime_error("Syntax Error: Unknown identifier '" + current_token + "'");
                }
                current_token = "";
            }
            else {
                throw std::runtime_error("Syntax Error: Invalid character '" + std::string(1, c) + "'");
            }
        }
        return tokens;
    }


    // --- 2. Shunting-Yard Algorithm ---
    // 
    std::map<std::string, int> precedence = {
        {"||", 1},
        {"&&", 2},
        {"==", 3}, {"!=", 3},
        {"<", 4}, {">", 4}, {"<=", 4}, {">=", 4},
        {"+", 5}, {"-", 5},
        {"*", 6}, {"/", 6}, {"%", 6},
        {"!", 7} // Unary 'not'
    };

    std::vector<std::string> shuntingYard(const std::vector<std::string>& tokens) {
        std::vector<std::string> output_queue;
        std::stack<std::string> operator_stack;

        for (const std::string& token : tokens) {
            std::string type = getTokenType(token);

            if (type == "int" || type == "float" || type == "string" || type == "bool") {
                output_queue.push_back(token);
            } 
            else if (token == "(") {
                operator_stack.push(token);
            }
            else if (token == ")") {
                while (!operator_stack.empty() && operator_stack.top() != "(") {
                    output_queue.push_back(operator_stack.top());
                    operator_stack.pop();
                }
                if (operator_stack.empty()) {
                    throw std::runtime_error("Syntax Error: Mismatched parentheses");
                }
                operator_stack.pop(); // Pop the "("
            }
            else { // It's an operator
                while (!operator_stack.empty() && operator_stack.top() != "(" &&
                       precedence[operator_stack.top()] >= precedence[token]) {
                    output_queue.push_back(operator_stack.top());
                    operator_stack.pop();
                }
                operator_stack.push(token);
            }
        }

        // Pop remaining operators
        while (!operator_stack.empty()) {
            if (operator_stack.top() == "(") {
                throw std::runtime_error("Syntax Error: Mismatched parentheses");
            }
            output_queue.push_back(operator_stack.top());
            operator_stack.pop();
        }

        return output_queue;
    }

    // --- 3. Postfix (RPN) Evaluator ---
    // 
    EvalResult evaluatePostfix(const std::vector<std::string>& postfix) {
        std::stack<EvalResult> stack;

        for (const std::string& token : postfix) {
            std::string type = getTokenType(token);

            if (type == "int" || type == "float" || type == "bool" || type == "string") {
                stack.push(EvalResult(token, type));
            } 
            else { // It's an operator
                // Handle unary '!'
                if (token == "!") {
                    if (stack.empty()) throw std::runtime_error("Syntax Error: Insufficient operands for '!'");
                    EvalResult operand = stack.top(); stack.pop();
                    stack.push(applyUnaryOp(operand, token));
                }
                // Handle all binary operators
                else {
                    if (stack.size() < 2) throw std::runtime_error("Syntax Error: Insufficient operands for '" + token + "'");
                    EvalResult rhs = stack.top(); stack.pop();
                    EvalResult lhs = stack.top(); stack.pop();
                    stack.push(applyBinaryOp(lhs, rhs, token));
                }
            }
        }

        if (stack.size() != 1) {
            throw std::runtime_error("Syntax Error: Invalid expression");
        }

        return stack.top();
    }

    // --- 4. Operation Helpers ---

    EvalResult applyUnaryOp(const EvalResult& operand, const std::string& op) {
        if (op == "!") {
            if (operand.type != "bool") {
                throw std::runtime_error("Type Error: Operator '!' requires a boolean operand");
            }
            return EvalResult(operand.asBool() ? "false" : "true", "bool");
        }
        throw std::runtime_error("Internal Error: Unknown unary operator '" + op + "'");
    }

    EvalResult applyBinaryOp(const EvalResult& lhs, const EvalResult& rhs, const std::string& op) {
        // --- Logical Operations ---
        if (op == "&&" || op == "||") {
            if (lhs.type != "bool" || rhs.type != "bool") {
                throw std::runtime_error("Type Error: Operator '" + op + "' requires boolean operands");
            }
            bool result = (op == "&&") ? (lhs.asBool() && rhs.asBool()) : (lhs.asBool() || rhs.asBool());
            return EvalResult(result ? "true" : "false", "bool");
        }

        // --- Comparison Operations ---
        if (op == "==" || op == "!=") {
            bool result;
            if (lhs.type == "string" && rhs.type == "string") result = (lhs.asString() == rhs.asString());
            else if (lhs.type == "bool" && rhs.type == "bool") result = (lhs.asBool() == rhs.asBool());
            else if ((lhs.type == "int" || lhs.type == "float") && (rhs.type == "int" || rhs.type == "float")) {
                result = (lhs.asFloat() == rhs.asFloat());
            } else {
                 throw std::runtime_error("Type Error: Cannot compare " + lhs.type + " with " + rhs.type);
            }
            if (op == "!=") result = !result;
            return EvalResult(result ? "true" : "false", "bool");
        }

        if (op == "<" || op == ">" || op == "<=" || op == ">=") {
            if (!((lhs.type == "int" || lhs.type == "float") && (rhs.type == "int" || rhs.type == "float"))) {
                throw std::runtime_error("Type Error: Operator '" + op + "' requires numerical operands");
            }
            float l = lhs.asFloat();
            float r = rhs.asFloat();
            bool result;
            if (op == "<") result = l < r;
            else if (op == ">") result = l > r;
            else if (op == "<=") result = l <= r;
            else result = l >= r; // ">="
            return EvalResult(result ? "true" : "false", "bool");
        }

        // --- Arithmetic Operations ---
        if (op == "+") {
            // String concatenation (requires both to be strings)
            if (lhs.type == "string" && rhs.type == "string") {
                return EvalResult('"' + lhs.asString() + rhs.asString() + '"', "string");
            }

            // Coerce operands for numeric addition
            EvalResult L = coerceToNumber(lhs);
            EvalResult R = coerceToNumber(rhs);
            
            // Check if L and R are now numerical types
            if ((L.type == "int" || L.type == "float") && (R.type == "int" || R.type == "float")) {
                float result = L.asFloat() + R.asFloat();
                // ... (rest of numeric addition logic remains unchanged) ...
            
            // If one is still a string and the other is numeric, perform string concatenation
            } else if (L.type == "string" || R.type == "string") {
                // This is flexible: "5" + 10 -> "510" (Coerce numeric to string)
                std::string s_l = (L.type == "string") ? L.asString() : L.value;
                std::string s_r = (R.type == "string") ? R.asString() : R.value;
                return EvalResult('"' + s_l + s_r + '"', "string");
            }
            
            // Fallback error
            throw std::runtime_error("Type Error: Operator '+' not supported for " + lhs.type + " and " + rhs.type);
        }

        // All remaining ops require numbers (-, *, /, %)

        // Coerce operands for non-addition numeric ops
        EvalResult L = coerceToNumber(lhs);
        EvalResult R = coerceToNumber(rhs);

        if (!((L.type == "int" || L.type == "float") && (R.type == "int" || R.type == "float"))) {
            // If coercion failed, it means the string wasn't a number (e.g., "hello")
            throw std::runtime_error("Type Error: Operator '" + op + "' requires numerical operands, found " + L.type + " and " + R.type);
        }

        float l = L.asFloat();
        float r = R.asFloat();
        float result;
        std::string result_type = (lhs.type == "float" || rhs.type == "float") ? "float" : "int";

        if (op == "-") result = l - r;
        else if (op == "*") result = l * r;
        else if (op == "/") {
            if (r == 0) throw std::runtime_error("Runtime Error: Division by zero");
            result = l / r;
            // Division *always* produces a float, unless it's a clean int division
            if (result_type == "int" && std::fmod(l, r) != 0) {
                result_type = "float";
            }
        }
        else if (op == "%") {
            if (lhs.type != "int" || rhs.type != "int") {
                throw std::runtime_error("Type Error: Operator '%' requires integer operands");
            }
            if (rhs.asInt() == 0) throw std::runtime_error("Runtime Error: Modulo by zero");
            result = lhs.asInt() % rhs.asInt();
            result_type = "int"; // Modulo always results in int
        }
        else {
            throw std::runtime_error("Internal Error: Unknown operator '" + op + "'");
        }
        
        // Final result formatting
        if (result_type == "float" || std::floor(result) != result) {
             return EvalResult(std::to_string(result), "float");
        }
        return EvalResult(std::to_string((long long)result), "int");
    }
};