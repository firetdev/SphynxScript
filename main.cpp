#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cctype>
#include <regex>
#include <map>

#include "evaluator.hpp"

class Variable {
public:
    std::string name;
    std::string value;
    std::string type;

    Variable(const std::string n)
        : name(n), value(""), type("undefined") {}
    
    void setValue(const EvalResult& result) {
        value = result.value;
        type = result.type;
    }

    EvalResult getAsResult() const {
        return EvalResult(value, type);
    }

    // Getters
    long long asInt() const {
        return std::stoll(value);
    }

    float asFloat() const {
        return std::stof(value);
    }

    bool asBool() const {
        return value == "true";
    }

    std::string asString() const {
        if (type == "string" && value.length() >= 2 && value.front() == '"') {
            return value.substr(1, value.length() - 2);
        }
        // Return raw value if it's not a standard string literal
        return value;
    }
};

// Forward declarations
bool isVariableName(const std::string& token);
std::string findAndReplaceVariables(const std::string& line, const std::map<std::string, Variable>& vars);
std::string handleInputCall(const std::string& line, const std::map<std::string, Variable>& vars);

int main() {
    std::map<std::string, Variable> variables;
    Evaluator eval;

    std::ifstream file("script.snx");

    if (!file.is_open()) {
        std::cerr << "Failed to open file\n";
        return 1;
    }

    // Regular expressions for parsing
    std::regex declarationRegex(R"(^\s*var\s+([a-zA-Z_]\w*)\s*=\s*(.*))"); // Matches: var x = ...
    std::regex assignmentRegex(R"(^\s*([a-zA-Z_]\w*)\s*=\s*(.*))");      // Matches: x = ... (must not start with 'var')
    std::string line;

    // Regex for print
    std::regex printRegex(R"(^\s*print\s+(.*))");    // Matches: print [expression]
    std::regex printlnRegex(R"(^\s*println\s+(.*))"); // Matches: println [expression]

    while (std::getline(file, line)) {
        std::smatch match;

        // Skip empty lines first
        if (line.empty() || std::regex_match(line, std::regex(R"(^\s*$)"))) {
            continue;
        }
        
        // --- STEP 1: Handle I/O Operations (Input Function) ---
        // Pre-process the line to execute and replace all 'input' calls.
        line = handleInputCall(line, variables);
        
        // --- STEP 2: Handle Declarations, Assignments, and Prints ---
        if (std::regex_match(line, match, declarationRegex)) {
            std::string varName = match[1].str();
            std::string expression = match[2].str();
            
            // Check if variable already exists (for a quick-and-dirty implementation)
            if (variables.find(varName) == variables.end()) {
                variables.emplace(varName, Variable(varName));
            }
            
            // --- Substitute variables in the expression ---
            std::string substitutedExpr = findAndReplaceVariables(expression, variables);
            
            // --- Evaluate the expression ---
            EvalResult result = eval.evaluate(substitutedExpr);
            
            // --- Store the result in the Variable object ---
            if (result.type != "error") {
                variables.at(varName).setValue(result);
            } else {
                std::cerr << "Runtime Error on line: '" << line << "'. " << result.value << std::endl;
            }

        } else if (std::regex_match(line, match, assignmentRegex)) {
            // Found a line like: x = 5 + 3
            std::string varName = match[1].str();
            std::string expression = match[2].str();

            // Only proceed if the variable was previously declared
            if (variables.find(varName) == variables.end()) {
                std::cerr << "Name Error: Variable '" << varName << "' used before declaration." << std::endl;
                continue; 
            }
            
            // --- Substitute variables in the expression ---
            std::string substitutedExpr = findAndReplaceVariables(expression, variables);
            
            // --- Evaluate the expression and store ---
            EvalResult result = eval.evaluate(substitutedExpr);
            if (result.type != "error") {
                variables.at(varName).setValue(result);
            } else {
                std::cerr << "Runtime Error on line: '" << line << "'. " << result.value << std::endl;
            }
            
        } 
        
        // PRINTING
        else if (std::regex_match(line, match, printRegex) || std::regex_match(line, match, printlnRegex)) {
            // The expression to print is always the second match group [1]
            std::string expression = match[1].str();
            bool isPrintln = (line.find("println") != std::string::npos);

            // Substitute variables and evaluate
            std::string substitutedExpr = findAndReplaceVariables(expression, variables);
            EvalResult result = eval.evaluate(substitutedExpr);

            // Handle output
            if (result.type != "error") {
                // The asString() helper will automatically unquote string literals
                std::cout << result.asString();
                
                if (isPrintln) {
                    std::cout << "\n";
                }
            } else {
                std::cerr << "Runtime Error in print statement: " << result.value << std::endl;
            }
        }
    }

    file.close();

    return 0;
}

// --- Variable Substitution Logic ---

/**
 * @brief Checks if a token is a valid variable name (alphanumeric, starts with letter/underscore).
 * Note: This must be synchronized with the name regexes used in main.
 */
bool isVariableName(const std::string& token) {
    if (token.empty() || !std::isalpha(token[0]) && token[0] != '_') {
        return false;
    }
    // Check if the name is a reserved keyword (like true, false, var)
    if (token == "true" || token == "false" || token == "var") {
        return false;
    }
    for (char c : token) {
        if (!std::isalnum(c) && c != '_') {
            return false;
        }
    }
    return true;
}

