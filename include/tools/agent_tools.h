// include/tools/agent_tools.h
//
// PROVIDED. The final project's tools. Each one is a thin wrapper: it checks
// its JSON arguments, then calls the provided MemoryStore / SkillCatalog /
// policy code, which in turn calls YOUR data structures.
//
//   remember        {"text": string}                    -> "remembered m3"
//   recall          {"query": string, "k"?: number}     -> "m3: ..." lines, best first
//   forget          {"id": "m3"}                        -> "forgot m3"
//   list_memories   {}                                  -> every memory, oldest first
//   list_skills     {"query"?: string, "k"?: number}    -> "name: description" lines
//   load_skill      {"name": string}                    -> bodies, dependencies first
//   skill_dependents{"name": string}                    -> skills that need it, nearest first
//   run_command     {"program": string, "args"?: [string]}
//        -> goes through ctx.policy (YOUR PolicyChain); Ask is resolved by
//           ctx.confirmer; the final decision is written to ctx.audit BEFORE
//           the command would run. Commands are SIMULATED: nothing is ever
//           executed.

#pragma once
#include "tools/tool.h"

#define MH_DECLARE_TOOL(Class, tool_name)                                  \
    class Class : public Tool {                                            \
    public:                                                                \
        Class();                                                           \
        std::string      name() const override { return tool_name; }      \
        std::string      description() const override;                    \
        const JsonValue& schema() const override { return schema_; }      \
        ToolResult       invoke(const JsonValue& args, ToolContext& ctx) override; \
    private:                                                               \
        JsonValue schema_;                                                 \
    };

MH_DECLARE_TOOL(RememberTool, "remember")
MH_DECLARE_TOOL(RecallTool, "recall")
MH_DECLARE_TOOL(ForgetTool, "forget")
MH_DECLARE_TOOL(ListMemoriesTool, "list_memories")
MH_DECLARE_TOOL(ListSkillsTool, "list_skills")
MH_DECLARE_TOOL(LoadSkillTool, "load_skill")
MH_DECLARE_TOOL(SkillDependentsTool, "skill_dependents")
MH_DECLARE_TOOL(RunCommandTool, "run_command")

#undef MH_DECLARE_TOOL
