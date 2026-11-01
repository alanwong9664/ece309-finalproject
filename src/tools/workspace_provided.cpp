#include "tools/workspace.h"

#include <system_error>
#include <utility>

namespace fs = std::filesystem;

Workspace::Workspace(fs::path root) {
    std::error_code ec;
    fs::path canon = fs::canonical(root, ec);
    root_ = ec ? fs::absolute(root).lexically_normal() : std::move(canon);
}

const fs::path& Workspace::root() const noexcept { return root_; }

static bool is_within(const fs::path& base, const fs::path& p) {
    auto q = p.begin();
    for (auto b = base.begin(); b != base.end(); ++b) {
        if (b->empty()) continue;
        if (q == p.end() || *b != *q) return false;
        ++q;
    }
    return true;
}

bool Workspace::check_symlinks(const fs::path& resolved, std::string* reason) const {

    std::error_code ec;
    fs::path canon = fs::weakly_canonical(resolved, ec);
    if (ec) {
        *reason = "cannot resolve path: " + ec.message();
        return false;
    }
    if (!is_within(root_, canon)) {
        *reason = "path escapes the workspace through a symbolic link";
        return false;
    }

    fs::path walk = root_;
    auto it = canon.begin();
    for (auto r = root_.begin(); r != root_.end(); ++r)
        if (!r->empty() && it != canon.end()) ++it;
    for (; it != canon.end(); ++it) {
        if (it->empty()) continue;
        walk /= *it;
        if (fs::is_symlink(fs::symlink_status(walk, ec))) {
            *reason = "path goes through a dangling symbolic link";
            return false;
        }
    }
    return true;
}
