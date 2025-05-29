#ifndef GEOMETRYUTILS_H
#define GEOMETRYUTILS_H

#include <QPointF>
#include <QPolygonF>
#include <QRectF>

namespace GeometryUtils {
    // Placeholder for various geometric utility functions


double signedArea(const QPolygonF& polygon);
double area(const QPolygonF& polygon);

QRectF boundingBox(const QPolygonF& polygon);

bool isPointInPolygon(const QPointF& point, const QPolygonF& polygon, Qt::FillRule fillRule = Qt::OddEvenFill );
}

#endif // GEOMETRYUTILS_H
