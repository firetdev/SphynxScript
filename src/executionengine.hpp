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

// Build a line map
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

        if (lineNumber == 1) {
            lineMap[1] = 0;
        }

        lineNumber++;
    }

    file.close();
    return lineMap;
}

// Function to split and trim arguments for the call
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

    int scopeLevel = 0;
    int programCounter = 1;
    const std::map<int, std::streampos>& fileLineMap;

    int functionDepth = 0;
    std::vector<int> returnStack;

    const std::regex declarationRegex = std::regex(R"(^\s*var\s+([a-zA-Z_]\w*)\s*=\s*(.*))");
    const std::regex assignmentRegex = std::regex(R"(^\s*([a-zA-Z_]\w*)\s*=\s*(.*))");
    const std::regex printRegex = std::regex(R"(^\s*print\s+(.*))");
    const std::regex printlnRegex = std::regex(R"(^\s*println\s+(.*))");
    const std::regex ifRegex = std::regex(R"(^\s*if\s+(.*)\s*\{\s*$)");
    
    const std::regex funcDefRegex = std::regex(R"(^\s*func\s+([a-zA-Z_][a-zA-Z0-9_]*)\s*\((.*?)\)\s*$)");
    const std::regex funcCallRegex = std::regex(R"(^\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*\((.*?)\)\s*$)");
    const std::regex funcEndRegex = std::regex(R"(^\s*end\s*$)");

    const std::regex returnRegex = std::regex(R"(^\s*return\s*;?\s*$)");
    const std::regex returnExpRegex = std::regex(R"(^\s*return\s+.+;?\s*$)");
    const std::regex endRegex = std::regex(R"(^\s*END\s*$)"); 
    const std::regex gotoRegex = std::regex(R"(^\s*GOTO\s+(\d+)\s*$)");
    const std::regex closeBlockRegex = std::regex(R"(^\s*\}\s*$)");

    // Helpers
    void jumpToLine(int targetLine);
    int findBlockEnd(int startLine);
    int findFunctionEnd(int startLine);
    
    // Handlers
    void handleIfStatement(const std::string& line);

public:
    ExecutionEngine(const std::string& filename, const std::map<int, std::streampos>& lineMap) : fileLineMap(lineMap) {
        file.open(filename);
        fileName = filename;
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open script file: " + filename);
        }
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
            // Skip comments
            if (!line.empty() && line[0] == '#') {
                programCounter++;
                continue;
            }

            // End program (Global END)
            if (std::regex_match(line, match, endRegex)) {
                std::cout << "\nProgram execution terminated by END command.\n";
                return;
            } 

            // Skip empty lines
            if (line.empty() || std::regex_match(line, std::regex(R"(^\s*$)"))) {
                programCounter++;
                continue;
            }

            // Close block (})
            if (std::regex_match(line, match, closeBlockRegex)) {
                if (scopeLevel > 0) {
                    decrementScope();
                } else {
                    std::cerr << "Syntax Error on line " << programCounter << ": Unexpected closing brace '}'." << std::endl;
                }
                programCounter++;
                continue;
            }

            // GOTO
            if (std::regex_match(line, match, gotoRegex)) {
                int targetLine = std::stoi(match[1].str());
                jumpToLine(targetLine);
                continue;
            }
            
            // If funcEndRegex is matched, treat it as an implicit return
            if (std::regex_match(line, match, returnRegex) || std::regex_match(line, match, funcEndRegex)) {
                if (returnStack.empty()) {
                    std::cerr << "Runtime Error on line " << programCounter << ": 'return' called outside of a function." << std::endl;
                    return;
                }
                
                jumpToLine(returnStack.back());
                returnStack.pop_back();
                
                // Only decrement scope if leaving the outermost function call
                if (functionDepth > 0) {
                    if (functionDepth == 1) {
                        decrementScope(); // This call clears function variables
                    }
                    functionDepth--;
                }
                
                continue;
            }

            if (std::regex_match(line, match, returnExpRegex)) {
                if (returnStack.empty()) {
                    std::cerr << "Runtime Error on line " << programCounter << ": 'return' called outside of a function." << std::endl;
                    return;
                }
                
                jumpToLine(returnStack.back());
                returnStack.pop_back();
                
                // Only decrement scope if leaving the outermost function call
                if (functionDepth > 0) {
                    if (functionDepth == 1) {
                        decrementScope(); // This call clears function variables
                    }
                    functionDepth--;
                }
                
                continue;
            }

            // Handle Input
            line = handleInputCall(line, variables);

            // Handle If
            if (std::regex_match(line, match, ifRegex)) {
                handleIfStatement(line);
                programCounter++;
                continue; 
            }
            
            // Handle function declaration
            if (std::regex_match(line, match, funcDefRegex)) {
                std::string funcName = match[1].str();
                std::string paramsString = match[2].str();
                
                std::vector<std::string> params = splitAndTrimArgs(paramsString);

                registerFunction(funcName, params, programCounter);

                int endLine = findFunctionEnd(programCounter + 1);

                if (endLine != -1) {
                    jumpToLine(endLine + 1);
                } else {
                    std::cerr << "Syntax Error: Function '" << funcName << "' starting at line " 
                              << programCounter << " is missing an 'end' statement." << std::endl;
                    return;
                }

                continue;
            }

            // Function call logic
            if (std::regex_match(line, match, funcCallRegex)) {
                std::string funcName = match[1].str();
                std::string argsString = match[2].str();
                
                if (functions.count(funcName)) {
                    
                    const Function& func = functions.at(funcName);
                    
                    returnStack.push_back(programCounter + 1); 
                    
                    if (functionDepth == 0) {
                        incrementScope();
                    }
                    functionDepth++;
                    
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

            programCounter++;
        }
    }
};

