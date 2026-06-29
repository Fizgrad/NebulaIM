#pragma once

#include <string>

namespace nebula {

class PasswordHasher {
public:
    static std::string hashPassword(const std::string& password);
    static bool verifyPassword(const std::string& password, const std::string& encoded_hash);

private:
    static std::string generateSalt(size_t bytes);
    static std::string pbkdf2Sha256(const std::string& password,
                                    const std::string& salt,
                                    int iterations,
                                    size_t output_len);
    static std::string toHex(const std::string& bytes);
    static std::string fromHex(const std::string& hex);
};

}  // namespace nebula
