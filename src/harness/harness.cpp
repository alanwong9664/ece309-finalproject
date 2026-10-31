// src/harness/harness.cpp — PROVIDED.

#include "harness/harness.h"
#include "core/sentinel_scanner.h"
#include "memory/context_builder.h"
#include "util/text.h"

#include <exception>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

// Pipes model output to the terminal through YOUR SentinelScanner, and keeps
// the text that was shown so it can be stored in the conversation.
class StreamingSink : public TokenSink {
public:
    explicit StreamingSink(OutputSink& out) : out_(out), scanner_(std::string(kEndSentinel)) {}

    void on_chunk(std::string_view chunk) override {
        if (stopped_) return;
        auto res = scanner_.feed(chunk);
        out_.write(res.safe_text);
        text_ += res.safe_text;
        if (res.sentinel_found) stopped_ = true;
    }

    void on_complete() override {
        if (stopped_) return;
        auto res = scanner_.flush();
        out_.write(res.safe_text);
        text_ += res.safe_text;
    }

    bool stopped() const { return stopped_; }
    const std::string& text() const { return text_; }

private:
    OutputSink&     out_;
    SentinelScanner scanner_;
    bool            stopped_ = false;
    std::string     text_;
};

std::string preview(const std::string& s, std::size_t limit = 60) {
    std::string out;
    for (char c : s) {
        if (out.size() >= limit) { out += "..."; break; }
        if (c == '\n') out += "\\n";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

}  // namespace

Harness::Harness(std::unique_ptr<ModelClient> model, HarnessConfig cfg,
                 ToolRegistry* tools, ToolContext* ctx)
    : model_(std::move(model)), cfg_(std::move(cfg)), ctx_(ctx) {
    if ((tools == nullptr) != (ctx == nullptr))
        throw std::invalid_argument("Harness: pass both a ToolRegistry and a ToolContext, or neither");
    if (tools) dispatcher_.emplace(*tools);
    if (!cfg_.system_message.empty()) append(Message(Role::System, cfg_.system_message));
}

void Harness::append(Message m, bool pinned) {
    conv_.append(std::move(m));
    pinned_.push_back(pinned);
}

Conversation Harness::context_for_model(OutputSink* report) const {
    if (cfg_.context_budget == 0) return conv_;
    std::vector<ContextItem> items;
    items.reserve(conv_.size());
    for (std::size_t i = 0; i < conv_.size(); ++i) {
        const Message& m = conv_.at(i);
        items.push_back({m.role(), count_tokens(m.content()), pinned_[i]});
    }
    ContextPlan plan = select_context(items, cfg_.context_budget, cfg_.keep_recent);
    Conversation sent;
    for (std::size_t i : plan.keep) {
        if (i >= conv_.size()) throw std::logic_error("select_context returned a bad index");
        sent.append(conv_.at(i));
    }
    if (report && plan.keep.size() < conv_.size()) {
        report->write("[context: sent " + std::to_string(plan.keep.size()) + " of " +
                      std::to_string(conv_.size()) + " messages, " +
                      std::to_string(plan.total_tokens) + " tokens" +
                      (plan.over_budget ? ", over budget" : "") + "]\n");
    }
    return sent;
}

void Harness::print_call(OutputSink& out, const DispatchedCall& call) const {
    std::string line = "[tool ";
    line += call.tool_name.empty() ? "?" : call.tool_name;
    if (!call.id.empty()) line += " " + call.id;
    line += call.result.ok ? " -> ok] " : " -> error] ";
    line += preview(call.result.ok ? call.result.content : call.result.error);
    line += "\n";
    out.write(line);
}

StopReason Harness::run(InputSource& in, OutputSink& out) {
    int turns = 0;

    while (turns < cfg_.max_turns) {
        out.write("you> ");
        std::string user_text = in.read_line();
        if (in.is_eof()) return {StopReason::Kind::UserExit, "EOF detected"};
        if (user_text.empty()) continue;

        append(Message(Role::User, user_text));

        // Inner tool loop: keep asking the model, without a new user turn,
        // for as long as it keeps calling tools (bounded by max_tool_steps).
        for (int step = 0;; ++step) {
            Conversation sent;   // only filled when a context budget is set
            if (cfg_.context_budget > 0) {
                try {
                    sent = context_for_model(&out);
                } catch (const std::exception& e) {
                    return {StopReason::Kind::ClientError, e.what()};
                }
            }
            out.write("assistant> ");
            StreamingSink sink(out);
            try {
                model_->generate(cfg_.context_budget > 0 ? sent : conv_, sink);
            } catch (const std::exception& e) {
                out.write("\n");
                return {StopReason::Kind::ClientError, e.what()};
            }
            out.write("\n");

            // Store what the model said, including the sentinel if it was
            // seen, so a saved transcript replays to the same ending.
            std::string reply = sink.text();
            if (sink.stopped()) reply += kEndSentinel;
            append(Message(Role::Assistant, reply));

            if (sink.stopped()) return {StopReason::Kind::Sentinel, "Stop sentinel detected"};
            if (!dispatcher_ || !ToolDispatcher::has_tool_call(reply)) break;

            if (step >= cfg_.max_tool_steps) {
                out.write("[tool step limit reached; returning to user]\n");
                break;
            }

            std::vector<DispatchedCall> calls;
            std::vector<Message> results =
                dispatcher_->handle(conv_.at(conv_.size() - 1), *ctx_, &calls);
            for (const DispatchedCall& c : calls) print_call(out, c);
            for (std::size_t i = 0; i < results.size(); ++i) {
                // Skill instructions stay in context: pin successful load_skill results.
                bool pin = i < calls.size() && calls[i].tool_name == "load_skill" && calls[i].result.ok;
                append(std::move(results[i]), pin);
            }
        }
        ++turns;
    }
    return {StopReason::Kind::TurnLimit, "Max turn limit reached"};
}
