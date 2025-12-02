#include <gtest/gtest.h>
#include <cmath>

// Constants from main.cpp
static constexpr int GRID_SIZE = 26;

// Helper struct to represent a 2D position
struct Vector2D {
    double x;
    double y;

    Vector2D(double x_, double y_) : x(x_), y(y_) {}

    Vector2D operator+(const Vector2D& other) const {
        return Vector2D{x + other.x, y + other.y};
    }
};

// Function to calculate cell dimensions
static void calculateCellDimensions(double monitorWidth, double monitorHeight,
                                    float& cellWidth, float& cellHeight) {
    cellWidth = monitorWidth / (float)GRID_SIZE;
    cellHeight = monitorHeight / (float)GRID_SIZE;
}

// Function to calculate cell center position (relative to monitor)
static Vector2D calculateCellCenter(int row, int col, float cellWidth, float cellHeight) {
    const float cellCenterX = col * cellWidth + cellWidth / 2.0f;
    const float cellCenterY = row * cellHeight + cellHeight / 2.0f;
    return Vector2D{cellCenterX, cellCenterY};
}

// Function to calculate absolute position
static Vector2D calculateAbsolutePosition(const Vector2D& monitorPos, const Vector2D& cellCenter) {
    return monitorPos + cellCenter;
}

class CoordinateTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup if needed
    }
};

// Test grid size constant
TEST_F(CoordinateTest, GridSizeIs26) {
    EXPECT_EQ(GRID_SIZE, 26);
}

// Test cell dimension calculations for standard monitor sizes
TEST_F(CoordinateTest, CellDimensions_1920x1080) {
    float cellWidth, cellHeight;
    calculateCellDimensions(1920.0, 1080.0, cellWidth, cellHeight);

    EXPECT_FLOAT_EQ(cellWidth, 1920.0f / 26.0f);   // ~73.846
    EXPECT_FLOAT_EQ(cellHeight, 1080.0f / 26.0f);  // ~41.538
}

TEST_F(CoordinateTest, CellDimensions_2560x1440) {
    float cellWidth, cellHeight;
    calculateCellDimensions(2560.0, 1440.0, cellWidth, cellHeight);

    EXPECT_FLOAT_EQ(cellWidth, 2560.0f / 26.0f);   // ~98.462
    EXPECT_FLOAT_EQ(cellHeight, 1440.0f / 26.0f);  // ~55.385
}

TEST_F(CoordinateTest, CellDimensions_3840x2160) {
    float cellWidth, cellHeight;
    calculateCellDimensions(3840.0, 2160.0, cellWidth, cellHeight);

    EXPECT_FLOAT_EQ(cellWidth, 3840.0f / 26.0f);   // ~147.692
    EXPECT_FLOAT_EQ(cellHeight, 2160.0f / 26.0f);  // ~83.077
}

// Test corner cell centers
TEST_F(CoordinateTest, CellCenter_TopLeft_0_0) {
    float cellWidth = 100.0f;
    float cellHeight = 50.0f;

    auto center = calculateCellCenter(0, 0, cellWidth, cellHeight);

    EXPECT_FLOAT_EQ(center.x, 50.0f);   // 0 * 100 + 100/2
    EXPECT_FLOAT_EQ(center.y, 25.0f);   // 0 * 50 + 50/2
}

TEST_F(CoordinateTest, CellCenter_TopRight_0_25) {
    float cellWidth = 100.0f;
    float cellHeight = 50.0f;

    auto center = calculateCellCenter(0, 25, cellWidth, cellHeight);

    EXPECT_FLOAT_EQ(center.x, 2550.0f);  // 25 * 100 + 100/2
    EXPECT_FLOAT_EQ(center.y, 25.0f);    // 0 * 50 + 50/2
}

TEST_F(CoordinateTest, CellCenter_BottomLeft_25_0) {
    float cellWidth = 100.0f;
    float cellHeight = 50.0f;

    auto center = calculateCellCenter(25, 0, cellWidth, cellHeight);

    EXPECT_FLOAT_EQ(center.x, 50.0f);    // 0 * 100 + 100/2
    EXPECT_FLOAT_EQ(center.y, 1275.0f);  // 25 * 50 + 50/2
}

