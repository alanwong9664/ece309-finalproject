#pragma once
#include "core/conversation.h"

#include <string>

bool save_transcript(const Conversation& conv, const std::string& path);

bool load_transcript(const std::string& path, Conversation* out, std::string* error);
