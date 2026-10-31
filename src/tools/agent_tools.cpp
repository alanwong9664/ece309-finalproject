// src/tools/agent_tools.cpp — PROVIDED.

#include "tools/agent_tools.h"
#include "memory/memory_store.h"
#include "policy/audit_log.h"
#include "policy/policy.h"
#include "skills/skill_catalog.h"

#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace {

JsonValue prop(const char* type, const char* description) {
    JsonValue p = JsonValue::object();
    p.set("type", JsonValue::string(type));
    p.set("description", JsonValue::string(description));
    return p;
}

JsonValue make_schema(std::vector<std::pair<std::string, JsonValue>> props,
                      std::vector<std::string> required) {
    JsonValue s = JsonValue::object();
    s.set("type", JsonValue::string("object"));
    JsonValue p = JsonValue::object();
    for (auto& kv : props) p.set(kv.first, std::move(kv.second));
    s.set("properties", std::move(p));
    JsonValue r = JsonValue::array();
    for (auto& name : required) r.push_back(JsonValue::string(name));
    s.set("required", std::move(r));
    return s;
}

bool get_string(const JsonValue& args, const char* key, std::string* out, std::string* err) {
    const JsonValue* v = args.find(key);
    if (v == nullptr) { *err = std::string("missing required argument: ") + key; return false; }
    if (v->type() != JsonValue::Type::String) { *err = std::string("argument '") + key + "' must be a string"; return false; }
    *out = v->as_string();
    return true;
}

bool get_opt_string(const JsonValue& args, const char* key, std::string* out, std::string* err) {
    if (args.find(key) == nullptr) { out->clear(); return true; }
    return get_string(args, key, out, err);
}

// Optional positive integer, 1..100.
bool get_k(const JsonValue& args, std::size_t dflt, std::size_t* out, std::string* err) {
    const JsonValue* v = args.find("k");
    if (v == nullptr) { *out = dflt; return true; }
    if (v->type() != JsonValue::Type::Number) { *err = "argument 'k' must be a number"; return false; }
    double d = v->as_number();
    if (!(d >= 1 && d <= 100) || std::floor(d) != d) { *err = "argument 'k' must be a whole number from 1 to 100"; return false; }
    *out = static_cast<std::size_t>(d);
    return true;
}

ToolResult save_memory(ToolContext& ctx, std::string ok_text) {
    if (!ctx.memory_file.empty()) {
        std::string err;
        if (!ctx.memory->save(ctx.memory_file, &err))
            return ToolResult::failure("changed in memory but could not save: " + err);
    }
    return ToolResult::success(std::move(ok_text));
}

}  // namespace

// ---- memory --------------------------------------------------------------

RememberTool::RememberTool()
    : schema_(make_schema({{"text", prop("string", "what to remember")}}, {"text"})) {}
std::string RememberTool::description() const { return "Store a fact in long-term memory."; }
ToolResult RememberTool::invoke(const JsonValue& args, ToolContext& ctx) {
    if (!ctx.memory) return ToolResult::failure("memory is disabled");
    std::string text, err;
    if (!get_string(args, "text", &text, &err)) return ToolResult::failure(err);
    if (text.empty()) return ToolResult::failure("text is empty");
    std::uint64_t id = ctx.memory->remember(text);
    return save_memory(ctx, "remembered " + MemoryStore::format_id(id));
}

RecallTool::RecallTool()
    : schema_(make_schema({{"query", prop("string", "words to look for")},
                           {"k", prop("number", "maximum results (default 3)")}},
                          {"query"})) {}
std::string RecallTool::description() const { return "Find the memories that best match a query."; }
ToolResult RecallTool::invoke(const JsonValue& args, ToolContext& ctx) {
    if (!ctx.memory) return ToolResult::failure("memory is disabled");
    std::string query, err;
    std::size_t k = 0;
    if (!get_string(args, "query", &query, &err) || !get_k(args, 3, &k, &err)) return ToolResult::failure(err);
    std::vector<MemoryRecord> hits = ctx.memory->recall(query, k);
    if (hits.empty()) return ToolResult::success("no matching memories");
    std::string out;
    for (const MemoryRecord& r : hits) out += MemoryStore::format_id(r.id) + ": " + r.text + "\n";
    return ToolResult::success(out);
}

ForgetTool::ForgetTool()
    : schema_(make_schema({{"id", prop("string", "memory id such as m3")}}, {"id"})) {}
std::string ForgetTool::description() const { return "Delete one memory by id."; }
ToolResult ForgetTool::invoke(const JsonValue& args, ToolContext& ctx) {
    if (!ctx.memory) return ToolResult::failure("memory is disabled");
    std::string id_text, err;
    if (!get_string(args, "id", &id_text, &err)) return ToolResult::failure(err);
    std::uint64_t id = 0;
    if (!MemoryStore::parse_id(id_text, &id)) return ToolResult::failure("bad memory id '" + id_text + "' (expected m<number>)");
    if (!ctx.memory->forget(id)) return ToolResult::failure("no memory " + id_text);
    return save_memory(ctx, "forgot " + id_text);
}

