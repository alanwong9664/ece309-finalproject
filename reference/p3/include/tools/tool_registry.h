#pragma once
#include "tools/tool.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string_view>
#include <vector>

class ToolRegistry {
public:
    ToolRegistry();
    ~ToolRegistry();

    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;

    bool add(std::unique_ptr<Tool> tool);

    Tool* find(std::string_view name) const;

    std::size_t size() const noexcept;

    void for_each(const std::function<void(const Tool&)>& fn) const;

    std::size_t bucket_count() const noexcept;

    double load_factor() const noexcept;

    std::size_t max_probe_length() const;

    static std::size_t hash(std::string_view key) noexcept;

private:

    struct Node {
        std::unique_ptr<Tool> tool;
        std::unique_ptr<Node> next;
    };

    void rehash(std::size_t new_bucket_count);

    std::vector<std::unique_ptr<Node>> buckets_;
    std::size_t                        count_ = 0;
};
