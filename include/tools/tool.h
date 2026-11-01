#pragma once
#include "json/json_value.h"
#include "tools/workspace.h"

#include <cstddef>
#include <filesystem>
#include <string>
#include <utility>

struct ToolResult {
    bool        ok = false;
    std::string content;
    std::string error;

    static ToolResult success(std::string content) {
        ToolResult r;
        r.ok = true;
        r.content = std::move(content);
        return r;
    }
    static ToolResult failure(std::string error) {
        ToolResult r;
        r.ok = false;
        r.error = std::move(error);
        return r;
    }
};

class MemoryStore;
class SkillCatalog;
class Policy;
class Confirmer;
class AuditLog;

struct ToolContext {
    explicit ToolContext(Workspace ws) : workspace(std::move(ws)) {}

    Workspace   workspace;
    std::size_t max_read_bytes = 64 * 1024;

    MemoryStore*          memory = nullptr;
    std::filesystem::path memory_file;
    SkillCatalog*         skills = nullptr;
    Policy*               policy = nullptr;
    Confirmer*            confirmer = nullptr;
    AuditLog*             audit = nullptr;
};

class Tool {
public:
    virtual ~Tool() = default;

    virtual std::string name() const = 0;

    virtual std::string description() const = 0;

    virtual const JsonValue& schema() const = 0;

    virtual ToolResult invoke(const JsonValue& args, ToolContext& ctx) = 0;
};
