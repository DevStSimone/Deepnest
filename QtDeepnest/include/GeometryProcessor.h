#ifndef GEOMETRY_PROCESSOR_H
#define GEOMETRY_PROCESSOR_H

#include "DataStructures.h" // Includes Point and Polygon
#include "clipper2/clipper.h"
#include <vector>

class GeometryProcessor {
public:
    // Clipper2 typically uses integer coordinates. Define a scaling factor.
    static const double CLIPPER_SCALE; // = 10000000.0; (Initialize in .cpp)

    // Converts our Polygon struct to Clipper2Lib::Paths64
    static Clipper2Lib::Paths64 PolygonToPaths64(const Polygon& poly);
    static Clipper2Lib::Path64 PointsToPath64(const std::vector<Point>& points);

    // Converts Clipper2Lib::Paths64 back to a list of our Polygon structs
    static std::vector<Polygon> Paths64ToPolygons(const Clipper2Lib::Paths64& paths);
    static Polygon Path64ToPolygon(const Clipper2Lib::Path64& path); // For single path to polygon with no holes initially
                                                                    // (holes might be handled by structure of Paths64)

    // Cleans a polygon (removes self-intersections, etc.)
    // fillRule corresponds to ClipperLib.PolyFillType (e.g., pftNonZero)
    static Polygon cleanPolygon(const Polygon& poly, Clipper2Lib::FillRule fillRule = Clipper2Lib::FillRule::NonZero);
    static std::vector<Polygon> cleanPolygons(const std::vector<Polygon>& polygons, Clipper2Lib::FillRule fillRule = Clipper2Lib::FillRule::NonZero);


    // Offsets a polygon or multiple polygons.
    // delta: positive for expansion, negative for contraction.
    // joinType: e.g., Clipper2Lib::JoinType::Miter, Round, Square.
    // endType: e.g., Clipper2Lib::EndType::Polygon, Joined, Square, Round, Butt.
    static std::vector<Polygon> offsetPolygons(const std::vector<Polygon>& polygons, double delta, 
                                               Clipper2Lib::JoinType jt = Clipper2Lib::JoinType::Square, 
                                               Clipper2Lib::EndType et = Clipper2Lib::EndType::Polygon);
    
    // Simplifies a polygon using Ramer-Douglas-Peucker.
    // epsilon: distance threshold for simplification.
    static Polygon simplifyPolygonRDP(const Polygon& poly, double epsilon);
    
    // A more complex simplification that tries to mimic deepnest.js's simplifyPolygon.
    // This might involve cleaning, offsetting, and then potentially RDP or other steps.
    // For now, this can be a simpler version or a placeholder for more detailed implementation.
    // curveTolerance is the main parameter from deepnest.js config.
    static Polygon simplifyPolygonDeepnest(const Polygon& poly, double curveTolerance, bool isHole);


    // Checks if a point is inside a polygon.
    // Returns: 0 for outside, 1 for inside, -1 for on boundary (if Clipper2 provides this distinction).
    // Clipper2's PointInPath returns PointInPolygonResult enum (IsInside, IsOutside, IsOn).
    static Clipper2Lib::PointInPolygonResult pointInPolygon(const Point& pt, const Polygon& poly);

    // Rotates a polygon around (0,0)
    static Polygon rotatePolygon(const Polygon& poly, double degrees);

    // Gets the minimum point of the bounding box of a polygon
    static Point getPolygonBoundsMin(const Polygon& poly);

    // Performs Minkowski sum of A + B using Clipper2
    static Clipper2Lib::Paths64 minkowskiSum(const Polygon& polyA, const Polygon& polyB, bool isPathClosed = true);
};

#endif // GEOMETRY_PROCESSOR_H
