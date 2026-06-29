#include "common/log/Logger.h"

#include <cassert>
#include <fstream>
#include <string>

int main() {
    auto& logger = nebula::Logger::instance();
    logger.enableConsole(true);
    logger.setLevel(nebula::LogLevel::DEBUG);

    const std::string path = "logs/test_logger.log";
    const bool opened = logger.setLogFile(path);
    assert(opened);

    LOG_DEBUG("debug message");
    LOG_INFO("info message");
    LOG_WARN("warn message");
    LOG_ERROR("error message");

    std::ifstream input(path);
    assert(input.is_open());

    bool found_info = false;
    std::string line;
    while (std::getline(input, line)) {
        if (line.find("info message") != std::string::npos) {
            found_info = true;
            break;
        }
    }
    assert(found_info);

    return 0;
}
