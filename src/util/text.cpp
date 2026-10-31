// src/util/text.cpp — PROVIDED.

#include "util/text.h"

#include <algorithm>

namespace {
bool is_word_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
}
bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}
char lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }
}  // namespace

std::vector<std::string> tokenize(std::string_view text) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : text) {
        if (is_word_char(c)) {
            cur += lower(c);
        } else if (!cur.empty()) {
            out.push_back(std::move(cur));
            cur.clear();
        }
    }
    if (!cur.empty()) out.push_back(std::move(cur));
    return out;
}

std::vector<std::string> unique_tokens(std::string_view text) {
    std::vector<std::string> out;
    for (std::string& t : tokenize(text))
        if (std::find(out.begin(), out.end(), t) == out.end()) out.push_back(std::move(t));
    return out;
}

std::size_t count_tokens(std::string_view text) {
    std::size_t n = 0;
    bool in_word = false;
    for (char c : text) {
        if (is_word_char(c)) {
            if (!in_word) ++n;
            in_word = true;
        } else {
            in_word = false;
            if (!is_space(c)) ++n;
        }
    }
    return n;
}
