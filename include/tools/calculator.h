#pragma once
#include <string>
#include <string_view>
#include <vector>

struct CalcToken {
    enum class Kind { Number, Plus, Minus, Times, Divide, LParen, RParen };

    Kind   kind  = Kind::Number;
    double value = 0.0;
};

bool calc_tokenize(std::string_view expression, std::vector<CalcToken>* out,
                   std::string* error);

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out, std::string* error);
