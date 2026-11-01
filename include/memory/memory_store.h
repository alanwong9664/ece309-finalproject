#pragma once
#include "memory/memory_index.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

class MemoryStore {
public:

    std::uint64_t remember(std::string text);

    bool forget(std::uint64_t id);

    std::vector<MemoryRecord> recall(std::string_view query, std::size_t k) const;

    std::vector<MemoryRecord> list() const;

    std::size_t size() const noexcept { return index_.size(); }
    const MemoryIndex& index() const noexcept { return index_; }

    bool save(const std::filesystem::path& file, std::string* error) const;

    bool load(const std::filesystem::path& file, std::string* error);

    static std::string format_id(std::uint64_t id);
    static bool        parse_id(std::string_view text, std::uint64_t* id);

private:
    MemoryIndex   index_;
    std::uint64_t next_id_ = 1;
};
