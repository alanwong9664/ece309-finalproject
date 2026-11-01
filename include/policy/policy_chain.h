#pragma once
#include "policy/policy.h"

#include <cstddef>
#include <memory>
#include <vector>

class PolicyChain : public Policy {
public:

    bool add(std::unique_ptr<Policy> child);

    std::size_t size() const noexcept;
    std::string name() const override { return "chain"; }

    Decision evaluate(const CommandRequest& req) override;

private:
    std::vector<std::unique_ptr<Policy>> children_;
};
