// include/util/text.h
//
// PROVIDED. Two small text helpers shared by memory, skills, and the
// context builder.

#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// Splits text into lowercase words. A word is a maximal run of ASCII letters
// and digits; everything else separates words.
//   tokenize("Read the CSV-file, twice!") == {"read", "the", "csv", "file", "twice"}
std::vector<std::string> tokenize(std::string_view text);

// tokenize() with duplicates removed, keeping each word's first occurrence.
//   unique_tokens("a b a c b") == {"a", "b", "c"}
std::vector<std::string> unique_tokens(std::string_view text);

// Approximate token count used for the context budget: one token per word
// (as in tokenize) plus one per other non-space character.
//   count_tokens("Hi, Bob!") == 4      ("hi" "," "bob" "!")
//   count_tokens("")         == 0
std::size_t count_tokens(std::string_view text);
