#pragma once
#include "core/message.h"

#include <cstddef>
#include <vector>

struct ContextItem {
    Role        role = Role::User;
    std::size_t tokens = 0;
    bool        pinned = false;
};

struct ContextPlan {
    std::vector<std::size_t> keep;
    std::size_t total_tokens = 0;
    bool        over_budget = false;
};

inline int drop_rank(Role role) noexcept {
    switch (role) {
        case Role::Tool:      return 0;
        case Role::Assistant: return 1;
        case Role::User:      return 2;
        case Role::System:    return 3;
    }
    return 3;
}

ContextPlan select_context(const std::vector<ContextItem>& items,
                           std::size_t budget, std::size_t keep_recent);
