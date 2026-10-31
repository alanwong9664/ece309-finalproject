// tests/final/test_final.cpp — YOUR TEST SUITE (spec §8).
//
// You need at least 20 tests. The 8 examples below are given and count:
//   Memory  (4): heap, lru, bst, ctx
//   Policy  (2): pol x2
//   Skills  (2): graph, index
// Write at least 12 more of your own, covering the list in spec §8.
// Name tests <area>_<what> with the areas heap, lru, bst, ctx, pol, graph,
// index, so the summary groups them.
//
//   ./build/test_final              run everything
//   ./build/test_final lru_         only tests whose names start with lru_
//   ./build/test_final =lru_evicts_least_recently_used    one test, exactly
//   ./build/test_final --list       list test names
//
// If a sanitizer error (ASan/UBSan) aborts the run, the tests after it do
// not run: rerun the failing test alone with =its_name and read the FIRST
// stack frame that is in your own file.
//
// CHECK_EQ(a, b) prints both values when it fails (vectors too), so prefer
// it over CHECK(a == b).
//
// Helpers you can use (tests/test_helpers.h): TempDir, write_text,
// write_skill, FixedPolicy, ScriptedConfirmer, LinesInput, StringOutput,
// parse_ok, args1. ManualClock is in policy/policy.h.

#include "../mini_test.h"
#include "../test_helpers.h"

#include "memory/context_builder.h"
#include "memory/memory_index.h"
#include "memory/memory_store.h"
#include "policy/policies.h"
#include "policy/policy_chain.h"
#include "skills/dep_graph.h"
#include "skills/skill_catalog.h"
#include "skills/skill_index.h"
#include "util/lru_cache.h"
#include "util/min_heap.h"

#include <cstdint>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <limits>
#include <map>
#include <memory>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// =========================================================================
// Memory — 4 examples
// =========================================================================

// Example 1 (heap): items come back smallest first, duplicates included.
TEST(heap_pops_in_order) {
    MinHeap<int> h;
    for (int x : {5, 1, 4, 1, 3}) h.push(x);
    CHECK_EQ(h.size(), std::size_t{5});
    CHECK_EQ(h.top(), 1);
    std::vector<int> got;
    while (!h.empty()) got.push_back(h.pop());
    CHECK_EQ(got, (std::vector<int>{1, 1, 3, 4, 5}));
}

// Example 2 (lru): the least recently USED entry is evicted, and get()
// counts as a use.
TEST(lru_evicts_least_recently_used) {
    LruCache<std::string, int> c(2);
    c.put("a", 1);
    c.put("b", 2);
    REQUIRE(c.get("a") != nullptr);   // "a" is now the most recent
    c.put("c", 3);                    // full: "b" must go
    CHECK(c.contains("a"));
    CHECK(!c.contains("b"));
    CHECK_EQ(c.keys_mru_to_lru(), (std::vector<std::string>{"c", "a"}));
    CHECK_EQ(c.stats().evictions, std::size_t{1});
}

// Example 3 (bst): removing a node with two children keeps the tree sorted,
// keeps every other record, and lowers size() by exactly one.
TEST(bst_remove_two_children) {
    MemoryIndex t;
    for (std::uint64_t id : {50, 30, 70, 60, 80}) REQUIRE(t.insert(MemoryRecord{id, "m" + std::to_string(id)}));
    REQUIRE(t.remove(70));
    std::vector<std::uint64_t> ids;
    t.for_each_in_order([&](const MemoryRecord& r) { ids.push_back(r.id); });
    CHECK_EQ(ids, (std::vector<std::uint64_t>{30, 50, 60, 80}));
    CHECK(t.find(70) == nullptr);
    REQUIRE(t.find(80) != nullptr);
    CHECK_EQ(t.find(80)->text, std::string("m80"));
    CHECK_EQ(t.size(), std::size_t{4});
}

// Example 4 (ctx): the worked example in spec §4.4.
TEST(ctx_spec_example) {
    std::vector<ContextItem> items{
        {Role::System, 10, false},     // 0  protected
        {Role::User, 10, false},       // 1
        {Role::Assistant, 10, false},  // 2  dropped 3rd (oldest assistant)
        {Role::Tool, 10, false},       // 3  dropped 1st
        {Role::Assistant, 10, false},  // 4
        {Role::Tool, 10, false},       // 5  dropped 2nd
        {Role::User, 10, false},       // 6  recent: protected
        {Role::Assistant, 10, false}}; // 7  recent: protected
    ContextPlan p = select_context(items, 50, 2);
    CHECK_EQ(p.keep, (std::vector<std::size_t>{0, 1, 4, 6, 7}));
    CHECK_EQ(p.total_tokens, std::size_t{50});
    CHECK(!p.over_budget);
}

// =========================================================================
// Policy — 2 examples
// =========================================================================

