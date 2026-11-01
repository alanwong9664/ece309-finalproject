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
    std::string              name;
    std::string              description;
    std::vector<std::string> requirements;
    std::filesystem::path    file;
};

class SkillCatalog {
public:
    explicit SkillCatalog(std::size_t body_cache_capacity = 8);

    struct ScanReport {
        std::size_t              loaded = 0;
        std::vector<std::string> errors;
    };

    ScanReport scan(const std::filesystem::path& root);

    const std::vector<SkillMeta>& skills() const noexcept { return skills_; }
    const SkillMeta* find(std::string_view name) const;

    bool load(std::string_view name, std::string* text, std::string* error);

    std::vector<const SkillMeta*> search(std::string_view query, std::size_t k) const;

    std::vector<std::string> dependents(std::string_view name) const;

    std::size_t body_reads() const noexcept { return body_reads_; }
    LruCache<std::string, std::string>::Stats cache_stats() const { return bodies_.stats(); }
    const DepGraph&   graph() const noexcept { return *graph_; }
    const SkillIndex& index() const noexcept { return *index_; }

    static bool parse_front_matter(std::string_view text, SkillMeta* out, std::string* error);

private:
    bool body(const SkillMeta& meta, std::string* out, std::string* error);

    std::vector<SkillMeta>             skills_;
    std::unique_ptr<DepGraph>          graph_;
    std::unique_ptr<SkillIndex>        index_;
    LruCache<std::string, std::string> bodies_;
    std::size_t                        body_reads_ = 0;
};
