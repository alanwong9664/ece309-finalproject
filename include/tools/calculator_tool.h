#pragma once
#include "tools/tool.h"

#include <string>
#include <string_view>

class CalculatorTool : public Tool {
public:
    CalculatorTool();

    std::string      name() const override;
    std::string      description() const override;
    const JsonValue& schema() const override;
    ToolResult       invoke(const JsonValue& args, ToolContext& ctx) override;

    static bool evaluate(std::string_view expression, double* out, std::string* error);

private:
    JsonValue schema_;
};