// Example 5 (pol): the sliding window. A request exactly `window` seconds
// after an allowed one no longer counts it.
TEST(pol_rate_limit_window_boundary) {
    ManualClock clock(100);
    RateLimitPolicy p(2, 10.0, clock);   // at most 2 per 10 s
    CommandRequest req;
    req.program = "ls";
    CHECK(p.evaluate(req).kind == Decision::Kind::Allow);   // t = 100
    clock.advance(1);
    CHECK(p.evaluate(req).kind == Decision::Kind::Allow);   // t = 101
    clock.advance(1);
    CHECK(p.evaluate(req).kind == Decision::Kind::Deny);    // t = 102: two in the window
    clock.advance(8);
    CHECK(p.evaluate(req).kind == Decision::Kind::Allow);   // t = 110: 100 has left (110 - 100 = 10)
    CHECK(p.tracked() <= 2u);
}

// Example 6 (pol): first Deny wins and later policies are not asked; with no
// Deny, the FIRST Ask is returned unchanged.
TEST(pol_chain_first_deny_stops_first_ask_wins) {
    auto ask1 = std::make_unique<FixedPolicy>(Decision::Kind::Ask, "ask1");
    auto ask2 = std::make_unique<FixedPolicy>(Decision::Kind::Ask, "ask2");
    PolicyChain chain;
    REQUIRE(chain.add(std::make_unique<FixedPolicy>(Decision::Kind::Allow, "ok")));
    REQUIRE(chain.add(std::move(ask1)));
    REQUIRE(chain.add(std::move(ask2)));
    CommandRequest req;
    req.program = "git";
    Decision d = chain.evaluate(req);
    CHECK(d.kind == Decision::Kind::Ask);
    CHECK_EQ(d.policy, std::string("ask1"));

    auto deny = std::make_unique<FixedPolicy>(Decision::Kind::Deny, "no");
    auto after = std::make_unique<FixedPolicy>(Decision::Kind::Allow, "after");
    FixedPolicy* after_ptr = after.get();   // still valid: the chain owns it now
    REQUIRE(chain.add(std::move(deny)));
    REQUIRE(chain.add(std::move(after)));
    d = chain.evaluate(req);
    CHECK(d.kind == Decision::Kind::Deny);
    CHECK_EQ(d.policy, std::string("no"));
    CHECK_EQ(after_ptr->calls, 0);     // never evaluated: the chain stopped at the Deny
}

// =========================================================================
// Skills — 2 examples
// =========================================================================

// Example 7 (graph): post-order load order, and a cycle message that lists
// only the nodes still on the DFS path (b is already finished).
TEST(graph_order_and_cycle_message) {
    DepGraph g;
    for (const char* n : {"a", "b", "c", "d"}) g.add_node(n);
    g.add_edge("a", "b");   // a requires b
    g.add_edge("a", "c");
    g.add_edge("b", "d");
    g.add_edge("c", "d");   // diamond: d is reached twice, but it is not a cycle
    std::vector<std::string> order;
    std::string error;
    REQUIRE(g.order_from("a", &order, &error));
    CHECK_EQ(order, (std::vector<std::string>{"d", "b", "c", "a"}));

    g.add_edge("c", "a");   // now a -> c -> a is a cycle
    CHECK(!g.order_from("a", &order, &error));
    CHECK_EQ(error, std::string("cycle: a -> c -> a"));
}

// Example 8 (index): rank by number of distinct query words; ties go to the
// smaller doc id.
TEST(index_ranking_and_ties) {
    SkillIndex ix;
    ix.add(0, "format numbers");
    ix.add(1, "csv report with table and numbers");
    ix.add(2, "table format");
    ix.add(3, "unrelated");
    CHECK_EQ(ix.search("csv table numbers", 10), (std::vector<std::size_t>{1, 0, 2}));
    CHECK_EQ(ix.search("format table", 1), (std::vector<std::size_t>{2}));
    REQUIRE(ix.postings("numbers") != nullptr);
    CHECK_EQ(*ix.postings("numbers"), (std::vector<std::size_t>{0, 1}));
}

// =========================================================================
// Your tests (at least 12 more). Spec §8 lists what they must cover.
// =========================================================================

namespace {
using BenchClock = std::chrono::steady_clock;
double elapsed_ms(BenchClock::time_point start) {
    return std::chrono::duration<double, std::milli>(BenchClock::now() - start).count();
}
std::vector<std::uint64_t> tree_ids(const MemoryIndex& tree) {
    std::vector<std::uint64_t> ids;
    tree.for_each_in_order([&](const MemoryRecord& r) { ids.push_back(r.id); });
    return ids;
}
CommandRequest command(std::string program, std::vector<std::string> args = {}) {
    return {std::move(program), std::move(args), "test"};
}
}

TEST(heap_empty_single_clear) {
    MinHeap<int> heap;
    CHECK_THROWS(heap.top(), std::out_of_range);
    CHECK_THROWS(heap.pop(), std::out_of_range);
    heap.push(7);
    CHECK_EQ(heap.top(), 7);
    CHECK_EQ(heap.pop(), 7);
    CHECK(heap.empty());
    heap.push(2);
    heap.push(1);
    heap.clear();
    CHECK(heap.empty());
    CHECK_THROWS(heap.pop(), std::out_of_range);
    heap.push(-3);
    CHECK_EQ(heap.pop(), -3);
}

