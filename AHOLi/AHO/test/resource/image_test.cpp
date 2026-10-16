#include <gtest/gtest.h>
#include <AHO/resource/image.hpp>
#include "../support/temp_directory.hpp"
#include <fstream>

namespace {
TEST(ImageTest, DefaultImageHasNoPixelData) {
    aho::resource::Image image;
    EXPECT_FALSE(image.data().has_value());
}
#ifdef AHO_STB_IMAGE_ENABLE
TEST(ImageTest, LoadsKnownPixelsAndCopiesKeepImageDataAlive) {
    aho_test::TempDirectory directory;
    const auto file = directory.path / "pixels.ppm";
    {
        std::ofstream stream(file, std::ios::binary);
        stream << "P6\n2 1\n255\n";
        const unsigned char pixels[]{255, 0, 0, 0, 255, 0};
        stream.write(reinterpret_cast<const char*>(pixels), sizeof pixels);
        ASSERT_TRUE(stream.good());
    }
    aho::resource::Image retained;
    {
        aho::resource::Image image(file);
        ASSERT_TRUE(image.data().has_value());
        ASSERT_NE(*image.data().value(), nullptr);
        retained = image;
    }
    ASSERT_TRUE(retained.data().has_value());
    const auto* pixels = *retained.data().value();
    EXPECT_EQ(pixels[0], 255); EXPECT_EQ(pixels[1], 0); EXPECT_EQ(pixels[2], 0); EXPECT_EQ(pixels[3], 255);
    EXPECT_EQ(pixels[4], 0); EXPECT_EQ(pixels[5], 255); EXPECT_EQ(pixels[6], 0); EXPECT_EQ(pixels[7], 255);
}
TEST(ImageTest, MissingFileReportsAbsentDataWithoutCrashing) {
    aho_test::TempDirectory directory;
    aho::resource::Image image(directory.path / "missing.png");
    EXPECT_FALSE(image.data().has_value());
}
TEST(ImageTest, MalformedFileReportsAbsentDataWithoutCrashing) {
    aho_test::TempDirectory directory;
    const auto path = directory.path / "malformed.png";
    { std::ofstream stream(path); stream << "not an image"; }
    aho::resource::Image image(path);
    EXPECT_FALSE(image.data().has_value());
}
TEST(ImageTest, ValidHeaderWithMissingPixelDataReportsAbsentData) {
    aho_test::TempDirectory directory;
    const auto path = directory.path / "truncated.png";
    // PNG signature and 1x1 RGB IHDR with a valid CRC, but no IDAT or IEND.
    constexpr unsigned char header[]{
        137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82,
        0, 0, 0, 1, 0, 0, 0, 1, 8, 2, 0, 0, 0, 144, 119, 83, 222
    };
    {
        std::ofstream stream(path, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(header), sizeof header);
        ASSERT_TRUE(stream.good());
    }
    aho::resource::Image image(path);
    EXPECT_FALSE(image.data().has_value());
    const auto copy = image;
    auto retained = copy;
    EXPECT_FALSE(retained.data().has_value());
}
#endif
}
