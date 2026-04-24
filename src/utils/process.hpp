#pragma once
/*
 * TitanShare Daemon — Process Utilities
 * Execute shell commands and capture output.
 */

#include <string>
#include <functional>

namespace titanshare {

struct ExecResult {
    int exitCode;
    std::string output;
    bool success() const { return exitCode == 0; }
};

class Process {
public:
    // Execute a command synchronously, return exit code + stdout
    static ExecResult exec(const std::string& command);

    // Execute a command asynchronously, call callback with result
    static void execAsync(const std::string& command,
                          std::function<void(const ExecResult&)> callback);

    // Fire-and-forget execution (no output capture)
    static void execDetached(const std::string& command);
};

} // namespace titanshare
