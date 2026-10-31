// src/tools/calculator_eval.cpp — P3 instructor reference (used only with -DUSE_P3_REFERENCE=ON).
//
// Two-stack evaluation (Dijkstra):
//   values : numbers waiting to be combined
//   ops    : operators and '(' waiting to be applied
// For each token:
//   number  -> push on values
//   '('     -> push on ops
//   ')'     -> apply ops until the matching '(' is on top; pop it
//   op      -> while the top of ops is an operator with precedence >= op
//              (that is what makes equal precedence left-to-right), apply it;
//              then push op
// At the end apply everything left on ops. The answer is the single value.

#include "tools/calculator.h"

#include <stack>

namespace {

using Kind = CalcToken::Kind;

int precedence(Kind k) {
    return (k == Kind::Times || k == Kind::Divide) ? 2 : 1;   // only called on operators
}

bool is_operator(Kind k) {
    return k == Kind::Plus || k == Kind::Minus || k == Kind::Times || k == Kind::Divide;
}

// Pops one operator and two values, pushes the result.
bool apply_top(std::stack<double>& values, std::stack<Kind>& ops, std::string* error) {
    if (ops.empty() || values.size() < 2) {
        *error = "malformed expression";
        return false;
    }
    Kind op = ops.top();
    ops.pop();
    double right = values.top();
    values.pop();
    double left = values.top();
    values.pop();

    double result = 0.0;
    switch (op) {
        case Kind::Plus:   result = left + right; break;
        case Kind::Minus:  result = left - right; break;
        case Kind::Times:  result = left * right; break;
        case Kind::Divide:
            if (right == 0.0) {
                *error = "division by zero";
                return false;
            }
            result = left / right;
            break;
        default:
            *error = "malformed expression";
            return false;
    }
    values.push(result);
    return true;
}

}  // namespace

bool calc_evaluate(const std::vector<CalcToken>& tokens, double* out, std::string* error) {
    std::stack<double> values;
    std::stack<Kind>   ops;

    for (const CalcToken& t : tokens) {
        if (t.kind == Kind::Number) {
            values.push(t.value);
        } else if (t.kind == Kind::LParen) {
            ops.push(Kind::LParen);
        } else if (t.kind == Kind::RParen) {
            while (!ops.empty() && ops.top() != Kind::LParen)
                if (!apply_top(values, ops, error)) return false;
            if (ops.empty()) {
                *error = "malformed expression";
                return false;
            }
            ops.pop();   // the '('
        } else {         // + - * /
            while (!ops.empty() && is_operator(ops.top()) &&
                   precedence(ops.top()) >= precedence(t.kind))
                if (!apply_top(values, ops, error)) return false;
            ops.push(t.kind);
        }
    }
    while (!ops.empty())
        if (!apply_top(values, ops, error)) return false;

    if (values.size() != 1) {
        *error = "malformed expression";
        return false;
    }
    *out = values.top();
    return true;
}
