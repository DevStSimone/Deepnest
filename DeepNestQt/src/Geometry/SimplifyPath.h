#ifndef SIMPLIFYPATH_H
#define SIMPLIFYPATH_H

#include <QPolygonF>

namespace Geometry {
    class SimplifyPath {
    public:
        // Ramer-Douglas-Peucker algorithm
        static QPolygonF simplify(const QPolygonF& points, double epsilon);
    };
}

#endif // SIMPLIFYPATH_H