/**
 * @brief Scans a line, finds variables, and replaces them with their stored values.
 * MODIFIED to handle ${} string interpolation inside string literals.
 */
std::string findAndReplaceVariables(const std::string& line, const std::map<std::string, Variable>& vars) {
    std::string substitutedLine;
    std::string currentToken;
    bool inStringLiteral = false;
    
    // We will build the new string character by character
    for (size_t i = 0; i < line.length(); ++i) {
        char c = line[i];
        
        // --- 1. STRING INTERPOLATION CHECK ---
        // Check for the start of interpolation: "${"
        if (inStringLiteral && c == '$' && i + 1 < line.length() && line[i+1] == '{') {
            i += 2; // Skip '$' and '{'
            std::string varName;
            
            // Collect the variable name until '}' is found
            while (i < line.length() && line[i] != '}') {
                varName += line[i];
                i++;
            }
            
            // Check for the closing brace '}'
            if (i == line.length()) {
                std::cerr << "Syntax Error: Unterminated string interpolation sequence starting at " << line.substr(i-2) << std::endl;
                // Append the raw failed sequence for the evaluator to deal with
                substitutedLine += "${" + varName;
                break; 
            }
            
            // Substitution for interpolated variable
            if (vars.count(varName)) {
                // Append the value, but wrap it in quotes if it's not a string (since we are *inside* a string literal)
                // This is a crucial step to correctly escape numbers/bools back into the literal.
                if (vars.at(varName).type == "string") {
                    // If it's already a string literal (e.g., "\"hello\""), use the unquoted asString()
                    // The overall Evaluator tokenizer will handle the final quotes of the containing string.
                    substitutedLine += vars.at(varName).asString(); 
                } else {
                    // For numbers/bools, just use the raw value.
                    substitutedLine += vars.at(varName).value;
                }
            } else {
                std::cerr << "Substitution Error: Undefined variable '" << varName << "' used in interpolation." << std::endl;
                substitutedLine += "0"; // Use default value
            }
            
            // Continue to the next character after '}'
            continue; 
        }

        // --- 2. String Literal Boundary Check ---
        if (c == '"') {
            inStringLiteral = !inStringLiteral;
        }

        // --- 3. Normal Variable/Token Processing ---
        // If we are INSIDE a string literal and NOT processing interpolation, just append the character.
        if (inStringLiteral) {
            substitutedLine += c;
            continue;
        }
        
        // If we are OUTSIDE a string literal, process variable names as before.
        if (std::isalnum(c) || c == '_') {
            currentToken += c;
        } else {
            // End of a token or variable name (OUTSIDE of quotes)
            if (!currentToken.empty()) {
                if (isVariableName(currentToken)) {
                    if (vars.count(currentToken)) {
                        substitutedLine += vars.at(currentToken).value;
                    } else {
                        std::cerr << "Substitution Error: Undefined variable '" << currentToken << "'" << std::endl;
                        substitutedLine += "0"; 
                    }
                } else {
                    substitutedLine += currentToken;
                }
                currentToken.clear();
            }
            substitutedLine += c;
        }
    }
    
    // Handle the last token if the line ended with a variable name (OUTSIDE of quotes)
    if (!currentToken.empty()) {
        if (isVariableName(currentToken)) {
            if (vars.count(currentToken)) {
                substitutedLine += vars.at(currentToken).value;
            } else {
                std::cerr << "Substitution Error: Undefined variable '" << currentToken << "'" << std::endl;
                substitutedLine += "0";
            }
        } else {
            substitutedLine += currentToken;
        }
    }
    
    return substitutedLine;
}

// --- New Helper Function Definition ---
std::string handleInputCall(const std::string& line, const std::map<std::string, Variable>& vars) {
    std::string processedLine = line;
    std::string::size_type pos = 0;
    const std::string INPUT_KEYWORD = "input";
    
    // We iterate through the line, searching for the keyword
    while ((pos = processedLine.find(INPUT_KEYWORD, pos)) != std::string::npos) {
        bool insideQuotes = false;
        
        // 1. Check if 'input' is inside quotes (essential for safety)
        for (std::string::size_type i = 0; i < pos; ++i) {
            if (processedLine[i] == '"') {
                insideQuotes = !insideQuotes;
            }
        }
        
        // Also check if 'input' is part of a larger word (e.g., 'userinput')
        bool isFullWord = (pos == 0 || !std::isalnum(processedLine[pos - 1]) && processedLine[pos - 1] != '_') &&
                          (pos + INPUT_KEYWORD.length() == processedLine.length() || !std::isalnum(processedLine[pos + INPUT_KEYWORD.length()]) && processedLine[pos + INPUT_KEYWORD.length()] != '_');

        if (insideQuotes || !isFullWord) {
            pos += INPUT_KEYWORD.length(); // Skip 'input' and continue searching
            continue;
        }
        
        // --- Found valid bare 'input' keyword ---
        
        // 2. Get User Input (No prompt evaluation needed)
        std::string user_input;
        std::getline(std::cin, user_input);
        
        // 3. Replace the 'input' keyword with the result (quoted string literal)
        // This makes the result a valid string token for the Evaluator.
        std::string replacement = '"' + user_input + '"';
        
        // Replace the substring: "input"
        processedLine.replace(pos, INPUT_KEYWORD.length(), replacement);
        
        // Reset search position to continue scanning the rest of the line
        pos += replacement.length();
    }
    
    return processedLine;
}