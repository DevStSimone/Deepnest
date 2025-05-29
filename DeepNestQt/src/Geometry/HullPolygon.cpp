#include "HullPolygon.h"
#include <algorithm> // For std::sort, std::unique (if needed)
#include <QVector>

namespace Geometry {

// Helper for cross product (P, Q, R)
// Returns > 0 if R is to the left of PQ, < 0 if to the right, = 0 if collinear
double cross_product(QPointF p, QPointF q, QPointF r) {
    return (q.x() - p.x()) * (r.y() - p.y()) - (q.y() - p.y()) * (r.x() - p.x());
}

QPolygonF HullPolygon::convexHull(const QPolygonF& pointsInput) {
    int n = pointsInput.size();
    if (n < 3) {
        // For n < 3, the concept of a convex hull is typically the points themselves
        // or an empty polygon if one prefers strict polygon definitions.
        // QPolygonF can handle 0, 1, or 2 points, often representing degenerate polygons.
        return pointsInput; 
    }

    // Make a mutable copy and sort points lexicographically
    QVector<QPointF> points = QVector<QPointF>::fromStdVector(pointsInput.toStdVector());
    std::sort(points.begin(), points.end(), [](const QPointF& a, const QPointF& b) {
        return a.x() < b.x() || (a.x() == b.x() && a.y() < b.y());
    });

    // Remove duplicates to ensure robustness, especially for collinear points.
    // std::unique returns an iterator to the end of the unique range.
    auto last = std::unique(points.begin(), points.end());
    points.erase(last, points.end());
    n = points.size();
    if (n < 3) { // Check again after duplicate removal
        return QPolygonF(points); // Return the (unique) points
    }


    QVector<QPointF> upperHull;
    QVector<QPointF> lowerHull;

    for (int i = 0; i < n; ++i) {
        // Build upper hull
        // Note: <= 0 for collinear points to be on the hull, < 0 to exclude them (stricter hull)
        while (upperHull.size() >= 2 && cross_product(upperHull[upperHull.size()-2], upperHull.last(), points[i]) <= 0) {
            upperHull.pop_back();
        }
        upperHull.push_back(points[i]);

        // Build lower hull
        // Processing points from right to left for the lower hull by using points[n - 1 - i]
        while (lowerHull.size() >= 2 && cross_product(lowerHull[lowerHull.size()-2], lowerHull.last(), points[n - 1 - i]) <= 0) {
            lowerHull.pop_back();
        }
        lowerHull.push_back(points[n - 1 - i]);
    }

    // Concatenate hulls
    QPolygonF hullPolygon;
    for (int i = 0; i < upperHull.size(); ++i) {
        hullPolygon.append(upperHull[i]);
    }
    // Append lower hull points in reverse order (which they are due to points[n-1-i]), 
    // skipping the first and last points of lowerHull as they are duplicates of the last and first points of upperHull respectively.
    for (int i = 1; i < lowerHull.size() - 1; ++i) {
        hullPolygon.append(lowerHull[i]);
    }
    
    // The QPolygonF created this way should be closed if it forms a valid polygon (n >= 3 unique points not all collinear).
    // QPolygonF automatically considers a polygon closed if the first and last points are the same.
    // If the hull results in a line (all points collinear), QPolygonF will represent it as such.
    // No explicit check like `hullPolygon.first() != hullPolygon.last()` is typically needed here,
    // as QPolygonF manages its own representation.

    return hullPolygon;
}
} // namespace Geometry
