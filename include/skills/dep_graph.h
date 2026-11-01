#pragma once
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

class DepGraph {
public:

    bool add_node(const std::string& name);

    bool add_edge(const std::string& from, const std::string& to);

    bool        has_node(const std::string& name) const;
    std::size_t node_count() const noexcept;

    const std::vector<std::string>& requirements(const std::string& name) const;

    bool order_from(const std::string& root, std::vector<std::string>* out,
                    std::string* error) const;

    std::vector<std::string> dependents(const std::string& name) const;

private:

    std::unordered_map<std::string, std::vector<std::string>> requires_;
    std::unordered_map<std::string, std::vector<std::string>> required_by_;

};
