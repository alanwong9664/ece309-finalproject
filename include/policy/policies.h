// include/policy/policies.h
//
// YOU IMPLEMENT (src/policy/policies.cpp). Spec §5.2.
//
// Four concrete policies. The PUBLIC parts are fixed (the tests construct
// them exactly like this). Choose your own private members. Any STL
// container is allowed here (e.g. std::unordered_set for the allowlist,
// std::queue for the rate limit's timestamps).

#pragma once
#include "policy/policy.h"

#include <cstddef>
#include <queue>
#include <string>
#include <unordered_set>
#include <vector>

// Allow if req.program is one of `programs` (exact match), else Deny.
class AllowlistPolicy : public Policy {
public:
    explicit AllowlistPolicy(const std::vector<std::string>& programs);
    std::string name() const override { return "allowlist"; }
    Decision    evaluate(const CommandRequest& req) override;
private:
    std::unordered_set<std::string> programs_;
};

// Deny if the whole command line, req.command_line() (program and args
// joined by single spaces), CONTAINS any of `patterns` as a substring, e.g.
// ";" or "rm -rf" (which matches {"rm", {"-rf", "/"}}); otherwise Allow.
// Empty patterns are ignored.
class ArgumentPatternPolicy : public Policy {
public:
    explicit ArgumentPatternPolicy(std::vector<std::string> patterns);
    std::string name() const override { return "patterns"; }
    Decision    evaluate(const CommandRequest& req) override;
private:
    std::vector<std::string> patterns_;
};

// Ask if req.program is one of `programs`, else Allow.
class ConfirmPolicy : public Policy {
public:
    explicit ConfirmPolicy(const std::vector<std::string>& programs);
    std::string name() const override { return "confirm"; }
    Decision    evaluate(const CommandRequest& req) override;
private:
    std::unordered_set<std::string> programs_;
};

// At most `max_commands` Allows in any window of `window_seconds`.
// Sliding window: a request at time t counts the earlier ALLOWED requests at
// times s with t - s < window_seconds. If there are already max_commands of
// them, Deny (and do not record this request); otherwise record t and Allow.
// Memory must stay O(max_commands): forget timestamps that left the window.
// max_commands == 0 denies everything.
class RateLimitPolicy : public Policy {
public:
    // `clock` is not owned and must outlive the policy.
    RateLimitPolicy(std::size_t max_commands, double window_seconds, const Clock& clock);
    std::string name() const override { return "ratelimit"; }
    Decision    evaluate(const CommandRequest& req) override;

    // How many timestamps are stored right now (for the memory test).
    std::size_t tracked() const noexcept;
private:
    std::size_t        max_;
    double             window_;
    const Clock&       clock_;
    std::queue<double> times_;
};
