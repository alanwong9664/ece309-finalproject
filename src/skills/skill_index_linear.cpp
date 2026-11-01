#include "skills/skill_index.h"
#include "util/text.h"

#include <algorithm>
#include <unordered_set>
#include <utility>

std::vector<std::size_t> linear_search(const std::vector<std::string>& docs,
                                       std::string_view query, std::size_t k) {
    std::vector<std::string> q = unique_tokens(query);
    std::vector<std::pair<std::size_t, std::size_t>> hits;
    for (std::size_t id = 0; id < docs.size(); ++id) {
        std::vector<std::string> words = tokenize(docs[id]);
        std::unordered_set<std::string> have(words.begin(), words.end());
        std::size_t s = 0;
        for (const std::string& w : q) s += have.count(w);
        if (s > 0) hits.push_back({s, id});
    }
    std::sort(hits.begin(), hits.end(), [](const auto& a, const auto& b) {
        return a.first != b.first ? a.first > b.first : a.second < b.second;
    });
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i < hits.size() && i < k; ++i) out.push_back(hits[i].second);
    return out;
}
