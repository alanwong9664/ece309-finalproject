#pragma once
#include "policy/policy.h"

#include <cstddef>
#include <queue>
#include <string>
#include <unordered_set>
#include <vector>

class AllowlistPolicy : public Policy {
public:
    explicit AllowlistPolicy(const std::vector<std::string>& programs);
    std::string name() const override { return "allowlist"; }
    Decision    evaluate(const CommandRequest& req) override;
private:
    std::unordered_set<std::string> programs_;
};

class ArgumentPatternPolicy : public Policy {
public:
    explicit ArgumentPatternPolicy(std::vector<std::string> patterns);
    std::string name() const override { return "patterns"; }
    Decision    evaluate(const CommandRequest& req) override;
private:
    std::vector<std::string> patterns_;
};

class ConfirmPolicy : public Policy {
public:
    explicit ConfirmPolicy(const std::vector<std::string>& programs);
    std::string name() const override { return "confirm"; }
    Decision    evaluate(const CommandRequest& req) override;
private:
    std::unordered_set<std::string> programs_;
};

class RateLimitPolicy : public Policy {
public:

    RateLimitPolicy(std::size_t max_commands, double window_seconds, const Clock& clock);
    std::string name() const override { return "ratelimit"; }
    Decision    evaluate(const CommandRequest& req) override;

    std::size_t tracked() const noexcept;
private:
    std::size_t        max_;
    double             window_;
    const Clock&       clock_;
    std::queue<double> times_;
};
