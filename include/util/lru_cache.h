#pragma once
#include <cstddef>
#include <functional>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class LruCache {
public:
    struct Stats {
        std::size_t hits = 0;
        std::size_t misses = 0;
        std::size_t evictions = 0;
    };
    explicit LruCache(std::size_t capacity) : capacity_(capacity) {}
    ~LruCache() { clear(); }
    LruCache(const LruCache&) = delete;
    LruCache& operator=(const LruCache&) = delete;

    const Value* get(const Key& key) {
        const auto it = map_.find(key);
        if (it == map_.end()) {
            ++stats_.misses;
            return nullptr;
        }
        ++stats_.hits;
        move_to_front(it->second);
        return &it->second->value;
    }
    void put(const Key& key, Value value) {
        if (capacity_ == 0) return;
        const auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->value = std::move(value);
            move_to_front(it->second);
            return;
        }
        // Guard ownership until the hash table has accepted the new entry.
        auto node = std::make_unique<Node>(Node{key, std::move(value), nullptr, nullptr});
        map_.emplace(node->key, node.get());
        push_front(node.release());
        if (map_.size() > capacity_) {
            Node* victim = tail_;
            map_.erase(victim->key);
            unlink(victim);
            delete victim;
            ++stats_.evictions;
        }
    }
    bool contains(const Key& key) const { return map_.find(key) != map_.end(); }
    bool erase(const Key& key) {
        const auto it = map_.find(key);
        if (it == map_.end()) return false;
        Node* node = it->second;
        map_.erase(it);
        unlink(node);
        delete node;
        return true;
    }
    void clear() {
        while (head_) {
            Node* next = head_->next;
            delete head_;
            head_ = next;
        }
        tail_ = nullptr;
        map_.clear();
    }
    std::size_t size() const noexcept { return map_.size(); }
    std::size_t capacity() const noexcept { return capacity_; }
    Stats stats() const noexcept { return stats_; }
    std::vector<Key> keys_mru_to_lru() const {
        std::vector<Key> result;
        for (Node* node = head_; node; node = node->next) result.push_back(node->key);
        return result;
    }
private:
    struct Node { Key key; Value value; Node* prev; Node* next; };
    void unlink(Node* node) {
        if (node->prev) node->prev->next = node->next;
        else head_ = node->next;
        if (node->next) node->next->prev = node->prev;
        else tail_ = node->prev;
    }
    void push_front(Node* node) {
        node->prev = nullptr;
        node->next = head_;
        if (head_) head_->prev = node;
        else tail_ = node;
        head_ = node;
    }
    void move_to_front(Node* node) {
        if (node == head_) return;
        unlink(node);
        push_front(node);
    }
    std::unordered_map<Key, Node*, Hash> map_;
    Node* head_ = nullptr;
    Node* tail_ = nullptr;
    std::size_t capacity_;
    Stats stats_;
};
