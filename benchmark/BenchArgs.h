#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <unistd.h>

namespace nebula::benchmark {

inline std::string argValue(int argc, char** argv, const std::string& name, const std::string& fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] != nullptr && name == argv[i]) return argv[i + 1];
    }
    return fallback;
}

inline int intArg(int argc, char** argv, const std::string& name, int fallback) {
    return std::stoi(argValue(argc, argv, name, std::to_string(fallback)));
}

inline uint16_t portArg(int argc, char** argv, const std::string& name, uint16_t fallback) {
    return static_cast<uint16_t>(intArg(argc, argv, name, static_cast<int>(fallback)));
}

inline uint64_t uint64Arg(int argc, char** argv, const std::string& name, uint64_t fallback) {
    return static_cast<uint64_t>(std::stoull(argValue(argc, argv, name, std::to_string(fallback))));
}

inline std::string runId() {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
                   .count();
    return std::to_string(now) + "_" + std::to_string(::getpid());
}

}  // namespace nebula::benchmark
