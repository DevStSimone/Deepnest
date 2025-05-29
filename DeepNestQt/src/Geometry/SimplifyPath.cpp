#include "SimplifyPath.h"
#include <cmath>    // For std::sqrt, std::pow
#include <QVector>  // For temporary point storage

namespace Geometry {

// Helper function to calculate perpendicular distance from a point to a line segment
double perpendicularDistance(const QPointF& pt, const QPointF& lineStart, const QPointF& lineEnd) {
    double dx = lineEnd.x() - lineStart.x();
    double dy = lineEnd.y() - lineStart.y();

    if (dx == 0 && dy == 0) { // lineStart and lineEnd are the same point
        return std::sqrt(std::pow(pt.x() - lineStart.x(), 2) + std::pow(pt.y() - lineStart.y(), 2));
    }

    double t = ((pt.x() - lineStart.x()) * dx + (pt.y() - lineStart.y()) * dy) / (dx * dx + dy * dy);

    QPointF closestPoint;
    if (t < 0) {
        closestPoint = lineStart;
    } else if (t > 1) {
        closestPoint = lineEnd;
    } else {
        closestPoint = QPointF(lineStart.x() + t * dx, lineStart.y() + t * dy);
    }

    return std::sqrt(std::pow(pt.x() - closestPoint.x(), 2) + std::pow(pt.y() - closestPoint.y(), 2));
}

// Recursive helper for RDP
void rdpSimplify(const QPolygonF& points, double epsilon, QVector<QPointF>& outPoints, int startIndex, int endIndex) {
    if (startIndex >= endIndex) {
        return;
    }

    double maxDist = 0;
    int index = startIndex;

    for (int i = startIndex + 1; i < endIndex; ++i) {
        double dist = perpendicularDistance(points[i], points[startIndex], points[endIndex]);
        if (dist > maxDist) {
            maxDist = dist;
            index = i;
        }
    }

    if (maxDist > epsilon) {
        // Recursive call for the two new segments
        rdpSimplify(points, epsilon, outPoints, startIndex, index);
        // outPoints.append(points[index]); // Add the point that was furthest // Bug: this adds points out of order
        rdpSimplify(points, epsilon, outPoints, index, endIndex);
    } else {
        // If no point is further than epsilon, the segment (startIndex, endIndex) is kept as is,
        // meaning all intermediate points are discarded.
        // The end point of this segment (points[endIndex]) will be added by the caller or subsequent calls.
        // Only points[startIndex] is implicitly kept by the structure of calls.
        // The current structure of rdpSimplify means we don't add points[endIndex] here,
        // it's handled by the main simplify function or subsequent recursive calls.
        // However, the provided logic in the main `simplify` function expects `outPoints` to be populated
        // with the simplified polyline *between* the first and last points.

        // Correct logic for RDP is typically to add the `index` point if `maxDist > epsilon`,
        // and do nothing if `maxDist <= epsilon` because the segment `points[startIndex]` to `points[endIndex]`
        // itself simplifies the points in between.
        // The main `simplify` function then reconstructs the polygon.

        // Let's adjust based on typical RDP: if segment is not to be simplified further,
        // it means the line from points[startIndex] to points[endIndex] represents this part.
        // The points themselves (points[startIndex] and points[endIndex]) are handled by the outer logic.
        // The points that get explicitly added to `outPoints` are those that *break* a segment.
    }
}


QPolygonF SimplifyPath::simplify(const QPolygonF& points, double epsilon) {
    if (epsilon <= 0 || points.size() < 3) {
        return points;
    }

    QVector<bool> keep(points.size(), false);
    keep[0] = true;
    keep[points.size() - 1] = true;

    std::function<void(int, int)> simplifyRecursive = 
        [&](int startIndex, int endIndex) {
        double maxDist = 0;
        int furthestIndex = startIndex;
        for (int i = startIndex + 1; i < endIndex; ++i) {
            double dist = perpendicularDistance(points[i], points[startIndex], points[endIndex]);
            if (dist > maxDist) {
                maxDist = dist;
                furthestIndex = i;
            }
        }
        if (maxDist > epsilon) {
            keep[furthestIndex] = true;
            simplifyRecursive(startIndex, furthestIndex);
            simplifyRecursive(furthestIndex, endIndex);
        }
    };

    simplifyRecursive(0, points.size() - 1);

    QPolygonF simplifiedPolygon;
    for(int i = 0; i < points.size(); ++i) {
        if (keep[i]) {
            simplifiedPolygon.append(points[i]);
        }
    }
    
    // The original prompt's rdpSimplify was trying to build the path directly,
    // which is harder to get right with recursion for polylines.
    // The typical RDP marks points to keep.

    // The old rdpSimplify logic:
    // QVector<QPointF> simplifiedPointsCollector;
    // simplifiedPointsCollector.append(points.first());
    // rdpSimplify(points, epsilon, simplifiedPointsCollector, 0, points.size() - 1);
    // simplifiedPointsCollector.append(points.last());
    // QPolygonF result = QPolygonF(simplifiedPointsCollector);
    // Deduplicate points that might have been added if first/last were also high-distance points
    // result.detach(); // Make sure it's not shared if we are about to modify
    // if (result.size() > 1 && result.first() == result.last() && !points.isClosed()) {
        // This case is tricky: if it wasn't closed but RDP made it so.
    // } else if (points.isClosed() && (result.isEmpty() || result.first() != result.last())) {
        // if (!result.isEmpty()) result.append(result.first()); // Force close
    // }


    // Ensure closure is maintained based on input, if it's a valid polygon
    if (points.isClosed() && !simplifiedPolygon.isEmpty() && simplifiedPolygon.first() != simplifiedPolygon.last()) {
        // If the original was closed, the simplified one should be too.
        // RDP might remove points such that first!=last.
        // If simplifiedPolygon has at least 1 point and it's not matching the first one, append the first one.
        // Or, if the original first and last were the same, and RDP kept them, it's fine.
        // If RDP removed the original last point (which was same as first), then the new last might differ.
        // A robust way for closed polygons is to run RDP on points[0...size-1] and then on points[size-1, 0] (the closing segment)
        // or ensure the logic correctly handles the cyclic nature.
        // The current RDP (marking points) on [0...size-1] should inherently handle this.
        // If points[0] and points[size-1] are the same, and both are marked true, they will be in simplifiedPolygon.
    }


    return simplifiedPolygon;
}


} // namespace Geometry
