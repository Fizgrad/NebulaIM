#pragma once

#include "common/trace/TraceSpan.h"

#include <string>
#include <vector>

namespace nebula {

struct OtlpTraceExporterConfig {
    std::string endpoint = "http://127.0.0.1:4318/v1/traces";
    int timeout_ms = 2000;
};

class OtlpTraceExporter {
public:
    explicit OtlpTraceExporter(OtlpTraceExporterConfig config = {});

    bool exportSpans(const std::vector<CompletedSpan>& spans, std::string* error = nullptr) const;
    std::string endpoint() const;

private:
    struct EndpointParts {
        std::string host;
        int port = 4318;
        std::string path = "/v1/traces";
    };

    static bool parseEndpoint(const std::string& endpoint, EndpointParts* parts, std::string* error);
    static std::string buildJson(const std::vector<CompletedSpan>& spans, const std::string& service_name);
    static std::string jsonEscape(const std::string& value);
    static int spanKindValue(TraceSpanKind kind);

private:
    OtlpTraceExporterConfig config_;
    std::string service_name_;
};

}  // namespace nebula
