// include/skills/dep_graph.h
//
// YOU IMPLEMENT (src/skills/dep_graph.cpp). Spec §6.2.
//
// A directed graph of skill names. An edge from -> to means "from REQUIRES
// to" (to must be loaded before from). The provided SkillCatalog adds one
// node per skill and one edge per "requires:" entry.
//
// Any STL container is allowed for the adjacency lists and the visited /
// color marks (std::unordered_map, std::vector, std::queue, ...). The
// traversals themselves (DFS, BFS) are yours: no graph library.
//
// The PUBLIC interface is fixed. The PRIVATE section is yours.

#pragma once
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

class DepGraph {
public:
    // Adds a node. Returns false if it already exists.
    bool add_node(const std::string& name);

    // Adds the edge from -> to ("from requires to"). Returns false (and adds
    // nothing) if `from` is not a node, or the same edge already exists.
    // `to` does NOT need to be a node: a requirement on a skill that does
    // not exist is allowed here and reported later by order_from().
    bool add_edge(const std::string& from, const std::string& to);

    bool        has_node(const std::string& name) const;
    std::size_t node_count() const noexcept;

    // The direct requirements of `name`, in the order the edges were added.
    // Empty for an unknown name.
    const std::vector<std::string>& requirements(const std::string& name) const;

    // DEPTH-FIRST SEARCH. The load order for `root`: root and everything it
    // needs, directly or indirectly, each exactly once, every skill AFTER all
    // of its requirements (so root is last). Visit requirements in the order
    // the edges were added; the output is the DFS post-order.
    //
    // On success: returns true, *out = the order, *error untouched.
    // On failure: returns false, *error = exactly one of
    //   "unknown: <root>"                  root is not a node
    //   "missing: <x> (required by <y>)"   the DFS reached an edge y -> x
    //                                      and x is not a node
    //   "cycle: <a> -> <b> -> ... -> <a>"  the DFS reached an edge to a node
    //                                      that is still on the current DFS
    //                                      path; list that path from that
    //                                      node, then the node again
    // reporting the FIRST problem the DFS meets. *out is unspecified.
    // Example: a requires b, b requires c, c requires a; order_from("a")
    // fails with "cycle: a -> b -> c -> a".
    bool order_from(const std::string& root, std::vector<std::string>* out,
                    std::string* error) const;

    // BREADTH-FIRST SEARCH over the edges REVERSED. Every skill that
    // requires `name` directly or indirectly ("what breaks if I delete
    // name?"), nearest first: sorted by BFS distance, and alphabetically
    // within the same distance. Does not include `name` itself. Works even
    // if `name` is not a node (a missing requirement) or the graph has
    // cycles. Empty if nothing depends on it.
    std::vector<std::string> dependents(const std::string& name) const;

private:
    // ===== YOUR REPRESENTATION GOES HERE =====
    // A suggested start (change it if you like):
    std::unordered_map<std::string, std::vector<std::string>> requires_;     // node -> requirements
    std::unordered_map<std::string, std::vector<std::string>> required_by_;  // target -> nodes needing it
    // Clue: if you keep required_by_ for the BFS, add_edge must fill it too.
    // Clue: order_from() and the others are const, so requires_[name] does
    // not compile there ("discards qualifiers") - and it would insert a new
    // key anyway. Use requires_.find(name) and compare with requires_.end().
    // Clue: a recursive DFS helper needs a declaration here, e.g.
    //   bool dfs(const std::string& node, std::unordered_map<std::string, int>& color,
    //            std::vector<std::string>& path, std::vector<std::string>* out,
    //            std::string* error) const;
    // (or write it as a free function in an anonymous namespace in the .cpp).
};
