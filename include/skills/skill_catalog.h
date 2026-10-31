// include/skills/skill_catalog.h
//
// PROVIDED. The skill library: scans a folder of skills, and uses YOUR
// DepGraph (load order), YOUR SkillIndex (search), and YOUR LruCache
// (skill bodies) to answer the list_skills / load_skill / skill_dependents
// tools.
//
// A skill is a folder containing SKILL.md:
//
//   ---
//   name: csv-report
//   description: Turn a CSV of sales rows into a formatted summary report.
//   requires: [read-csv, table-format, number-format]
//   ---
//   # CSV report
//   1. Load the rows (read-csv).
//   ...
//
// Only the front matter (between the two --- lines) is read by scan(). The
// body is read the first time load() needs it and then kept in the LRU cache.

#pragma once
#include "skills/dep_graph.h"
#include "skills/skill_index.h"
#include "util/lru_cache.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct SkillMeta {
    std::string              name;          // [a-z0-9-]+
    std::string              description;   // non-empty, one line
    std::vector<std::string> requirements;  // the "requires:" list; may be empty
    std::filesystem::path    file;          // .../SKILL.md
};

class SkillCatalog {
public:
    explicit SkillCatalog(std::size_t body_cache_capacity = 8);

    struct ScanReport {
        std::size_t              loaded = 0;
        std::vector<std::string> errors;    // one line per rejected skill
    };

    // Reads <root>/*/SKILL.md front matter. A malformed or duplicate skill is
    // reported in errors and skipped; the scan goes on. Replaces any earlier
    // scan.
    ScanReport scan(const std::filesystem::path& root);

    // Skills sorted by name. A skill's position here is its SkillIndex doc_id.
    const std::vector<SkillMeta>& skills() const noexcept { return skills_; }
    const SkillMeta* find(std::string_view name) const;

    // load_skill: the bodies of `name` and everything it requires, in
    // DepGraph::order_from order, formatted for the model. False + *error on
    // unknown / missing / cycle / unreadable file.
    bool load(std::string_view name, std::string* text, std::string* error);

    // list_skills with a query: SkillIndex::search over "name description".
    std::vector<const SkillMeta*> search(std::string_view query, std::size_t k) const;

    // skill_dependents: DepGraph::dependents.
    std::vector<std::string> dependents(std::string_view name) const;

    // How many times a SKILL.md body was read from disk (cache misses).
    std::size_t body_reads() const noexcept { return body_reads_; }
    LruCache<std::string, std::string>::Stats cache_stats() const { return bodies_.stats(); }
    const DepGraph&   graph() const noexcept { return *graph_; }
    const SkillIndex& index() const noexcept { return *index_; }

    // Parses one SKILL.md front matter (exposed for tests).
    static bool parse_front_matter(std::string_view text, SkillMeta* out, std::string* error);

private:
    bool body(const SkillMeta& meta, std::string* out, std::string* error);

    std::vector<SkillMeta>             skills_;
    std::unique_ptr<DepGraph>          graph_;   // recreated by every scan()
    std::unique_ptr<SkillIndex>        index_;
    LruCache<std::string, std::string> bodies_;
    std::size_t                        body_reads_ = 0;
};
