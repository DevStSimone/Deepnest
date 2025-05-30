#ifndef MINKOWSKI_WRAPPER_H
#define MINKOWSKI_WRAPPER_H

#include <vector>
#include <list> // Using std::list as a generic container

// Forward declare Boost.Polygon types to minimize direct Boost includes in this header if possible,
// though for implementation, the .cpp will need full Boost.Polygon headers.
// However, for the interface, we can use our own simple structs.
// namespace boost { namespace polygon {
//     template <typename T> class point_data;
//     template <typename T> class polygon_with_holes_data;
//     template <typename T> class polygon_set_data;
// }}

namespace CustomMinkowski {

// Simple point structure for the interface.
struct Point { 
    double x; 
    double y; 
    // Add other members if needed, e.g., an ID or Z-value if relevant from original.
};
typedef std::vector<Point> PolygonPath; // A single polygon contour (outer or hole)

struct PolygonWithHoles {
    PolygonPath outer;
    std::list<PolygonPath> holes;
    // Add any other properties that might be needed, e.g., an ID for the part.
    // For now, keeping it purely geometric.
};

// Result of NFP calculation - typically a list of polygons (often just one).
// These polygons are the boundaries of the No-Fit Polygon.
typedef std::list<PolygonPath> NfpResultPolygons;

// Calculates NFP of A (orbiting part) around B (static part).
// The NFP represents the boundary of all locations B's reference point can take
// such that A and B do not overlap.
// Or, more commonly for nesting: NFP represents the boundary of A's reference point
// as it slides around B.
// The implementation will need to handle the coordinate transformations and scaling
// identified in the analysis of the original minkowski.cc.
//
// Parameters:
// - partA_orbiting: The polygon that is considered to be moving or "orbiting".
// - partB_static: The polygon that is considered static.
// - nfp_result: Output parameter to store the resulting NFP polygons.
// - input_scale_override: Optional. If > 0, this scale is used instead of dynamic calculation.
//                         This might be useful for consistency with other geometry operations.
//                         If <= 0, the function might use its internal dynamic scaling logic.
//                         (This is a design choice for the wrapper).
//
// Returns true on success, false on failure or if NFP is empty/invalid.
bool CalculateNfp(
    const PolygonWithHoles& partA_orbiting, 
    const PolygonWithHoles& partB_static,
    NfpResultPolygons& nfp_result,
    double fixed_scale_for_boost_poly // The scale factor to convert doubles to integers for Boost.Polygon
    // Consider adding a parameter for `use_holes` if the original logic supports it differently
    // or if sometimes we want NFP of just outer boundaries. For now, assume holes are always processed.
);

} // namespace CustomMinkowski
#endif // MINKOWSKI_WRAPPER_H