TEST(heap_custom_comparators) {
    MinHeap<int, std::greater<int>> maximum;
    for (int x : {2, 9, 1, 9, 5, 4}) maximum.push(x);
    std::vector<int> got;
    while (!maximum.empty()) got.push_back(maximum.pop());
    CHECK_EQ(got, (std::vector<int>{9, 9, 5, 4, 2, 1}));
    struct Item { int score; };
    const int direction = 1;
    const auto less = [direction](const Item& a, const Item& b) { return direction * a.score < direction * b.score; };
    MinHeap<Item, decltype(less)> heap(less);
    heap.push({3}); heap.push({1}); heap.push({2});
    CHECK_EQ(heap.pop().score, 1);
    const auto best = top_k(std::vector<Item>{{3}, {1}, {2}}, 2, less);
    REQUIRE(best.size() == 2);
    CHECK_EQ(best[0].score, 3);
    CHECK_EQ(best[1].score, 2);
    CHECK_EQ(top_k(std::vector<int>{3, 1, 2}, 2, std::greater<int>{}), (std::vector<int>{1, 2}));
}

TEST(heap_top_k_edges_random_and_benchmark) {
    CHECK(top_k(std::vector<int>{}, 3).empty());
    CHECK(top_k(std::vector<int>{1, 2}, 0).empty());
    CHECK_EQ(top_k(std::vector<int>{2, 1, 2}, std::numeric_limits<std::size_t>::max()),
             (std::vector<int>{2, 2, 1}));
    std::mt19937 random(309);
    for (int trial = 0; trial < 100; ++trial) {
        std::vector<int> values(static_cast<std::size_t>(trial));
        for (int& x : values) x = static_cast<int>(random() % 31) - 15;
        auto sorted = values;
        std::sort(sorted.begin(), sorted.end(), std::greater<int>{});
        const std::size_t k = random() % 120;
        sorted.resize(std::min(k, sorted.size()));
        CHECK_EQ(top_k(values, k), sorted);
    }
    std::vector<int> data(100000);
    for (int& x : data) x = static_cast<int>(random() % 1000000);
    auto sorted = data;
    auto start = BenchClock::now();
    const auto best = top_k(data, 10);
    const double heap_ms = elapsed_ms(start);
    start = BenchClock::now();
    std::sort(sorted.begin(), sorted.end(), std::greater<int>{});
    const double sort_ms = elapsed_ms(start);
    sorted.resize(10);
    CHECK_EQ(best, sorted);
    std::printf("BENCH heap n=100000 k=10 top_k_ms=%.6f sort_ms=%.6f\n", heap_ms, sort_ms);
}

TEST(heap_top_k_comparison_bound) {
    std::vector<int> data(100000);
    std::iota(data.begin(), data.end(), 0);
    std::size_t comparisons = 0;
    const auto less = [&comparisons](int a, int b) { ++comparisons; return a < b; };
    const auto best = top_k(data, 10, less);
    REQUIRE(best.size() == 10);
    CHECK_EQ(best.front(), 99999);
    CHECK_EQ(best.back(), 99990);
    CHECK(comparisons < data.size() * 12);
}

TEST(lru_capacity_zero_one_stats) {
    LruCache<int, std::string> zero(0);
    zero.put(1, "ignored");
    CHECK_EQ(zero.capacity(), std::size_t{0});
    CHECK_EQ(zero.size(), std::size_t{0});
    CHECK(zero.get(1) == nullptr);
    CHECK(!zero.erase(1));
    CHECK_EQ(zero.stats().misses, std::size_t{1});
    CHECK_EQ(zero.stats().evictions, std::size_t{0});
    LruCache<int, std::string> one(1);
    one.put(1, "a");
    one.put(1, "updated");
    REQUIRE(one.get(1));
    CHECK_EQ(*one.get(1), std::string("updated"));
    CHECK_EQ(one.stats().evictions, std::size_t{0});
    const auto before = one.stats();
    CHECK(one.contains(1));
    CHECK_EQ(one.stats().hits, before.hits);
    one.put(2, "b");
    CHECK(!one.contains(1));
    CHECK_EQ(one.stats().evictions, std::size_t{1});
    CHECK(one.erase(2));
    CHECK(one.keys_mru_to_lru().empty());
    CHECK_EQ(one.stats().evictions, std::size_t{1});
}

TEST(lru_erase_positions_and_clear) {
    LruCache<int, int> cache(4);
    for (int key : {1, 2, 3, 4}) cache.put(key, key * 10);
    CHECK(cache.erase(4)); // head
    CHECK_EQ(cache.keys_mru_to_lru(), (std::vector<int>{3, 2, 1}));
    CHECK(cache.erase(2)); // middle
    CHECK_EQ(cache.keys_mru_to_lru(), (std::vector<int>{3, 1}));
    CHECK(cache.erase(1)); // tail
    CHECK_EQ(cache.keys_mru_to_lru(), (std::vector<int>{3}));
    CHECK(!cache.erase(99));
    REQUIRE(cache.get(3));
    CHECK(cache.get(99) == nullptr);
    cache.clear();
    cache.clear();
    CHECK_EQ(cache.size(), std::size_t{0});
    CHECK_EQ(cache.stats().hits, std::size_t{1});
    CHECK_EQ(cache.stats().misses, std::size_t{1});
    CHECK_EQ(cache.stats().evictions, std::size_t{0});
    cache.put(8, 80);
    CHECK_EQ(cache.keys_mru_to_lru(), (std::vector<int>{8}));
}

