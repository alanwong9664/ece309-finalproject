#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class SkillIndex {
public:

    void add(std::size_t doc_id, std::string_view text);

    const std::vector<std::size_t>* postings(std::string_view word) const;

    std::size_t word_count() const noexcept;

    std::vector<std::size_t> search(std::string_view query, std::size_t k) const;

private:

    std::unordered_map<std::string, std::vector<std::size_t>> postings_;
};

std::vector<std::size_t> linear_search(const std::vector<std::string>& docs,
                                       std::string_view query, std::size_t k);
