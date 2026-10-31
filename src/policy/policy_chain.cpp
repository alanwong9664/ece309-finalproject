#include "policy/policy_chain.h"
#include <utility>

bool PolicyChain::add(std::unique_ptr<Policy> child) {
    if (!child) return false;
    children_.push_back(std::move(child));
    return true;
}
std::size_t PolicyChain::size() const noexcept { return children_.size(); }
Decision PolicyChain::evaluate(const CommandRequest& req) {
    if (children_.empty()) return Decision::deny("empty policy chain", name());
    Decision first_ask;
    bool have_ask = false;
    for (const auto& child : children_) {
        Decision decision = child->evaluate(req);
        if (decision.kind == Decision::Kind::Deny) return decision;
        if (decision.kind == Decision::Kind::Ask && !have_ask) {
            first_ask = std::move(decision);
            have_ask = true;
        }
    }
    return have_ask ? first_ask : Decision::allow("all policies allowed", name());
}
