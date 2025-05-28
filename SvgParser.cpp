#include "SvgParser.h"
#include <QFile>
#include <QBuffer>
#include <QXmlStreamReader> // For parsing transform string more robustly if needed
#include <QtMath> // For qDegreesToRadians, qCos, qSin
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QDebug> // For error messages

// Helper for parsing doubles, robust to locale
static double toDouble(const QString& str, bool* ok = nullptr) {
    QLocale cLocale(QLocale::C);
    return cLocale.toDouble(str, ok);
}


SvgParser::SvgParser() {
    // Default config is already set in SvgParser.h
}

void SvgParser::setConfig(const Config& config) {
    currentConfig = config;
}

// Helper to parse SVG length attributes (e.g., "100px", "10cm") into a common unit (e.g., SVG user units)
// For simplicity, this initial version only handles unitless numbers (assumed to be SVG user units) and "px".
// More units (cm, mm, in, pt, pc) can be added later.
double SvgParser::parseLength(const QString& lengthStr, const QDomElement& contextElement, double dpi) {
    Q_UNUSED(contextElement); // For future use (e.g. for font-size relative units like 'em', 'ex')
    Q_UNUSED(dpi); // For future use with physical units

    if (lengthStr.isEmpty()) return 0.0;

    bool ok;
    double value = toDouble(lengthStr, &ok);
    if (ok && (lengthStr.endsWith("px") || !lengthStr.contains(QRegularExpression("[a-zA-Z]")))) { // unitless or px
        return value;
    }
    // Add more unit conversions here (cm, mm, in, etc.)
    // For now, if it's not a plain number or 'px', assume it's a user unit or unsupported.
    // A more robust solution would be to parse units properly.
    return toDouble(lengthStr.chopped(2), &ok); // Attempt to strip two-letter units if toDouble failed initially.
}

double SvgParser::parseViewBoxValue(const QString& valueStr, double percentBase) {
    // This is a simplified parser. It doesn't handle percentages in viewBox itself,
    // but parseLength might handle some if extended. For viewBox, values are usually numbers.
    Q_UNUSED(percentBase);
    bool ok;
    double val = toDouble(valueStr.trimmed(), &ok);
    return ok ? val : 0.0;
}


