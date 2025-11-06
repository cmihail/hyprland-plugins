#include <gtest/gtest.h>
#include "../stroke.hpp"
#include <cmath>

class StrokeTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Common setup if needed
    }
};

// Test basic stroke creation
TEST_F(StrokeTest, CreateEmptyStroke) {
    Stroke stroke;
    EXPECT_EQ(stroke.size(), 0);
    EXPECT_FALSE(stroke.isFinished());
}

// Test adding points
TEST_F(StrokeTest, AddPoints) {
    Stroke stroke;
    EXPECT_TRUE(stroke.addPoint(100.0, 200.0));
    EXPECT_TRUE(stroke.addPoint(150.0, 250.0));
    EXPECT_TRUE(stroke.addPoint(200.0, 300.0));
    EXPECT_EQ(stroke.size(), 3);
    EXPECT_FALSE(stroke.isFinished());
}

// Test that adding points after finish fails
TEST_F(StrokeTest, CannotAddPointsAfterFinish) {
    Stroke stroke;
    stroke.addPoint(100.0, 200.0);
    stroke.addPoint(150.0, 250.0);
    stroke.finish();

    EXPECT_TRUE(stroke.isFinished());
    EXPECT_FALSE(stroke.addPoint(200.0, 300.0));
}

// Test finishing stroke with insufficient points
TEST_F(StrokeTest, CannotFinishWithLessThanTwoPoints) {
    Stroke stroke;
    stroke.addPoint(100.0, 200.0);
    EXPECT_FALSE(stroke.finish());
    EXPECT_FALSE(stroke.isFinished());
}

// Test finishing stroke with two points
TEST_F(StrokeTest, FinishWithTwoPoints) {
    Stroke stroke;
    stroke.addPoint(100.0, 200.0);
    stroke.addPoint(200.0, 300.0);
    EXPECT_TRUE(stroke.finish());
    EXPECT_TRUE(stroke.isFinished());
    EXPECT_EQ(stroke.size(), 2);
}

// Test coordinate normalization
TEST_F(StrokeTest, CoordinateNormalization) {
    Stroke stroke;
    stroke.addPoint(0.0, 0.0);
    stroke.addPoint(100.0, 0.0);
    stroke.addPoint(100.0, 100.0);
    stroke.addPoint(0.0, 100.0);
    EXPECT_TRUE(stroke.finish());

    // After normalization, all points should be in range [0, 1]
    const auto& points = stroke.getPoints();
    for (const auto& p : points) {
        EXPECT_GE(p.x, 0.0);
        EXPECT_LE(p.x, 1.0);
        EXPECT_GE(p.y, 0.0);
        EXPECT_LE(p.y, 1.0);
    }
}

// Test serialization
TEST_F(StrokeTest, Serialization) {
    Stroke stroke;
    stroke.addPoint(100.0, 200.0);
    stroke.addPoint(150.0, 250.0);
    stroke.addPoint(200.0, 300.0);
    stroke.finish();

    std::string serialized = stroke.serialize();
    EXPECT_FALSE(serialized.empty());

    // Check format: should contain semicolons and commas
    EXPECT_NE(serialized.find(';'), std::string::npos);
    EXPECT_NE(serialized.find(','), std::string::npos);
}

// Test deserialization
TEST_F(StrokeTest, Deserialization) {
    std::string data = "0.5,0.3;0.6,0.4;0.7,0.5;";
    Stroke stroke = Stroke::deserialize(data);

    EXPECT_TRUE(stroke.isFinished());
    EXPECT_EQ(stroke.size(), 3);
}

// Test round-trip serialization/deserialization
TEST_F(StrokeTest, SerializationRoundTrip) {
    Stroke original;
    original.addPoint(100.0, 200.0);
    original.addPoint(150.0, 250.0);
    original.addPoint(200.0, 300.0);
    original.finish();

    std::string serialized = original.serialize();
    Stroke deserialized = Stroke::deserialize(serialized);

    EXPECT_TRUE(deserialized.isFinished());
    EXPECT_EQ(deserialized.size(), original.size());

    // The serialized versions should be identical
    EXPECT_EQ(deserialized.serialize(), serialized);
}

// Test deserialization with invalid data
TEST_F(StrokeTest, DeserializationInvalidData) {
    // Empty string
    Stroke stroke1 = Stroke::deserialize("");
    EXPECT_FALSE(stroke1.isFinished());

    // Invalid format (no semicolons)
    Stroke stroke2 = Stroke::deserialize("0.5,0.3,0.6,0.4");
    EXPECT_FALSE(stroke2.isFinished());

    // Invalid numbers
    Stroke stroke3 = Stroke::deserialize("abc,def;ghi,jkl;");
    EXPECT_FALSE(stroke3.isFinished());
}

// Test comparison of identical strokes
TEST_F(StrokeTest, CompareIdenticalStrokes) {
    Stroke stroke1;
    stroke1.addPoint(100.0, 200.0);
    stroke1.addPoint(150.0, 250.0);
    stroke1.addPoint(200.0, 300.0);
    stroke1.finish();

    Stroke stroke2;
    stroke2.addPoint(100.0, 200.0);
    stroke2.addPoint(150.0, 250.0);
    stroke2.addPoint(200.0, 300.0);
    stroke2.finish();

    double cost = stroke1.compare(stroke2);

    // Identical strokes should have very low cost (close to 0)
    EXPECT_LT(cost, 0.01);
}