TEST(lru_randomized_reference) {
    LruCache<int, int> cache(7);
    std::vector<std::pair<int, int>> model; // MRU first; a deliberately simple reference
    std::mt19937 random(310);
    std::size_t hits = 0, misses = 0, evictions = 0;
    for (int step = 0; step < 20000; ++step) {
        const int key = static_cast<int>(random() % 13);
        const int op = static_cast<int>(random() % 4);
        auto it = std::find_if(model.begin(), model.end(), [key](const auto& item) { return item.first == key; });
        if (op == 0) {
            const int* found = cache.get(key);
            if (it == model.end()) { CHECK(found == nullptr); ++misses; }
            else {
                REQUIRE(found);
                CHECK_EQ(*found, it->second);
                const auto item = *it;
                model.erase(it); model.insert(model.begin(), item); ++hits;
            }
        } else if (op == 1) {
            cache.put(key, step);
            if (it != model.end()) model.erase(it);
            else if (model.size() == 7) { model.pop_back(); ++evictions; }
            model.insert(model.begin(), {key, step});
        } else if (op == 2) {
            CHECK_EQ(cache.erase(key), it != model.end());
            if (it != model.end()) model.erase(it);
        } else CHECK_EQ(cache.contains(key), it != model.end());
        std::vector<int> expected;
        for (const auto& item : model) expected.push_back(item.first);
        CHECK_EQ(cache.keys_mru_to_lru(), expected);
        CHECK_EQ(cache.size(), model.size());
    }
    CHECK_EQ(cache.stats().hits, hits);
    CHECK_EQ(cache.stats().misses, misses);
    CHECK_EQ(cache.stats().evictions, evictions);
}

TEST(lru_move_only_values_and_large_workload) {
    LruCache<std::string, std::unique_ptr<int>> owning(2);
    owning.put("a", std::make_unique<int>(42));
    REQUIRE(owning.get("a"));
    CHECK_EQ(**owning.get("a"), 42);
    owning.put("a", std::make_unique<int>(7));
    CHECK_EQ(**owning.get("a"), 7);
    LruCache<int, int> cache(1000);
    for (int i = 0; i < 300000; ++i) {
        cache.put(i, i);
        const int* value = cache.get(i);
        REQUIRE(value);
        CHECK_EQ(*value, i);
    }
    CHECK_EQ(cache.size(), std::size_t{1000});
    CHECK_EQ(cache.stats().evictions, std::size_t{299000});
}

TEST(bst_remove_cases_duplicates_height) {
    MemoryIndex tree;
    CHECK_EQ(tree.height(), std::size_t{0});
    CHECK(!tree.remove(5));
    for (std::uint64_t id : {50, 30, 70, 20, 40, 60, 80}) REQUIRE(tree.insert({id, std::to_string(id)}));
    CHECK_EQ(tree.height(), std::size_t{3});
    CHECK(!tree.insert({30, "replacement"}));
    REQUIRE(tree.find(30));
    CHECK_EQ(tree.find(30)->text, std::string("30"));
    CHECK(tree.remove(20)); // leaf
    CHECK(tree.remove(30)); // one child
    CHECK(tree.remove(50)); // root, two children
    CHECK_EQ(tree_ids(tree), (std::vector<std::uint64_t>{40, 60, 70, 80}));
    CHECK_EQ(tree.size(), std::size_t{4});
    for (std::uint64_t id : {60, 70, 80, 40}) CHECK(tree.remove(id));
    CHECK_EQ(tree.height(), std::size_t{0});
    tree.clear();
    REQUIRE(tree.insert({2, "root"}));
    REQUIRE(tree.insert({1, "left"}));
    CHECK(tree.remove(2));
    CHECK(tree.remove(1));
    REQUIRE(tree.insert({1, "root"}));
    REQUIRE(tree.insert({2, "right"}));
    CHECK(tree.remove(1));
    CHECK(tree.remove(2));
}

TEST(bst_successor_with_right_child) {
    MemoryIndex tree;
    for (std::uint64_t id : {50, 30, 80, 60, 65, 90}) REQUIRE(tree.insert({id, "record" + std::to_string(id)}));
    CHECK(tree.remove(50));
    CHECK_EQ(tree_ids(tree), (std::vector<std::uint64_t>{30, 60, 65, 80, 90}));
    REQUIRE(tree.find(60));
    CHECK_EQ(tree.find(60)->text, std::string("record60"));
    REQUIRE(tree.find(65));
    CHECK_EQ(tree.find(65)->text, std::string("record65"));
    CHECK(tree.remove(80)); // immediate-right successor
    CHECK_EQ(tree.size(), std::size_t{4});
}

