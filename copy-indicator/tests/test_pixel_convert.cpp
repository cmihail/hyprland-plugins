#include <gtest/gtest.h>
#include <cairo/cairo.h>
#include "pixel_convert.hpp"

TEST(PixelConvertTest, SwapsRedAndBlueChannels) {
    unsigned char bgra[] = {0x10, 0x20, 0x30, 0x40};

    auto rgba = convertBGRAtoRGBA(bgra, 4, 1, 1);

    ASSERT_EQ(rgba.size(), 4u);
    EXPECT_EQ(rgba[0], 0x30);
    EXPECT_EQ(rgba[1], 0x20);
    EXPECT_EQ(rgba[2], 0x10);
    EXPECT_EQ(rgba[3], 0x40);
}

TEST(PixelConvertTest, SkipsStridePadding) {
    // 2x2 image with 4 bytes of padding per row
    unsigned char bgra[] = {
        1, 2, 3, 4,     5, 6, 7, 8,     0xEE, 0xEE, 0xEE, 0xEE,
        9, 10, 11, 12,  13, 14, 15, 16, 0xEE, 0xEE, 0xEE, 0xEE,
    };

    auto rgba = convertBGRAtoRGBA(bgra, 12, 2, 2);

    std::vector<uint8_t> expected = {
        3, 2, 1, 4,    7, 6, 5, 8,
        11, 10, 9, 12, 15, 14, 13, 16,
    };
    EXPECT_EQ(rgba, expected);
}

TEST(PixelConvertTest, CairoRedPixelStaysRed) {
    auto* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 1, 1);
    auto* cr = cairo_create(surface);
    cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 1.0);
    cairo_paint(cr);
    cairo_surface_flush(surface);

    auto rgba = convertBGRAtoRGBA(cairo_image_surface_get_data(surface),
                                  cairo_image_surface_get_stride(surface), 1, 1);

    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    EXPECT_EQ(rgba, (std::vector<uint8_t>{255, 0, 0, 255}));
}
