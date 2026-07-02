#include "common/conversation/ConversationServiceHelper.h"

namespace nebula {
namespace {

bool isContinuationByte(unsigned char value) {
    return (value & 0xC0) == 0x80;
}

size_t utf8SafePrefixLength(const std::string& value, size_t max_len) {
    if (value.size() <= max_len) return value.size();

    size_t end = max_len;
    while (end > 0 && isContinuationByte(static_cast<unsigned char>(value[end]))) {
        --end;
    }
    if (end == max_len) return end;

    unsigned char lead = static_cast<unsigned char>(value[end]);
    size_t expected_len = 1;
    if ((lead & 0xE0) == 0xC0) expected_len = 2;
    else if ((lead & 0xF0) == 0xE0) expected_len = 3;
    else if ((lead & 0xF8) == 0xF0) expected_len = 4;
    else return end;

    return end + expected_len <= max_len ? end + expected_len : end;
}

}  // namespace

std::string ConversationServiceHelper::buildPreview(const std::string& content, size_t max_len) {
    if (content.size() <= max_len) return content;
    return content.substr(0, utf8SafePrefixLength(content, max_len));
}

}  // namespace nebula
