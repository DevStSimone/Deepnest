#ifndef SVGPARSER_H
#define SVGPARSER_H

#include <QString>
#include <QList>
#include <QDomDocument>
#include <QTransform>
#include <QPainterPath> // Added include
#include "DataStructures.h" // Contains Part, Polygon, Point

class SvgParser {
public:
    SvgParser();

    struct Config {
        double tolerance;        // Max bound for bezier->line segment conversion
        double endpointTolerance; // For merging lines
        // scale is handled more dynamically by the load function's output
        Config() : tolerance(0.1), endpointTolerance(1e-5) {} // Default values
    };

    void setConfig(const Config& config);

    // Loads SVG, determines initial scale relative to 96 DPI (SVG default), applies initial transform.
    // Returns the root QDomElement. outDeviceUnitsToSvgUnitsScale is the scale factor to convert device units (e.g. pixels from a GUI) to SVG units.
    // The caller can then combine this with user-defined scaling.
    QDomElement load(const QString& svgString, double& outSvgUnitsToDeviceUnitsScale);
    
    // Extracts parts from the SVG DOM structure.
    // unitConversionFactor: a global factor to convert SVG units (potentially already scaled by `load`) to the application's internal units.
    QList<Part> getParts(const QDomElement& rootElement, double unitConversionFactor);

    // Converts a single DOM element to a Polygon.
    Polygon polygonify(const QDomElement& element, double unitConversionFactor, const QTransform& initialTransform = QTransform());
    
    // Recursively applies transformations. Modifies the element's coordinate-defining attributes.
    void applyTransformRecursive(QDomElement& element, const QTransform& accumulatedTransform);

    // Placeholder for cleaning operations (merging lines, etc.)
    // This will be complex and might be a separate subtask to fully implement.
    void cleanSvgDom(QDomElement& rootElement);


private:
    QDomDocument doc; // Holds the current SVG document
    Config currentConfig;

    // Path parsing and linearization (will be complex)
    std::vector<Point> polygonifyPath(const QDomElement& pathElement, const QTransform& currentTransform, double unitConversionFactor);
    std::vector<Point> linearizePathSegment(const QPainterPath::Element& segment, const QPointF& lastPoint, double tolerance); // Corrected signature


    // Helpers for specific shapes
    Polygon polygonifyRect(const QDomElement& rectElement, const QTransform& currentTransform, double unitConversionFactor);
    Polygon polygonifyCircle(const QDomElement& circleElement, const QTransform& currentTransform, double unitConversionFactor);
    Polygon polygonifyEllipse(const QDomElement& ellipseElement, const QTransform& currentTransform, double unitConversionFactor);
    Polygon polygonifyPolyline(const QDomElement& polylineElement, const QTransform& currentTransform, double unitConversionFactor);
    Polygon polygonifyPolygon(const QDomElement& polygonElement, const QTransform& currentTransform, double unitConversionFactor);
    Polygon polygonifyLine(const QDomElement& lineElement, const QTransform& currentTransform, double unitConversionFactor);
    
    // Helper to parse transform attributes from SVG elements
    QTransform parseTransform(const QString& transformString);

    // Helper to convert QPainterPath to our Polygon struct
    Polygon qPainterPathToPolygon(const QPainterPath& path, const QTransform& transform, double unitConversionFactor);

    // Helper to parse points string from polyline/polygon
    std::vector<Point> parsePointsString(const QString& pointsStr, const QTransform& transform, double unitConversionFactor);

    // Helper to parse SVG length attributes (e.g., "100px", "10cm") into a common unit (e.g., SVG user units)
    double parseLength(const QString& lengthStr, const QDomElement& contextElement, double dpi = 96.0);
    double parseViewBoxValue(const QString& valueStr, double percentBase = 100.0); // Helper for viewBox and percent lengths
};

#endif // SVGPARSER_H
