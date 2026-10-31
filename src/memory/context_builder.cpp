#include "memory/context_builder.h"
#include "util/min_heap.h"
#include <utility>

ContextPlan select_context(const std::vector<ContextItem>& items,
                           std::size_t budget, std::size_t keep_recent) {
    ContextPlan plan;
    const std::size_t recent_start = keep_recent >= items.size() ? 0 : items.size() - keep_recent;
    MinHeap<std::pair<int, std::size_t>> candidates;
    std::vector<bool> kept(items.size(), true);
    for (std::size_t i = 0; i < items.size(); ++i) {
        plan.total_tokens += items[i].tokens;
        if (items[i].role != Role::System && !items[i].pinned && i < recent_start)
            candidates.push({drop_rank(items[i].role), i});
    }
    while (plan.total_tokens > budget && !candidates.empty()) {
        const std::size_t index = candidates.pop().second;
        kept[index] = false;
        plan.total_tokens -= items[index].tokens;
    }
    for (std::size_t i = 0; i < items.size(); ++i)
        if (kept[i]) plan.keep.push_back(i);
    plan.over_budget = plan.total_tokens > budget;
    return plan;
}
