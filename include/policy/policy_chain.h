// include/policy/policy_chain.h
//
// YOU IMPLEMENT (src/policy/policy_chain.cpp). Spec §5.3.
//
// The composite pattern: a PolicyChain IS a Policy that holds other
// policies (which may themselves be chains). The harness only ever holds
// one Policy and never needs to know how many rules are inside.

#pragma once
#include "policy/policy.h"

#include <cstddef>
#include <memory>
#include <vector>

class PolicyChain : public Policy {
public:
    // Appends a child; children are evaluated in the order added.
    // Returns false (and adds nothing) if `child` is null.
    bool add(std::unique_ptr<Policy> child);

    std::size_t size() const noexcept;
    std::string name() const override { return "chain"; }

    // Most restrictive wins: Deny > Ask > Allow.
    //   * Empty chain: Deny, policy "chain" (deny by default).
    //   * Evaluate children in order. On the first Deny, stop immediately
    //     (later children are NOT evaluated) and return that child's
    //     Decision unchanged.
    //   * Otherwise, if any child said Ask, return the FIRST Ask Decision
    //     unchanged.
    //   * Otherwise (all Allow) return Allow with policy "chain" and a
    //     non-empty reason.
    // "Unchanged" means the reason and policy fields are the child's, so a
    // Deny from a chain nested inside a chain still names the leaf policy.
    Decision evaluate(const CommandRequest& req) override;

private:
    std::vector<std::unique_ptr<Policy>> children_;
};
