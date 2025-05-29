#ifndef HULLPOLYGON_H
#define HULLPOLYGON_H

#include <QPolygonF>

namespace Geometry {
    class HullPolygon {
    public:
        // Monotone Chain algorithm for convex hull
        static QPolygonF convexHull(const QPolygonF& points);
    };
}

#endif // HULLPOLYGON_H