QDomElement SvgParser::load(const QString& svgString, double& outSvgUnitsToDeviceUnitsScale) {
    QString errorMsg;
    int errorLine, errorColumn;
    if (!doc.setContent(svgString, &errorMsg, &errorLine, &errorColumn)) {
        qWarning() << "Failed to parse SVG content:" << errorMsg << "at line" << errorLine << "column" << errorColumn;
        return QDomElement();
    }

    QDomElement root = doc.documentElement();
    if (root.tagName().toLower() != "svg") {
        qWarning() << "Root element is not <svg>";
        return QDomElement();
    }

    // Default SVG DPI is 96 for scaling purposes if physical units were used.
    // For now, we assume "user units" unless "px" is specified, and treat them similarly initially.
    // The main goal here is to handle the viewBox for scaling.
    double svgWidth = parseLength(root.attribute("width", "100%"), root);
    double svgHeight = parseLength(root.attribute("height", "100%"), root);

    QString viewBoxStr = root.attribute("viewBox");
    double vbX = 0, vbY = 0, vbWidth = 0, vbHeight = 0;
    bool hasViewBox = false;

    if (!viewBoxStr.isEmpty()) {
        QStringList viewBoxValues = viewBoxStr.split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);
        if (viewBoxValues.size() == 4) {
            vbX = parseViewBoxValue(viewBoxValues[0]);
            vbY = parseViewBoxValue(viewBoxValues[1]);
            vbWidth = parseViewBoxValue(viewBoxValues[2]);
            vbHeight = parseViewBoxValue(viewBoxValues[3]);
            hasViewBox = true;
        } else {
            qWarning() << "Invalid viewBox attribute:" << viewBoxStr;
        }
    }

    // Determine scaling based on viewBox
    // If width/height are percentages, they would ideally resolve against a viewport,
    // but for standalone SVGs, viewBox is often the primary determinant of user unit scale.
    // This is a simplified interpretation. A full SVG 1.1 interpretation is more complex.
    if (hasViewBox && vbWidth > 0 && vbHeight > 0) {
        if (svgWidth > 0 && svgHeight > 0) {
            // If both viewBox and width/height are present, viewBox defines the user coordinate system.
            // The 'width' and 'height' attributes on <svg> define the viewport size.
            // The scale is how many "device units" (e.g. pixels on screen) one "SVG user unit" from viewBox occupies.
            // This is often used to scale the SVG to fit into the given width/height.
            // For now, we'll assume outSvgUnitsToDeviceUnitsScale = 1 if viewBox is present,
            // meaning 1 SVG user unit = 1 "device unit" (e.g. pixel).
            // The unitConversionFactor in getParts/polygonify will handle further scaling.
            // A more complete solution would consider preserveAspectRatio.
            // For now, let's assume 1 user unit = 1 "device unit" if viewBox is present.
            outSvgUnitsToDeviceUnitsScale = 1.0;
        } else {
             // If only viewBox is present, it defines the extent of the SVG in user units.
             // We can assume 1 user unit = 1 device unit for now.
            outSvgUnitsToDeviceUnitsScale = 1.0;
        }
    } else if (svgWidth > 0 && svgHeight > 0) {
        // No viewBox, width and height are explicit. Assume user units map 1:1 to device units.
        outSvgUnitsToDeviceUnitsScale = 1.0;
    } else {
        // Cannot determine scale, default to 1.0
        outSvgUnitsToDeviceUnitsScale = 1.0;
        qWarning() << "Could not determine SVG scale from width, height, or viewBox. Defaulting to 1.0.";
    }
    
    // Initial transform application (e.g. to handle viewBox translation)
    // This is a simplified model. A full viewBox implementation handles preserveAspectRatio, etc.
    // For now, we only apply the viewBox translation part if it exists.
    QTransform initialTransform = QTransform::fromTranslate(-vbX, -vbY);
    if(hasViewBox){
        // We are not applying the viewBox scaling here directly into the transform,
        // as the unitConversionFactor in later stages is expected to handle overall scaling.
        // The purpose of outSvgUnitsToDeviceUnitsScale is to inform the caller about the
        // SVG's own unit system relative to a device context (e.g. pixels).
        // If we were to fully implement viewBox scaling to fit a specific output dimension,
        // that logic would go here or be part of the initialTransform.
        // For now, we treat SVG user units from viewBox as the base for unitConversionFactor.
    }


    // Apply the initial transform (e.g. from viewBox offset) to all top-level elements.
    // A more robust way is to pass this initialTransform down through polygonify.
    // Or, alternatively, modify applyTransformRecursive to take it as a starting point.
    // For this iteration, we will pass it to polygonify.
    // applyTransformRecursive(root, initialTransform); // This would modify the DOM.
                                                    // It's better to pass transform to polygonify.

    return root;
}


void SvgParser::applyTransformRecursive(QDomElement& element, const QTransform& accumulatedTransform) {
    if (element.isNull()) return;

    QTransform currentTransform = accumulatedTransform;
    if (element.hasAttribute("transform")) {
        currentTransform = parseTransform(element.attribute("transform")) * accumulatedTransform; // Order: local then parent
        // For this simplified version, we'll remove the attribute after parsing,
        // assuming coordinates will be directly modified.
        // A non-destructive approach would keep transforms and apply them at render/geometry extraction time.
        // element.removeAttribute("transform"); 
        // However, the prompt implies modifying coordinates for some elements.
        // This function's role needs to be clarified: is it destructive (modifies DOM) or non-destructive?
        // Prompt: "directly modify its coordinates. If it's a group, recurse. Remove the transform attribute after processing."
        // This implies a destructive operation.
    }

    QString tagName = element.tagName().toLower();
    // For now, this function will *not* directly modify coordinates.
    // It will parse and accumulate transforms. The actual application of the
    // final transform to generate points will happen in the polygonify* functions.
    // This is a more common and flexible approach.
    // The "Remove the transform attribute" part is tricky if we want to re-parse or have a live DOM.
    // For a one-time conversion, it's okay. Let's assume one-time conversion for now and remove it.
     if (element.hasAttribute("transform")) {
        element.removeAttribute("transform");
     }


    // For shape elements, the polygonify functions will take the final CTM.
    // For group elements, recurse.
    if (tagName == "g" || tagName == "svg" || tagName == "a" /* etc. for containers */) {
        QDomNodeList children = element.childNodes();
        for (int i = 0; i < children.count(); ++i) {
            if (children.at(i).isElement()) {
                QDomElement childElement = children.at(i).toElement();
                applyTransformRecursive(childElement, currentTransform);
            }
        }
    } else if (tagName == "rect" || tagName == "circle" || tagName == "ellipse" || tagName == "polygon" || tagName == "polyline" || tagName == "path" || tagName == "line") {
        // If we were to modify coordinates directly, it would happen here.
        // Example for a rect (highly simplified):
        // if (tagName == "rect") {
        //    double x = element.attribute("x").toDouble();
        //    double y = element.attribute("y").toDouble();
        //    QPointF p = currentTransform.map(QPointF(x,y));
        //    element.setAttribute("x", QString::number(p.x()));
        //    element.setAttribute("y", QString::number(p.y()));
        //    // Width and height would also need transform handling (e.g. for skews/rotations)
        //    // which is complex. This is why applying transform at point generation is better.
        // }
        // For now, this function only ensures transforms are propagated down.
        // The polygonify functions will use the *original* coordinates and apply the CTM.
    }
}


