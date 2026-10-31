// include/policy/policy.h
//
// PROVIDED. The types every policy uses. Spec §5.

#pragma once
#include <chrono>
#include <string>
#include <utility>
#include <vector>

// One command the model wants to run, e.g. {"git", {"status"}}.
struct CommandRequest {
    std::string              program;
    std::vector<std::string> args;
    std::string              tool_call_id;   // for the audit log; may be empty

    // "git status" — program and args joined by single spaces.
    std::string command_line() const {
        std::string s = program;
        for (const std::string& a : args) s += " " + a;
        return s;
    }
};

struct Decision {
    // Ordered from least to most restrictive, so "most restrictive wins" is
    // simply the larger Kind. It is a nested enum class, so you write
    // Decision::Kind::Deny (and d.kind == Decision::Kind::Ask).
    enum class Kind { Allow = 0, Ask = 1, Deny = 2 };

    Kind        kind = Kind::Deny;
    std::string reason;   // never empty: always shown and always logged
    std::string policy;   // name() of the policy that made this decision

    static Decision allow(std::string why, std::string who) { return {Kind::Allow, std::move(why), std::move(who)}; }
    static Decision ask(std::string why, std::string who)   { return {Kind::Ask,   std::move(why), std::move(who)}; }
    static Decision deny(std::string why, std::string who)  { return {Kind::Deny,  std::move(why), std::move(who)}; }
};

inline const char* to_string(Decision::Kind k) {
    switch (k) {
        case Decision::Kind::Allow: return "allow";
        case Decision::Kind::Ask:   return "ask";
        case Decision::Kind::Deny:  return "deny";
    }
    return "deny";
}

class Policy {
public:
    virtual ~Policy() = default;

    // Short fixed identifier, e.g. "allowlist". Copied into Decision::policy.
    virtual std::string name() const = 0;

    // Not const: some policies (the rate limit) remember past requests.
    virtual Decision evaluate(const CommandRequest& req) = 0;
};

// Where "now" comes from. The rate limit takes a Clock so tests can move
// time forward by hand instead of sleeping.
class Clock {
public:
    virtual ~Clock() = default;
    virtual double now_seconds() const = 0;
};

class SteadyClock : public Clock {
public:
    double now_seconds() const override {
        using namespace std::chrono;
        return duration<double>(steady_clock::now().time_since_epoch()).count();
    }
};

// For tests: time only moves when you call advance().
// Tip: advance by whole numbers in boundary tests; 0.1 + 0.1 + 0.1 is not
// exactly 0.3 in floating point.
class ManualClock : public Clock {
public:
    explicit ManualClock(double start = 0.0) : t_(start) {}
    double now_seconds() const override { return t_; }
    void   advance(double seconds) { t_ += seconds; }
private:
    double t_;
};

// Asked when the final decision is Ask. The harness's version prints a y/n
// prompt on the terminal; tests use a scripted one.
class Confirmer {
public:
    virtual ~Confirmer() = default;
    virtual bool confirm(const CommandRequest& req, const std::string& reason) = 0;
};
