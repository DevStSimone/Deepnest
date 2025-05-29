#include "geometryUtils.h"
#include <cmath>      // For M_PI, std::abs
#include <limits>     // For std::numeric_limits
#include <QRectF>     // Included via QPolygonF but good for clarity
#include <QPainterPath> // For more robust point-in-polygon if QPolygonF's is not sufficient

namespace GeometryUtils {

    // Calculates the area of a polygon using the shoelace formula.
    // Assumes QPolygonF points are ordered (clockwise or counter-clockwise).
    // The result will be positive for counter-clockwise and negative for clockwise.
    double signedArea(const QPolygonF& polygon) {
        if (polygon.size() < 3) {
            return 0.0;
        }
        double area = 0.0;
        for (int i = 0; i < polygon.size(); ++i) {
            const QPointF& p1 = polygon[i];
            // For a non-closed QPolygonF (where first!=last), the last segment connects last to first.
            // QPolygonF handles this implicitly in some contexts, but for shoelace, explicit is better.
            // If polygon.isClosed() is true, polygon.last() == polygon.first().
            // If not, QPolygonF still represents a closed figure for area calculations.
            const QPointF& p2 = polygon[(i + 1) % polygon.size()]; 
            area += (p1.x() * p2.y() - p2.x() * p1.y());
        }
        return area / 2.0;
    }
    
    double area(const QPolygonF& polygon) {
        return std::abs(signedArea(polygon));
    }

    QRectF boundingBox(const QPolygonF& polygon) {
        // QPolygonF::boundingRect() is efficient and directly available.
        return polygon.boundingRect();
    }

    // Basic point-in-polygon test using the ray casting algorithm.
    // Handles convex and non-convex polygons.
    // Note: QPolygonF::containsPoint can be used, but its behavior with points on edge
    // needs to be specified by Qt::FillRule. Ray casting is explicit.
    // Qt::FillRule is for QPainterPath, QPolygonF::containsPoint has its own rules.
    bool isPointInPolygon(const QPointF& point, const QPolygonF& polygon, Qt::FillRule fillRule /* = Qt::OddEvenFill */ ) {
        // QPolygonF::containsPoint is generally recommended and uses non-zero winding rule by default
        // for determining containment. For OddEvenFill, QPainterPath is better or implement ray casting.
        if (polygon.isEmpty()) return false;

        if (fillRule == Qt::OddEvenFill) {
            // Ray casting for OddEvenFill
            int n = polygon.size();
            if (n < 3) return false; // A polygon needs at least 3 vertices.
            
            bool inside = false;
            QPointF p1 = polygon.last(); // Start with the last vertex to form segment with the first vertex
            
            for (int i = 0; i < n; ++i) {
                QPointF p2 = polygon[i];
                // Check if the ray from the point intersects with the edge (p1, p2)
                if (((p1.y() <= point.y()) && (p2.y() > point.y())) || ((p2.y() <= point.y()) && (p1.y() > point.y()))) {
                    // Edge crosses the horizontal line at point.y(). Check intersection x-coordinate.
                    // vt is the x-coordinate of the intersection.
                    double vt = (point.y() - p1.y()) / (p2.y() - p1.y());
                    double intersectX = p1.x() + vt * (p2.x() - p1.x());
                    if (intersectX > point.x()) { // Point is to the left of the intersection.
                        inside = !inside;
                    }
                }
                p1 = p2; // Move to the next edge
            }
            return inside;

        } else { // Qt::WindingFill (default for QPolygonF::containsPoint)
            // QPolygonF::containsPoint(const QPointF &point, Qt::FillRule fillRule)
            // The overload with Qt::FillRule is available in some Qt versions/contexts but not universally for QPolygonF.
            // The standard QPolygonF::containsPoint usually implies WindingFill.
            // For clarity and explicit control, using QPainterPath is more robust if specific fill rules beyond default are needed.
            QPainterPath path;
            path.addPolygon(polygon); // Adds the polygon to the path
            path.setFillRule(fillRule); // Explicitly set the fill rule for contains()
            return path.contains(point);
        }
    }

} // namespace GeometryUtils
