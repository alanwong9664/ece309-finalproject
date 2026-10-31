#include "memory/memory_index.h"
#include <algorithm>
#include <utility>
#include <vector>

MemoryIndex::~MemoryIndex() { clear(); }
bool MemoryIndex::insert(MemoryRecord rec) {
    Node** link = &root_;
    while (*link) {
        if (rec.id == (*link)->rec.id) return false;
        link = rec.id < (*link)->rec.id ? &(*link)->left : &(*link)->right;
    }
    *link = new Node{std::move(rec), nullptr, nullptr};
    ++count_;
    return true;
}
const MemoryRecord* MemoryIndex::find(std::uint64_t id) const {
    const Node* node = root_;
    while (node) {
        if (id == node->rec.id) return &node->rec;
        node = id < node->rec.id ? node->left : node->right;
    }
    return nullptr;
}
bool MemoryIndex::remove(std::uint64_t id) {
    Node** link = &root_;
    while (*link && (*link)->rec.id != id)
        link = id < (*link)->rec.id ? &(*link)->left : &(*link)->right;
    if (!*link) return false;
    Node* victim = *link;
    if (victim->left && victim->right) {
        Node** successor = &victim->right;
        while ((*successor)->left) successor = &(*successor)->left;
        victim->rec = std::move((*successor)->rec);
        victim = *successor;
        *successor = victim->right;
    } else *link = victim->left ? victim->left : victim->right;
    delete victim;
    --count_;
    return true;
}
std::size_t MemoryIndex::size() const noexcept { return count_; }
std::size_t MemoryIndex::height() const {
    if (!root_) return 0;
    std::vector<std::pair<const Node*, std::size_t>> pending{{root_, 1}};
    std::size_t result = 0;
    while (!pending.empty()) {
        const auto current = pending.back();
        pending.pop_back();
        result = std::max(result, current.second);
        if (current.first->left) pending.push_back({current.first->left, current.second + 1});
        if (current.first->right) pending.push_back({current.first->right, current.second + 1});
    }
    return result;
}
void MemoryIndex::for_each_in_order(const std::function<void(const MemoryRecord&)>& fn) const {
    std::vector<const Node*> path;
    const Node* node = root_;
    while (node || !path.empty()) {
        while (node) {
            path.push_back(node);
            node = node->left;
        }
        node = path.back();
        path.pop_back();
        fn(node->rec);
        node = node->right;
    }
}
void MemoryIndex::clear() {
    // Rotate left children upward, then delete roots with no left child.
    // Destruction needs no recursion or allocation, even for a deep tree.
    while (root_) {
        if (root_->left) {
            Node* left = root_->left;
            root_->left = left->right;
            left->right = root_;
            root_ = left;
        } else {
            Node* next = root_->right;
            delete root_;
            root_ = next;
        }
    }
    count_ = 0;
}
