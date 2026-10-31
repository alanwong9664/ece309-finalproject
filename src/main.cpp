// src/main.cpp — PROVIDED.
//
// Usage:
//   miniharness [--script FILE | --replay FILE] [--save FILE]
//               [--max-turns N] [--max-tool-steps N]
//               [--workspace DIR] [--no-tools] [--tools]
//               [--skills DIR] [--memory FILE] [--audit FILE]
//               [--context-budget N] [--keep-recent N]
//
//   Final project:
//   --skills DIR          skill library to scan (default: ./skills if present)
//   --memory FILE         load memories from FILE and save every change to it
//                         (default: memories live only for this run)
//   --audit FILE          append every run_command decision to FILE
//   --context-budget N    send the model at most ~N tokens (select_context)
//   --keep-recent N       newest messages never dropped (default 4)
//
//   --tools        print every registered tool (name, description, schema)
//                  and exit
//   --no-tools     run exactly like P2: no registry, no system message
//   --workspace    root directory the file tools may touch (default:
//                  ./workspace, created if missing)

#include "harness/harness.h"
#include "harness/transcript.h"
#include "model/replay_client.h"
#include "model/scripted_client.h"
#include "tools/builtin_tools.h"
#include "memory/memory_store.h"
#include "policy/audit_log.h"
#include "policy/policies.h"
#include "policy/policy_chain.h"
#include "skills/skill_catalog.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

namespace {

class StdioInput : public InputSource {
public:
    std::string read_line() override {
        std::string line;
        std::getline(std::cin, line);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        eof_ = std::cin.eof() && line.empty();
        return line;
    }
    bool is_eof() const override { return eof_; }
private:
    bool eof_ = false;
};

class StdioOutput : public OutputSink {
public:
    void write(std::string_view text) override { std::cout << text << std::flush; }
};

// Asks the person at the terminal to approve an Ask decision.
class TerminalConfirmer : public Confirmer {
public:
    TerminalConfirmer(InputSource& in, OutputSink& out) : in_(in), out_(out) {}
    bool confirm(const CommandRequest& req, const std::string& reason) override {
        (void)reason;   // the policy's wording is the student's; keep the prompt fixed
        out_.write("[confirm] Run `" + req.command_line() + "`? (y/n) ");
        std::string answer = in_.read_line();
        if (in_.is_eof()) { out_.write("\n"); return false; }
        return answer == "y" || answer == "Y" || answer == "yes";
    }
private:
    InputSource& in_;
    OutputSink&  out_;
};

// The default permission policy for run_command (spec §5.4).
std::unique_ptr<PolicyChain> build_default_policy(const Clock& clock) {
    auto chain = std::make_unique<PolicyChain>();
    chain->add(std::make_unique<ArgumentPatternPolicy>(std::vector<std::string>{
        ";", "|", "&", "`", "$(", ">", "<", "rm -rf", "sudo"}));
    chain->add(std::make_unique<AllowlistPolicy>(std::vector<std::string>{
        "ls", "cat", "echo", "grep", "wc", "head", "tail", "git", "make", "g++", "python3"}));
    chain->add(std::make_unique<ConfirmPolicy>(std::vector<std::string>{"git", "make", "g++", "python3"}));
    chain->add(std::make_unique<RateLimitPolicy>(5, 60.0, clock));
    return chain;
}

std::string build_system_message(const ToolRegistry& registry, const SkillCatalog* skills) {
    std::string s = "You are MiniHarness, a helpful assistant.\n"
                    "To use a tool, emit <|tool_call|>{\"id\":\"...\",\"name\":\"...\",\"arguments\":{...}}<|/tool_call|>\n"
                    "Available tools:\n" +
                    describe_tools(registry);
    if (skills != nullptr && !skills->skills().empty()) {
        s += "Available skills (load one with load_skill):\n";
        for (const SkillMeta& m : skills->skills()) s += "- " + m.name + ": " + m.description + "\n";
    }
    return s;
}

int usage(const char* argv0) {
    std::cerr << "usage: " << argv0
              << " [--script FILE | --replay FILE] [--save FILE] [--max-turns N]\n"
                 "       [--max-tool-steps N] [--workspace DIR] [--no-tools] [--tools]\n"
                 "       [--skills DIR] [--memory FILE] [--audit FILE]\n"
                 "       [--context-budget N] [--keep-recent N]\n";
    return 2;
}

}  // namespace

