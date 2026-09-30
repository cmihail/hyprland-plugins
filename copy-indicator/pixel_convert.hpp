#pragma once

#include <cstdint>
#include <vector>

// Helper: Convert BGRA to RGBA pixel data
inline std::vector<uint8_t> convertBGRAtoRGBA(unsigned char* data, int stride,
                                              int width, int height) {
    std::vector<uint8_t> pixelData(width * height * 4);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const auto SRC = data + y * stride + x * 4;
            auto dst = pixelData.data() + (y * width + x) * 4;
            dst[0] = SRC[2]; // R
            dst[1] = SRC[1]; // G
            dst[2] = SRC[0]; // B
            dst[3] = SRC[3]; // A
        }
    }
    return pixelData;
}