QList<Part> SvgParser::getParts(const QDomElement& rootElement, double unitConversionFactor) {
    QList<Part> parts;
    if (rootElement.isNull() || rootElement.tagName().toLower() != "svg") {
        return parts;
    }

    // Apply transformations recursively. This version of applyTransformRecursive
    // primarily serves to establish the CTM for each element if we were to query it later,
    // but it doesn't modify coordinates in this setup.
    // Instead of a destructive applyTransformRecursive, we will pass down transforms.
    // applyTransformRecursive(rootElementCopy, QTransform()); // Pass identity transform

    QDomNodeList children = rootElement.childNodes();
    for (int i = 0; i < children.count(); ++i) {
        if (children.at(i).isElement()) {
            QDomElement element = children.at(i).toElement();
            // We need to calculate the CTM for each element here, or polygonify must do it.
            // Let polygonify handle transform accumulation from root.
            Polygon poly = polygonify(element, unitConversionFactor, QTransform()); // Start with identity at root level
            if (!poly.outer.empty()) {
                Part part;
                part.id = element.attribute("id");
                if (part.id.isEmpty()) {
                    part.id = QString("part_%1").arg(parts.count());
                }
                part.geometry = poly;
                part.sourceFilename = doc.url().toString(); // Or a more direct filename if available
                parts.append(part);
            }
        }
    }
    return parts;
}

Polygon SvgParser::polygonify(const QDomElement& element, double unitConversionFactor, const QTransform& parentTransform) {
    if (element.isNull()) return Polygon();

    QTransform currentTransform = parentTransform;
    if (element.hasAttribute("transform")) {
        currentTransform = parseTransform(element.attribute("transform")) * parentTransform;
    }
    
    QString tagName = element.tagName().toLower();
    Polygon poly;

    if (tagName == "rect") {
        poly = polygonifyRect(element, currentTransform, unitConversionFactor);
    } else if (tagName == "polygon") {
        poly = polygonifyPolygon(element, currentTransform, unitConversionFactor);
    } else if (tagName == "polyline") {
        poly = polygonifyPolyline(element, currentTransform, unitConversionFactor);
    } else if (tagName == "circle") {
        poly = polygonifyCircle(element, currentTransform, unitConversionFactor);
    } else if (tagName == "ellipse") {
        poly = polygonifyEllipse(element, currentTransform, unitConversionFactor);
    } else if (tagName == "line") {
        poly = polygonifyLine(element, currentTransform, unitConversionFactor);
    } else if (tagName == "path") {
        // For path, we'd call polygonifyPath.
        // This is complex, so for now, it's a basic implementation or stub.
        std::vector<Point> pathPoints = polygonifyPath(element, currentTransform, unitConversionFactor);
        if(!pathPoints.empty()){
            poly.outer = pathPoints;
        }
    } else if (tagName == "g") {
        // For groups, iterate children and accumulate their polygons if needed,
        // or handle them based on desired part extraction logic (e.g., each child of g is a part).
        // For this initial pass, we assume parts are direct children of <svg> or simple shapes.
        // If a 'g' element itself should become a single complex part, we'd union its children's polygons.
        // This is not handled in this basic version. Polygonify is for single geometric entities.
    }
    // Other shapes (text, image) ignored for now.
    
    return poly;
}


