#ifndef INTERNALTYPES_H
#define INTERNALTYPES_H

#include <QPolygonF>
#include <QList>
#include <QString>
#include <QRectF> // For bounding box

namespace Core {

// Represents a single polygon, which could be an outer boundary or a hole.
// Orientation (clockwise/counter-clockwise) typically defines this in many geometry libraries.
// For Clipper2, specific functions handle paths and holes.
// typedef QPolygonF Polygon; // Simple alias

struct TransformedPath {
    QPolygonF path; // Path data, translated and rotated to its final position for NFP calculation
    // The NFP calculation itself might not need the original ID or source transformations,
    // just the geometry of the two shapes as they are to be compared.
};

struct InternalSheet;
// Represents a part to be nested, including its main shape and any holes.
// This structure would be the output of converting QPainterPath from SvgNest input.
struct InternalPart {
    QString id;
    QPolygonF outerBoundary;    // The main outline of the part
    QList<QPolygonF> holes;     // List of polygons representing holes within the part

    // Optional: Pre-calculated properties
    QRectF bounds;              // Bounding box of the outerBoundary

    // Constructor
    InternalPart(QString p_id = "", QPolygonF p_outer = QPolygonF(), QList<QPolygonF> p_holes = QList<QPolygonF>())
        : id(p_id), outerBoundary(p_outer), holes(p_holes) {
        if (!p_outer.isEmpty()) {
            bounds = p_outer.boundingRect();
        }
    }
    
    InternalPart(const InternalSheet& sheet);

    bool isValid() const { return !outerBoundary.isEmpty(); }
};

// Represents the sheet material
struct InternalSheet {
    QString id; // Optional identifier
    QPolygonF outerBoundary;
    QList<QPolygonF> holes; // Sheets can also have holes/cutouts

    QRectF bounds;

    InternalSheet(QPolygonF p_outer = QPolygonF(), QList<QPolygonF> p_holes = QList<QPolygonF>())
        : outerBoundary(p_outer), holes(p_holes) {
        if (!p_outer.isEmpty()) {
            bounds = p_outer.boundingRect();
        }
    }
     bool isValid() const { return !outerBoundary.isEmpty(); }

    InternalSheet(const InternalPart& part )
        : outerBoundary(part.outerBoundary), holes(part.holes) {
        if (!outerBoundary.isEmpty()) {
            bounds = outerBoundary.boundingRect();
        }
    }
};

InternalPart::InternalPart(const InternalSheet& part )
    : outerBoundary(part.outerBoundary), holes(part.holes) {
    if (!outerBoundary.isEmpty()) {
        bounds = outerBoundary.boundingRect();
    }
}

} // namespace Core
#endif // INTERNALTYPES_H
