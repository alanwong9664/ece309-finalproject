#pragma once
#include <algorithm>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

template <typename T, typename Less = std::less<T>>
class MinHeap {
public:
    explicit MinHeap(Less less = Less()) : less_(std::move(less)) {}
    void push(T value) {
        data_.push_back(std::move(value));
        std::size_t i = data_.size() - 1;
        while (i > 0) {
            const std::size_t parent = (i - 1) / 2;
            if (!less_(data_[i], data_[parent])) break;
            std::swap(data_[i], data_[parent]);
            i = parent;
        }
    }
    const T& top() const {
        if (empty()) throw std::out_of_range("MinHeap::top on empty heap");
        return data_.front();
    }
    T pop() {
        if (empty()) throw std::out_of_range("MinHeap::pop on empty heap");
        T result = std::move(data_.front());
        if (data_.size() == 1) {
            data_.pop_back();
            return result;
        }
        data_.front() = std::move(data_.back());
        data_.pop_back();
        std::size_t i = 0;
        while (i < data_.size() / 2) {
            std::size_t child = 2 * i + 1;
            if (child + 1 < data_.size() && less_(data_[child + 1], data_[child])) ++child;
            if (!less_(data_[child], data_[i])) break;
            std::swap(data_[child], data_[i]);
            i = child;
        }
        return result;
    }
    std::size_t size() const noexcept { return data_.size(); }
    bool empty() const noexcept { return data_.empty(); }
    void clear() noexcept { data_.clear(); }
private:
    std::vector<T> data_;
    Less less_;
};

template <typename T, typename Less = std::less<T>>
std::vector<T> top_k(const std::vector<T>& items, std::size_t k, Less less = Less()) {
    if (k == 0 || items.empty()) return {};
    MinHeap<T, Less> heap(less);
    for (const T& item : items) {
        if (heap.size() < k) heap.push(item);
        else if (less(heap.top(), item)) {
            heap.pop();
            heap.push(item);
        }
    }
    std::vector<T> result;
    result.reserve(heap.size());
    while (!heap.empty()) result.push_back(heap.pop());
    std::reverse(result.begin(), result.end());
    return result;
}