TEST(bst_randomized_reference) {
    MemoryIndex tree;
    std::map<std::uint64_t, std::string> model;
    std::mt19937 random(311);
    for (int step = 0; step < 6000; ++step) {
        const std::uint64_t id = random() % 400;
        const std::string text = "value" + std::to_string(step);
        switch (random() % 3) {
            case 0: CHECK_EQ(tree.insert({id, text}), model.emplace(id, text).second); break;
            case 1: CHECK_EQ(tree.remove(id), model.erase(id) != 0); break;
            default: {
                const auto it = model.find(id);
                const auto* found = tree.find(id);
                CHECK_EQ(found != nullptr, it != model.end());
                if (found && it != model.end()) CHECK_EQ(found->text, it->second);
            }
        }
        CHECK_EQ(tree.size(), model.size());
        if (step % 20 == 0) {
            std::vector<std::uint64_t> ids;
            for (const auto& item : model) ids.push_back(item.first);
            CHECK_EQ(tree_ids(tree), ids);
        }
    }
    tree.clear(); tree.clear();
    CHECK_EQ(tree.size(), std::size_t{0});
    CHECK(tree_ids(tree).empty());
}

TEST(bst_sequential_and_shuffled_heights) {
    std::vector<std::uint64_t> ids(1000);
    std::iota(ids.begin(), ids.end(), std::uint64_t{1});
    MemoryIndex sequential, shuffled;
    for (auto id : ids) REQUIRE(sequential.insert({id, "x"}));
    std::mt19937 random(312);
    std::shuffle(ids.begin(), ids.end(), random);
    for (auto id : ids) REQUIRE(shuffled.insert({id, "x"}));
    CHECK_EQ(sequential.height(), std::size_t{1000});
    CHECK(shuffled.height() < 1000);
    CHECK_EQ(tree_ids(sequential), tree_ids(shuffled));
    std::printf("BENCH bst n=1000 sequential_height=%zu shuffled_height=%zu seed=312\n",
                sequential.height(), shuffled.height());
    MemoryIndex deep;
    for (std::uint64_t id = 1; id <= 5000; ++id) REQUIRE(deep.insert({id, "deep"}));
    CHECK_EQ(deep.height(), std::size_t{5000});
    REQUIRE(deep.find(5000));
    CHECK_EQ(tree_ids(deep).size(), std::size_t{5000});
    for (std::uint64_t id = 1; id <= 5000; ++id) REQUIRE(deep.remove(id));
    CHECK_EQ(deep.size(), std::size_t{0});
}

TEST(bst_store_persistence_and_recall) {
    TempDir temp;
    MemoryStore original;
    const auto older = original.remember("demo laptop\nwith tab\tand slash\\");
    const auto newer = original.remember("demo laptop");
    const auto found = original.recall("demo demo laptop", 2);
    REQUIRE(found.size() == 2);
    CHECK_EQ(found[0].id, newer);
    CHECK_EQ(found[1].id, older);
    std::string error;
    const auto file = temp.path() / "memories.txt";
    REQUIRE(original.save(file, &error));
    MemoryStore restored;
    REQUIRE(restored.load(file, &error));
    REQUIRE(restored.list().size() == 2);
    const auto restored_records = restored.list();
    const auto original_records = original.list();
    CHECK_EQ(restored_records[0].text, original_records[0].text);
    CHECK(restored.forget(older));
    CHECK_EQ(restored.remember("new"), newer + 1);
}

TEST(ctx_protection_zero_budget_and_exact_fit) {
    CHECK(select_context({}, 0, 100).keep.empty());
    const std::vector<ContextItem> items{{Role::System, 10, false}, {Role::Tool, 5, true}, {Role::User, 7, false}};
    const auto protected_plan = select_context(items, 0, 1);
    CHECK_EQ(protected_plan.keep, (std::vector<std::size_t>{0, 1, 2}));
    CHECK_EQ(protected_plan.total_tokens, std::size_t{22});
    CHECK(protected_plan.over_budget);
    CHECK_EQ(select_context(items, 22, 0).keep.size(), std::size_t{3});
    CHECK(!select_context(items, 22, 0).over_budget);
    CHECK_EQ(select_context(items, 1, 99).keep.size(), std::size_t{3});
    const auto zero = select_context({{Role::User, 1, false}}, 0, 0);
    CHECK(zero.keep.empty());
    CHECK(!zero.over_budget);
}

TEST(ctx_rank_before_age_and_oldest_first) {
    const std::vector<ContextItem> items{{Role::User, 5, false}, {Role::Assistant, 5, false},
        {Role::Tool, 0, false}, {Role::Tool, 5, false}, {Role::Assistant, 5, false}};
    CHECK_EQ(select_context(items, 15, 0).keep, (std::vector<std::size_t>{0, 1, 4}));
    CHECK_EQ(select_context(items, 10, 0).keep, (std::vector<std::size_t>{0, 4}));
    CHECK_EQ(select_context(items, 5, 0).keep, (std::vector<std::size_t>{0}));
}

