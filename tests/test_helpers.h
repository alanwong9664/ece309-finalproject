#pragma once
#include "harness/harness.h"
#include "json/json_value.h"
#include "policy/policy.h"
#include "tools/tool.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include <stdlib.h>

namespace test_helpers {

namespace fs = std::filesystem;

class TempDir {
public:
    TempDir() {
        std::string tmpl = (fs::temp_directory_path() / "mh_p3_XXXXXX").string();
        std::vector<char> buf(tmpl.begin(), tmpl.end());
        buf.push_back('\0');
        if (mkdtemp(buf.data()) == nullptr) throw std::runtime_error("mkdtemp failed");
        path_ = fs::path(buf.data());
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    const fs::path& path() const { return path_; }
private:
    fs::path path_;
};

inline void write_text(const fs::path& p, const std::string& s) {
    fs::create_directories(p.parent_path());
    std::ofstream(p, std::ios::binary) << s;
}

inline std::string read_text(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), {});
}

inline JsonValue parse_ok(const std::string& text) {
    std::string err = "not cleared";
    JsonValue v = JsonValue::parse(text, &err);
    if (!err.empty()) throw std::runtime_error("unexpected parse error for " + text + ": " + err);
    return v;
}

inline bool parse_fails(const std::string& text, std::size_t depth = JsonValue::kDefaultMaxDepth) {
    std::string err;
    JsonValue v = JsonValue::parse(text, &err, depth);
    return !err.empty() && v.type() == JsonValue::Type::Null;
}

inline JsonValue args1(const std::string& k, JsonValue v) {
    JsonValue o = JsonValue::object();
    o.set(k, std::move(v));
    return o;
}

class EchoTool : public Tool {
public:
    explicit EchoTool(std::string name = "echo") : name_(std::move(name)), schema_(JsonValue::object()) {
        schema_.set("type", JsonValue::string("object"));
    }
    std::string name() const override { return name_; }
    std::string description() const override { return "Echo the 'text' argument."; }
    const JsonValue& schema() const override { return schema_; }
    ToolResult invoke(const JsonValue& args, ToolContext&) override {
        ++calls;
        const JsonValue* t = args.find("text");
        if (!t || t->type() != JsonValue::Type::String) return ToolResult::failure("need text");
        return ToolResult::success(t->as_string());
    }
    int calls = 0;
private:
    std::string name_;
    JsonValue   schema_;
};

class ThrowingTool : public EchoTool {
public:
    ThrowingTool() : EchoTool("explode") {}
    ToolResult invoke(const JsonValue&, ToolContext&) override { throw std::runtime_error("boom"); }
};

class LinesInput : public InputSource {
public:
    explicit LinesInput(std::vector<std::string> lines) : lines_(std::move(lines)) {}
    std::string read_line() override {
        ++reads;
        if (i_ >= lines_.size()) { eof_ = true; return ""; }
        return lines_[i_++];
    }
    bool is_eof() const override { return eof_; }
    int reads = 0;
private:
    std::vector<std::string> lines_;
    std::size_t i_ = 0;
    bool eof_ = false;
};

class StringOutput : public OutputSink {
public:
    std::string data;
    void write(std::string_view t) override { data += t; }
};

class ListModel : public ModelClient {
public:
    using ModelClient::generate;
    explicit ListModel(std::vector<std::string> replies, int* calls = nullptr,
                       std::vector<Role>* last_roles = nullptr)
        : replies_(std::move(replies)), calls_(calls), last_roles_(last_roles) {}
    void generate(const Conversation& conv, TokenSink& sink) override {
        if (calls_) ++*calls_;
        if (last_roles_ && conv.size() > 0) last_roles_->push_back(conv.at(conv.size() - 1).role());
        if (i_ >= replies_.size()) throw std::runtime_error("ListModel exhausted");
        const std::string& r = replies_[i_++];
        for (std::size_t k = 0; k < r.size(); k += 5) sink.on_chunk(r.substr(k, 5));
        sink.on_complete();
    }
private:
    std::vector<std::string> replies_;
    std::size_t i_ = 0;
    int* calls_;
    std::vector<Role>* last_roles_;
};

inline std::string call(const std::string& id, const std::string& name, const std::string& args_json) {
    return "<|tool_call|>{\"id\":\"" + id + "\",\"name\":\"" + name + "\",\"arguments\":" +
           args_json + "}<|/tool_call|>";
}

inline const JsonValue& field(const JsonValue& obj, const char* key) {
    const JsonValue* v = obj.find(key);
    if (v == nullptr) throw std::runtime_error(std::string("missing JSON member: ") + key);
    return *v;
}

inline JsonValue result_json(const Message& m) {
    const std::string& s = m.content();
    const std::string open = "<|tool_result|>", close = "<|/tool_result|>";
    if (s.compare(0, open.size(), open) != 0 || s.size() < open.size() + close.size() ||
        s.compare(s.size() - close.size(), close.size(), close) != 0)
        throw std::runtime_error("not a tool result: " + s);
    return parse_ok(s.substr(open.size(), s.size() - open.size() - close.size()));
}

class FixedPolicy : public Policy {
public:
    explicit FixedPolicy(Decision::Kind k, std::string name = "fixed") : kind_(k), name_(std::move(name)) {}
    std::string name() const override { return name_; }
    Decision evaluate(const CommandRequest&) override {
        ++calls;
        return {kind_, std::string("fixed ") + to_string(kind_), name_};
    }
    int calls = 0;
private:
    Decision::Kind kind_;
    std::string    name_;
};

class ScriptedConfirmer : public Confirmer {
public:
    explicit ScriptedConfirmer(std::vector<bool> answers) : answers_(std::move(answers)) {}
    bool confirm(const CommandRequest& req, const std::string&) override {
        asked.push_back(req.command_line());
        return i_ < answers_.size() ? answers_[i_++] : false;
    }
    std::vector<std::string> asked;
private:
    std::vector<bool> answers_;
    std::size_t i_ = 0;
};

inline void write_skill(const fs::path& root, const std::string& name, const std::string& description,
                        const std::vector<std::string>& requires_list, const std::string& body) {
    std::string fm = "---\nname: " + name + "\ndescription: " + description + "\n";
    if (!requires_list.empty()) {
        fm += "requires: [";
        for (std::size_t i = 0; i < requires_list.size(); ++i) fm += (i ? ", " : "") + requires_list[i];
        fm += "]\n";
    }
    write_text(root / name / "SKILL.md", fm + "---\n" + body + "\n");
}

}

using namespace test_helpers;
