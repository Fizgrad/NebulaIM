#include "common/security/AdminAuth.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <openssl/sha.h>
#include <sstream>
#include <utility>

namespace nebula {

AdminAuth AdminAuth::fromConfig(const Config& config) {
    AdminAuth auth;
    const std::string spec = config.getString("admin_service.admin_tokens", "");
    for (const auto& entry : split(spec, ';')) {
        const std::string text = trim(entry);
        if (text.empty()) continue;
        std::vector<std::string> parts = split(text, ':');
        if (parts.size() == 4 && trim(parts[1]) == "sha256") {
            auth.addSha256Token(trim(parts[0]), trim(parts[2]), split(parts[3], ','));
        }
    }
    return auth;
}

bool AdminAuth::enabled() const {
    return !rules_.empty();
}

void AdminAuth::addSha256Token(std::string name, std::string token_sha256_hex, std::vector<std::string> scopes) {
    TokenRule rule;
    rule.name = std::move(name);
    rule.token_sha256_hex = std::move(token_sha256_hex);
    std::transform(rule.token_sha256_hex.begin(), rule.token_sha256_hex.end(), rule.token_sha256_hex.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    for (const auto& scope : scopes) {
        std::string trimmed = trim(scope);
        if (!trimmed.empty()) rule.scopes.insert(trimmed);
    }
    addRule(std::move(rule));
}

AdminAuthResult AdminAuth::authorize(const std::string& token, const std::string& required_scope) const {
    AdminAuthResult result;
    if (!enabled()) {
        result.decision = AdminAuthDecision::UNAUTHENTICATED;
        return result;
    }
    if (token.empty()) {
        result.decision = AdminAuthDecision::UNAUTHENTICATED;
        return result;
    }
    const std::string token_sha = sha256Hex(token);
    for (const auto& rule : rules_) {
        const bool token_match = !rule.token_sha256_hex.empty() && token_sha == rule.token_sha256_hex;
        if (!token_match) continue;
        result.principal.name = rule.name;
        result.principal.scopes = rule.scopes;
        result.decision = hasScope(rule.scopes, required_scope) ? AdminAuthDecision::ALLOW : AdminAuthDecision::PERMISSION_DENIED;
        return result;
    }
    result.decision = AdminAuthDecision::UNAUTHENTICATED;
    return result;
}

std::vector<std::string> AdminAuth::split(const std::string& value, char delim) {
    std::vector<std::string> parts;
    std::string current;
    std::istringstream input(value);
    while (std::getline(input, current, delim)) {
        parts.push_back(current);
    }
    return parts;
}

std::string AdminAuth::trim(const std::string& value) {
    const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    if (begin == value.end()) return "";
    const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    return std::string(begin, end);
}

std::string AdminAuth::sha256Hex(const std::string& value) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(value.data()), value.size(), digest);
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (unsigned char byte : digest) {
        output << std::setw(2) << static_cast<int>(byte);
    }
    return output.str();
}

bool AdminAuth::hasScope(const std::unordered_set<std::string>& scopes, const std::string& required_scope) {
    return scopes.find("*") != scopes.end() || scopes.find(required_scope) != scopes.end();
}

void AdminAuth::addRule(TokenRule rule) {
    if (rule.name.empty() || rule.token_sha256_hex.empty()) return;
    if (rule.scopes.empty()) rule.scopes.insert("health");
    rules_.push_back(std::move(rule));
}

}  // namespace nebula