TEST(ctx_randomized_reference) {
    std::mt19937 random(313);
    const Role roles[]{Role::System, Role::User, Role::Assistant, Role::Tool};
    for (int trial = 0; trial < 500; ++trial) {
        std::vector<ContextItem> items(random() % 40);
        for (auto& item : items) item = {roles[random() % 4], random() % 20, random() % 7 == 0};
        const std::size_t recent = random() % 50, budget = random() % 300;
        std::vector<std::size_t> drops;
        std::vector<bool> keep(items.size(), true);
        std::size_t total = 0;
        for (std::size_t i = 0; i < items.size(); ++i) {
            total += items[i].tokens;
            if (items[i].role != Role::System && !items[i].pinned && items.size() - i > recent) drops.push_back(i);
        }
        std::sort(drops.begin(), drops.end(), [&](std::size_t a, std::size_t b) {
            const int rank_a = drop_rank(items[a].role), rank_b = drop_rank(items[b].role);
            return rank_a == rank_b ? a < b : rank_a < rank_b;
        });
        for (auto i : drops) {
            if (total <= budget) break;
            total -= items[i].tokens; keep[i] = false;
        }
        std::vector<std::size_t> expected;
        for (std::size_t i = 0; i < keep.size(); ++i) if (keep[i]) expected.push_back(i);
        const auto actual = select_context(items, budget, recent);
        CHECK_EQ(actual.keep, expected);
        CHECK_EQ(actual.total_tokens, total);
        CHECK_EQ(actual.over_budget, total > budget);
    }
}

TEST(pol_concrete_allow_deny_ask) {
    AllowlistPolicy allowed({"ls", "git"});
    ConfirmPolicy confirm({"git"});
    ArgumentPatternPolicy patterns({"", ";", "rm -rf"});
    CHECK(allowed.evaluate(command("ls")).kind == Decision::Kind::Allow);
    CHECK(allowed.evaluate(command("LS")).kind == Decision::Kind::Deny);
    CHECK(allowed.evaluate(command("/bin/ls")).kind == Decision::Kind::Deny);
    CHECK(confirm.evaluate(command("git")).kind == Decision::Kind::Ask);
    CHECK(confirm.evaluate(command("ls")).kind == Decision::Kind::Allow);
    CHECK(patterns.evaluate(command("echo", {"hi"})).kind == Decision::Kind::Allow);
    CHECK(patterns.evaluate(command(";echo")).kind == Decision::Kind::Deny);
    CHECK(patterns.evaluate(command("rm", {"-rf", "/"})).kind == Decision::Kind::Deny);
    CHECK(patterns.evaluate(command("cat", {"a;rm b"})).kind == Decision::Kind::Deny);
    const auto result = confirm.evaluate(command("git"));
    CHECK_EQ(result.policy, std::string("confirm"));
    CHECK(!result.reason.empty());
}

TEST(pol_zero_limit_and_bounded_queue) {
    ManualClock clock;
    RateLimitPolicy zero(0, 10, clock);
    CHECK(zero.evaluate(command("ls")).kind == Decision::Kind::Deny);
    CHECK_EQ(zero.tracked(), std::size_t{0});
    RateLimitPolicy limit(2, 10, clock);
    CHECK(limit.evaluate(command("ls")).kind == Decision::Kind::Allow);
    CHECK(limit.evaluate(command("ls")).kind == Decision::Kind::Allow);
    clock.advance(9);
    CHECK(limit.evaluate(command("ls")).kind == Decision::Kind::Deny);
    CHECK_EQ(limit.tracked(), std::size_t{2});
    clock.advance(1);
    CHECK(limit.evaluate(command("ls")).kind == Decision::Kind::Allow);
    CHECK_EQ(limit.tracked(), std::size_t{1});
    for (int i = 0; i < 10000; ++i) {
        clock.advance(1);
        limit.evaluate(command("echo"));
        CHECK(limit.tracked() <= 2);
    }
}

TEST(pol_chain_empty_null_and_nested) {
    PolicyChain outer;
    CHECK(!outer.add(nullptr));
    CHECK_EQ(outer.size(), std::size_t{0});
    const auto empty = outer.evaluate(command("git"));
    CHECK(empty.kind == Decision::Kind::Deny);
    CHECK_EQ(empty.policy, std::string("chain"));
    auto nested = std::make_unique<PolicyChain>();
    REQUIRE(nested->add(std::make_unique<FixedPolicy>(Decision::Kind::Ask, "inner-ask")));
    REQUIRE(outer.add(std::move(nested)));
    CHECK_EQ(outer.evaluate(command("git")).policy, std::string("inner-ask"));
    auto deny = std::make_unique<PolicyChain>();
    REQUIRE(deny->add(std::make_unique<FixedPolicy>(Decision::Kind::Deny, "leaf-deny")));
    REQUIRE(outer.add(std::move(deny)));
    const auto result = outer.evaluate(command("git"));
    CHECK(result.kind == Decision::Kind::Deny);
    CHECK_EQ(result.policy, std::string("leaf-deny"));
    CHECK_EQ(result.reason, std::string("fixed deny"));
}

