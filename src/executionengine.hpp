#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cctype>
#include <regex>
#include <map>
#include <sstream>

#include "evaluator.hpp"
#include "variable.hpp"
#include "helpers.hpp"
#include "function.hpp"

using LineMap = std::map<int, std::streampos>;

LineMap buildLineMap(const std::string& filename) {
    LineMap lineMap;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Error: Could not open " << filename << "\n";
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

// Function to split and trim arguments for function calls
std::vector<std::string> splitAndTrimArgs(const std::string& paramsString) {
    std::vector<std::string> args;
    std::stringstream ss(paramsString);
    std::string segment;
    while(std::getline(ss, segment, ',')) {
        // Simple trim logic
        size_t first = segment.find_first_not_of(' ');
        if (std::string::npos == first) {
            continue;
        }
        size_t last = segment.find_last_not_of(' ');
        args.push_back(segment.substr(first, (last - first + 1)));
    }
    return args;
}

class ExecutionEngine {
private:
    std::map<std::string, Variable> variables;
    std::map<std::string, Function> functions;
    Evaluator eval;
    std::ifstream file;

    std::string fileName;

    std::string style = "end";  // "end" or "brackets"

    int scopeLevel = 0;  // Tracks current scope level
    int programCounter = 1; // Tracks the current line number for context
    const std::map<int, std::streampos>& fileLineMap; // Reference to the line map

    int functionDepth = 0;
    std::vector<int> returnStack;

    bool ignoreLine = false;
    
    // Regular expressions for parsing
    const std::regex declarationRegex = std::regex(R"(^\s*var\s+([a-zA-Z_]\w*)\s*=\s*(.*))");
    const std::regex assignmentRegex = std::regex(R"(^\s*([a-zA-Z_]\w*)\s*=\s*(.*))");
    const std::regex printRegex = std::regex(R"(^\s*print\s+(.*))");
    const std::regex printlnRegex = std::regex(R"(^\s*println\s+(.*))");
    std::regex ifRegex;

    // Function regexes
    std::regex funcDefRegex;
    const std::regex funcCallRegex = std::regex(R"(^\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*\((.*?)\)\s*$)");
    const std::regex returnRegex = std::regex(R"(^\s*return\s*;?\s*$)");
    const std::regex returnExpRegex = std::regex(R"(^\s*return\s+.+;?\s*$)");

    // Control flow regexes
    const std::regex endRegex = std::regex(R"(^\s*END\s*$)"); // Matches: END
    const std::regex gotoRegex = std::regex(R"(^\s*GOTO\s+(\d+)\s*$)");
    const std::regex styleRegex = std::regex(R"(^\s*STYLE\s*=\s*([a-z]+)\s*$)");
    std::regex closeBlockRegex;

    // Helpers
    void jumpToLine(int targetLine);
    int findBlockEnd(int startLine);
    
    // Handlers
    bool handleIfStatement(const std::string& line);

    // Style setup
    void setupStyleRegexes() {
        if (style == "brackets") {
            ifRegex = std::regex(R"(^\s*if\s+(.*)\s*\{\s*$)");
            funcDefRegex = std::regex(R"(^\s*func\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\((.*?)\)\s*\{\s*$)");
            closeBlockRegex = std::regex(R"(^\s*\}\s*$)");
        } else if (style == "end") {
            ifRegex = std::regex(R"(^\s*if\s+(.*)\s*$)");
            funcDefRegex = std::regex(R"(^\s*func\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\((.*?)\)\s*$)");
            closeBlockRegex = std::regex(R"(^\s*end\s*$)");
        }
    }

public:
    ExecutionEngine(const std::string& filename, const std::map<int, std::streampos>& lineMap) : fileLineMap(lineMap) {
        file.open(filename);
        fileName = filename;
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open script file: " + filename);
        }

        setupStyleRegexes();
    }

    ~ExecutionEngine() {
        if (file.is_open()) {
            file.close();
        }
    }

    void registerFunction(std::string name, std::vector<std::string> parameters, int startingLine) {
        if (scopeLevel == 0)
            functions.emplace(name, Function(name, parameters, startingLine));
        else
            std::cerr << "Error: Function declarations are only allowed in the global scope." << std::endl;
    }

    void removeVariablesByScope() {
        // Iterate through the map safely, handling element deletion.
        for (auto it = variables.begin(); it != variables.end(); ) {
            if (it->second.scopeLevel >= scopeLevel) {
                it = variables.erase(it);
            } else {
                ++it;
            }
        }
    }

    void incrementScope() {
        scopeLevel++;
    }

    void decrementScope() {
        if (scopeLevel > 0) {
            removeVariablesByScope(); 
            scopeLevel--;
        } else {
            std::cout << "Warning: Attempted to decrement scope below 0." << std::endl;
        }
    }

    void run() {
        std::string line;
        std::smatch match;

        while (std::getline(file, line)) {
            // Set STYLE
            if (std::regex_match(line, match, styleRegex)) {
                std::string styleInput = match[1];
                if (styleInput == "brackets") {
                    style = "brackets";
                    setupStyleRegexes();
                }
                if (styleInput == "end") {
                    style = "end";
                    setupStyleRegexes();
                }

                programCounter++;
                continue;
            }
            // Skip comments
            if (line[0] == '#') {
                programCounter++;
                continue;
            }

            // Ignore line
            if (ignoreLine) {
                ignoreLine = false;
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

            // Close block
            if (std::regex_match(line, match, closeBlockRegex)) {
                if (scopeLevel == 1 && functionDepth > 0) {
                    if (returnStack.empty()) {
                        std::cerr << "Runtime Error on line " << programCounter << std::endl;
                        return;
                    }
                    
                    jumpToLine(returnStack.back());
                    returnStack.pop_back();
                    
                    // Only decrement scope if leaving the outermost function call
                    if (functionDepth > 0) {
                        if (functionDepth == 1) {
                            decrementScope();  // This call clears function variables
                        }
                        functionDepth--;
                    }
                    
                    continue;
                } else if (scopeLevel > 0) {
                    decrementScope();
                } else {
                    // Error handling for an unexpected '}' if you need it
                    std::cerr << "Syntax Error on line " << programCounter << ": Unexpected closing brace '}' or end statement 'end'." << std::endl;
                }
                programCounter++;
                continue;
            }

            // Function returns
            if (std::regex_match(line, match, returnRegex)) {
                if (returnStack.empty()) {
                    std::cerr << "Runtime Error on line " << programCounter << ": 'return' called outside of a function." << std::endl;
                    return;
                }
                
                jumpToLine(returnStack.back());
                returnStack.pop_back();
                
                // Only decrement scope if leaving the outermost function call
                if (functionDepth > 0) {
                    if (functionDepth == 1) {
                        decrementScope();  // This call clears function variables
                    }
                    functionDepth--;
                }
                
                continue;
            }

            if (std::regex_match(line, match, returnExpRegex)) {
                // Return with expression--not yet implemented
            }

            // FUNCTION LOGIC
            // Handle function declaration
            if (std::regex_match(line, match, funcDefRegex)) {
                std::string funcName = match[1].str();
                std::string paramsString = match[2].str();
                
                std::vector<std::string> params = splitAndTrimArgs(paramsString);

                registerFunction(funcName, params, programCounter);

                int endLine = findBlockEnd(programCounter + 1);
                if (endLine != -1) {
                    jumpToLine(endLine + 1);
                }

                continue;
            }

            // Function call logic
            if (std::regex_match(line, match, funcCallRegex)) {
                std::string funcName = match[1].str();
                std::string argsString = match[2].str();
                
                if (funcName == "input" || funcName == "print" || funcName == "println" || funcName == "var"
                    || funcName == "if" || funcName == "GOTO" || funcName == "return" || funcName == "END"
                    || funcName == "STYLE" || funcName == "func" || funcName == "end" || funcName == "while") {
                    // Handled elsewhere
                    programCounter++;
                    continue;
                }
                if (functions.count(funcName)) {
                    while (scopeLevel > 0) {
                        decrementScope();  // Wipe scope variables
                    }
                    incrementScope();
                    functionDepth++;

                    const Function& func = functions.at(funcName);
                    returnStack.push_back(programCounter + 1);
                    
                    // Handle Parameters/Arguments
                    std::vector<std::string> callArgs = splitAndTrimArgs(argsString);
                    const std::vector<std::string>& funcParams = func.parameters;
                    
                    for (size_t i = 0; i < funcParams.size(); ++i) {
                        std::string paramName = funcParams[i];
                        EvalResult result;
                        
                        if (i < callArgs.size() && !callArgs[i].empty()) {
                            std::string substitutedArg = findAndReplaceVariables(callArgs[i], variables);
                            result = eval.evaluate(substitutedArg);
                        } else {
                            result.type = "number";
                            result.value = "0";
                        }
                        
                        if (variables.find(paramName) == variables.end()) {
                            variables.emplace(paramName, Variable(paramName, scopeLevel));
                            
                            if (result.type != "error") {
                                variables.at(paramName).setValue(result);
                            } else {
                                variables.at(paramName).setValue({ "number", "0" });
                                std::cerr << "Runtime Warning on line " << programCounter << ": Failed to evaluate argument for parameter '" 
                                          << paramName << "'. Defaulting to 0." << std::endl;
                            }
                        } else {
                            std::cerr << "Runtime Error on line " << programCounter << ": Function parameter '" << paramName 
                                      << "' conflicts with existing variable in the current scope." << std::endl;
                        }
                    }

                    // Goto function body
                    jumpToLine(func.startingLine + 1); 
                    continue;

                } else {
                    std::cerr << "Name Error on line " << programCounter << ": Function '" << funcName << "' is not defined." << std::endl;
                }
            }

            // GOTO
            if (std::regex_match(line, match, gotoRegex)) {
                int targetLine = std::stoi(match[1].str());
                jumpToLine(targetLine);
                continue;
            }
            
            // STEP 1: Handle I/O Operations (Input Function)
            line = handleInputCall(line, variables);

            // STEP 2: Handle if statements
            if (std::regex_match(line, match, ifRegex)) {
                // Store whether we jumped
                bool jumped = handleIfStatement(line);

                if (!jumped) {
                    programCounter++;
                }
                continue;
            }
            
            // STEP 3: Handle Declarations, Assignments, and Prints
            if (std::regex_match(line, match, declarationRegex)) {
                
                std::string varName = match[1].str();
                std::string expression = match[2].str();
                
                // Ensure variable exists (or create it)
                if (variables.find(varName) != variables.end()) {
                    // Variable already exists, print an error and skip the rest of the block
                    std::cerr << "Compilation Error: Cannot redeclare variable '" << varName 
                            << "'. A variable with that name already exists." << std::endl;
                } else {
                    variables.emplace(varName, Variable(varName, scopeLevel));
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
        std::exit(EXIT_FAILURE);
    }
}

int ExecutionEngine::findBlockEnd(int startLine) {
    int currentLine = startLine;
    int nestedLevel = 1; // We assume we are inside the block already (matched the opening '{')

    // Use a temporary file stream for safe parsing without disrupting the main execution stream
    std::ifstream tempFile(fileName); 
    
    // Jump the temporary file to the start of the block
    if (fileLineMap.count(startLine)) {
        tempFile.seekg(fileLineMap.at(startLine));
    } else {
        return -1; // Should not happen if startLine is correct
    }

    std::string line;
    while (nestedLevel > 0 && std::getline(tempFile, line)) {
        // --- Brace Counting Logic ---
        if (style == "brackets") {
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
        } else if (style == "end") {
            // Detect block openers
            // Extend this when necessary for other block types
            if (std::regex_match(line, funcDefRegex) ||
                std::regex_match(line, ifRegex))
            {
                nestedLevel++;
            }


            // Detect block close
            else if (std::regex_match(line, closeBlockRegex)) {
                nestedLevel--;

                if (nestedLevel == 0) {
                    tempFile.close();
                    return currentLine;   // the line number containing "end"
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

bool ExecutionEngine::handleIfStatement(const std::string& line) {
    std::smatch match;

    if (!std::regex_match(line, match, ifRegex)) {
        std::cerr << "Internal Error: Called handleIfStatement with invalid line format." << std::endl;
        return false; // Did not jump
    }
    
    std::string conditionExpression = match[1].str();
    std::string substitutedCond = findAndReplaceVariables(conditionExpression, variables); 
    EvalResult conditionResult = eval.evaluate(substitutedCond);

    if (conditionResult.type == "error") {
        std::cerr << "Runtime Error on line " << programCounter << ": " << conditionResult.value << std::endl;
        return false; 
    }

    // If condition is FALSE, jump past the block
    if (conditionResult.asBool() == false) {     
        int blockStartLine = programCounter + 1;
        int blockEndLine = findBlockEnd(blockStartLine); 

        if (blockEndLine != -1) {
            jumpToLine(blockEndLine + 1); 
            return true; // <--- WE JUMPED
        }
    } else {
        incrementScope();
    }
    
    return false; // <--- WE DID NOT JUMP
}