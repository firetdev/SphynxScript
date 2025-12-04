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

// PLAN:
/*
Have a "function" struct that holds:
- Name
- Parameter List (a vector of strings)
- Starting Line Number

When a function is called, the interpreter will search the map of functions for it
It will then goto the starting line. If the function has parameters, it will automatically create
local variables for each parameter, with the values in the function call.
The code in the ExecutionEngine will need to be modified to handle the following:
- Returns:
-- Checks for an expression following the return
-- If there is none, it will goto the line after the function call. This will be stored in a vector
of lines, so that nested function calls can be handled. (for example, f1 is called from line 10. f1
calls f2 from line 7. The first return will go to line 8, the second to line 11)
-- If there is an expression, it will evaluate it. The engine will then goto the line it wall called
from, and replace the function call with the returned value in the expression. I don't yet know how
I will implement this.
- Function scope:
-- The system will need to store a "function scope level" that is incremented when a function is called,
and decremented when it returns or ends (which will automatically do the same as a return with no value).
This will allow returns from functions to work correctly.
*/