TEST(pol_all_three_child_combinations) {
    for (int code = 0; code < 27; ++code) {
        PolicyChain chain;
        std::vector<FixedPolicy*> children;
        std::vector<Decision::Kind> kinds;
        int digits = code;
        for (int i = 0; i < 3; ++i) {
            const auto kind = static_cast<Decision::Kind>(digits % 3); digits /= 3;
            kinds.push_back(kind);
            auto child = std::make_unique<FixedPolicy>(kind, "child" + std::to_string(i));
            children.push_back(child.get());
            REQUIRE(chain.add(std::move(child)));
        }
        int chosen = -1, first_ask = -1, last_evaluated = 2;
        for (int i = 0; i < 3; ++i) {
            if (kinds[static_cast<std::size_t>(i)] == Decision::Kind::Deny) { chosen = i; last_evaluated = i; break; }
            if (kinds[static_cast<std::size_t>(i)] == Decision::Kind::Ask && first_ask < 0) first_ask = i;
        }
        if (chosen < 0) chosen = first_ask;
        const auto result = chain.evaluate(command("git"));
        if (chosen < 0) {
            CHECK(result.kind == Decision::Kind::Allow);
            CHECK_EQ(result.policy, std::string("chain"));
        } else {
            CHECK(result.kind == kinds[static_cast<std::size_t>(chosen)]);
            CHECK_EQ(result.policy, "child" + std::to_string(chosen));
        }
        for (int i = 0; i < 3; ++i) CHECK_EQ(children[static_cast<std::size_t>(i)]->calls, i <= last_evaluated ? 1 : 0);
    }
}

TEST(pol_later_deny_still_consumes_rate_slot) {
    ManualClock clock;
    PolicyChain chain;
    auto rate = std::make_unique<RateLimitPolicy>(1, 10, clock);
    auto* tracked = rate.get();
    REQUIRE(chain.add(std::move(rate)));
    REQUIRE(chain.add(std::make_unique<FixedPolicy>(Decision::Kind::Deny, "later")));
    CHECK_EQ(chain.evaluate(command("ls")).policy, std::string("later"));
    CHECK_EQ(tracked->tracked(), std::size_t{1});
    CHECK_EQ(chain.evaluate(command("ls")).policy, std::string("ratelimit"));
}

TEST(graph_unknown_missing_and_self_loop) {
    DepGraph graph;
    std::vector<std::string> order;
    std::string error;
    CHECK(!graph.order_from("absent", &order, &error));
    CHECK_EQ(error, std::string("unknown: absent"));
    CHECK(!graph.add_edge("absent", "x"));
    CHECK_EQ(graph.node_count(), std::size_t{0});
    REQUIRE(graph.add_node("a"));
    CHECK(!graph.add_node("a"));
    REQUIRE(graph.add_edge("a", "gone"));
    CHECK(!graph.add_edge("a", "gone"));
    CHECK_EQ(graph.requirements("a"), (std::vector<std::string>{"gone"}));
    CHECK(graph.requirements("absent").empty());
    CHECK(!graph.order_from("a", &order, &error));
    CHECK_EQ(error, std::string("missing: gone (required by a)"));
    REQUIRE(graph.add_node("gone"));
    error = "leave intact";
    REQUIRE(graph.order_from("a", &order, &error));
    CHECK_EQ(order, (std::vector<std::string>{"gone", "a"}));
    CHECK_EQ(error, std::string("leave intact"));
    REQUIRE(graph.add_edge("gone", "gone"));
    CHECK(!graph.order_from("a", &order, &error));
    CHECK_EQ(error, std::string("cycle: gone -> gone"));
}

TEST(graph_three_node_and_nonroot_cycles) {
    DepGraph graph;
    for (const auto* name : {"root", "a", "b", "c"}) graph.add_node(name);
    graph.add_edge("root", "a"); graph.add_edge("a", "b"); graph.add_edge("b", "c"); graph.add_edge("c", "a");
    std::vector<std::string> order;
    std::string error;
    CHECK(!graph.order_from("root", &order, &error));
    CHECK_EQ(error, std::string("cycle: a -> b -> c -> a"));
    CHECK(!graph.order_from("a", &order, &error));
    CHECK_EQ(error, std::string("cycle: a -> b -> c -> a"));
}

TEST(graph_dependents_distance_cycles_and_missing) {
    DepGraph graph;
    for (const auto* name : {"z", "b", "a", "c", "far"}) graph.add_node(name);
    graph.add_edge("z", "missing"); graph.add_edge("b", "missing");
    graph.add_edge("a", "z"); graph.add_edge("c", "b"); graph.add_edge("far", "a");
    graph.add_edge("b", "c"); // reverse traversal must terminate despite this cycle
    CHECK_EQ(graph.dependents("missing"), (std::vector<std::string>{"b", "z", "a", "c", "far"}));
    CHECK_EQ(graph.dependents("b"), (std::vector<std::string>{"c"}));
    CHECK(graph.dependents("unknown").empty());
}

TEST(graph_deep_chain) {
    DepGraph graph;
    for (int i = 0; i < 5000; ++i) graph.add_node(std::to_string(i));
    for (int i = 0; i < 4999; ++i) graph.add_edge(std::to_string(i), std::to_string(i + 1));
    std::vector<std::string> order;
    std::string error;
    REQUIRE(graph.order_from("0", &order, &error));
    REQUIRE(order.size() == 5000);
    CHECK_EQ(order.front(), std::string("4999"));
    CHECK_EQ(order.back(), std::string("0"));
    const auto dependents = graph.dependents("4999");
    REQUIRE(dependents.size() == 4999);
    CHECK_EQ(dependents.front(), std::string("4998"));
    CHECK_EQ(dependents.back(), std::string("0"));
}