int main(int argc, char* argv[]) {
    std::string script_path = "scripts/greeting.script";
    std::string replay_path, save_path, workspace_dir = "workspace";
    std::string skills_dir, memory_file, audit_file;
    bool list_tools = false, no_tools = false;
    HarnessConfig config;

    try {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            auto next = [&]() -> std::string {
                if (i + 1 >= argc) throw std::invalid_argument("missing value for " + arg);
                return argv[++i];
            };
            if      (arg == "--script")         script_path = next();
            else if (arg == "--replay")         replay_path = next();
            else if (arg == "--save")           save_path = next();
            else if (arg == "--max-turns")      config.max_turns = std::stoi(next());
            else if (arg == "--max-tool-steps") config.max_tool_steps = std::stoi(next());
            else if (arg == "--workspace")      workspace_dir = next();
            else if (arg == "--tools")          list_tools = true;
            else if (arg == "--no-tools")       no_tools = true;
            else if (arg == "--skills")         skills_dir = next();
            else if (arg == "--memory")         memory_file = next();
            else if (arg == "--audit")          audit_file = next();
            else if (arg == "--context-budget") config.context_budget = static_cast<std::size_t>(std::stoul(next()));
            else if (arg == "--keep-recent")    config.keep_recent = static_cast<std::size_t>(std::stoul(next()));
            else return usage(argv[0]);
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return usage(argv[0]);
    }

    try {
        // With --no-tools nothing from P3 is constructed, so the harness runs
        // even before your P3 classes work.
        std::optional<ToolRegistry> registry;
        std::optional<ToolContext>  ctx;
        if (!no_tools || list_tools) {
            registry.emplace();
            register_builtin_tools(*registry);
        }

        if (list_tools) {
            std::cout << describe_tools(*registry);
            return 0;
        }

        StdioInput in;
        StdioOutput out;

        // Final project services. They live here, in main, and the tools
        // reach them through the ToolContext pointers.
        MemoryStore                  memory;
        SkillCatalog                 skills;
        SteadyClock                  clock;
        std::unique_ptr<PolicyChain> policy;
        std::optional<AuditLog>      audit;
        TerminalConfirmer            confirmer(in, out);
        bool                         have_skills = false;

        if (!no_tools) {
            std::filesystem::create_directories(workspace_dir);
            ctx.emplace(Workspace(workspace_dir));

            if (!memory_file.empty() && std::filesystem::exists(memory_file)) {
                std::string err;
                if (!memory.load(memory_file, &err))
                    std::cerr << "warning: could not load " << memory_file << ": " << err << "\n";
            }
            ctx->memory = &memory;
            ctx->memory_file = memory_file;

            if (skills_dir.empty() && std::filesystem::is_directory("skills")) skills_dir = "skills";
            if (!skills_dir.empty()) {
                SkillCatalog::ScanReport rep = skills.scan(skills_dir);
                for (const std::string& e : rep.errors) std::cerr << "warning: skill skipped: " << e << "\n";
                have_skills = true;
                ctx->skills = &skills;
            }

            policy = build_default_policy(clock);
            audit.emplace(audit_file);
            ctx->policy = policy.get();
            ctx->confirmer = &confirmer;
            ctx->audit = &*audit;
        }

        std::unique_ptr<ModelClient> model;
        if (!replay_path.empty()) model = std::make_unique<ReplayModelClient>(replay_path);
        else                      model = std::make_unique<ScriptedModelClient>(script_path);

        std::unique_ptr<Harness> harness;
        if (no_tools) {
            harness = std::make_unique<Harness>(std::move(model), config);
        } else {
            config.system_message = build_system_message(*registry, have_skills ? &skills : nullptr);
            harness = std::make_unique<Harness>(std::move(model), config, &*registry, &*ctx);
        }

        StopReason reason = harness->run(in, out);
        std::cout << "[conversation ended: " << reason.detail << "]\n";

        if (!save_path.empty()) {
            if (save_transcript(harness->conversation(), save_path))
                std::cout << "[transcript saved to " << save_path << "]\n";
            else
                std::cerr << "error: could not write " << save_path << "\n";
        }
        return reason.kind == StopReason::Kind::ClientError ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
}
