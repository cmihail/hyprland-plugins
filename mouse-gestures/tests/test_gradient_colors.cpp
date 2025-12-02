#include <gtest/gtest.h>
#include <vector>

// Test fixture for gradient color tests
class GradientColorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup test environment
    }

    void TearDown() override {
        // Cleanup
    }

    // Helper struct for color representation
    struct Color {
        float r;
        float g;
        float b;
        float a;
    };

    // Helper function to interpolate between two colors
    Color interpolateColor(const Color& start, const Color& end, float t) {
        Color result;
        result.r = start.r + (end.r - start.r) * t;
        result.g = start.g + (end.g - start.g) * t;
        result.b = start.b + (end.b - start.b) * t;
        result.a = start.a + (end.a - start.a) * t;
        return result;
    }
};

// Test default start color (blue)
TEST_F(GradientColorTest, DefaultStartColor) {
    uint32_t defaultStartColor = 0x4C7FA6FF;

    // Extract RGBA components from hex (RGBA format)
    float r = ((defaultStartColor >> 24) & 0xFF) / 255.0f;
    float g = ((defaultStartColor >> 16) & 0xFF) / 255.0f;
    float b = ((defaultStartColor >> 8) & 0xFF) / 255.0f;
    float a = (defaultStartColor & 0xFF) / 255.0f;

    EXPECT_FLOAT_EQ(r, 0x4C / 255.0f);
    EXPECT_FLOAT_EQ(g, 0x7F / 255.0f);
    EXPECT_FLOAT_EQ(b, 0xA6 / 255.0f);
    EXPECT_FLOAT_EQ(a, 1.0f);

    // Verify all components are in valid range
    EXPECT_GE(r, 0.0f);
    EXPECT_LE(r, 1.0f);
    EXPECT_GE(g, 0.0f);
    EXPECT_LE(g, 1.0f);
    EXPECT_GE(b, 0.0f);
    EXPECT_LE(b, 1.0f);
    EXPECT_GE(a, 0.0f);
    EXPECT_LE(a, 1.0f);
}

// Test default end color (purple)
TEST_F(GradientColorTest, DefaultEndColor) {
    uint32_t defaultEndColor = 0xA64C7FFF;

    // Extract RGBA components from hex (RGBA format)
    float r = ((defaultEndColor >> 24) & 0xFF) / 255.0f;
    float g = ((defaultEndColor >> 16) & 0xFF) / 255.0f;
    float b = ((defaultEndColor >> 8) & 0xFF) / 255.0f;
    float a = (defaultEndColor & 0xFF) / 255.0f;

    EXPECT_FLOAT_EQ(r, 0xA6 / 255.0f);
    EXPECT_FLOAT_EQ(g, 0x4C / 255.0f);
    EXPECT_FLOAT_EQ(b, 0x7F / 255.0f);
    EXPECT_FLOAT_EQ(a, 1.0f);

    // Verify all components are in valid range
    EXPECT_GE(r, 0.0f);
    EXPECT_LE(r, 1.0f);
    EXPECT_GE(g, 0.0f);
    EXPECT_LE(g, 1.0f);
    EXPECT_GE(b, 0.0f);
    EXPECT_LE(b, 1.0f);
    EXPECT_GE(a, 0.0f);
    EXPECT_LE(a, 1.0f);
}

// Test color interpolation at start (t=0.0)
TEST_F(GradientColorTest, InterpolationAtStart) {
    Color startColor = {1.0f, 0.0f, 0.0f, 1.0f};  // Red
    Color endColor = {0.0f, 0.0f, 1.0f, 1.0f};    // Blue

    Color result = interpolateColor(startColor, endColor, 0.0f);

    EXPECT_FLOAT_EQ(result.r, 1.0f);
    EXPECT_FLOAT_EQ(result.g, 0.0f);
    EXPECT_FLOAT_EQ(result.b, 0.0f);
    EXPECT_FLOAT_EQ(result.a, 1.0f);
}

