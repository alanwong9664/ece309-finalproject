// src/memory/memory_store.cpp — PROVIDED.

#include "memory/memory_store.h"
#include "util/min_heap.h"
#include "util/text.h"

#include <algorithm>
#include <fstream>
#include <system_error>
#include <unordered_set>

namespace fs = std::filesystem;

std::uint64_t MemoryStore::remember(std::string text) {
    std::uint64_t id = next_id_++;
    index_.insert(MemoryRecord{id, std::move(text)});
    return id;
}

bool MemoryStore::forget(std::uint64_t id) { return index_.remove(id); }

std::vector<MemoryRecord> MemoryStore::list() const {
    std::vector<MemoryRecord> out;
    index_.for_each_in_order([&](const MemoryRecord& r) { out.push_back(r); });
    return out;
}

namespace {
struct Scored {
    std::size_t   score;
    std::uint64_t id;
    std::string   text;
};
// "less" = worse: lower score, or equal score and older.
struct WorseThan {
    bool operator()(const Scored& a, const Scored& b) const {
        if (a.score != b.score) return a.score < b.score;
        return a.id < b.id;
    }
};
}  // namespace

std::vector<MemoryRecord> MemoryStore::recall(std::string_view query, std::size_t k) const {
    std::vector<std::string> q = unique_tokens(query);
    std::vector<Scored> scored;
    index_.for_each_in_order([&](const MemoryRecord& r) {
        std::vector<std::string> words = tokenize(r.text);
        std::unordered_set<std::string> have(words.begin(), words.end());
        std::size_t s = 0;
        for (const std::string& w : q) s += have.count(w);
        if (s > 0) scored.push_back({s, r.id, r.text});
    });
    std::vector<MemoryRecord> out;
    for (Scored& s : top_k(scored, k, WorseThan{})) out.push_back({s.id, std::move(s.text)});
    return out;
}

std::string MemoryStore::format_id(std::uint64_t id) { return "m" + std::to_string(id); }

bool MemoryStore::parse_id(std::string_view text, std::uint64_t* id) {
    if (text.size() < 2 || text.size() > 20 || text[0] != 'm') return false;
    std::uint64_t v = 0;
    for (std::size_t i = 1; i < text.size(); ++i) {
        if (text[i] < '0' || text[i] > '9') return false;
        v = v * 10 + static_cast<std::uint64_t>(text[i] - '0');
    }
    if (v == 0) return false;
    *id = v;
    return true;
}

namespace {
std::string escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\t') o += "\\t";
        else o += c;
    }
    return o;
}
bool unescape(const std::string& s, std::string* out) {
    out->clear();
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '\\') { *out += s[i]; continue; }
        if (++i == s.size()) return false;
        if (s[i] == '\\') *out += '\\';
        else if (s[i] == 'n') *out += '\n';
        else if (s[i] == 't') *out += '\t';
        else return false;
    }
    return true;
}
bool parse_u64(const std::string& s, std::uint64_t* v) {
    if (s.empty() || s.size() > 19) return false;
    *v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        *v = *v * 10 + static_cast<std::uint64_t>(c - '0');
    }
    return true;
}
}  // namespace

bool MemoryStore::save(const fs::path& file, std::string* error) const {
    fs::path tmp = file;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) { *error = "cannot write " + tmp.string(); return false; }
        out << "next " << next_id_ << "\n";
        index_.for_each_in_order([&](const MemoryRecord& r) {
            out << r.id << "\t" << escape(r.text) << "\n";
        });
        out.flush();
        if (!out) { *error = "write failed: " + tmp.string(); return false; }
    }
    std::error_code ec;
    fs::rename(tmp, file, ec);   // atomic on POSIX: readers see old or new, never half
    if (ec) { *error = "rename failed: " + ec.message(); return false; }
    return true;
}

bool MemoryStore::load(const fs::path& file, std::string* error) {
    std::ifstream in(file, std::ios::binary);
    if (!in) { *error = "cannot open " + file.string(); return false; }
    std::string line;
    std::uint64_t next = 0;
    if (!std::getline(in, line) || line.rfind("next ", 0) != 0 || !parse_u64(line.substr(5), &next)) {
        *error = "bad header line";
        return false;
    }
    std::vector<MemoryRecord> recs;
    std::unordered_set<std::uint64_t> seen;
    std::size_t lineno = 1;
    while (std::getline(in, line)) {
        ++lineno;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::size_t tab = line.find('\t');
        MemoryRecord r;
        if (tab == std::string::npos || !parse_u64(line.substr(0, tab), &r.id) || r.id == 0 ||
            !unescape(line.substr(tab + 1), &r.text)) {
            *error = "bad record on line " + std::to_string(lineno);
            return false;
        }
        if (!seen.insert(r.id).second) {
            *error = "duplicate id on line " + std::to_string(lineno);
            return false;
        }
        if (r.id >= next) next = r.id + 1;
        recs.push_back(std::move(r));
    }
    index_.clear();
    for (MemoryRecord& r : recs) index_.insert(std::move(r));
    next_id_ = next;
    return true;
}
