#pragma once
#include "tools/tool_registry.h"

#include <string>

std::size_t register_builtin_tools(ToolRegistry& registry);

std::string describe_tools(const ToolRegistry& registry);
