#include "common/conversation/ConversationServiceHelper.h"

#include <cassert>
#include <string>

namespace {
bool isValidUtf8(const std::string& value) {
    size_t index = 0;
    while (index < value.size()) {
        unsigned char lead = static_cast<unsigned char>(value[index]);
        size_t width = 0;
        if ((lead & 0x80) == 0) width = 1;
        else if ((lead & 0xE0) == 0xC0) width = 2;
        else if ((lead & 0xF0) == 0xE0) width = 3;
        else if ((lead & 0xF8) == 0xF0) width = 4;
        else return false;

        if (index + width > value.size()) return false;
        for (size_t offset = 1; offset < width; ++offset) {
            unsigned char next = static_cast<unsigned char>(value[index + offset]);
            if ((next & 0xC0) != 0x80) return false;
        }
        index += width;
    }
    return true;
}
}  // namespace

int main() {
    std::string ascii = "abcdefghijklmnopqrstuvwxyz";
    assert(nebula::ConversationServiceHelper::buildPreview(ascii, 10) == "abcdefghij");

    std::string chinese = "律师函?就算是uu含我也不虚！先边缘ob一波，看她怎么说！诶，她想吃我大鸟，不慌让她吃，我往后拉，诶，再拉，还想含我不存在的！";
    std::string preview = nebula::ConversationServiceHelper::buildPreview(chinese, 120);
    assert(preview.size() <= 120);
    assert(isValidUtf8(preview));
    assert(preview == chinese.substr(0, preview.size()));

    std::string single = "你";
    assert(nebula::ConversationServiceHelper::buildPreview(single, 1).empty());
    assert(nebula::ConversationServiceHelper::buildPreview(single, 2).empty());
    assert(nebula::ConversationServiceHelper::buildPreview(single, 3) == single);

    return 0;
}
