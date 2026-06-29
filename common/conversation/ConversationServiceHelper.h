#pragma once

#include <string>

namespace nebula {

class ConversationServiceHelper {
public:
    static std::string buildPreview(const std::string& content, size_t max_len = 120);
};

}  // namespace nebula
