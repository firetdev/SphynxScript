// TODO: Add while loops, dictionaries, arrays, piping, filters. Add return values in function.hpp.
// Add "filters" and "piping (->)"
// Filter example (end style):
// filter MyFilter
//     201+ => 200
//     99- => 100
//     + => -
//     x.0-x.99 => x
// end
//
// Piping example:
// var result = ParseJSON(file) -> MyFilter -> findKeys("age") -> adult() -> print()
//
// Dictionary example (always uses brackets):
// dict x = {
//    name: "John",
//    age: 30,
//    isStudent: false
// }
//
// Array example:
// arr numbers = [1, 2, 3, 4, 5]


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