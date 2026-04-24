#include "file_transfer/file_receiver.hpp"
#include "config.hpp"

#include <filesystem>
#include <algorithm>

namespace bybridge {

std::string FileReceiver::getReceivedDir() {
    return config::RECEIVED_FILES_DIR;
}

bool FileReceiver::ensureDir() {
    try {
        std::filesystem::create_directories(config::RECEIVED_FILES_DIR);
        return true;
    } catch (...) {
        return false;
    }
}

std::string FileReceiver::sanitizeFilename(const std::string& name) {
    std::string result = name;
    std::replace(result.begin(), result.end(), '/', '_');
    std::replace(result.begin(), result.end(), '\\', '_');
    // Remove potentially dangerous chars
    result.erase(std::remove(result.begin(), result.end(), '\0'), result.end());
    return result;
}

} // namespace bybridge