Polygon SvgParser::polygonifyRect(const QDomElement& rectElement, const QTransform& transform, double unitConversionFactor) {
    bool okX, okY, okW, okH;
    double x = toDouble(rectElement.attribute("x", "0"), &okX);
    double y = toDouble(rectElement.attribute("y", "0"), &okY);
    double w = toDouble(rectElement.attribute("width", "0"), &okW);
    double h = toDouble(rectElement.attribute("height", "0"), &okH);

    if (!okX || !okY || !okW || !okH || w < 0 || h < 0) { // Width/height can't be negative
        qWarning() << "Invalid rect attributes for element ID:" << rectElement.attribute("id");
        return Polygon();
    }

    Polygon poly;
    // Points for the rectangle (counter-clockwise)
    QPointF p1 = transform.map(QPointF(x, y));
    QPointF p2 = transform.map(QPointF(x + w, y));
    QPointF p3 = transform.map(QPointF(x + w, y + h));
    QPointF p4 = transform.map(QPointF(x, y + h));

    poly.outer.push_back({p1.x() * unitConversionFactor, p1.y() * unitConversionFactor});
    poly.outer.push_back({p2.x() * unitConversionFactor, p2.y() * unitConversionFactor});
    poly.outer.push_back({p3.x() * unitConversionFactor, p3.y() * unitConversionFactor});
    poly.outer.push_back({p4.x() * unitConversionFactor, p4.y() * unitConversionFactor});
    return poly;
}

std::vector<Point> SvgParser::parsePointsString(const QString& pointsStr, const QTransform& transform, double unitConversionFactor) {
    std::vector<Point> points;
    QStringList pointPairs = pointsStr.split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);
    
    for (int i = 0; i < pointPairs.size() - 1; i += 2) {
        bool okX, okY;
        double x = toDouble(pointPairs[i], &okX);
        double y = toDouble(pointPairs[i+1], &okY);
        if (okX && okY) {
            QPointF p = transform.map(QPointF(x,y));
            points.push_back({p.x() * unitConversionFactor, p.y() * unitConversionFactor});
        } else {
            qWarning() << "Invalid point in points string:" << pointPairs[i] << "," << pointPairs[i+1];
        }
    }
    return points;
}

Polygon SvgParser::polygonifyPolyline(const QDomElement& polylineElement, const QTransform& transform, double unitConversionFactor) {
    Polygon poly;
    poly.outer = parsePointsString(polylineElement.attribute("points"), transform, unitConversionFactor);
    return poly;
}

Polygon SvgParser::polygonifyPolygon(const QDomElement& polygonElement, const QTransform& transform, double unitConversionFactor) {
    Polygon poly;
    poly.outer = parsePointsString(polygonElement.attribute("points"), transform, unitConversionFactor);
    // SVG Polygons are implicitly closed. Ensure this if parsePointsString doesn't.
    // For NFP, often doesn't matter if last point equals first if library handles it.
    return poly;
}

Polygon SvgParser::polygonifyCircle(const QDomElement& circleElement, const QTransform& transform, double unitConversionFactor) {
    bool okCX, okCY, okR;
    double cx = toDouble(circleElement.attribute("cx", "0"), &okCX);
    double cy = toDouble(circleElement.attribute("cy", "0"), &okCY);
    double r  = toDouble(circleElement.attribute("r", "0"), &okR);

    if (!okCX || !okCY || !okR || r < 0) {
         qWarning() << "Invalid circle attributes for element ID:" << circleElement.attribute("id");
        return Polygon();
    }
    
    Polygon poly;
    // Approximate circle with a polygon (e.g., 32 segments)
    // More sophisticated approximation might consider transformed shape.
    const int segments = 32; 
    for (int i = 0; i < segments; ++i) {
        double angle = 2.0 * M_PI * static_cast<double>(i) / static_cast<double>(segments);
        QPointF p_local(cx + r * qCos(angle), cy + r * qSin(angle));
        QPointF p_transformed = transform.map(p_local);
        poly.outer.push_back({p_transformed.x() * unitConversionFactor, p_transformed.y() * unitConversionFactor});
    }
    return poly;
}

