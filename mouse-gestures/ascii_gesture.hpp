#pragma once

#include "stroke.hpp"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

class AsciiGestureRenderer {
private:
    static constexpr int MAX_HEIGHT = 6;
    static constexpr double CHAR_ASPECT = 2.5; // Character height/width ratio

    struct BezierPoint {
        double x, y;
    };

    // Sample a cubic Bezier curve at parameter t
    static BezierPoint sampleBezier(const BezierPoint& p0, const BezierPoint& p1,
                                     const BezierPoint& p2, const BezierPoint& p3,
                                     double t) {
        double u = 1.0 - t;
        double u2 = u * u;
        double u3 = u2 * u;
        double t2 = t * t;
        double t3 = t2 * t;

        return {
            u3 * p0.x + 3.0 * u2 * t * p1.x + 3.0 * u * t2 * p2.x + t3 * p3.x,
            u3 * p0.y + 3.0 * u2 * t * p1.y + 3.0 * u * t2 * p2.y + t3 * p3.y
        };
    }

    // Calculate Bezier control points using easystroke's smoothing algorithm
    static void calculateBezierSegments(const std::vector<Point>& points,
                                       std::vector<BezierPoint>& bezierPoints) {
        const int n = points.size();
        if (n < 2) return;

        // Lambda coefficients for smoothing (from easystroke)
        std::vector<double> lambda(n);
        lambda[0] = lambda[n - 1] = 0.0;

        for (int i = 1; i < n - 1; i++) {
            double dt_prev = (i > 0) ? points[i].dt : 0.0;
            double dt_next = points[i].dt;
            double sum = dt_prev + dt_next;
            if (sum > 0.0001) {
                lambda[i] = dt_next / sum;
            } else {
                lambda[i] = 0.5;
            }
        }

        // Generate Bezier control points for each segment
        for (int i = 0; i < n - 1; i++) {
            BezierPoint p0 = {points[i].x, points[i].y};
            BezierPoint p3 = {points[i + 1].x, points[i + 1].y};

            // Calculate control points p1 and p2
            double dx = p3.x - p0.x;
            double dy = p3.y - p0.y;

            // Use lambda for smoothing
            double l0 = (i > 0) ? lambda[i] : 0.5;
            double l1 = (i < n - 2) ? lambda[i + 1] : 0.5;

            BezierPoint p1 = {
                p0.x + dx * l0 / 3.0,
                p0.y + dy * l0 / 3.0
            };

            BezierPoint p2 = {
                p3.x - dx * (1.0 - l1) / 3.0,
                p3.y - dy * (1.0 - l1) / 3.0
            };

            // Sample the Bezier curve
            int samples = std::max(3, static_cast<int>(
                std::hypot(dx, dy) * 20)); // More samples for longer segments

            for (int j = 0; j < samples; j++) {
                double t = static_cast<double>(j) / samples;
                bezierPoints.push_back(sampleBezier(p0, p1, p2, p3, t));
            }
        }

        // Add final point
        bezierPoints.push_back({points[n - 1].x, points[n - 1].y});
    }

    // Determine direction character based on angle
    static char getDirectionChar(double dx, double dy) {
        double angle = std::atan2(dy, dx);
        double degrees = angle * 180.0 / M_PI;

        // Normalize to 0-360
        if (degrees < 0) degrees += 360.0;

        // 8 directions
        if (degrees >= 337.5 || degrees < 22.5) return '-';      // Right
        if (degrees >= 22.5 && degrees < 67.5) return '\\';      // Down-right
        if (degrees >= 67.5 && degrees < 112.5) return '|';      // Down
        if (degrees >= 112.5 && degrees < 157.5) return '/';     // Down-left
        if (degrees >= 157.5 && degrees < 202.5) return '-';     // Left
        if (degrees >= 202.5 && degrees < 247.5) return '\\';    // Up-left
        if (degrees >= 247.5 && degrees < 292.5) return '|';     // Up
        return '/';                                               // Up-right
    }

public:
    static std::vector<std::string> render(const Stroke& stroke) {
        std::vector<std::string> result;

        if (!stroke.isFinished() || stroke.size() < 2) {
            result.push_back("#  (empty gesture)");
            return result;
        }

        const auto& points = stroke.getPoints();

        // Calculate Bezier curve points for smooth rendering
        std::vector<BezierPoint> bezierPoints;
        calculateBezierSegments(points, bezierPoints);

        if (bezierPoints.empty()) {
            result.push_back("#  (invalid gesture)");
            return result;
        }

        // Find bounding box of Bezier points
        double minX = bezierPoints[0].x, maxX = bezierPoints[0].x;
        double minY = bezierPoints[0].y, maxY = bezierPoints[0].y;

        for (const auto& p : bezierPoints) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }

