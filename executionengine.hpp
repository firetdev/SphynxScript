#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cctype>
#include <regex>
#include <map>

#include "evaluator.hpp"
#include "variable.hpp"
#include "helpers.hpp"

using LineMap = std::map<int, std::streampos>;

LineMap buildLineMap(const std::string& filename) {
    LineMap lineMap;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open " << filename << " to build LineMap.\n";
        return lineMap;
    }

    int lineNumber = 1;
    std::string line;

    while (std::getline(file, line)) {
        // Record the position of the current line before reading the next one
        lineMap[lineNumber] = (long long)file.tellg() - (long long)line.length() - 1;

        // Special case for line 1: file.tellg() returns the position *after* the first line
        // You need to ensure lineMap[1] is 0 (the start of the file).
        if (lineNumber == 1) {
            lineMap[1] = 0;
        }

        lineNumber++;
    }

    // Close the file stream used for mapping
    file.close();
    return lineMap;
}

class ExecutionEngine {
private:
    std::map<std::string, Variable> variables;
    Evaluator eval;
    std::ifstream file;

    int programCounter = 1; // Tracks the current line number for context
    const std::map<int, std::streampos>& fileLineMap; // Reference to the line map
    
    // Regular expressions for parsing
    const std::regex declarationRegex = std::regex(R"(^\s*var\s+([a-zA-Z_]\w*)\s*=\s*(.*))");
    const std::regex assignmentRegex = std::regex(R"(^\s*([a-zA-Z_]\w*)\s*=\s*(.*))");
    const std::regex printRegex = std::regex(R"(^\s*print\s+(.*))");
    const std::regex printlnRegex = std::regex(R"(^\s*println\s+(.*))");

    const std::regex ifRegex = std::regex(R"(^\s*if\s+(.*)\s*\{\s*$)");

    const std::regex endRegex = std::regex(R"(^\s*END\s*$)"); // Matches: END

    // Helpers
    void jumpToLine(int targetLine);
    int findBlockEnd(int startLine);
    
    // Handlers
    void handleIfStatement(const std::string& line);

public:
    ExecutionEngine(const std::string& filename, const std::map<int, std::streampos>& lineMap) : fileLineMap(lineMap) {
        file.open(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open script file: " + filename);
        }
    }

    ~ExecutionEngine() {
        if (file.is_open()) {
            file.close();
        }
    }

    void run() {
        std::string line;
        std::smatch match;

        while (std::getline(file, line)) {
            // Skip comments
            if (line[0] == '#') {
                programCounter++;
                continue;
            }

            // End program
            if (std::regex_match(line, match, endRegex)) {
                std::cout << "\nProgram execution terminated by END command.\n";
                return;
            } 

            // Skip empty lines
            if (line.empty() || std::regex_match(line, std::regex(R"(^\s*$)"))) {
                programCounter++;
                continue;
            }
            
            // STEP 1: Handle I/O Operations (Input Function)
            line = handleInputCall(line, variables);

            // STEP 2: Handle if statements
            if (std::regex_match(line, match, ifRegex)) {
                handleIfStatement(line);
                programCounter++;
                continue; // Move to the next line after handling the if
            }
            
            // STEP 3: Handle Declarations, Assignments, and Prints
            if (std::regex_match(line, match, declarationRegex)) {
                
                std::string varName = match[1].str();
                std::string expression = match[2].str();
                
                // Ensure variable exists (or create it)
                if (variables.find(varName) == variables.end()) {
                    variables.emplace(varName, Variable(varName));
                }
                
                // Substitute and Evaluate
                std::string substitutedExpr = findAndReplaceVariables(expression, variables);
                EvalResult result = eval.evaluate(substitutedExpr);
                
                // Store result
                if (result.type != "error") {
                    variables.at(varName).setValue(result);
                } else {
                    std::cerr << "Runtime Error on line: '" << line << "'. " << result.value << std::endl;
                }

            } else if (std::regex_match(line, match, assignmentRegex)) {
                
                std::string varName = match[1].str();
                std::string expression = match[2].str();

                // Check for declaration
                if (variables.find(varName) == variables.end()) {
                    std::cerr << "Name Error: Variable '" << varName << "' used before declaration." << std::endl;
                    continue; 
                }
                
                // Substitute and Evaluate
                std::string substitutedExpr = findAndReplaceVariables(expression, variables);
                EvalResult result = eval.evaluate(substitutedExpr);
                
                // Store result
                if (result.type != "error") {
                    variables.at(varName).setValue(result);
                } else {
                    std::cerr << "Runtime Error on line: '" << line << "'. " << result.value << std::endl;
                }
                
            } else if (std::regex_match(line, match, printRegex) || std::regex_match(line, match, printlnRegex)) {
                
                std::string expression = match[1].str();
                bool isPrintln = (line.find("println") != std::string::npos);

                // Substitute and evaluate
                std::string substitutedExpr = findAndReplaceVariables(expression, variables);
                EvalResult result = eval.evaluate(substitutedExpr);

                // Handle output
                if (result.type != "error") {
                    std::cout << result.asString();
                    if (isPrintln) {
                        std::cout << "\n";
                    }
                } else {
                    std::cerr << "Runtime Error in print statement: " << result.value << std::endl;
                }
            } 
            // NOTE: A new 'else' block will go here for unhandled lines (e.g., standalone expression or control flow)
            // For now, any unhandled line is simply skipped, which is fine for your current set of commands.
            programCounter++;
        }
    }
};

