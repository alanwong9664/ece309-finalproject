#pragma once
#include "tools/tool.h"

class ReadFileTool : public Tool {
public:
    ReadFileTool();
    std::string      name() const override { return "read_file"; }
    std::string      description() const override;
    const JsonValue& schema() const override { return schema_; }
    ToolResult       invoke(const JsonValue& args, ToolContext& ctx) override;
private:
    JsonValue schema_;
};

class WriteFileTool : public Tool {
public:
    WriteFileTool();
    std::string      name() const override { return "write_file"; }
    std::string      description() const override;
    const JsonValue& schema() const override { return schema_; }
    ToolResult       invoke(const JsonValue& args, ToolContext& ctx) override;
private:
    JsonValue schema_;
};

class ListDirTool : public Tool {
public:
    ListDirTool();
    std::string      name() const override { return "list_dir"; }
    std::string      description() const override;
    const JsonValue& schema() const override { return schema_; }
    ToolResult       invoke(const JsonValue& args, ToolContext& ctx) override;
private:
    JsonValue schema_;
};