TEST_F(CoordinateTest, CellCenter_BottomRight_25_25) {
    float cellWidth = 100.0f;
    float cellHeight = 50.0f;

    auto center = calculateCellCenter(25, 25, cellWidth, cellHeight);

    EXPECT_FLOAT_EQ(center.x, 2550.0f);  // 25 * 100 + 100/2
    EXPECT_FLOAT_EQ(center.y, 1275.0f);  // 25 * 50 + 50/2
}

// Test middle cell center
TEST_F(CoordinateTest, CellCenter_Middle_13_13) {
    float cellWidth = 100.0f;
    float cellHeight = 50.0f;

    auto center = calculateCellCenter(13, 13, cellWidth, cellHeight);

    EXPECT_FLOAT_EQ(center.x, 1350.0f);  // 13 * 100 + 100/2
    EXPECT_FLOAT_EQ(center.y, 675.0f);   // 13 * 50 + 50/2
}

// Test absolute position calculation with monitor offset
TEST_F(CoordinateTest, AbsolutePosition_NoOffset) {
    Vector2D monitorPos{0.0, 0.0};
    Vector2D cellCenter{100.0, 50.0};

    auto absolute = calculateAbsolutePosition(monitorPos, cellCenter);

    EXPECT_DOUBLE_EQ(absolute.x, 100.0);
    EXPECT_DOUBLE_EQ(absolute.y, 50.0);
}

TEST_F(CoordinateTest, AbsolutePosition_WithOffset) {
    Vector2D monitorPos{1920.0, 0.0};  // Second monitor to the right
    Vector2D cellCenter{100.0, 50.0};

    auto absolute = calculateAbsolutePosition(monitorPos, cellCenter);

    EXPECT_DOUBLE_EQ(absolute.x, 2020.0);  // 1920 + 100
    EXPECT_DOUBLE_EQ(absolute.y, 50.0);
}

TEST_F(CoordinateTest, AbsolutePosition_VerticalMonitor) {
    Vector2D monitorPos{0.0, 1080.0};  // Monitor below
    Vector2D cellCenter{50.0, 25.0};

    auto absolute = calculateAbsolutePosition(monitorPos, cellCenter);

    EXPECT_DOUBLE_EQ(absolute.x, 50.0);
    EXPECT_DOUBLE_EQ(absolute.y, 1105.0);  // 1080 + 25
}

// Test real-world scenario
TEST_F(CoordinateTest, RealWorld_1920x1080_Cell_B_M) {
    // B = row 1, M = col 12
    float cellWidth, cellHeight;
    calculateCellDimensions(1920.0, 1080.0, cellWidth, cellHeight);

    auto center = calculateCellCenter(1, 12, cellWidth, cellHeight);

    Vector2D monitorPos{0.0, 0.0};
    auto absolute = calculateAbsolutePosition(monitorPos, center);

    // Verify it's somewhere reasonable in the screen
    EXPECT_GT(absolute.x, 0.0);
    EXPECT_LT(absolute.x, 1920.0);
    EXPECT_GT(absolute.y, 0.0);
    EXPECT_LT(absolute.y, 1080.0);
}

// Test boundary conditions
TEST_F(CoordinateTest, CellDimensions_SmallMonitor) {
    float cellWidth, cellHeight;
    calculateCellDimensions(800.0, 600.0, cellWidth, cellHeight);

    EXPECT_GT(cellWidth, 0.0f);
    EXPECT_GT(cellHeight, 0.0f);
    EXPECT_FLOAT_EQ(cellWidth, 800.0f / 26.0f);
    EXPECT_FLOAT_EQ(cellHeight, 600.0f / 26.0f);
}

TEST_F(CoordinateTest, CellDimensions_LargeMonitor) {
    float cellWidth, cellHeight;
    calculateCellDimensions(7680.0, 4320.0, cellWidth, cellHeight);  // 8K

    EXPECT_GT(cellWidth, 0.0f);
    EXPECT_GT(cellHeight, 0.0f);
    EXPECT_FLOAT_EQ(cellWidth, 7680.0f / 26.0f);
    EXPECT_FLOAT_EQ(cellHeight, 4320.0f / 26.0f);
}
