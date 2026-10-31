// include/policy/audit_log.h
//
// PROVIDED. An append-only record of every run_command decision.
// One line per decision, written and flushed BEFORE the command would run,
// so even a crash during the command leaves the decision on record.

#pragma once
#include "policy/policy.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

struct AuditEntry {
    std::size_t seq = 0;          // 1, 2, 3, ... within one file
    std::string decision;         // "allow" / "deny"  (Ask is always resolved first)
    std::string policy;
    std::string command;          // CommandRequest::command_line()
    std::string reason;
};

class AuditLog {
public:
    // Appends to `file` (created if missing). An empty path keeps entries
    // in memory only.
    explicit AuditLog(std::filesystem::path file = {});

    void record(const CommandRequest& req, const Decision& final_decision);

    std::size_t size() const noexcept { return entries_.size(); }
    const std::vector<AuditEntry>& entries() const noexcept { return entries_; }

    // Reads a log file written by record(). Malformed lines are skipped.
    static std::vector<AuditEntry> read_file(const std::filesystem::path& file);

private:
    std::filesystem::path   file_;
    std::vector<AuditEntry> entries_;
    std::size_t             next_seq_ = 1;
};
