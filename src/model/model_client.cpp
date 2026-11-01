#include "model/model_client.h"

class StringSink : public TokenSink {
public:
    std::string content;
    void on_chunk(std::string_view chunk) override { content += chunk; }
    void on_complete() override {}
};

Message ModelClient::generate(const Conversation& conv) {
    StringSink sink;
    generate(conv, sink);
    return Message(Role::Assistant, std::move(sink.content));
}
