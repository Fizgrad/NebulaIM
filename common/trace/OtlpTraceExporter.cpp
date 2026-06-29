#include "common/trace/OtlpTraceExporter.h"

#include "common/trace/TraceManager.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <sstream>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace nebula {

OtlpTraceExporter::OtlpTraceExporter(OtlpTraceExporterConfig config)
    : config_(std::move(config)), service_name_(TraceManager::instance().serviceName()) {}

bool OtlpTraceExporter::exportSpans(const std::vector<CompletedSpan>& spans, std::string* error) const {
    if (spans.empty()) return true;
    EndpointParts parts;
    if (!parseEndpoint(config_.endpoint, &parts, error)) return false;

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    const std::string port = std::to_string(parts.port);
    int rc = ::getaddrinfo(parts.host.c_str(), port.c_str(), &hints, &result);
    if (rc != 0) {
        if (error != nullptr) *error = std::string("getaddrinfo failed: ") + gai_strerror(rc);
        return false;
    }

    int fd = -1;
    for (addrinfo* rp = result; rp != nullptr; rp = rp->ai_next) {
        fd = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0) continue;
        if (::connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        ::close(fd);
        fd = -1;
    }
    ::freeaddrinfo(result);
    if (fd < 0) {
        if (error != nullptr) *error = "failed to connect OTLP endpoint";
        return false;
    }

    const std::string body = buildJson(spans, service_name_);
    std::ostringstream request;
    request << "POST " << parts.path << " HTTP/1.1\r\n"
            << "Host: " << parts.host << ":" << parts.port << "\r\n"
            << "Content-Type: application/json\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Connection: close\r\n\r\n"
            << body;
    const std::string bytes = request.str();
    size_t written = 0;
    while (written < bytes.size()) {
        ssize_t n = ::write(fd, bytes.data() + written, bytes.size() - written);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) {
            if (error != nullptr) *error = std::string("failed to write OTLP request: ") + std::strerror(errno);
            ::close(fd);
            return false;
        }
        written += static_cast<size_t>(n);
    }

    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    timeval tv{};
    tv.tv_sec = config_.timeout_ms / 1000;
    tv.tv_usec = (config_.timeout_ms % 1000) * 1000;
    rc = ::select(fd + 1, &rfds, nullptr, nullptr, &tv);
    if (rc <= 0) {
        if (error != nullptr) *error = "timeout waiting for OTLP response";
        ::close(fd);
        return false;
    }
    char response[256];
    ssize_t n = ::read(fd, response, sizeof(response) - 1);
    ::close(fd);
    if (n <= 0) {
        if (error != nullptr) *error = "empty OTLP response";
        return false;
    }
    response[n] = '\0';
    std::string text(response);
    const bool ok = text.find(" 200 ") != std::string::npos || text.find(" 202 ") != std::string::npos || text.find(" 204 ") != std::string::npos;
    if (!ok && error != nullptr) *error = "OTLP collector rejected spans: " + text;
    return ok;
}

std::string OtlpTraceExporter::endpoint() const {
    return config_.endpoint;
}

bool OtlpTraceExporter::parseEndpoint(const std::string& endpoint, EndpointParts* parts, std::string* error) {
    constexpr const char* prefix = "http://";
    if (endpoint.rfind(prefix, 0) != 0) {
        if (error != nullptr) *error = "only http:// OTLP endpoints are supported by the lightweight exporter";
        return false;
    }
    std::string rest = endpoint.substr(std::strlen(prefix));
    size_t slash = rest.find('/');
    std::string host_port = slash == std::string::npos ? rest : rest.substr(0, slash);
    parts->path = slash == std::string::npos ? "/v1/traces" : rest.substr(slash);
    size_t colon = host_port.rfind(':');
    if (colon == std::string::npos) {
        parts->host = host_port;
        parts->port = 4318;
    } else {
        parts->host = host_port.substr(0, colon);
        try {
            parts->port = std::stoi(host_port.substr(colon + 1));
        } catch (...) {
            if (error != nullptr) *error = "invalid OTLP endpoint port";
            return false;
        }
    }
    if (parts->host.empty() || parts->path.empty()) {
        if (error != nullptr) *error = "invalid OTLP endpoint";
        return false;
    }
    return true;
}

std::string OtlpTraceExporter::buildJson(const std::vector<CompletedSpan>& spans, const std::string& service_name) {
    std::ostringstream out;
    out << "{\"resourceSpans\":[{\"resource\":{\"attributes\":[{\"key\":\"service.name\",\"value\":{\"stringValue\":\""
        << jsonEscape(service_name.empty() ? "nebulaim" : service_name) << "\"}}]},\"scopeSpans\":[{\"scope\":{\"name\":\"nebulaim\"},\"spans\":[";
    for (size_t i = 0; i < spans.size(); ++i) {
        const auto& span = spans[i];
        if (i > 0) out << ",";
        out << "{\"traceId\":\"" << jsonEscape(span.trace_id)
            << "\",\"spanId\":\"" << jsonEscape(span.span_id) << "\"";
        if (!span.parent_span_id.empty()) {
            out << ",\"parentSpanId\":\"" << jsonEscape(span.parent_span_id) << "\"";
        }
        out << ",\"name\":\"" << jsonEscape(span.name)
            << "\",\"kind\":" << spanKindValue(span.kind)
            << ",\"startTimeUnixNano\":\"" << span.start_time_unix_nano
            << "\",\"endTimeUnixNano\":\"" << span.end_time_unix_nano << "\"";
        out << ",\"attributes\":[";
        size_t attr_index = 0;
        for (const auto& attr : span.attributes) {
            if (attr_index++ > 0) out << ",";
            out << "{\"key\":\"" << jsonEscape(attr.first)
                << "\",\"value\":{\"stringValue\":\"" << jsonEscape(attr.second) << "\"}}";
        }
        out << "],\"status\":{\"code\":\"" << jsonEscape(span.status_code) << "\"";
        if (!span.status_message.empty()) {
            out << ",\"message\":\"" << jsonEscape(span.status_message) << "\"";
        }
        out << "}}";
    }
    out << "]}]}]}]}";
    return out.str();
}

std::string OtlpTraceExporter::jsonEscape(const std::string& value) {
    std::ostringstream out;
    for (char c : value) {
        switch (c) {
            case '\\': out << "\\\\"; break;
            case '"': out << "\\\""; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default: out << c; break;
        }
    }
    return out.str();
}

int OtlpTraceExporter::spanKindValue(TraceSpanKind kind) {
    switch (kind) {
        case TraceSpanKind::INTERNAL: return 1;
        case TraceSpanKind::SERVER: return 2;
        case TraceSpanKind::CLIENT: return 3;
        case TraceSpanKind::PRODUCER: return 4;
        case TraceSpanKind::CONSUMER: return 5;
    }
    return 1;
}

}  // namespace nebula
