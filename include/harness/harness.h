#pragma once
#include "harness/tool_dispatcher.h"
#include "model/model_client.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

inline constexpr std::string_view kEndSentinel = "<|end_conversation|>";

struct HarnessConfig {
    int         max_turns = 20;
    int         max_tool_steps = 8;
    std::string system_message;

    std::size_t context_budget = 0;
    std::size_t keep_recent = 4;
};

class InputSource {
public:
    virtual ~InputSource() = default;
    virtual std::string read_line() = 0;
    virtual bool is_eof() const = 0;
};

class OutputSink {
public:
    virtual ~OutputSink() = default;
    virtual void write(std::string_view text) = 0;
};

class Harness {
public:

    Harness(std::unique_ptr<ModelClient> model, HarnessConfig cfg,
            ToolRegistry* tools = nullptr, ToolContext* ctx = nullptr);

    StopReason run(InputSource& in, OutputSink& out);

    const Conversation& conversation() const { return conv_; }

    Conversation context_for_model(OutputSink* report = nullptr) const;

private:
    void print_call(OutputSink& out, const DispatchedCall& call) const;
    void append(Message m, bool pinned = false);

    std::unique_ptr<ModelClient>  model_;
    Conversation                  conv_;
    std::vector<bool>             pinned_;
    HarnessConfig                 cfg_;
    ToolContext*                  ctx_ = nullptr;
    std::optional<ToolDispatcher> dispatcher_;
};
