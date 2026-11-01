#include "model/scripted_client.h"
#include <fstream>
#include <stdexcept>

static std::string trim_left(const std::string& s) {
    size_t start = s.find_first_not_of(" \t");
    return (start == std::string::npos) ? "" : s.substr(start);
}

ScriptedModelClient::ScriptedModelClient(const std::string& script_path) {
    std::ifstream file(script_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open script file: " + script_path);
    }

    std::string line;
    int current_chunk = -1;
    bool in_assistant_block = false;
    std::string current_content;

    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == "---") {

            if (in_assistant_block) {
                blocks_.push_back({current_content, current_chunk});
                current_content.clear();
                in_assistant_block = false;
            }
            current_chunk = -1;
            continue;
        }

        if (!in_assistant_block) {

            if (line.rfind("chunk:", 0) == 0) {
                try {
                    current_chunk = std::stoi(trim_left(line.substr(6)));
                } catch (...) {
                    current_chunk = -1;
                }
            } else if (line.rfind("match:", 0) == 0) {

                continue;
            } else if (line == "role: assistant") {
                in_assistant_block = true;
            }

        } else {

            if (!current_content.empty()) {
                current_content += "\n";
            }
            current_content += line;
        }
    }

    if (in_assistant_block) {
        blocks_.push_back({current_content, current_chunk});
    }
}

void ScriptedModelClient::generate(const Conversation&  , TokenSink& sink) {

    if (current_block_idx_ >= blocks_.size()) {
        throw std::runtime_error("script exhausted: no more assistant blocks");
    }

    const ScriptBlock& block = blocks_[current_block_idx_++];

    if (block.chunk_size > 0) {
        for (std::size_t i = 0; i < block.content.size(); i += block.chunk_size) {
            sink.on_chunk(block.content.substr(i, block.chunk_size));
        }
    } else {

        sink.on_chunk(block.content);
    }

    sink.on_complete();
}
