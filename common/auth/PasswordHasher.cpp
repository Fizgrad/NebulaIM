#include "common/auth/PasswordHasher.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <iomanip>
#include <sstream>
#include <vector>

namespace nebula {

namespace {
constexpr int kIterations = 100000;
constexpr size_t kSaltBytes = 16;
constexpr size_t kHashBytes = 32;
constexpr const char* kAlgorithm = "pbkdf2_sha256";
}

std::string PasswordHasher::hashPassword(const std::string& password) {
    if (password.empty()) {
        return "";
    }
    std::string salt = generateSalt(kSaltBytes);
    if (salt.empty()) {
        return "";
    }
    std::string hash = pbkdf2Sha256(password, salt, kIterations, kHashBytes);
    if (hash.empty()) {
        return "";
    }
    return std::string(kAlgorithm) + "$" + std::to_string(kIterations) + "$" + toHex(salt) + "$" + toHex(hash);
}

bool PasswordHasher::verifyPassword(const std::string& password, const std::string& encoded_hash) {
    if (password.empty() || encoded_hash.empty()) {
        return false;
    }

    std::vector<std::string> parts;
    size_t start = 0;
    while (true) {
        size_t pos = encoded_hash.find('$', start);
        if (pos == std::string::npos) {
            parts.push_back(encoded_hash.substr(start));
            break;
        }
        parts.push_back(encoded_hash.substr(start, pos - start));
        start = pos + 1;
    }
    if (parts.size() != 4 || parts[0] != kAlgorithm) {
        return false;
    }

    int iterations = 0;
    try {
        iterations = std::stoi(parts[1]);
    } catch (...) {
        return false;
    }
    if (iterations <= 0) {
        return false;
    }

    std::string salt = fromHex(parts[2]);
    std::string expected = fromHex(parts[3]);
    if (salt.empty() || expected.empty()) {
        return false;
    }

    std::string actual = pbkdf2Sha256(password, salt, iterations, expected.size());
    if (actual.size() != expected.size()) {
        return false;
    }

    unsigned char diff = 0;
    for (size_t i = 0; i < actual.size(); ++i) {
        diff |= static_cast<unsigned char>(actual[i] ^ expected[i]);
    }
    return diff == 0;
}

std::string PasswordHasher::generateSalt(size_t bytes) {
    std::string salt(bytes, '\0');
    if (RAND_bytes(reinterpret_cast<unsigned char*>(salt.data()), static_cast<int>(salt.size())) != 1) {
        return "";
    }
    return salt;
}

std::string PasswordHasher::pbkdf2Sha256(const std::string& password,
                                         const std::string& salt,
                                         int iterations,
                                         size_t output_len) {
    std::string output(output_len, '\0');
    int ok = PKCS5_PBKDF2_HMAC(password.c_str(),
                               static_cast<int>(password.size()),
                               reinterpret_cast<const unsigned char*>(salt.data()),
                               static_cast<int>(salt.size()),
                               iterations,
                               EVP_sha256(),
                               static_cast<int>(output.size()),
                               reinterpret_cast<unsigned char*>(output.data()));
    if (ok != 1) {
        return "";
    }
    return output;
}

std::string PasswordHasher::toHex(const std::string& bytes) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned char ch : bytes) {
        oss << std::setw(2) << static_cast<int>(ch);
    }
    return oss.str();
}

std::string PasswordHasher::fromHex(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        return "";
    }
    std::string out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        unsigned int value = 0;
        std::istringstream iss(hex.substr(i, 2));
        iss >> std::hex >> value;
        if (iss.fail()) {
            return "";
        }
        out.push_back(static_cast<char>(value));
    }
    return out;
}

}  // namespace nebula
