// src/policy/audit_log.cpp — PROVIDED.

#include "policy/audit_log.h"

#include <fstream>

namespace {
// Fields are tab-separated; tabs/newlines/backslashes inside a field are escaped.
std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '\\') o += "\\\\";
        else if (c == '\t') o += "\\t";
        else if (c == '\n') o += "\\n";
        else o += c;
    }
    return o;
}
std::string unesc(const std::string& s) {
    std::string o;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char n = s[++i];
            o += n == 't' ? '\t' : n == 'n' ? '\n' : n;
        } else {
            o += s[i];
        }
    }
    return o;
}
std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> f(1);
    for (char c : line) {
        if (c == '\t') f.emplace_back();
        else f.back() += c;
    }
    return f;
}
}  // namespace

AuditLog::AuditLog(std::filesystem::path file) : file_(std::move(file)) {
    if (!file_.empty()) {
        std::vector<AuditEntry> old = read_file(file_);
        if (!old.empty()) next_seq_ = old.back().seq + 1;
    }
}

void AuditLog::record(const CommandRequest& req, const Decision& d) {
    AuditEntry e{next_seq_++, to_string(d.kind), d.policy, req.command_line(), d.reason};
    if (!file_.empty()) {
        std::ofstream out(file_, std::ios::app | std::ios::binary);
        out << e.seq << '\t' << esc(e.decision) << '\t' << esc(e.policy) << '\t'
            << esc(e.command) << '\t' << esc(e.reason) << '\n';
        out.flush();   // on disk (in the OS) before the caller goes on
    }
    entries_.push_back(std::move(e));
}

std::vector<AuditEntry> AuditLog::read_file(const std::filesystem::path& file) {
    std::vector<AuditEntry> out;
    std::ifstream in(file, std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
        std::vector<std::string> f = split_tabs(line);
        if (f.size() != 5) continue;
        AuditEntry e;
        try { e.seq = static_cast<std::size_t>(std::stoull(f[0])); } catch (...) { continue; }
        e.decision = unesc(f[1]);
        e.policy = unesc(f[2]);
        e.command = unesc(f[3]);
        e.reason = unesc(f[4]);
        out.push_back(std::move(e));
    }
    return out;
}
