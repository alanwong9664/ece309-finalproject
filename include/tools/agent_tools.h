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