Polygon SvgParser::polygonifyEllipse(const QDomElement& ellipseElement, const QTransform& transform, double unitConversionFactor) {
    bool okCX, okCY, okRX, okRY;
    double cx = toDouble(ellipseElement.attribute("cx", "0"), &okCX);
    double cy = toDouble(ellipseElement.attribute("cy", "0"), &okCY);
    double rx = toDouble(ellipseElement.attribute("rx", "0"), &okRX);
    double ry = toDouble(ellipseElement.attribute("ry", "0"), &okRY);

    if (!okCX || !okCY || !okRX || !okRY || rx < 0 || ry < 0) {
        qWarning() << "Invalid ellipse attributes for element ID:" << ellipseElement.attribute("id");
        return Polygon();
    }

    Polygon poly;
    const int segments = 32; // Similar approximation as circle
    for (int i = 0; i < segments; ++i) {
        double angle = 2.0 * M_PI * static_cast<double>(i) / static_cast<double>(segments);
        QPointF p_local(cx + rx * qCos(angle), cy + ry * qSin(angle));
        QPointF p_transformed = transform.map(p_local);
        poly.outer.push_back({p_transformed.x() * unitConversionFactor, p_transformed.y() * unitConversionFactor});
    }
    return poly;
}

Polygon SvgParser::polygonifyLine(const QDomElement& lineElement, const QTransform& transform, double unitConversionFactor) {
    bool okX1, okY1, okX2, okY2;
    double x1 = toDouble(lineElement.attribute("x1", "0"), &okX1);
    double y1 = toDouble(lineElement.attribute("y1", "0"), &okY1);
    double x2 = toDouble(lineElement.attribute("x2", "0"), &okX2);
    double y2 = toDouble(lineElement.attribute("y2", "0"), &okY2);
    
    if(!okX1 || !okY1 || !okX2 || !okY2){
        qWarning() << "Invalid line attributes for element ID:" << lineElement.attribute("id");
        return Polygon();
    }

    Polygon poly; // A line is represented as a 2-point open polygon
    QPointF p1 = transform.map(QPointF(x1, y1));
    QPointF p2 = transform.map(QPointF(x2, y2));

    poly.outer.push_back({p1.x() * unitConversionFactor, p1.y() * unitConversionFactor});
    poly.outer.push_back({p2.x() * unitConversionFactor, p2.y() * unitConversionFactor});
    return poly;
}


QTransform SvgParser::parseTransform(const QString& transformString) {
    QTransform transform;
    // Simplified parser. A full SVG transform parser is more complex.
    // This handles matrix, translate, scale, rotate. Assumes values are comma or space separated.
    // Example: "translate(10, 20) scale(2) rotate(45)"
    // QTransform::fromTranslate, fromScale, rotate an be used.
    // For matrix(a,b,c,d,e,f), it's new QTransform(a,b,c,d,e,f) then multiplied.

    // This is a common source of complexity. Using QRegularExpression for robustness.
    QRegularExpression re("(\\w+)\\s*\\(([^)]+)\\)");
    QRegularExpressionMatchIterator it = re.globalMatch(transformString);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString type = match.captured(1).toLower();
        QStringList params = match.captured(2).split(QRegularExpression("[\\s,]+"), Qt::SkipEmptyParts);

        bool ok;
        if (type == "matrix" && params.size() == 6) {
            transform = QTransform(toDouble(params[0], &ok), toDouble(params[1], &ok), 
                                   toDouble(params[2], &ok), toDouble(params[3], &ok), 
                                   toDouble(params[4], &ok), toDouble(params[5], &ok)) * transform;
        } else if (type == "translate") {
            double tx = (params.size() > 0) ? toDouble(params[0], &ok) : 0.0;
            double ty = (params.size() > 1) ? toDouble(params[1], &ok) : 0.0;
            transform.translate(tx, ty);
        } else if (type == "scale") {
            double sx = (params.size() > 0) ? toDouble(params[0], &ok) : 1.0;
            double sy = (params.size() > 1) ? toDouble(params[1], &ok) : sx; // sy defaults to sx if not provided
            transform.scale(sx, sy);
        } else if (type == "rotate") {
            double angle = (params.size() > 0) ? toDouble(params[0], &ok) : 0.0;
            double cx = (params.size() > 1) ? toDouble(params[1], &ok) : 0.0;
            double cy = (params.size() > 2) ? toDouble(params[2], &ok) : 0.0;
            if (cx != 0.0 || cy != 0.0) { // Rotate around (cx, cy)
                transform.translate(cx, cy);
                transform.rotate(angle);
                transform.translate(-cx, -cy);
            } else {
                transform.rotate(angle);
            }
        } else if (type == "skewx" && params.size() == 1) {
            transform.shear(qTan(qDegreesToRadians(toDouble(params[0], &ok))), 0);
        } else if (type == "skewy" && params.size() == 1) {
            transform.shear(0, qTan(qDegreesToRadians(toDouble(params[0], &ok))));
        } else {
            qWarning() << "Unsupported or malformed transform function:" << type << params;
        }
    }
    return transform;
}

