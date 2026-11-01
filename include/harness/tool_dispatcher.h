#pragma once
#include "core/message.h"
#include "tools/tool_registry.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

inline constexpr std::string_view kToolCallOpen    = "<|tool_call|>";
inline constexpr std::string_view kToolCallClose   = "<|/tool_call|>";
inline constexpr std::string_view kToolResultOpen  = "<|tool_result|>";
inline constexpr std::string_view kToolResultClose = "<|/tool_result|>";

struct DispatchedCall {
    std::string id;
    std::string tool_name;
    ToolResult  result;
};

class ToolDispatcher {
public:
    explicit ToolDispatcher(ToolRegistry& registry, std::size_t max_calls_per_message = 16);

    static bool has_tool_call(std::string_view text);

    std::vector<Message> handle(const Message& assistant_msg, ToolContext& ctx,
                                std::vector<DispatchedCall>* calls_out = nullptr);

    static std::string format_result(const std::string& id, const ToolResult& r);

private:
    ToolRegistry& registry_;
    std::size_t   max_calls_;
};
