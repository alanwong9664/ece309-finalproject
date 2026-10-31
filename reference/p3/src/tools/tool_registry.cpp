// src/tools/tool_registry.cpp — P3 instructor reference (used only with -DUSE_P3_REFERENCE=ON).
//
// Separate chaining. Buckets are a vector of owning singly-linked lists.
//   hash        : 64-bit FNV-1a
//   bucket count: power of two (index = hash & (n - 1)), starts at 8
//   resize      : double the bucket count when an insert would push the
//                 load factor above 0.75
// Nodes are moved (not copied) between buckets on rehash, so Tool* handed
// out by find() stay valid.

#include "tools/tool_registry.h"

#include <cstdint>
#include <utility>

namespace {
constexpr std::size_t kInitialBuckets = 8;
constexpr double      kMaxLoadFactor  = 0.75;
}  // namespace

ToolRegistry::ToolRegistry() : buckets_(kInitialBuckets) {}

ToolRegistry::~ToolRegistry() {
    // Unlink chains iteratively so a long chain cannot overflow the stack
    // through recursive unique_ptr<Node> destructors.
    for (auto& head : buckets_) {
        while (head) head = std::move(head->next);
    }
}

std::size_t ToolRegistry::hash(std::string_view key) noexcept {
    std::uint64_t h = 1469598103934665603ULL;          // FNV offset basis
    for (unsigned char c : key) {
        h ^= c;
        h *= 1099511628211ULL;                          // FNV prime
    }
    return static_cast<std::size_t>(h);
}

void ToolRegistry::rehash(std::size_t new_bucket_count) {
    std::vector<std::unique_ptr<Node>> fresh(new_bucket_count);
    for (auto& head : buckets_) {
        while (head) {
            std::unique_ptr<Node> node = std::move(head);
            head = std::move(node->next);
            std::size_t idx = hash(node->tool->name()) & (new_bucket_count - 1);
            node->next = std::move(fresh[idx]);
            fresh[idx] = std::move(node);
        }
    }
    buckets_ = std::move(fresh);
}

bool ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (!tool) return false;
    const std::string name = tool->name();
    if (find(name) != nullptr) return false;

    if (static_cast<double>(count_ + 1) > kMaxLoadFactor * static_cast<double>(buckets_.size()))
        rehash(buckets_.size() * 2);

    std::size_t idx = hash(name) & (buckets_.size() - 1);
    auto node = std::make_unique<Node>();
    node->tool = std::move(tool);
    node->next = std::move(buckets_[idx]);
    buckets_[idx] = std::move(node);
    ++count_;
    return true;
}

Tool* ToolRegistry::find(std::string_view name) const {
    if (buckets_.empty()) return nullptr;
    std::size_t idx = hash(name) & (buckets_.size() - 1);
    for (const Node* n = buckets_[idx].get(); n != nullptr; n = n->next.get()) {
        if (n->tool->name() == name) return n->tool.get();
    }
    return nullptr;
}

std::size_t ToolRegistry::size() const noexcept { return count_; }

void ToolRegistry::for_each(const std::function<void(const Tool&)>& fn) const {
    for (const auto& head : buckets_)
        for (const Node* n = head.get(); n != nullptr; n = n->next.get())
            fn(*n->tool);
}

std::size_t ToolRegistry::bucket_count() const noexcept { return buckets_.size(); }

double ToolRegistry::load_factor() const noexcept {
    return buckets_.empty() ? 0.0
                            : static_cast<double>(count_) / static_cast<double>(buckets_.size());
}

std::size_t ToolRegistry::max_probe_length() const {
    std::size_t longest = 0;
    for (const auto& head : buckets_) {
        std::size_t len = 0;
        for (const Node* n = head.get(); n != nullptr; n = n->next.get()) ++len;
        if (len > longest) longest = len;
    }
    return longest;
}