// Test color interpolation at end (t=1.0)
TEST_F(GradientColorTest, InterpolationAtEnd) {
    Color startColor = {1.0f, 0.0f, 0.0f, 1.0f};  // Red
    Color endColor = {0.0f, 0.0f, 1.0f, 1.0f};    // Blue

    Color result = interpolateColor(startColor, endColor, 1.0f);

    EXPECT_FLOAT_EQ(result.r, 0.0f);
    EXPECT_FLOAT_EQ(result.g, 0.0f);
    EXPECT_FLOAT_EQ(result.b, 1.0f);
    EXPECT_FLOAT_EQ(result.a, 1.0f);
}

// Test color interpolation at midpoint (t=0.5)
TEST_F(GradientColorTest, InterpolationAtMidpoint) {
    Color startColor = {1.0f, 0.0f, 0.0f, 1.0f};  // Red
    Color endColor = {0.0f, 0.0f, 1.0f, 1.0f};    // Blue

    Color result = interpolateColor(startColor, endColor, 0.5f);

    EXPECT_FLOAT_EQ(result.r, 0.5f);
    EXPECT_FLOAT_EQ(result.g, 0.0f);
    EXPECT_FLOAT_EQ(result.b, 0.5f);  // Purple (mix of red and blue)
    EXPECT_FLOAT_EQ(result.a, 1.0f);
}

// Test color interpolation at quarter point (t=0.25)
TEST_F(GradientColorTest, InterpolationAtQuarter) {
    Color startColor = {1.0f, 0.0f, 0.0f, 1.0f};  // Red
    Color endColor = {0.0f, 0.0f, 1.0f, 1.0f};    // Blue

    Color result = interpolateColor(startColor, endColor, 0.25f);

    EXPECT_FLOAT_EQ(result.r, 0.75f);
    EXPECT_FLOAT_EQ(result.g, 0.0f);
    EXPECT_FLOAT_EQ(result.b, 0.25f);
    EXPECT_FLOAT_EQ(result.a, 1.0f);
}

// Test color interpolation at three-quarter point (t=0.75)
TEST_F(GradientColorTest, InterpolationAtThreeQuarters) {
    Color startColor = {1.0f, 0.0f, 0.0f, 1.0f};  // Red
    Color endColor = {0.0f, 0.0f, 1.0f, 1.0f};    // Blue

    Color result = interpolateColor(startColor, endColor, 0.75f);

    EXPECT_FLOAT_EQ(result.r, 0.25f);
    EXPECT_FLOAT_EQ(result.g, 0.0f);
    EXPECT_FLOAT_EQ(result.b, 0.75f);
    EXPECT_FLOAT_EQ(result.a, 1.0f);
}

// Test path position calculation for single point
TEST_F(GradientColorTest, PathPositionSinglePoint) {
    size_t numPoints = 1;
    size_t index = 0;

    float pathPosition = (numPoints > 1) ?
        static_cast<float>(index) / static_cast<float>(numPoints - 1) : 0.0f;

    EXPECT_FLOAT_EQ(pathPosition, 0.0f);
}

// Test path position calculation for two points
TEST_F(GradientColorTest, PathPositionTwoPoints) {
    size_t numPoints = 2;

    // First point
    float pathPosition0 = static_cast<float>(0) / static_cast<float>(numPoints - 1);
    EXPECT_FLOAT_EQ(pathPosition0, 0.0f);

    // Second point
    float pathPosition1 = static_cast<float>(1) / static_cast<float>(numPoints - 1);
    EXPECT_FLOAT_EQ(pathPosition1, 1.0f);
}

// Test path position calculation for multiple points
TEST_F(GradientColorTest, PathPositionMultiplePoints) {
    size_t numPoints = 5;
    std::vector<float> expectedPositions = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (size_t i = 0; i < numPoints; ++i) {
        float pathPosition = static_cast<float>(i) / static_cast<float>(numPoints - 1);
        EXPECT_FLOAT_EQ(pathPosition, expectedPositions[i]);
    }
}

