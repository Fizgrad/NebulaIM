#include "common/websocket/WebSocketHandshake.h"

#include <openssl/evp.h>
#include <openssl/sha.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

namespace nebula {

namespace {

constexpr const char* kWebSocketGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

std::string trim(const std::string& input) {
    size_t begin = 0;
    while (begin < input.size() && std::isspace(static_cast<unsigned char>(input[begin]))) ++begin;
    size_t end = input.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(input[end - 1]))) --end;
    return input.substr(begin, end - begin);
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool containsToken(const std::string& value, const std::string& token) {
    std::stringstream ss(value);
    std::string part;
    const std::string needle = lower(token);
    while (std::getline(ss, part, ',')) {
        if (lower(trim(part)) == needle) return true;
    }
    return false;
}

std::string header(const WebSocketHandshakeRequest& request, const std::string& name) {
    auto it = request.headers.find(lower(name));
    return it == request.headers.end() ? "" : it->second;
}

std::string base64Encode(const unsigned char* data, size_t len) {
    std::vector<unsigned char> out(4 * ((len + 2) / 3) + 1);
    int written = EVP_EncodeBlock(out.data(), data, static_cast<int>(len));
    if (written <= 0) return "";
    return std::string(reinterpret_cast<char*>(out.data()), static_cast<size_t>(written));
}

}  // namespace

WebSocketHandshakeResult WebSocketHandshake::buildServerResponse(const std::string& http_request) {
    WebSocketHandshakeResult result;
    std::string error;
    if (!parseRequest(http_request, &result.request, &error)) {
        result.ok = false;
        result.error = error;
        result.response = badRequestResponse(error);
        return result;
    }

    const std::string upgrade = lower(header(result.request, "Upgrade"));
    const std::string connection = header(result.request, "Connection");
    const std::string version = trim(header(result.request, "Sec-WebSocket-Version"));
    const std::string key = trim(header(result.request, "Sec-WebSocket-Key"));

    if (result.request.method != "GET") {
        error = "method must be GET";
    } else if (upgrade != "websocket") {
        error = "Upgrade must be websocket";
    } else if (!containsToken(connection, "Upgrade")) {
        error = "Connection must contain Upgrade";
    } else if (version != "13") {
        error = "Sec-WebSocket-Version must be 13";
    } else if (key.empty()) {
        error = "Sec-WebSocket-Key is required";
    }

    if (!error.empty()) {
        result.ok = false;
        result.error = error;
        result.response = badRequestResponse(error);
        return result;
    }

    result.ok = true;
    const std::string accept = computeAcceptKey(key);
    result.response = "HTTP/1.1 101 Switching Protocols\r\n"
                      "Upgrade: websocket\r\n"
                      "Connection: Upgrade\r\n"
                      "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
    return result;
}

std::string WebSocketHandshake::badRequestResponse(const std::string& reason) {
    std::string body = "Bad Request: " + reason + "\n";
    return "HTTP/1.1 400 Bad Request\r\n"
           "Connection: close\r\n"
           "Content-Type: text/plain\r\n"
           "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}

std::string WebSocketHandshake::computeAcceptKey(const std::string& sec_websocket_key) {
    const std::string source = sec_websocket_key + kWebSocketGuid;
    unsigned char digest[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(source.data()), source.size(), digest);
    return base64Encode(digest, SHA_DIGEST_LENGTH);
}

bool WebSocketHandshake::parseRequest(const std::string& http_request,
                                      WebSocketHandshakeRequest* request,
                                      std::string* error) {
    if (request == nullptr) return false;
    std::istringstream stream(http_request);
    std::string line;
    if (!std::getline(stream, line)) {
        if (error) *error = "empty request";
        return false;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();

    std::istringstream request_line(line);
    std::string version;
    request_line >> request->method >> request->path >> version;
    if (request->method.empty() || request->path.empty() || version != "HTTP/1.1") {
        if (error) *error = "invalid request line";
        return false;
    }

    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        auto pos = line.find(':');
        if (pos == std::string::npos) continue;
        std::string name = lower(trim(line.substr(0, pos)));
        std::string value = trim(line.substr(pos + 1));
        request->headers[name] = value;
    }
    return true;
}

}  // namespace nebula
