#pragma once
#include "policy/policy.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

struct AuditEntry {
    std::size_t seq = 0;
    std::string decision;
    std::string policy;
    std::string command;
    std::string reason;
};

class AuditLog {
public:

    explicit AuditLog(std::filesystem::path file = {});

    void record(const CommandRequest& req, const Decision& final_decision);

    std::size_t size() const noexcept { return entries_.size(); }
    const std::vector<AuditEntry>& entries() const noexcept { return entries_; }

    static std::vector<AuditEntry> read_file(const std::filesystem::path& file);

private:
    std::filesystem::path   file_;
    std::vector<AuditEntry> entries_;
    std::size_t             next_seq_ = 1;
};