// Test gradient with same start and end colors
TEST_F(GradientColorTest, SameStartAndEndColors) {
    Color startColor = {0.5f, 0.5f, 0.5f, 1.0f};  // Gray
    Color endColor = {0.5f, 0.5f, 0.5f, 1.0f};    // Same gray

    // Test at multiple positions
    for (float t = 0.0f; t <= 1.0f; t += 0.1f) {
        Color result = interpolateColor(startColor, endColor, t);

        EXPECT_FLOAT_EQ(result.r, 0.5f);
        EXPECT_FLOAT_EQ(result.g, 0.5f);
        EXPECT_FLOAT_EQ(result.b, 0.5f);
        EXPECT_FLOAT_EQ(result.a, 1.0f);
    }
}

// Test color component bounds
TEST_F(GradientColorTest, ColorComponentBounds) {
    Color startColor = {0.0f, 0.0f, 0.0f, 0.0f};  // Black, transparent
    Color endColor = {1.0f, 1.0f, 1.0f, 1.0f};    // White, opaque

    // Test at multiple positions
    for (float t = 0.0f; t <= 1.0f; t += 0.1f) {
        Color result = interpolateColor(startColor, endColor, t);

        // All components should be in valid range [0.0, 1.0]
        EXPECT_GE(result.r, 0.0f);
        EXPECT_LE(result.r, 1.0f);
        EXPECT_GE(result.g, 0.0f);
        EXPECT_LE(result.g, 1.0f);
        EXPECT_GE(result.b, 0.0f);
        EXPECT_LE(result.b, 1.0f);
        EXPECT_GE(result.a, 0.0f);
        EXPECT_LE(result.a, 1.0f);
    }
}

// Test gradient along actual gesture path
TEST_F(GradientColorTest, GradientAlongGesturePath) {
    Color startColor = {0.3f, 0.5f, 0.65f, 1.0f};  // Blue-ish
    Color endColor = {0.65f, 0.3f, 0.5f, 1.0f};    // Purple-ish

    size_t numPoints = 10;
    std::vector<Color> gradientColors;

    for (size_t i = 0; i < numPoints; ++i) {
        float pathPosition = (numPoints > 1) ?
            static_cast<float>(i) / static_cast<float>(numPoints - 1) : 0.0f;

        Color color = interpolateColor(startColor, endColor, pathPosition);
        gradientColors.push_back(color);

        // Verify each color is valid
        EXPECT_GE(color.r, 0.0f);
        EXPECT_LE(color.r, 1.0f);
        EXPECT_GE(color.g, 0.0f);
        EXPECT_LE(color.g, 1.0f);
        EXPECT_GE(color.b, 0.0f);
        EXPECT_LE(color.b, 1.0f);
    }

    // Verify we have the expected number of colors
    EXPECT_EQ(gradientColors.size(), numPoints);

    // Verify first and last colors match start and end
    EXPECT_FLOAT_EQ(gradientColors.front().r, startColor.r);
    EXPECT_FLOAT_EQ(gradientColors.front().g, startColor.g);
    EXPECT_FLOAT_EQ(gradientColors.front().b, startColor.b);

    EXPECT_FLOAT_EQ(gradientColors.back().r, endColor.r);
    EXPECT_FLOAT_EQ(gradientColors.back().g, endColor.g);
    EXPECT_FLOAT_EQ(gradientColors.back().b, endColor.b);
}

// Test gradient with alpha interpolation
TEST_F(GradientColorTest, GradientWithAlphaInterpolation) {
    Color startColor = {1.0f, 0.0f, 0.0f, 1.0f};   // Red, opaque
    Color endColor = {0.0f, 0.0f, 1.0f, 0.0f};     // Blue, transparent

    // Test at midpoint
    Color result = interpolateColor(startColor, endColor, 0.5f);

    EXPECT_FLOAT_EQ(result.r, 0.5f);
    EXPECT_FLOAT_EQ(result.g, 0.0f);
    EXPECT_FLOAT_EQ(result.b, 0.5f);
    EXPECT_FLOAT_EQ(result.a, 0.5f);  // Half transparent
}

