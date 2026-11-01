#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

struct MemoryRecord {
    std::uint64_t id = 0;
    std::string   text;
};

class MemoryIndex {
public:
    MemoryIndex() = default;
    ~MemoryIndex();

    MemoryIndex(const MemoryIndex&) = delete;
    MemoryIndex& operator=(const MemoryIndex&) = delete;

    bool insert(MemoryRecord rec);

    const MemoryRecord* find(std::uint64_t id) const;

    bool remove(std::uint64_t id);

    std::size_t size() const noexcept;

    std::size_t height() const;

    void for_each_in_order(const std::function<void(const MemoryRecord&)>& fn) const;

    void clear();

private:

    struct Node {
        MemoryRecord rec;
        Node*        left = nullptr;
        Node*        right = nullptr;
    };

    Node*       root_ = nullptr;
    std::size_t count_ = 0;
};
