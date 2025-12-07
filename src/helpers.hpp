#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cctype>
#include <regex>
#include <map>

#include "evaluator.hpp"
#include "executionengine.hpp"
#include "variable.hpp"

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
    if (token == "true" || token == "false" || token == "var" || token == "print" || token == "println" || token == "input"
        || token == "func" || token == "return" || token == "if" || token == "else" || token == "while" || token == "import"
        || token == "END" || token == "GOTO" || token == "end" || token == "STYLE") {
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