// Test gradient monotonicity (colors should transition smoothly)
TEST_F(GradientColorTest, GradientMonotonicity) {
    Color startColor = {0.0f, 0.0f, 0.0f, 1.0f};  // Black
    Color endColor = {1.0f, 1.0f, 1.0f, 1.0f};    // White

    float prevBrightness = 0.0f;

    for (float t = 0.0f; t <= 1.0f; t += 0.1f) {
        Color result = interpolateColor(startColor, endColor, t);

        // Calculate brightness (simple average of RGB)
        float brightness = (result.r + result.g + result.b) / 3.0f;

        // Brightness should increase monotonically
        EXPECT_GE(brightness, prevBrightness - 0.001f);  // Small epsilon for float comparison
        prevBrightness = brightness;
    }
}

// Test edge case: t slightly outside [0, 1] range
TEST_F(GradientColorTest, InterpolationOutsideRange) {
    Color startColor = {1.0f, 0.0f, 0.0f, 1.0f};  // Red
    Color endColor = {0.0f, 0.0f, 1.0f, 1.0f};    // Blue

    // Test t slightly less than 0
    Color resultNegative = interpolateColor(startColor, endColor, -0.1f);
    // Should extrapolate (though in practice we clamp to [0, 1])
    EXPECT_FLOAT_EQ(resultNegative.r, 1.1f);  // More red than start

    // Test t slightly greater than 1
    Color resultGreater = interpolateColor(startColor, endColor, 1.1f);
    // Should extrapolate (though in practice we clamp to [0, 1])
    EXPECT_FLOAT_EQ(resultGreater.b, 1.1f);  // More blue than end
}

// Test color difference between consecutive points
TEST_F(GradientColorTest, ColorDifferenceBetweenPoints) {
    Color startColor = {1.0f, 0.0f, 0.0f, 1.0f};  // Red
    Color endColor = {0.0f, 1.0f, 0.0f, 1.0f};    // Green

    size_t numPoints = 100;
    float expectedStepSize = 1.0f / (numPoints - 1);

    for (size_t i = 0; i < numPoints - 1; ++i) {
        float t1 = static_cast<float>(i) / static_cast<float>(numPoints - 1);
        float t2 = static_cast<float>(i + 1) / static_cast<float>(numPoints - 1);

        Color color1 = interpolateColor(startColor, endColor, t1);
        Color color2 = interpolateColor(startColor, endColor, t2);

        // Color difference should be consistent (smooth gradient)
        float redDiff = std::abs(color2.r - color1.r);
        float greenDiff = std::abs(color2.g - color1.g);

        // Each step should change by approximately expectedStepSize
        EXPECT_NEAR(redDiff, expectedStepSize, 0.01f);
        EXPECT_NEAR(greenDiff, expectedStepSize, 0.01f);
    }
}

// Test gradient renders different colors at start and end
TEST_F(GradientColorTest, GradientShowsDirectionality) {
    Color startColor = {0.3f, 0.5f, 0.65f, 1.0f};  // Blue-ish
    Color endColor = {0.65f, 0.3f, 0.5f, 1.0f};    // Purple-ish

    Color firstPoint = interpolateColor(startColor, endColor, 0.0f);
    Color lastPoint = interpolateColor(startColor, endColor, 1.0f);

    // Start and end should be noticeably different
    EXPECT_NE(firstPoint.r, lastPoint.r);
    EXPECT_NE(firstPoint.g, lastPoint.g);
    EXPECT_NE(firstPoint.b, lastPoint.b);

    // Verify the gradient shows clear directionality
    EXPECT_LT(firstPoint.r, lastPoint.r);  // Red increases
    EXPECT_GT(firstPoint.g, lastPoint.g);  // Green decreases
    EXPECT_GT(firstPoint.b, lastPoint.b);  // Blue decreases
}
