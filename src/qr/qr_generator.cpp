#include "qr/qr_generator.hpp"
#include "utils/logger.hpp"

#include <qrencode.h>
#include <png.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace titanshare {

bool QrGenerator::generate(const std::string& jsonData, const std::string& outputPath) {
    // Generate QR code
    QRcode* qr = QRcode_encodeString(jsonData.c_str(), 0, QR_ECLEVEL_M, QR_MODE_8, 1);
    if (!qr) {
        Logger::error("QR", "QRcode_encodeString() failed");
        return false;
    }

    // Create PNG
    int scale = 8; // pixels per QR module
    int margin = 4; // modules of white margin
    int imgSize = (qr->width + margin * 2) * scale;

    FILE* fp = fopen(outputPath.c_str(), "wb");
    if (!fp) {
        Logger::error("QR", "Cannot open output file: " + outputPath);
        QRcode_free(qr);
        return false;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        fclose(fp);
        QRcode_free(qr);
        return false;
    }

    png_infop pngInfo = png_create_info_struct(png);
    if (!pngInfo) {
        png_destroy_write_struct(&png, nullptr);
        fclose(fp);
        QRcode_free(qr);
        return false;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &pngInfo);
        fclose(fp);
        QRcode_free(qr);
        return false;
    }

    png_init_io(png, fp);
    png_set_IHDR(png, pngInfo, imgSize, imgSize, 8,
                 PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, pngInfo);

    auto* row = static_cast<png_bytep>(malloc(imgSize));

    for (int y = 0; y < imgSize; y++) {
        for (int x = 0; x < imgSize; x++) {
            int qx = x / scale - margin;
            int qy = y / scale - margin;

            if (qx >= 0 && qx < qr->width && qy >= 0 && qy < qr->width) {
                // QR module: 1 = black (0x00), 0 = white (0xFF)
                row[x] = (qr->data[qy * qr->width + qx] & 1) ? 0x00 : 0xFF;
            } else {
                row[x] = 0xFF; // White margin
            }
        }
        png_write_row(png, row);
    }

    png_write_end(png, nullptr);
    free(row);
    png_destroy_write_struct(&png, &pngInfo);
    fclose(fp);
    QRcode_free(qr);

    Logger::info("QR", "✔ QR generated: " + outputPath);
    return true;
}

} // namespace titanshare
