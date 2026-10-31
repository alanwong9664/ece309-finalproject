// include/memory/memory_index.h
//
// YOU IMPLEMENT (src/memory/memory_index.cpp). Spec §4.3.
//
// A binary search tree of memory records, keyed by id. The provided
// MemoryStore uses it for remember (insert), forget (remove), and
// list_memories (in-order traversal).
//
// Rules: write the BST yourself with Node pointers. Do NOT use std::map,
// std::set, std::unordered_map, or any other associative container, and do
// not keep the records in a sorted std::vector. The tree does not need to be
// balanced (balancing it as an AVL tree is the bonus, spec §9).
//
// The PUBLIC interface is fixed. The PRIVATE section is yours.

#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

struct MemoryRecord {
    std::uint64_t id = 0;     // unique key; shown to the model as "m<id>"
    std::string   text;
};

class MemoryIndex {
public:
    MemoryIndex() = default;
    ~MemoryIndex();                       // frees every node

    MemoryIndex(const MemoryIndex&) = delete;
    MemoryIndex& operator=(const MemoryIndex&) = delete;

    // Adds `rec`. Returns false (and changes nothing) if a record with the
    // same id is already stored.
    bool insert(MemoryRecord rec);

    // The record with this id, or nullptr. The pointer is valid until the
    // next insert/remove/clear.
    const MemoryRecord* find(std::uint64_t id) const;

    // Removes the record with this id. Returns false if there is none.
    // Handles all three cases: leaf, one child, two children.
    bool remove(std::uint64_t id);

    std::size_t size() const noexcept;

    // Number of nodes on the longest root-to-leaf path: 0 for an empty
    // tree, 1 for a single node.
    std::size_t height() const;

    // Calls fn on every record in ascending id order.
    void for_each_in_order(const std::function<void(const MemoryRecord&)>& fn) const;

    // Removes every record.
    void clear();

private:
    // ===== YOUR REPRESENTATION GOES HERE =====
    // Suggested: a Node struct holding a MemoryRecord and left/right
    // pointers, a root pointer, and a node count (below), plus helpers that
    // you declare here and define in memory_index.cpp, for example:
    //
    //   static void        destroy(Node* n);              // post-order delete
    //   static std::size_t height_of(const Node* n);
    //   static void        in_order(const Node* n,
    //                               const std::function<void(const MemoryRecord&)>& fn);
    //   static Node*       remove_node(Node* n, std::uint64_t id, bool* removed);
    //       // returns the new root of this subtree, so the caller writes
    //       //   root_ = remove_node(root_, id, &removed);
    //
    // Clue: a static helper cannot touch count_. Change count_ in the public
    // insert()/remove() instead, exactly once per success.
    // Clue: in the .cpp, a helper that RETURNS Node* must be written
    //   MemoryIndex::Node* MemoryIndex::remove_node(...)
    // (plain "Node*" there gives "'Node' does not name a type").
    struct Node {
        MemoryRecord rec;
        Node*        left = nullptr;
        Node*        right = nullptr;
    };

    Node*       root_ = nullptr;
    std::size_t count_ = 0;
};