void ExecutionEngine::jumpToLine(int targetLine) {
    if (fileLineMap.count(targetLine)) {
        file.seekg(fileLineMap.at(targetLine));
        programCounter = targetLine;
    } else {
        std::cerr << "Runtime Error on line " << programCounter << ": Invalid jump target " << targetLine << std::endl;
    }
}

int ExecutionEngine::findBlockEnd(int startLine) {
    int currentLine = startLine;
    int nestedLevel = 1;

    std::ifstream tempFile(fileName); 
    
    if (fileLineMap.count(startLine)) {
        tempFile.seekg(fileLineMap.at(startLine));
    } else {
        return -1; 
    }

    std::string line;
    while (nestedLevel > 0 && std::getline(tempFile, line)) {
        for (char c : line) {
            if (c == '{') {
                nestedLevel++;
            } else if (c == '}') {
                nestedLevel--;
                if (nestedLevel == 0) {
                    tempFile.close();
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

int ExecutionEngine::findFunctionEnd(int startLine) {
    int currentLine = startLine;
    std::ifstream tempFile(fileName); 
    
    if (fileLineMap.count(startLine)) {
        tempFile.seekg(fileLineMap.at(startLine));
    } else {
        return -1; 
    }

    std::string line;
    while (std::getline(tempFile, line)) {
        if (std::regex_match(line, funcEndRegex)) {
            tempFile.close();
            return currentLine;
        }
        currentLine++;
    }

    tempFile.close();
    return -1; 
}

void ExecutionEngine::handleIfStatement(const std::string& line) {
    std::smatch match;

    if (!std::regex_match(line, match, ifRegex)) {
        std::cerr << "Internal Error: Called handleIfStatement with invalid line format." << std::endl;
        return;
    }
    
    std::string conditionExpression = match[1].str();
    
    std::string substitutedCond = findAndReplaceVariables(conditionExpression, variables); 
    EvalResult conditionResult = eval.evaluate(substitutedCond);

    if (conditionResult.type == "error") {
        std::cerr << "Runtime Error on line " << programCounter << ": " << conditionResult.value << std::endl;
        return; 
    }

    if (conditionResult.asBool() == false) {
        int blockStartLine = programCounter + 1;
        int blockEndLine = findBlockEnd(blockStartLine); 

        if (blockEndLine != -1) {
            jumpToLine(blockEndLine + 1); 
        }
    } else {
        incrementScope();
    }
}