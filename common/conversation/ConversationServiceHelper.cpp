#include "common/conversation/ConversationServiceHelper.h"

namespace nebula {

std::string ConversationServiceHelper::buildPreview(const std::string& content, size_t max_len) {
    if (content.size() <= max_len) return content;
    return content.substr(0, max_len);
}

}  // namespace nebula
