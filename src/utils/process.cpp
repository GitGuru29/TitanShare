#include "utils/process.hpp"
#include "utils/logger.hpp"

#include <cstdio>
#include <array>
#include <thread>
#include <cstdlib>
#include <sys/wait.h>
#include <unistd.h>

namespace bybridge {

ExecResult Process::exec(const std::string& command) {
    ExecResult result{-1, ""};

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        Logger::error("PROC", "popen() failed for: " + command);
        return result;
    }

    std::array<char, 256> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result.output += buffer.data();
    }

    int status = pclose(pipe);
    result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return result;
}

void Process::execAsync(const std::string& command,
                        std::function<void(const ExecResult&)> callback) {
    std::thread([command, callback]() {
        auto result = exec(command);
        if (callback) callback(result);
    }).detach();
}

void Process::execDetached(const std::string& command) {
    std::thread([command]() {
        exec(command);
    }).detach();
}

} // namespace bybridge
