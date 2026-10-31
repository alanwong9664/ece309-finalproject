// include/memory/memory_store.h
//
// PROVIDED. Long-term memory that survives restarts. Built on YOUR
// MemoryIndex (the BST) and YOUR top_k (the heap).

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
    // Stores `text` under the next id (1, 2, 3, ...) and returns that id.
    std::uint64_t remember(std::string text);

    // Removes a memory. Returns false if the id does not exist.
    bool forget(std::uint64_t id);

    // Up to k memories that share at least one word with `query`, best
    // first. Score = number of distinct query words (tokenize()) that also
    // appear in the memory. Ties: the newer memory (larger id) first.
    // Uses top_k(), so it costs O(n log k), not a full sort.
    std::vector<MemoryRecord> recall(std::string_view query, std::size_t k) const;

    // Every memory, oldest (smallest id) first: an in-order walk of the BST.
    std::vector<MemoryRecord> list() const;

    std::size_t size() const noexcept { return index_.size(); }
    const MemoryIndex& index() const noexcept { return index_; }

    // Text file: "next <id>" on the first line, then one "<id>\t<text>" line
    // per memory with \\, \n, \t escaped. save() writes a temporary file and
    // renames it over `file`, so a crash mid-save never leaves a half-written
    // store behind.
    bool save(const std::filesystem::path& file, std::string* error) const;
    // Replaces the contents with the file's. On error, leaves the store
    // unchanged and returns false.
    bool load(const std::filesystem::path& file, std::string* error);

    // "m12" <-> 12. parse_id returns false for anything else.
    static std::string format_id(std::uint64_t id);
    static bool        parse_id(std::string_view text, std::uint64_t* id);

private:
    MemoryIndex   index_;
    std::uint64_t next_id_ = 1;
};