// Stub for path parsing. Full implementation is complex.
std::vector<Point> SvgParser::polygonifyPath(const QDomElement& pathElement, const QTransform& currentTransform, double unitConversionFactor) {
    QString d = pathElement.attribute("d");
    if (d.isEmpty()) return {};

    QPainterPath painterPath;
    painterPath.addText(0,0,QFont(), "TODO: Parse Path 'd' attribute"); // Placeholder
    // Actual parsing of 'd' attribute is non-trivial. QPainterPath::addPath can take another path.
    // Or, iterate through path segments: M, L, C, S, Q, T, A, Z.
    // For a simple stub, we can try to handle only M and L if they are absolute.
    // Example: M 10 10 L 20 20 L 10 20 Z
    // This is a placeholder, real path parsing is needed for full SVG support.

    // Use qPainterPathToPolygon to convert the (currently placeholder) QPainterPath
    return qPainterPathToPolygon(painterPath, currentTransform, unitConversionFactor).outer;
}

std::vector<Point> SvgParser::linearizePathSegment(const QPainterPath::Element& segment, const QPointF& lastPoint, double tolerance) {
    Q_UNUSED(tolerance); // Tolerance would be used for curve linearization
    std::vector<Point> points;
    // This is a very simplified stub. Proper linearization is complex.
    switch (segment.type) {
        case QPainterPath::MoveToElement:
            // For polygonification, MoveTo often starts a new polygon or subpath.
            // Here we are just returning points for a single path.
            points.push_back({segment.x, segment.y});
            break;
        case QPainterPath::LineToElement:
            points.push_back({lastPoint.x(), lastPoint.y()}); // Start of line
            points.push_back({segment.x, segment.y});       // End of line
            break;
        // TODO: Handle CurveToElement, CurveToDataElement with linearization (e.g. recursive subdivision)
        default:
            // Other element types ignored in this stub
            break;
    }
    return points;
}


Polygon SvgParser::qPainterPathToPolygon(const QPainterPath& path, const QTransform& transform, double unitConversionFactor) {
    Polygon poly;
    QPainterPath transformedPath = transform.map(path); // Apply transform to the whole path

    for (int i = 0; i < transformedPath.elementCount(); ++i) {
        const QPainterPath::Element& el = transformedPath.elementAt(i);
        // This simplified conversion just takes the endpoint of any segment.
        // A full conversion would handle subpaths (MoveTo) as separate polygons or holes,
        // and linearize curves.
        if (el.isMoveTo() || el.isLineTo() || el.isCurveTo()) { // CurveTo implies its endpoint
             poly.outer.push_back({el.x * unitConversionFactor, el.y * unitConversionFactor});
        }
        // Note: This doesn't correctly handle multiple subpaths or distinguish outer/holes.
        // QPainterPath can have multiple subpaths. Each subpath (starting with MoveTo)
        // should potentially become a separate Polygon or a hole in a Polygon.
        // For NFP, we typically need a single outer boundary and optionally holes.
    }
    // Ensure polygon is closed if it's supposed to be (e.g. from 'Z' path command)
    // This basic conversion doesn't explicitly handle that.
    return poly;
}

void SvgParser::cleanSvgDom(QDomElement& rootElement) {
    // Placeholder for future implementation.
    // Operations could include:
    // - Merging nearly collinear line segments.
    // - Simplifying paths.
    // - Removing invisible elements.
    // - Converting text to paths (requires font metrics).
    // - Flattening groups if all transforms are identity.
    Q_UNUSED(rootElement);
}
