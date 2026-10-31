// include/harness/harness.h
//
// PROVIDED. The P2 turn loop, extended with the P3 tool loop:
//
//   user turn ──► model ──► [tool calls?] ──yes──► run tools, append results
//                   ▲                                   │
//                   └──────────── (no user turn) ───────┘
//                               no
//                               └──► back to the user
//
// Constructing a Harness without a ToolRegistry gives exactly the P2
// behavior, so every P2 test still compiles and passes unchanged.
//
// Final project: optional context budget (HarnessConfig::context_budget).

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
    int         max_turns = 20;       // user turns before TurnLimit
    int         max_tool_steps = 8;   // tool rounds allowed per user turn
    std::string system_message;       // appended first if non-empty

    // Final project: if context_budget > 0, every model call receives only
    // the messages chosen by YOUR select_context(); the full conversation is
    // still kept (and saved by --save).
    std::size_t context_budget = 0;   // tokens; 0 = send everything
    std::size_t keep_recent = 4;      // newest messages that are never dropped
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
    // `tools` and `ctx` are optional and NOT owned; if given, they must
    // outlive the Harness. Pass both or neither.
    Harness(std::unique_ptr<ModelClient> model, HarnessConfig cfg,
            ToolRegistry* tools = nullptr, ToolContext* ctx = nullptr);

    StopReason run(InputSource& in, OutputSink& out);

    const Conversation& conversation() const { return conv_; }

    // Final project: the messages the model would receive right now.
    Conversation context_for_model(OutputSink* report = nullptr) const;

private:
    void print_call(OutputSink& out, const DispatchedCall& call) const;
    void append(Message m, bool pinned = false);

    std::unique_ptr<ModelClient>  model_;
    Conversation                  conv_;
    std::vector<bool>             pinned_;   // parallel to conv_: load_skill results
    HarnessConfig                 cfg_;
    ToolContext*                  ctx_ = nullptr;
    std::optional<ToolDispatcher> dispatcher_;
};
