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

struct Function {
    std::string name;
    std::vector<std::string> parameters;
    int startingLine;

    Function(const std::string& funcName, const std::vector<std::string>& params, int line)
        : name(funcName), parameters(params), startingLine(line) {}
};

// PLAN:
/*
TODO:
-Allow return values
-Fix bug where function call followed by end statement doesnt end, eg:

func b()
    print "b"
end

func a()
    b()
end

a()

END

expected output:
b
Program execution terminated by END command.

actual output:
b
(Program still running)
*/