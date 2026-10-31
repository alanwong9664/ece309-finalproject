// include/skills/skill_index.h
//
// YOU IMPLEMENT (src/skills/skill_index.cpp). Spec §6.3.
//
// An INVERTED INDEX for keyword search: word -> list of document ids that
// contain it (the "posting list"). Searching looks only at the posting lists
// of the query's words instead of scanning every document.
//
// std::unordered_map and std::vector are allowed (and expected). Ranking
// must use YOUR top_k() from util/min_heap.h, not a sort of all matches.

#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class SkillIndex {
public:
    // Indexes `text` under `doc_id`: every distinct word of unique_tokens(text)
    // gets doc_id appended to its posting list (once per document, even if
    // the word repeats). Documents are added with increasing doc_ids;
    // re-adding an id is not required to work.
    void add(std::size_t doc_id, std::string_view text);

    // The posting list for one word (already lowercase), or nullptr if no
    // document contains it. Ids are in the order they were added.
    const std::vector<std::size_t>* postings(std::string_view word) const;

    // Number of distinct words indexed.
    std::size_t word_count() const noexcept;

    // Up to k documents that contain at least one distinct word of the query
    // (unique_tokens(query)), best first:
    //   score = number of distinct query words the document contains;
    //   higher score first; equal scores -> smaller doc_id first.
    // An empty query, a query with no indexed words, or k == 0 -> empty.
    std::vector<std::size_t> search(std::string_view query, std::size_t k) const;

private:
    // ===== YOUR REPRESENTATION GOES HERE =====
    std::unordered_map<std::string, std::vector<std::size_t>> postings_;
};

// PROVIDED (src/skills/skill_index_linear.cpp). The same ranking computed by
// re-tokenizing every document: the baseline your design log compares
// against.
std::vector<std::size_t> linear_search(const std::vector<std::string>& docs,
                                       std::string_view query, std::size_t k);
