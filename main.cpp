// TODO: Add if statements, while loops, and functions in the future.

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
#include "helpers.hpp"

int main() {
    const std::string scriptFilename = "script.snx";

    try {
        LineMap scriptLineMap = buildLineMap(scriptFilename);
        
        if (scriptLineMap.empty()) {
             // Handle case where file was empty or could not be opened
             return 1;
        }
        
        // The ExecutionEngine takes the map by const reference in its constructor
        ExecutionEngine engine(scriptFilename, scriptLineMap);
        
        engine.run();

    } catch (const std::runtime_error& e) {
        std::cerr << "Execution Fatal Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}