#pragma once
#include <chrono>
#include <string>
#include <utility>
#include <vector>

struct CommandRequest {
    std::string              program;
    std::vector<std::string> args;
    std::string              tool_call_id;

    std::string command_line() const {
        std::string s = program;
        for (const std::string& a : args) s += " " + a;
        return s;
    }
};

struct Decision {

    enum class Kind { Allow = 0, Ask = 1, Deny = 2 };

    Kind        kind = Kind::Deny;
    std::string reason;
    std::string policy;

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

    virtual std::string name() const = 0;

    virtual Decision evaluate(const CommandRequest& req) = 0;
};

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

class ManualClock : public Clock {
public:
    explicit ManualClock(double start = 0.0) : t_(start) {}
    double now_seconds() const override { return t_; }
    void   advance(double seconds) { t_ += seconds; }
private:
    double t_;
};

class Confirmer {
public:
    virtual ~Confirmer() = default;
    virtual bool confirm(const CommandRequest& req, const std::string& reason) = 0;
};
