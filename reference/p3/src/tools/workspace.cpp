// src/tools/workspace.cpp — P3 instructor reference (used only with -DUSE_P3_REFERENCE=ON).
//
// Workspace::resolve: a purely lexical check (spec §4.4, steps 1–6).

#include "tools/workspace.h"

namespace fs = std::filesystem;

bool Workspace::resolve(std::string_view user_path, fs::path* out,
                        std::string* reason) const {
    // 1. Empty path.
    if (user_path.empty()) {
        *reason = "empty path";
        return false;
    }
    // 2. Embedded NUL: the OS would silently cut the path off there.
    if (user_path.find('\0') != std::string_view::npos) {
        *reason = "path contains a NUL byte";
        return false;
    }
    // 3. Only workspace-relative paths are allowed.
    fs::path p{std::string(user_path)};
    if (p.is_absolute() || p.has_root_path()) {
        *reason = "absolute paths are not allowed: " + std::string(user_path);
        return false;
    }
    // 4. Fold ".", "//" and "x/.." pairs.
    fs::path n = p.lexically_normal();
    // 5. A leading ".." means the path climbs above root at some point.
    if (!n.empty() && *n.begin() == "..") {
        *reason = "path escapes the workspace: " + std::string(user_path);
        return false;
    }
    // 6. Attach to root.
    *out = (n == ".") ? root() : root() / n;
    return true;
}
