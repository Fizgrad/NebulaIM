#pragma once

#include "common/config/Config.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace nebula {

struct AdminPrincipal {
    std::string name;
    std::unordered_set<std::string> scopes;
};

enum class AdminAuthDecision {
    ALLOW,
    UNAUTHENTICATED,
    PERMISSION_DENIED
};

struct AdminAuthResult {
    AdminAuthDecision decision = AdminAuthDecision::UNAUTHENTICATED;
    AdminPrincipal principal;
};

class AdminAuth {
public:
    static AdminAuth fromConfig(const Config& config);

    bool enabled() const;

    void addSha256Token(std::string name, std::string token_sha256_hex, std::vector<std::string> scopes);

    AdminAuthResult authorize(const std::string& token, const std::string& required_scope) const;

private:
    struct TokenRule {
        std::string name;
        std::string token_sha256_hex;
        std::unordered_set<std::string> scopes;
    };

    static std::vector<std::string> split(const std::string& value, char delim);
    static std::string trim(const std::string& value);
    static std::string sha256Hex(const std::string& value);
    static bool hasScope(const std::unordered_set<std::string>& scopes, const std::string& required_scope);
    void addRule(TokenRule rule);

private:
    std::vector<TokenRule> rules_;
};

}  // namespace nebula
