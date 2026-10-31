// include/memory/context_builder.h
//
// select_context(): YOU IMPLEMENT (src/memory/context_builder.cpp). Spec §4.4.
// Everything else in this header is PROVIDED.
//
// A real model can only read a limited number of tokens. Before every model
// call, the harness describes each message of the conversation as a
// ContextItem and asks select_context() which ones to send. The full
// conversation is kept for the transcript; only the model sees the trimmed
// copy.

#pragma once
#include "core/message.h"

#include <cstddef>
#include <vector>

struct ContextItem {
    Role        role = Role::User;
    std::size_t tokens = 0;     // count_tokens(content)
    bool        pinned = false; // e.g. skill instructions loaded by load_skill
};

struct ContextPlan {
    std::vector<std::size_t> keep;   // indices of the kept items, ascending
    std::size_t total_tokens = 0;    // sum of the kept items' tokens
    bool        over_budget = false; // true if even the protected items exceed the budget
};

// PROVIDED. Which kind of message to drop first: lower = dropped earlier.
// Tool results are cheapest to lose (the model can call the tool again),
// then assistant replies, then user messages.
inline int drop_rank(Role role) noexcept {
    switch (role) {
        case Role::Tool:      return 0;
        case Role::Assistant: return 1;
        case Role::User:      return 2;
        case Role::System:    return 3;   // never dropped (protected)
    }
    return 3;
}

// YOU IMPLEMENT.
//
// Protected items are never dropped:
//   * every Role::System item,
//   * every pinned item,
//   * the last `keep_recent` items (all of them if keep_recent >= size).
//
// If the total of all items fits in `budget`, keep everything. Otherwise
// drop unprotected items one at a time, in this order, until the total fits:
//   lowest drop_rank(role) first; among equal ranks, the OLDEST (smallest
//   index) first.
// Stop as soon as the total is <= budget. If every unprotected item has been
// dropped and the total is still over budget, keep the protected items
// anyway: over_budget is true exactly when total_tokens > budget.
//
// Required: choose the next item to drop with YOUR MinHeap (util/min_heap.h),
// keyed on (drop_rank, index). Do not sort the items.
ContextPlan select_context(const std::vector<ContextItem>& items,
                           std::size_t budget, std::size_t keep_recent);
