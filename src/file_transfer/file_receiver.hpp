#pragma once
/*
 * ByBridge Daemon — File Receiver
 * Handles FILE_START/FILE_END protocol for receiving files from Android.
 * (This module is used by ClientSession, kept as a separate header for clarity)
 */

#include <string>

namespace bybridge {

class FileReceiver {
public:
    // Get the default received files directory
    static std::string getReceivedDir();

    // Ensure the received files directory exists
    static bool ensureDir();

    // Sanitize a filename (remove path separators)
    static std::string sanitizeFilename(const std::string& name);
};

} // namespace bybridge
