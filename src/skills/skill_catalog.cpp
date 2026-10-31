// src/skills/skill_catalog.cpp — PROVIDED.

#include "skills/skill_catalog.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>
#include <unordered_set>

namespace fs = std::filesystem;

namespace {

std::string trim(std::string_view s) {
    std::size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r')) ++b;
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r')) --e;
    return std::string(s.substr(b, e - b));
}

bool valid_name(const std::string& n) {
    if (n.empty() || n.size() > 64) return false;
    for (char c : n)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
    return true;
}

// Reads lines up to and including the closing "---" of the front matter and
// stops there: the body is not read.
bool read_front_matter(const fs::path& file, std::string* out) {
    std::ifstream in(file, std::ios::binary);
    if (!in) return false;
    std::string line;
    int fences = 0;
    while (fences < 2 && std::getline(in, line)) {
        if (trim(line) == "---") ++fences;
        *out += line + "\n";
    }
    return true;
}

}  // namespace

bool SkillCatalog::parse_front_matter(std::string_view text, SkillMeta* out, std::string* error) {
    // Split into lines.
    std::vector<std::string> lines;
    std::string cur;
    for (char c : text) {
        if (c == '\n') { lines.push_back(cur); cur.clear(); }
        else cur += c;
    }
    if (!cur.empty()) lines.push_back(cur);

    std::size_t i = 0;
    while (i < lines.size() && trim(lines[i]).empty()) ++i;
    if (i == lines.size() || trim(lines[i]) != "---") { *error = "missing opening ---"; return false; }
    ++i;
    SkillMeta m;
    bool closed = false, have_name = false, have_desc = false;
    for (; i < lines.size(); ++i) {
        std::string line = trim(lines[i]);
        if (line == "---") { closed = true; break; }
        if (line.empty()) continue;
        std::size_t colon = line.find(':');
        if (colon == std::string::npos) { *error = "line without ':' in front matter: " + line; return false; }
        std::string key = trim(std::string_view(line).substr(0, colon));
        std::string val = trim(std::string_view(line).substr(colon + 1));
        if (key == "name") {
            if (!valid_name(val)) { *error = "bad name '" + val + "' (use a-z, 0-9, -)"; return false; }
            m.name = val;
            have_name = true;
        } else if (key == "description") {
            if (val.empty()) { *error = "empty description"; return false; }
            m.description = val;
            have_desc = true;
        } else if (key == "requires") {
            if (val.size() < 2 || val.front() != '[' || val.back() != ']') {
                *error = "requires must look like [a, b]";
                return false;
            }
            std::string inner = val.substr(1, val.size() - 2);
            std::string item;
            auto flush = [&]() -> bool {
                std::string t = trim(item);
                item.clear();
                if (t.empty()) return true;
                if (!valid_name(t)) { *error = "bad requirement '" + t + "'"; return false; }
                if (std::find(m.requirements.begin(), m.requirements.end(), t) == m.requirements.end())
                    m.requirements.push_back(t);
                return true;
            };
            for (char c : inner) {
                if (c == ',') { if (!flush()) return false; }
                else item += c;
            }
            if (!flush()) return false;
        } else {
            *error = "unknown front-matter key '" + key + "'";
            return false;
        }
    }
    if (!closed) { *error = "missing closing ---"; return false; }
    if (!have_name) { *error = "missing name"; return false; }
    if (!have_desc) { *error = "missing description"; return false; }
    *out = std::move(m);
    return true;
}

SkillCatalog::SkillCatalog(std::size_t body_cache_capacity)
    : graph_(std::make_unique<DepGraph>()), index_(std::make_unique<SkillIndex>()),
      bodies_(body_cache_capacity) {}

