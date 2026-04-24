#pragma once
/*
 * ByBridge Daemon — QR Code Generator
 * Generates QR code PNG images using libqrencode + libpng.
 */

#include <string>

namespace bybridge {

class QrGenerator {
public:
    // Generate a QR code PNG from the given JSON data string
    // Saves to the configured QR_IMAGE_PATH
    static bool generate(const std::string& jsonData, const std::string& outputPath);
};

} // namespace bybridge
