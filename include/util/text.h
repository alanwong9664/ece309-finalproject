#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

std::vector<std::string> tokenize(std::string_view text);

std::vector<std::string> unique_tokens(std::string_view text);

std::size_t count_tokens(std::string_view text);