SkillCatalog::ScanReport SkillCatalog::scan(const fs::path& root) {
    ScanReport report;
    skills_.clear();
    graph_ = std::make_unique<DepGraph>();
    index_ = std::make_unique<SkillIndex>();
    bodies_.clear();

    std::error_code ec;
    std::vector<fs::path> files;
    for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        fs::path f = it->path() / "SKILL.md";
        if (it->is_directory(ec) && fs::is_regular_file(f, ec)) files.push_back(f);
    }
    if (ec) report.errors.push_back("cannot read " + root.string() + ": " + ec.message());
    std::sort(files.begin(), files.end());

    std::unordered_set<std::string> names;
    for (const fs::path& f : files) {
        std::string fm, err;
        SkillMeta m;
        if (!read_front_matter(f, &fm)) { report.errors.push_back(f.string() + ": unreadable"); continue; }
        if (!parse_front_matter(fm, &m, &err)) { report.errors.push_back(f.string() + ": " + err); continue; }
        if (!names.insert(m.name).second) {
            report.errors.push_back(f.string() + ": duplicate skill name '" + m.name + "'");
            continue;
        }
        m.file = f;
        skills_.push_back(std::move(m));
    }
    std::sort(skills_.begin(), skills_.end(),
              [](const SkillMeta& a, const SkillMeta& b) { return a.name < b.name; });
    for (std::size_t id = 0; id < skills_.size(); ++id) {
        const SkillMeta& m = skills_[id];
        graph_->add_node(m.name);
        index_->add(id, m.name + " " + m.description);
    }
    for (const SkillMeta& m : skills_)
        for (const std::string& r : m.requirements) graph_->add_edge(m.name, r);
    report.loaded = skills_.size();
    return report;
}

const SkillMeta* SkillCatalog::find(std::string_view name) const {
    auto it = std::lower_bound(skills_.begin(), skills_.end(), name,
                               [](const SkillMeta& m, std::string_view n) { return m.name < n; });
    return (it != skills_.end() && it->name == name) ? &*it : nullptr;
}

bool SkillCatalog::body(const SkillMeta& meta, std::string* out, std::string* error) {
    if (const std::string* cached = bodies_.get(meta.name)) {
        *out = *cached;
        return true;
    }
    std::ifstream in(meta.file, std::ios::binary);
    if (!in) { *error = "cannot read " + meta.file.string(); return false; }
    std::string all((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ++body_reads_;
    // Skip the front matter: everything up to and including the 2nd "---" line.
    std::size_t pos = 0;
    int fences = 0;
    while (pos < all.size() && fences < 2) {
        std::size_t nl = all.find('\n', pos);
        std::size_t end = nl == std::string::npos ? all.size() : nl;
        if (trim(std::string_view(all).substr(pos, end - pos)) == "---") ++fences;
        pos = nl == std::string::npos ? all.size() : nl + 1;
    }
    std::string b = all.substr(pos);
    while (!b.empty() && (b.back() == '\n' || b.back() == '\r' || b.back() == ' ')) b.pop_back();
    while (!b.empty() && (b.front() == '\n' || b.front() == '\r')) b.erase(b.begin());
    bodies_.put(meta.name, b);
    *out = std::move(b);
    return true;
}

bool SkillCatalog::load(std::string_view name, std::string* text, std::string* error) {
    std::vector<std::string> order;
    if (!graph_->order_from(std::string(name), &order, error)) return false;
    std::string names, bodies;
    for (const std::string& n : order) {
        const SkillMeta* m = find(n);
        if (m == nullptr) { *error = "missing: " + n; return false; }
        std::string b;
        if (!body(*m, &b, error)) return false;
        names += (names.empty() ? "" : ", ") + n;
        bodies += "\n\n## " + n + "\n" + b;
    }
    *text = "Loaded " + std::to_string(order.size()) + " skill" + (order.size() == 1 ? "" : "s") +
            " (dependencies first): " + names + bodies;
    return true;
}

std::vector<const SkillMeta*> SkillCatalog::search(std::string_view query, std::size_t k) const {
    std::vector<const SkillMeta*> out;
    for (std::size_t id : index_->search(query, k))
        if (id < skills_.size()) out.push_back(&skills_[id]);
    return out;
}

std::vector<std::string> SkillCatalog::dependents(std::string_view name) const {
    return graph_->dependents(std::string(name));
}
