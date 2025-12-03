#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cctype>
#include <regex>
#include <map>

class Variable {
public:
    std::string name;
    std::string value;
    std::string type;
    int scopeLevel = 0;

    Variable(const std::string n, int scope)
        : name(n), value(""), type("undefined"), scopeLevel(scope) {}
    
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