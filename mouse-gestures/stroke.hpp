/*
 * Copyright (c) 2009, Thomas Jaeger <ThJaeger@gmail.com>
 * Adapted to C++ for Hyprland mouse-gestures plugin
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 */

#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <cassert>
#include <algorithm>
#include <limits>

constexpr double STROKE_INFINITY = 0.2;
constexpr double EPS = 0.000001;

struct Point {
    double x;      // Normalized x coordinate [0, 1]
    double y;      // Normalized y coordinate [0, 1]
    double t;      // Arc-length parameter [0, 1]
    double dt;     // Time delta to next point
    double alpha;  // Angle in range [-1, 1] (normalized by PI)
};

class Stroke {
private:
    std::vector<Point> points;
    bool finished = false;

    static inline double angleDifference(double alpha, double beta) {
        double d = alpha - beta;
        if (d < -1.0)
            d += 2.0;
        else if (d > 1.0)
            d -= 2.0;
        return d;
    }

    static inline double sqr(double x) {
        return x * x;
    }

    static inline void step(
        const Stroke& a, const Stroke& b, const int N,
        std::vector<double>& dist,
        std::vector<int>& prev_x, std::vector<int>& prev_y,
        const int x, const int y,
        const double tx, const double ty,
        int& k,
        const int x2, const int y2
    ) {
        // Bounds checking
        if (x2 >= (int)a.points.size() || y2 >= (int)b.points.size()) {
            return;
        }

        double dtx = a.points[x2].t - tx;
        double dty = b.points[y2].t - ty;
        if (dtx >= dty * 2.2 || dty >= dtx * 2.2 || dtx < EPS || dty < EPS)
            return;
        k++;

        double d = 0.0;
        int i = x, j = y;
        double next_tx = (a.points[i + 1].t - tx) / dtx;
        double next_ty = (b.points[j + 1].t - ty) / dty;
        double cur_t = 0.0;

        for (;;) {
            // Bounds checking
            if (i >= (int)a.points.size() || j >= (int)b.points.size()) {
                break;
            }

            double ad = sqr(angleDifference(
                a.points[i].alpha,
                b.points[j].alpha
            ));
            double next_t = std::min(next_tx, next_ty);
            bool done = next_t >= 1.0 - EPS;
            if (done)
                next_t = 1.0;
            d += (next_t - cur_t) * ad;
            if (done)
                break;
            cur_t = next_t;
            if (next_tx < next_ty) {
                i++;
                if (i + 1 >= (int)a.points.size())
                    break;
                next_tx = (a.points[i + 1].t - tx) / dtx;
            } else {
                j++;
                if (j + 1 >= (int)b.points.size())
                    break;
                next_ty = (b.points[j + 1].t - ty) / dty;
            }
        }
        double new_dist = dist[x * N + y] + d * (dtx + dty);

        if (new_dist >= dist[x2 * N + y2])
            return;

        prev_x[x2 * N + y2] = x;
        prev_y[x2 * N + y2] = y;
        dist[x2 * N + y2] = new_dist;
    }

public:
    Stroke() = default;

    bool addPoint(double x, double y) {
        if (finished) {
            return false;
        }
        points.push_back({x, y, 0.0, 0.0, 0.0});
        return true;
    }

    bool finish() {
        if (finished || points.size() < 2) {
            return false;
        }
        finished = true;

        const int n = points.size() - 1;

        // Calculate arc-length parametrization
        double total = 0.0;
        points[0].t = 0.0;
        for (int i = 0; i < n; i++) {
            double dx = points[i + 1].x - points[i].x;
            double dy = points[i + 1].y - points[i].y;
            total += std::hypot(dx, dy);
            points[i + 1].t = total;
        }

        // Normalize time parameter to [0, 1]
        for (int i = 0; i <= n; i++)
            points[i].t /= total;

        // Find bounding box
        double minX = points[0].x, minY = points[0].y;
        double maxX = minX, maxY = minY;
        for (int i = 1; i <= n; i++) {
            minX = std::min(minX, points[i].x);
            maxX = std::max(maxX, points[i].x);
            minY = std::min(minY, points[i].y);
            maxY = std::max(maxY, points[i].y);
        }

        // Normalize coordinates to [0, 1] centered at 0.5
        double scaleX = maxX - minX;
        double scaleY = maxY - minY;
        double scale = std::max(scaleX, scaleY);
        if (scale < 0.001)
            scale = 1.0;

        for (int i = 0; i <= n; i++) {
            points[i].x = (points[i].x - (minX + maxX) / 2) / scale + 0.5;
            points[i].y = (points[i].y - (minY + maxY) / 2) / scale + 0.5;
        }

        // Calculate angles and time deltas
        for (int i = 0; i < n; i++) {
            points[i].dt = points[i + 1].t - points[i].t;
            double dx = points[i + 1].x - points[i].x;
            double dy = points[i + 1].y - points[i].y;
            points[i].alpha = std::atan2(dy, dx) / M_PI;
        }

        return true;
    }