ListMemoriesTool::ListMemoriesTool() : schema_(make_schema({}, {})) {}
std::string ListMemoriesTool::description() const { return "List every memory, oldest first."; }
ToolResult ListMemoriesTool::invoke(const JsonValue& /*args*/, ToolContext& ctx) {
    if (!ctx.memory) return ToolResult::failure("memory is disabled");
    std::vector<MemoryRecord> all = ctx.memory->list();
    if (all.empty()) return ToolResult::success("no memories");
    std::string out;
    for (const MemoryRecord& r : all) out += MemoryStore::format_id(r.id) + ": " + r.text + "\n";
    return ToolResult::success(out);
}

// ---- skills --------------------------------------------------------------

ListSkillsTool::ListSkillsTool()
    : schema_(make_schema({{"query", prop("string", "optional search words")},
                           {"k", prop("number", "maximum results when searching (default 5)")}},
                          {})) {}
std::string ListSkillsTool::description() const { return "List available skills, or search them by keyword."; }
ToolResult ListSkillsTool::invoke(const JsonValue& args, ToolContext& ctx) {
    if (!ctx.skills) return ToolResult::failure("skills are disabled");
    std::string query, err;
    std::size_t k = 0;
    if (!get_opt_string(args, "query", &query, &err) || !get_k(args, 5, &k, &err)) return ToolResult::failure(err);
    std::string out;
    if (query.empty()) {
        for (const SkillMeta& m : ctx.skills->skills()) out += m.name + ": " + m.description + "\n";
        if (out.empty()) out = "no skills installed";
    } else {
        for (const SkillMeta* m : ctx.skills->search(query, k)) out += m->name + ": " + m->description + "\n";
        if (out.empty()) out = "no skills match '" + query + "'";
    }
    return ToolResult::success(out);
}

LoadSkillTool::LoadSkillTool()
    : schema_(make_schema({{"name", prop("string", "skill name")}}, {"name"})) {}
std::string LoadSkillTool::description() const { return "Load a skill's instructions, with everything it requires."; }
ToolResult LoadSkillTool::invoke(const JsonValue& args, ToolContext& ctx) {
    if (!ctx.skills) return ToolResult::failure("skills are disabled");
    std::string name, err, text;
    if (!get_string(args, "name", &name, &err)) return ToolResult::failure(err);
    if (!ctx.skills->load(name, &text, &err)) return ToolResult::failure(err);
    return ToolResult::success(text);
}

SkillDependentsTool::SkillDependentsTool()
    : schema_(make_schema({{"name", prop("string", "skill name")}}, {"name"})) {}
std::string SkillDependentsTool::description() const { return "List the skills that depend on a skill, nearest first."; }
ToolResult SkillDependentsTool::invoke(const JsonValue& args, ToolContext& ctx) {
    if (!ctx.skills) return ToolResult::failure("skills are disabled");
    std::string name, err;
    if (!get_string(args, "name", &name, &err)) return ToolResult::failure(err);
    std::vector<std::string> deps = ctx.skills->dependents(name);
    if (deps.empty()) return ToolResult::success("nothing depends on " + name);
    std::string out = "depend on " + name + ":";
    for (const std::string& d : deps) out += " " + d;
    return ToolResult::success(out);
}

// ---- run_command ------------------------------------------------------------

RunCommandTool::RunCommandTool()
    : schema_(make_schema({{"program", prop("string", "program name, e.g. ls")},
                           {"args", prop("array", "arguments, each a string")}},
                          {"program"})) {}
std::string RunCommandTool::description() const {
    return "Run a command (simulated) after the permission policy approves it.";
}
ToolResult RunCommandTool::invoke(const JsonValue& args, ToolContext& ctx) {
    if (!ctx.policy) return ToolResult::failure("run_command is disabled");
    CommandRequest req;
    std::string err;
    if (!get_string(args, "program", &req.program, &err)) return ToolResult::failure(err);
    if (req.program.empty()) return ToolResult::failure("program is empty");
    if (const JsonValue* a = args.find("args")) {
        if (a->type() != JsonValue::Type::Array) return ToolResult::failure("argument 'args' must be an array of strings");
        for (std::size_t i = 0; i < a->size(); ++i) {
            if ((*a)[i].type() != JsonValue::Type::String) return ToolResult::failure("argument 'args' must be an array of strings");
            req.args.push_back((*a)[i].as_string());
        }
    }

    Decision d = ctx.policy->evaluate(req);
    if (d.kind == Decision::Kind::Ask) {
        bool yes = ctx.confirmer != nullptr && ctx.confirmer->confirm(req, d.reason);
        d = yes ? Decision::allow("confirmed by the user (" + d.reason + ")", d.policy)
                : Decision::deny(ctx.confirmer ? "the user declined" : "needs confirmation, but no one can confirm", d.policy);
    }
    // Record BEFORE running: if the command hung or crashed the harness, the
    // decision would still be on file.
    if (ctx.audit) ctx.audit->record(req, d);

    if (d.kind != Decision::Kind::Allow)
        return ToolResult::failure("denied by " + d.policy + ": " + d.reason);
    return ToolResult::success("[simulated] $ " + req.command_line() + "\n(exit 0; commands are simulated, nothing ran)");
}