// Test comparison of different strokes
TEST_F(StrokeTest, CompareDifferentStrokes) {
    // Horizontal stroke
    Stroke stroke1;
    stroke1.addPoint(0.0, 50.0);
    stroke1.addPoint(100.0, 50.0);
    stroke1.finish();

    // Vertical stroke
    Stroke stroke2;
    stroke2.addPoint(50.0, 0.0);
    stroke2.addPoint(50.0, 100.0);
    stroke2.finish();

    double cost = stroke1.compare(stroke2);

    // Different strokes should have high cost
    EXPECT_GT(cost, 0.1);
}

// Test comparison of similar but not identical strokes
TEST_F(StrokeTest, CompareSimilarStrokes) {
    // Original stroke (diagonal line)
    Stroke stroke1;
    stroke1.addPoint(100.0, 200.0);
    stroke1.addPoint(150.0, 250.0);
    stroke1.addPoint(200.0, 300.0);
    stroke1.finish();

    // Similar stroke with noticeable variation (slightly curved)
    Stroke stroke2;
    stroke2.addPoint(100.0, 200.0);
    stroke2.addPoint(160.0, 250.0);  // Shifted right by 10
    stroke2.addPoint(200.0, 300.0);
    stroke2.finish();

    double cost = stroke1.compare(stroke2);

    // Similar strokes should have low but non-zero cost
    EXPECT_LT(cost, 0.1);
    EXPECT_GT(cost, 0.0);
}

// Test that comparison returns STROKE_INFINITY for invalid strokes
TEST_F(StrokeTest, CompareInvalidStrokes) {
    Stroke finished;
    finished.addPoint(100.0, 200.0);
    finished.addPoint(150.0, 250.0);
    finished.finish();

    Stroke unfinished;
    unfinished.addPoint(100.0, 200.0);
    unfinished.addPoint(150.0, 250.0);

    double cost = finished.compare(unfinished);
    EXPECT_EQ(cost, STROKE_INFINITY);
}

// Test stroke with many points
TEST_F(StrokeTest, StrokeWithManyPoints) {
    Stroke stroke;

    // Create a circular motion
    const int numPoints = 50;
    for (int i = 0; i < numPoints; i++) {
        double angle = (2.0 * M_PI * i) / numPoints;
        stroke.addPoint(100.0 + 50.0 * cos(angle),
                       100.0 + 50.0 * sin(angle));
    }

    EXPECT_TRUE(stroke.finish());
    EXPECT_EQ(stroke.size(), numPoints);
}

// Test stroke normalization preserves relative positions
TEST_F(StrokeTest, NormalizationPreservesShape) {
    // Create an L-shape
    Stroke stroke1;
    stroke1.addPoint(0.0, 0.0);
    stroke1.addPoint(0.0, 100.0);
    stroke1.addPoint(100.0, 100.0);
    stroke1.finish();

    // Create the same L-shape at different position and scale
    Stroke stroke2;
    stroke2.addPoint(500.0, 500.0);
    stroke2.addPoint(500.0, 700.0);
    stroke2.addPoint(700.0, 700.0);
    stroke2.finish();

    double cost = stroke1.compare(stroke2);

    // Should recognize as the same shape despite translation/scale
    EXPECT_LT(cost, 0.05);
}

// Test arc-length parametrization
TEST_F(StrokeTest, ArcLengthParametrization) {
    Stroke stroke;
    stroke.addPoint(0.0, 0.0);
    stroke.addPoint(100.0, 0.0);
    stroke.addPoint(100.0, 100.0);
    stroke.finish();

    const auto& points = stroke.getPoints();

    // Time parameter should start at 0 and end at 1
    EXPECT_DOUBLE_EQ(points.front().t, 0.0);
    EXPECT_DOUBLE_EQ(points.back().t, 1.0);

    // Time should be monotonically increasing
    for (size_t i = 1; i < points.size(); i++) {
        EXPECT_GT(points[i].t, points[i-1].t);
    }
}

// Test that empty serialization doesn't crash
TEST_F(StrokeTest, EmptyStrokeSerialization) {
    Stroke stroke;
    std::string serialized = stroke.serialize();
    // Should return empty or minimal string, but shouldn't crash
    EXPECT_TRUE(serialized.empty() || serialized.size() < 10);
}

// Test deserialization with trailing semicolon
TEST_F(StrokeTest, DeserializationWithTrailingSemicolon) {
    std::string data1 = "0.5,0.3;0.6,0.4;0.7,0.5;";
    std::string data2 = "0.5,0.3;0.6,0.4;0.7,0.5";

    Stroke stroke1 = Stroke::deserialize(data1);
    Stroke stroke2 = Stroke::deserialize(data2);

    EXPECT_TRUE(stroke1.isFinished());
    EXPECT_EQ(stroke1.size(), 3);

    // Without trailing semicolon, last point is not parsed
    EXPECT_TRUE(stroke2.isFinished());
    EXPECT_EQ(stroke2.size(), 2);
}
