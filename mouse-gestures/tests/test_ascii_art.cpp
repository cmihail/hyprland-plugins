#include "../ascii_gesture.hpp"
#include <iostream>
#include <cassert>

void testDiagonalLine() {
    std::cout << "Testing diagonal line...\n";
    Stroke stroke;
    for (int i = 0; i <= 10; i++) {
        stroke.addPoint(i * 100.0, i * 100.0);
    }
    stroke.finish();

    auto art = AsciiGestureRenderer::render(stroke);

    assert(!art.empty());
    assert(art.size() <= 6);

    // Check that all lines start with "# "
    for (const auto& line : art) {
        assert(line.substr(0, 2) == "# ");
    }

    // Check for no trailing spaces
    for (const auto& line : art) {
        if (line.length() > 2) {
            assert(line.back() != ' ');
        }
    }

    std::cout << "Diagonal line test passed\n";
}

void testHorizontalLine() {
    std::cout << "Testing horizontal line...\n";
    Stroke stroke;
    for (int i = 0; i <= 10; i++) {
        stroke.addPoint(i * 100.0, 500.0);
    }
    stroke.finish();

    auto art = AsciiGestureRenderer::render(stroke);

    assert(!art.empty());
    assert(art.size() <= 6);

    // Should contain horizontal line characters
    bool hasHorizontal = false;
    for (const auto& line : art) {
        if (line.find('-') != std::string::npos) {
            hasHorizontal = true;
            break;
        }
    }
    assert(hasHorizontal);

    std::cout << "Horizontal line test passed\n";
}

void testVerticalLine() {
    std::cout << "Testing vertical line...\n";
    Stroke stroke;
    for (int i = 0; i <= 10; i++) {
        stroke.addPoint(500.0, i * 100.0);
    }
    stroke.finish();

    auto art = AsciiGestureRenderer::render(stroke);

    assert(!art.empty());
    assert(art.size() <= 6);

    // Should contain vertical line characters
    bool hasVertical = false;
    for (const auto& line : art) {
        if (line.find('|') != std::string::npos) {
            hasVertical = true;
            break;
        }
    }
    assert(hasVertical);

    std::cout << "Vertical line test passed\n";
}

void testCircleGesture() {
    std::cout << "Testing circle gesture...\n";
    Stroke stroke;
    for (int i = 0; i < 20; i++) {
        double angle = i * 2.0 * M_PI / 20;
        stroke.addPoint(500.0 + 400.0 * std::cos(angle),
                       500.0 + 400.0 * std::sin(angle));
    }
    stroke.finish();

    auto art = AsciiGestureRenderer::render(stroke);

    assert(!art.empty());
    assert(art.size() <= 6);

    // Should contain various directional characters
    std::string allLines;
    for (const auto& line : art) {
        allLines += line;
    }

    // A circle should have multiple directions
    int directionCount = 0;
    if (allLines.find('-') != std::string::npos) directionCount++;
    if (allLines.find('|') != std::string::npos) directionCount++;
    if (allLines.find('/') != std::string::npos) directionCount++;
    if (allLines.find('\\') != std::string::npos) directionCount++;

    assert(directionCount >= 3);

    std::cout << "Circle gesture test passed\n";
}

void testEmptyGesture() {
    std::cout << "Testing empty gesture...\n";
    Stroke stroke;

    auto art = AsciiGestureRenderer::render(stroke);

    assert(!art.empty());
    assert(art[0].find("empty") != std::string::npos);

    std::cout << "Empty gesture test passed\n";
}

void testStartEndMarkers() {
    std::cout << "Testing start/end markers...\n";
    Stroke stroke;
    for (int i = 0; i <= 10; i++) {
        stroke.addPoint(i * 100.0, 500.0);
    }
    stroke.finish();

    auto art = AsciiGestureRenderer::render(stroke);

    std::string allLines;
    for (const auto& line : art) {
        allLines += line;
    }

    // Should have both S and E markers
    assert(allLines.find('S') != std::string::npos);
    assert(allLines.find('E') != std::string::npos);

    std::cout << "Start/end markers test passed\n";
}

void testNoTrailingSpaces() {
    std::cout << "Testing no trailing spaces...\n";

    // Test with various gestures
    std::vector<Stroke> strokes;

    // Diagonal
    Stroke s1;
    for (int i = 0; i <= 10; i++) s1.addPoint(i * 100.0, i * 100.0);
    s1.finish();
    strokes.push_back(s1);

    // L-shape
    Stroke s2;
    for (int i = 0; i <= 5; i++) s2.addPoint(100.0, i * 100.0);
    for (int i = 1; i <= 5; i++) s2.addPoint(100.0 + i * 100.0, 500.0);
    s2.finish();
    strokes.push_back(s2);

    for (const auto& stroke : strokes) {
        auto art = AsciiGestureRenderer::render(stroke);
        for (const auto& line : art) {
            if (line.length() > 2) {
                assert(line.back() != ' ');
            }
        }
    }

    std::cout << "No trailing spaces test passed\n";
}

int main() {
    std::cout << "Running ASCII art tests...\n\n";

    testDiagonalLine();
    testHorizontalLine();
    testVerticalLine();
    testCircleGesture();
    testEmptyGesture();
    testStartEndMarkers();
    testNoTrailingSpaces();

    std::cout << "\nAll tests passed!\n";
    return 0;
}
