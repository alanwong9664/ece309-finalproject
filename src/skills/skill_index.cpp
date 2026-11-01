#include "skills/skill_index.h"
#include "util/min_heap.h"
#include "util/text.h"
#include <utility>

void SkillIndex::add(std::size_t doc_id, std::string_view text) {
    for (const auto& word : unique_tokens(text)) postings_[word].push_back(doc_id);
}
const std::vector<std::size_t>* SkillIndex::postings(std::string_view word) const {
    const auto it = postings_.find(std::string(word));
    return it == postings_.end() ? nullptr : &it->second;
}
std::size_t SkillIndex::word_count() const noexcept { return postings_.size(); }
std::vector<std::size_t> SkillIndex::search(std::string_view query, std::size_t k) const {
    if (k == 0) return {};
    std::unordered_map<std::size_t, std::size_t> scores;
    for (const auto& word : unique_tokens(query)) {
        const auto* ids = postings(word);
        if (ids) for (std::size_t id : *ids) ++scores[id];
    }
    using Hit = std::pair<std::size_t, std::size_t>;
    std::vector<Hit> hits;
    hits.reserve(scores.size());
    for (const auto& entry : scores) hits.push_back({entry.second, entry.first});
    const auto worse = [](const Hit& a, const Hit& b) {
        return a.first != b.first ? a.first < b.first : a.second > b.second;
    };
    std::vector<std::size_t> result;
    for (const auto& hit : top_k(hits, k, worse)) result.push_back(hit.second);
    return result;
}
