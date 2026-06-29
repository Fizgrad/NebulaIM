#pragma once

#include <string>
#include <unordered_map>

namespace nebula {

struct WebSocketHandshakeRequest {
    std::string method;
    std::string path;
    std::unordered_map<std::string, std::string> headers;
};

struct WebSocketHandshakeResult {
    bool ok = false;
    std::string response;
    std::string error;
    WebSocketHandshakeRequest request;
};

class WebSocketHandshake {
public:
    static WebSocketHandshakeResult buildServerResponse(const std::string& http_request);
    static std::string badRequestResponse(const std::string& reason);
    static std::string computeAcceptKey(const std::string& sec_websocket_key);

private:
    static bool parseRequest(const std::string& http_request,
                             WebSocketHandshakeRequest* request,
                             std::string* error);
};

}  // namespace nebula
