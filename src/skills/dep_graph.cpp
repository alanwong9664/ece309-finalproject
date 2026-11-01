#include "skills/dep_graph.h"
#include <algorithm>
#include <unordered_set>
#include <utility>

bool DepGraph::add_node(const std::string& name) {
    return requires_.emplace(name, std::vector<std::string>{}).second;
}
bool DepGraph::add_edge(const std::string& from, const std::string& to) {
    const auto it = requires_.find(from);
    if (it == requires_.end()) return false;
    auto& edges = it->second;
    if (std::find(edges.begin(), edges.end(), to) != edges.end()) return false;
    edges.push_back(to);
    required_by_[to].push_back(from);
    return true;
}
bool DepGraph::has_node(const std::string& name) const { return requires_.find(name) != requires_.end(); }
std::size_t DepGraph::node_count() const noexcept { return requires_.size(); }
const std::vector<std::string>& DepGraph::requirements(const std::string& name) const {
    static const std::vector<std::string> empty;
    const auto it = requires_.find(name);
    return it == requires_.end() ? empty : it->second;
}
bool DepGraph::order_from(const std::string& root, std::vector<std::string>* out,
                          std::string* error) const {
    out->clear();
    if (!has_node(root)) {
        *error = "unknown: " + root;
        return false;
    }

    struct Frame { std::string name; std::size_t next; };
    std::vector<Frame> path{{root, 0}};
    std::unordered_map<std::string, int> color;
    color[root] = 1;
    while (!path.empty()) {
        Frame& frame = path.back();
        const auto& edges = requirements(frame.name);
        if (frame.next == edges.size()) {
            color[frame.name] = 2;
            out->push_back(frame.name);
            path.pop_back();
            continue;
        }
        const std::string& target = edges[frame.next++];
        if (!has_node(target)) {
            *error = "missing: " + target + " (required by " + frame.name + ")";
            return false;
        }
        if (color[target] == 1) {
            *error = "cycle: ";
            auto first = std::find_if(path.begin(), path.end(), [&](const Frame& f) { return f.name == target; });
            for (auto it = first; it != path.end(); ++it) *error += it->name + " -> ";
            *error += target;
            return false;
        }
        if (color[target] == 0) {
            color[target] = 1;
            path.push_back({target, 0});
        }
    }
    return true;
}
std::vector<std::string> DepGraph::dependents(const std::string& name) const {
    std::unordered_set<std::string> seen{name};
    std::vector<std::string> level{name}, result;
    while (!level.empty()) {
        std::vector<std::string> next;
        for (const auto& node : level) {
            const auto it = required_by_.find(node);
            if (it == required_by_.end()) continue;
            for (const auto& dependent : it->second)
                if (seen.insert(dependent).second) next.push_back(dependent);
        }
        std::sort(next.begin(), next.end());
        result.insert(result.end(), next.begin(), next.end());
        level = std::move(next);
    }
    return result;
}