TEST(graph_random_dags_against_recursive_reference) {
    std::mt19937 random(314);
    for (int trial = 0; trial < 30; ++trial) {
        DepGraph graph;
        std::vector<std::vector<int>> edges(40);
        for (int i = 0; i < 40; ++i) graph.add_node(std::to_string(i));
        for (int i = 0; i < 40; ++i) for (int j = i + 1; j < 40; ++j)
            if (random() % 8 == 0) { edges[static_cast<std::size_t>(i)].push_back(j); graph.add_edge(std::to_string(i), std::to_string(j)); }
        std::vector<bool> visited(40, false);
        std::vector<std::string> expected;
        const std::function<void(int)> visit = [&](int i) {
            if (visited[static_cast<std::size_t>(i)]) return;
            visited[static_cast<std::size_t>(i)] = true;
            for (int j : edges[static_cast<std::size_t>(i)]) visit(j);
            expected.push_back(std::to_string(i));
        };
        visit(0);
        std::vector<std::string> actual;
        std::string error = "sentinel";
        REQUIRE(graph.order_from("0", &actual, &error));
        CHECK_EQ(actual, expected);
        CHECK_EQ(error, std::string("sentinel"));
    }
}

TEST(graph_catalog_lazy_bodies_and_cache) {
    TempDir temp;
    write_skill(temp.path(), "base", "base instructions", {}, "original base body");
    write_skill(temp.path(), "report", "report instructions", {"base"}, "report body");
    SkillCatalog catalog(2);
    const auto scanned = catalog.scan(temp.path());
    CHECK_EQ(scanned.loaded, std::size_t{2});
    CHECK(scanned.errors.empty());
    CHECK_EQ(catalog.body_reads(), std::size_t{0});
    // A body changed after scanning is read lazily at the first load.
    write_skill(temp.path(), "base", "base instructions", {}, "changed base body");
    std::string text, error;
    REQUIRE(catalog.load("report", &text, &error));
    CHECK(text.find("changed base body") != std::string::npos);
    CHECK(text.find("changed base body") < text.find("report body"));
    CHECK_EQ(catalog.body_reads(), std::size_t{2});
    const std::string first = text;
    write_skill(temp.path(), "base", "base instructions", {}, "changed again");
    REQUIRE(catalog.load("report", &text, &error));
    CHECK_EQ(text, first);
    CHECK_EQ(catalog.body_reads(), std::size_t{2});
    CHECK_EQ(catalog.cache_stats().hits, std::size_t{2});
    CHECK_EQ(catalog.dependents("base"), (std::vector<std::string>{"report"}));
}

TEST(index_postings_dedup_and_empty_cases) {
    SkillIndex index;
    CHECK(index.postings("none") == nullptr);
    index.add(0, "CSV csv CSV table");
    index.add(1, "table table");
    REQUIRE(index.postings("csv"));
    CHECK_EQ(*index.postings("csv"), (std::vector<std::size_t>{0}));
    CHECK_EQ(*index.postings("table"), (std::vector<std::size_t>{0, 1}));
    CHECK_EQ(index.word_count(), std::size_t{2});
    CHECK_EQ(index.search("table TABLE csv csv", 5), (std::vector<std::size_t>{0, 1}));
    CHECK(index.search("", 5).empty());
    CHECK(index.search("absent", 5).empty());
    CHECK(index.search("csv", 0).empty());
}

TEST(index_randomized_reference) {
    std::mt19937 random(315);
    SkillIndex index;
    std::vector<std::string> documents(3000);
    for (std::size_t id = 0; id < documents.size(); ++id) {
        for (int word = 0; word < 8; ++word) documents[id] += " word" + std::to_string(random() % 150);
        index.add(id, documents[id]);
    }
    for (int query = 0; query < 40; ++query) {
        const std::string text = "word" + std::to_string(random() % 180) + " word" + std::to_string(random() % 180);
        const std::size_t k = random() % 30;
        CHECK_EQ(index.search(text, k), linear_search(documents, text, k));
    }
}

TEST(index_benchmark_sizes) {
    for (std::size_t count : {100u, 1000u, 5000u}) {
        std::vector<std::string> documents;
        SkillIndex index;
        for (std::size_t id = 0; id < count; ++id) {
            documents.push_back("document " + std::to_string(id) + " team" + std::to_string(id % 97) +
                                " topic" + std::to_string(id % 211) + " ordinary text");
            index.add(id, documents.back());
        }
        const std::string query = "topic17 topic43";
        const auto expected = linear_search(documents, query, 10);
        CHECK_EQ(index.search(query, 10), expected);
        constexpr int repeats = 20;
        std::size_t indexed_hits = 0, linear_hits = 0;
        auto start = BenchClock::now();
        for (int i = 0; i < repeats; ++i) indexed_hits += index.search(query, 10).size();
        const double index_ms = elapsed_ms(start) / repeats;
        start = BenchClock::now();
        for (int i = 0; i < repeats; ++i) linear_hits += linear_search(documents, query, 10).size();
        const double linear_ms = elapsed_ms(start) / repeats;
        CHECK_EQ(indexed_hits, linear_hits);
        std::printf("BENCH index docs=%zu repeats=%d index_ms_per_query=%.6f linear_ms_per_query=%.6f\n",
                    count, repeats, index_ms, linear_ms);
    }
}

int main(int argc, char** argv) { return mt::run_all(argc, argv); }
