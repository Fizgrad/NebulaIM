#include "common/websocket/WebSocketFrame.h"

#include <array>
#include <random>

namespace nebula {

namespace {

bool isControlOpCode(WebSocketOpCode opcode) {
    return opcode == WebSocketOpCode::CLOSE || opcode == WebSocketOpCode::PING || opcode == WebSocketOpCode::PONG;
}

bool isKnownOpCode(uint8_t opcode) {
    switch (static_cast<WebSocketOpCode>(opcode)) {
        case WebSocketOpCode::CONTINUATION:
        case WebSocketOpCode::TEXT:
        case WebSocketOpCode::BINARY:
        case WebSocketOpCode::CLOSE:
        case WebSocketOpCode::PING:
        case WebSocketOpCode::PONG:
            return true;
    }
    return false;
}

uint64_t readBigEndian(const char* data, size_t len) {
    uint64_t value = 0;
    for (size_t i = 0; i < len; ++i) {
        value = (value << 8) | static_cast<unsigned char>(data[i]);
    }
    return value;
}

void appendBigEndian(std::string* out, uint64_t value, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        const size_t shift = (len - 1 - i) * 8;
        out->push_back(static_cast<char>((value >> shift) & 0xFF));
    }
}

std::array<unsigned char, 4> randomMask() {
    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, 255);
    return {static_cast<unsigned char>(dist(rng)),
            static_cast<unsigned char>(dist(rng)),
            static_cast<unsigned char>(dist(rng)),
            static_cast<unsigned char>(dist(rng))};
}

}  // namespace

WebSocketFrameStatus WebSocketFrameCodec::decode(const char* data,
                                                 size_t len,
                                                 bool require_mask,
                                                 WebSocketFrame* frame,
                                                 size_t* consumed) {
    if (frame == nullptr || consumed == nullptr || data == nullptr) {
        return WebSocketFrameStatus::PROTOCOL_ERROR;
    }
    *consumed = 0;
    if (len < 2) {
        return WebSocketFrameStatus::INCOMPLETE;
    }

    const auto b0 = static_cast<unsigned char>(data[0]);
    const auto b1 = static_cast<unsigned char>(data[1]);
    const bool fin = (b0 & 0x80) != 0;
    const bool rsv = (b0 & 0x70) != 0;
    const auto opcode_raw = static_cast<uint8_t>(b0 & 0x0F);
    if (rsv || !isKnownOpCode(opcode_raw)) {
        return WebSocketFrameStatus::PROTOCOL_ERROR;
    }

    WebSocketOpCode opcode = static_cast<WebSocketOpCode>(opcode_raw);
    if (!fin || opcode == WebSocketOpCode::CONTINUATION) {
        return WebSocketFrameStatus::PROTOCOL_ERROR;
    }

    const bool masked = (b1 & 0x80) != 0;
    if (require_mask && !masked) {
        return WebSocketFrameStatus::PROTOCOL_ERROR;
    }

    uint64_t payload_len = b1 & 0x7F;
    size_t offset = 2;
    if (payload_len == 126) {
        if (len < offset + 2) return WebSocketFrameStatus::INCOMPLETE;
        payload_len = readBigEndian(data + offset, 2);
        offset += 2;
    } else if (payload_len == 127) {
        if (len < offset + 8) return WebSocketFrameStatus::INCOMPLETE;
        payload_len = readBigEndian(data + offset, 8);
        offset += 8;
        if ((payload_len >> 63) != 0) {
            return WebSocketFrameStatus::PROTOCOL_ERROR;
        }
    }

    if (isControlOpCode(opcode) && payload_len > 125) {
        return WebSocketFrameStatus::PROTOCOL_ERROR;
    }

    std::array<unsigned char, 4> mask_key{};
    if (masked) {
        if (len < offset + mask_key.size()) return WebSocketFrameStatus::INCOMPLETE;
        for (size_t i = 0; i < mask_key.size(); ++i) {
            mask_key[i] = static_cast<unsigned char>(data[offset + i]);
        }
        offset += mask_key.size();
    }

    if (payload_len > static_cast<uint64_t>(len - offset)) {
        return WebSocketFrameStatus::INCOMPLETE;
    }
    if (payload_len > static_cast<uint64_t>(static_cast<size_t>(-1))) {
        return WebSocketFrameStatus::PROTOCOL_ERROR;
    }

    std::string payload(data + offset, static_cast<size_t>(payload_len));
    if (masked) {
        for (size_t i = 0; i < payload.size(); ++i) {
            payload[i] = static_cast<char>(static_cast<unsigned char>(payload[i]) ^ mask_key[i % 4]);
        }
    }

    frame->fin = fin;
    frame->opcode = opcode;
    frame->masked = masked;
    frame->payload = std::move(payload);
    *consumed = offset + static_cast<size_t>(payload_len);
    return WebSocketFrameStatus::OK;
}

std::string WebSocketFrameCodec::encode(WebSocketOpCode opcode,
                                        const std::string& payload,
                                        bool mask_payload) {
    std::string out;
    out.reserve(payload.size() + 14);
    out.push_back(static_cast<char>(0x80 | static_cast<uint8_t>(opcode)));

    const uint8_t mask_bit = mask_payload ? 0x80 : 0x00;
    if (payload.size() <= 125) {
        out.push_back(static_cast<char>(mask_bit | static_cast<uint8_t>(payload.size())));
    } else if (payload.size() <= 0xFFFF) {
        out.push_back(static_cast<char>(mask_bit | 126));
        appendBigEndian(&out, payload.size(), 2);
    } else {
        out.push_back(static_cast<char>(mask_bit | 127));
        appendBigEndian(&out, payload.size(), 8);
    }

    if (!mask_payload) {
        out.append(payload);
        return out;
    }

    auto mask_key = randomMask();
    for (unsigned char b : mask_key) {
        out.push_back(static_cast<char>(b));
    }
    for (size_t i = 0; i < payload.size(); ++i) {
        out.push_back(static_cast<char>(static_cast<unsigned char>(payload[i]) ^ mask_key[i % 4]));
    }
    return out;
}

}  // namespace nebula