        // Calculate grid dimensions with aspect ratio correction
        double rangeX = maxX - minX;
        double rangeY = maxY - minY;

        if (rangeX < 0.001) rangeX = 0.1;
        if (rangeY < 0.001) rangeY = 0.1;

        double aspect = rangeX / rangeY;

        int width, height;
        if (aspect * CHAR_ASPECT > 1.0) {
            // Wider than tall
            width = static_cast<int>(MAX_HEIGHT * aspect * CHAR_ASPECT);
            height = MAX_HEIGHT;
        } else {
            // Taller than wide
            height = MAX_HEIGHT;
            width = static_cast<int>(height * aspect * CHAR_ASPECT);
        }

        width = std::max(3, std::min(width, 50)); // Clamp width
        height = std::max(1, std::min(height, MAX_HEIGHT));

        // Create grid
        std::vector<std::vector<char>> grid(height, std::vector<char>(width, ' '));

        // Draw the path
        for (size_t i = 1; i < bezierPoints.size(); i++) {
            const auto& prev = bezierPoints[i - 1];
            const auto& curr = bezierPoints[i];

            // Map to grid coordinates
            int x1 = static_cast<int>((prev.x - minX) / rangeX * (width - 1));
            int y1 = static_cast<int>((prev.y - minY) / rangeY * (height - 1));
            int x2 = static_cast<int>((curr.x - minX) / rangeX * (width - 1));
            int y2 = static_cast<int>((curr.y - minY) / rangeY * (height - 1));

            // Bresenham's line algorithm
            int dx = std::abs(x2 - x1);
            int dy = std::abs(y2 - y1);
            int sx = (x1 < x2) ? 1 : -1;
            int sy = (y1 < y2) ? 1 : -1;
            int err = dx - dy;

            int x = x1, y = y1;

            while (true) {
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    double dirX = curr.x - prev.x;
                    double dirY = curr.y - prev.y;
                    grid[y][x] = getDirectionChar(dirX, dirY);
                }

                if (x == x2 && y == y2) break;

                int e2 = 2 * err;
                if (e2 > -dy) {
                    err -= dy;
                    x += sx;
                }
                if (e2 < dx) {
                    err += dx;
                    y += sy;
                }
            }
        }

        // Mark start and end
        int startX = static_cast<int>((bezierPoints.front().x - minX) / rangeX * (width - 1));
        int startY = static_cast<int>((bezierPoints.front().y - minY) / rangeY * (height - 1));
        int endX = static_cast<int>((bezierPoints.back().x - minX) / rangeX * (width - 1));
        int endY = static_cast<int>((bezierPoints.back().y - minY) / rangeY * (height - 1));

        if (startX >= 0 && startX < width && startY >= 0 && startY < height) {
            grid[startY][startX] = 'S';
        }
        if (endX >= 0 && endX < width && endY >= 0 && endY < height) {
            grid[endY][endX] = 'E';
        }

        // Convert grid to strings
        for (const auto& row : grid) {
            std::string line = "# ";
            for (char c : row) {
                line += c;
            }
            // Remove trailing spaces
            size_t end = line.find_last_not_of(' ');
            if (end != std::string::npos) {
                line = line.substr(0, end + 1);
            }
            result.push_back(line);
        }

        return result;
    }
};