    // Compare two strokes using dynamic programming
    // Returns cost (lower is better, < STROKE_INFINITY means match)
    // Returns STROKE_INFINITY on error
    double compare(const Stroke& other) const {
        if (!finished || !other.finished || points.empty() || other.points.empty()) {
            return STROKE_INFINITY;
        }

        const int M = points.size();
        const int N = other.points.size();
        const int m = M - 1;
        const int n = N - 1;

        std::vector<double> dist(M * N, STROKE_INFINITY);
        std::vector<int> prev_x(M * N);
        std::vector<int> prev_y(M * N);

        dist[0] = 0.0;

        for (int x = 0; x < m; x++) {
            for (int y = 0; y < n; y++) {
                if (dist[x * N + y] >= STROKE_INFINITY)
                    continue;

                double tx = points[x].t;
                double ty = other.points[y].t;
                int max_x = x;
                int max_y = y;
                int k = 0;

                while (k < 4) {
                    if (points[max_x + 1].t - tx >
                        other.points[max_y + 1].t - ty) {
                        max_y++;
                        if (max_y == n) {
                            step(*this, other, N, dist, prev_x, prev_y,
                                 x, y, tx, ty, k, m, n);
                            break;
                        }
                        for (int x2 = x + 1; x2 <= max_x; x2++)
                            step(*this, other, N, dist, prev_x, prev_y,
                                 x, y, tx, ty, k, x2, max_y);
                    } else {
                        max_x++;
                        if (max_x == m) {
                            step(*this, other, N, dist, prev_x, prev_y,
                                 x, y, tx, ty, k, m, n);
                            break;
                        }
                        for (int y2 = y + 1; y2 <= max_y; y2++)
                            step(*this, other, N, dist, prev_x, prev_y,
                                 x, y, tx, ty, k, max_x, y2);
                    }
                }
            }
        }

        return dist[M * N - 1];
    }

    size_t size() const { return points.size(); }
    bool isFinished() const { return finished; }
    const std::vector<Point>& getPoints() const { return points; }

    // Serialize stroke to string for configuration
    std::string serialize() const {
        std::string result;
        for (const auto& p : points) {
            result += std::to_string(p.x) + "," +
                      std::to_string(p.y) + ";";
        }
        return result;
    }

    // Deserialize stroke from string
    // Returns empty stroke on error
    static Stroke deserialize(const std::string& data) {
        Stroke stroke;
        try {
            size_t pos = 0;
            while (pos < data.size()) {
                size_t comma = data.find(',', pos);
                size_t semi = data.find(';', pos);
                if (comma == std::string::npos || semi == std::string::npos)
                    break;

                // Check bounds
                if (comma >= data.size() || semi >= data.size() ||
                    comma <= pos || semi <= comma) {
                    break;
                }

                double x = std::stod(data.substr(pos, comma - pos));
                double y = std::stod(data.substr(comma + 1, semi - comma - 1));

                // Validate coordinates (should be reasonable screen coords)
                if (!std::isfinite(x) || !std::isfinite(y)) {
                    return Stroke();
                }

                if (!stroke.addPoint(x, y)) {
                    return Stroke();
                }

                pos = semi + 1;
            }
            if (stroke.size() > 1) {
                if (!stroke.finish()) {
                    return Stroke();
                }
            }
        } catch (const std::exception&) {
            // Return empty stroke on any parsing error
            return Stroke();
        }
        return stroke;
    }
};