void ExecutionEngine::jumpToLine(int targetLine) {
    if (fileLineMap.count(targetLine)) {
        // Set the file pointer (seekg) to the start of the target line
        file.seekg(fileLineMap.at(targetLine));
        // Update the program counter
        programCounter = targetLine;
    } else {
        std::cerr << "Runtime Error on line " << programCounter << ": Invalid jump target " << targetLine << std::endl;
    }
}

int ExecutionEngine::findBlockEnd(int startLine) {
    int currentLine = startLine;
    int nestedLevel = 1; // We assume we are inside the block already (matched the opening '{')

    // Use a temporary file stream for safe parsing without disrupting the main execution stream
    std::ifstream tempFile("script.snx"); 
    
    // Jump the temporary file to the start of the block
    if (fileLineMap.count(startLine)) {
        tempFile.seekg(fileLineMap.at(startLine));
    } else {
        return -1; // Should not happen if startLine is correct
    }

    std::string line;
    while (nestedLevel > 0 && std::getline(tempFile, line)) {
        // --- Brace Counting Logic ---
        for (char c : line) {
            if (c == '{') {
                nestedLevel++;
            } else if (c == '}') {
                nestedLevel--;
                if (nestedLevel == 0) {
                    tempFile.close();
                    // currentLine is the line number *containing* the closing '}'
                    return currentLine; 
                }
            }
        }
        currentLine++;
    }

    tempFile.close();
    std::cerr << "Syntax Error: Unmatched opening brace starting near line " << startLine - 1 << std::endl;
    return -1; 
}

// Method inside ExecutionEngine

void ExecutionEngine::handleIfStatement(const std::string& line) {
    std::smatch match;

    if (!std::regex_match(line, match, ifRegex)) {
        std::cerr << "Internal Error: Called handleIfStatement with invalid line format." << std::endl;
        return;
    }
    
    std::string conditionExpression = match[1].str();
    
    // Evaluate the condition
    // NOTE: We assume findAndReplaceVariables can access the private 'variables' map
    std::string substitutedCond = findAndReplaceVariables(conditionExpression, variables); 
    EvalResult conditionResult = eval.evaluate(substitutedCond);

    if (conditionResult.type == "error") {
        std::cerr << "Runtime Error on line " << programCounter << ": " << conditionResult.value << std::endl;
        return; 
    }

    // If condition is FALSE, jump past the block
    if (conditionResult.asBool() == false) {
        
        // Block starts on the line *after* the if statement
        int blockStartLine = programCounter + 1;
        int blockEndLine = findBlockEnd(blockStartLine); 

        if (blockEndLine != -1) {
            // Jump to the line *immediately following* the closing '}'
            jumpToLine(blockEndLine + 1); 
        }
    }
}