#include "GeometryProcessor.h"
#include <algorithm> // For std::remove_if if needed for filtering empty paths, std::min/max (though not directly used in prompt funcs)
#include <cmath>     // For std::cos, std::sin, M_PI
#include <limits>    // For std::numeric_limits

#ifndef M_PI // Define M_PI if not defined (e.g. on Windows with MSVC without _USE_MATH_DEFINES)
    #define M_PI 3.14159265358979323846
#endif

// Include for BoostMinkowski structures
#include "boost_minkowski.h" 
// DataStructures.h is included via GeometryProcessor.h
// clipper2/clipper.h is included via GeometryProcessor.h

const double GeometryProcessor::CLIPPER_SCALE = 10000000.0;

// --- Static Conversion Utilities for BoostMinkowski Interface ---

BoostMinkowski::PolygonDouble ToPolygonDouble(const Polygon& ourPoly) {
    BoostMinkowski::PolygonDouble pd;
    pd.outer.reserve(ourPoly.outer.size());
    for (const auto& pt : ourPoly.outer) {
        pd.outer.push_back({pt.x, pt.y});
    }

    pd.holes.reserve(ourPoly.holes.size());
    for (const auto& hole_vec : ourPoly.holes) {
        std::vector<BoostMinkowski::PointDouble> current_hole_pd;
        current_hole_pd.reserve(hole_vec.size());
        for (const auto& pt : hole_vec) {
            current_hole_pd.push_back({pt.x, pt.y});
        }
        pd.holes.push_back(current_hole_pd);
    }
    return pd;
}

Clipper2Lib::Paths64 BoostPolygonsToPaths64(
    const std::vector<boost::polygon::polygon_with_holes_data<int>>& boostPolys,
    double boost_calc_scale,
    const BoostMinkowski::PointDouble& b_ref_shift_coords) {
    
    Clipper2Lib::Paths64 resultPathsScaled;

    if (boost_calc_scale == 0) { // Avoid division by zero
        // Optionally, log an error or warning
        return resultPathsScaled; // Return empty
    }

    for (const auto& boost_poly : boostPolys) {
        // Process Outer Boundary
        Clipper2Lib::Path64 outer_path_clipper;
        outer_path_clipper.reserve(boost_poly.size()); // boost_poly.size() gives vertex count of outer
        
        for (auto it = boost_poly.begin(); it != boost_poly.end(); ++it) {
            const auto& boost_pt = *it;
            double x_unscaled = static_cast<double>(boost_pt.x()) / boost_calc_scale;
            double y_unscaled = static_cast<double>(boost_pt.y()) / boost_calc_scale;
            double x_shifted = x_unscaled + b_ref_shift_coords.x;
            double y_shifted = y_unscaled + b_ref_shift_coords.y;
            long long x_clipper = static_cast<long long>(x_shifted * GeometryProcessor::CLIPPER_SCALE);
            long long y_clipper = static_cast<long long>(y_shifted * GeometryProcessor::CLIPPER_SCALE);
            outer_path_clipper.push_back(Clipper2Lib::Point64(x_clipper, y_clipper));
        }
        if (!outer_path_clipper.empty()) {
            resultPathsScaled.push_back(outer_path_clipper);
        }

        // Process Holes
        for (auto hole_it = boost_poly.begin_holes(); hole_it != boost_poly.end_holes(); ++hole_it) {
            const auto& boost_hole_contour = *hole_it;
            Clipper2Lib::Path64 hole_path_clipper;
            hole_path_clipper.reserve(boost_hole_contour.size());

            for (auto pt_it = boost_hole_contour.begin(); pt_it != boost_hole_contour.end(); ++pt_it) {
                const auto& boost_hole_pt = *pt_it;
                double x_unscaled = static_cast<double>(boost_hole_pt.x()) / boost_calc_scale;
                double y_unscaled = static_cast<double>(boost_hole_pt.y()) / boost_calc_scale;
                double x_shifted = x_unscaled + b_ref_shift_coords.x;
                double y_shifted = y_unscaled + b_ref_shift_coords.y;
                long long x_clipper = static_cast<long long>(x_shifted * GeometryProcessor::CLIPPER_SCALE);
                long long y_clipper = static_cast<long long>(y_shifted * GeometryProcessor::CLIPPER_SCALE);
                hole_path_clipper.push_back(Clipper2Lib::Point64(x_clipper, y_clipper));
            }
            if (!hole_path_clipper.empty()) {
                resultPathsScaled.push_back(hole_path_clipper);
            }
        }
    }
    return resultPathsScaled;
}


// --- Coordinate Conversion Helpers ---
Clipper2Lib::Path64 GeometryProcessor::PointsToPath64(const std::vector<Point>& points) {
    Clipper2Lib::Path64 path;
    path.reserve(points.size());
    for (const auto& pt : points) {
        path.push_back(Clipper2Lib::Point64(
            static_cast<long long>(pt.x * CLIPPER_SCALE),
            static_cast<long long>(pt.y * CLIPPER_SCALE)
        ));
    }
    return path;
}

Clipper2Lib::Paths64 GeometryProcessor::PolygonToPaths64(const Polygon& poly) {
    Clipper2Lib::Paths64 paths;
    if (!poly.outer.empty()) {
        paths.push_back(PointsToPath64(poly.outer));
    }
    for (const auto& hole : poly.holes) {
        if (!hole.empty()) {
            // For Clipper2, hole paths should typically have an orientation opposite to the outer path.
            // E.g., if outer is CCW, holes should be CW for positive fill rules.
            // The PointsToPath64 function doesn't enforce orientation; it preserves vertex order.
            // Clipper2's Union and InflatePaths are generally robust to path orientations if fill rules are used correctly.
            // If specific orientation is needed before a Clipper2 operation, it must be ensured here or before calling.
            Clipper2Lib::Path64 hole_path = PointsToPath64(hole);
            // Example: if (Clipper2Lib::IsClockwise(hole_path) != (fillRule == Clipper2Lib::FillRule::Positive)) // pseudo-code for orientation check
            //    Clipper2Lib::ReversePath(hole_path);
            paths.push_back(hole_path);
        }
    }
    return paths; 
}


Polygon GeometryProcessor::Path64ToPolygon(const Clipper2Lib::Path64& path) {
    Polygon poly;
    poly.outer.reserve(path.size());
    for (const auto& pt64 : path) {
        poly.outer.push_back({
            static_cast<double>(pt64.x) / CLIPPER_SCALE,
            static_cast<double>(pt64.y) / CLIPPER_SCALE
        });
    }
    return poly;
}

std::vector<Polygon> GeometryProcessor::Paths64ToPolygons(const Clipper2Lib::Paths64& paths) {
    std::vector<Polygon> polygons;
    // This basic conversion creates one polygon per path.
    // To reconstruct Polygon structs with actual outer/hole relationships from a general Paths64,
    // one would typically use a PolyTree or PolyTreeD structure with Clipper2.
    // For example, after a Union operation:
    // Clipper2Lib::PolyTree64 polyTree;
    // Clipper2Lib::Clipper64 clipper;
    // clipper.AddSubject(inputPaths);
    // clipper.Execute(ClipType::Union, FillRule::NonZero, polyTree);
    // Then, iterate through polyTree to build Polygon structs with proper hole assignments.
    // For now, this simplified version treats each path as a separate polygon's outer boundary.
    for (const auto& path : paths) {
        if (!path.empty()) {
            polygons.push_back(Path64ToPolygon(path));
        }
    }
    return polygons;
}


// --- Processing Functions ---

Polygon GeometryProcessor::cleanPolygon(const Polygon& poly, Clipper2Lib::FillRule fillRule) {
    if (poly.outer.empty()) return {};

    Clipper2Lib::Paths64 subj = PolygonToPaths64(poly); // This includes outer and holes as separate paths
    
    Clipper2Lib::PolyTree64 solution_tree; // Use PolyTree to reconstruct holes correctly
    Clipper2Lib::Clipper64 c;
    c.AddSubject(subj);
    // Union with itself effectively cleans individual polygon parts (outer, holes)
    // and resolves self-intersections within each part.
    // To reconstruct the main polygon and its holes correctly after cleaning,
    // it's better to process the outer path and holes separately if they are not already simple.
    // Or, use a PolyTree to get the structure back.
    
    // For cleaning a single polygon (potentially with self-intersections in its definition),
    // a robust way is to union its parts.
    c.Execute(Clipper2Lib::ClipType::Union, fillRule, solution_tree); 
    
    // Convert PolyTree to Polygons (this part needs a helper function for proper hole reconstruction)
    // For simplicity, let's try to get the first polygon from the tree.
    // A full PolyTreeToPolygons function would be more robust.
    Clipper2Lib::Paths64 solution_paths;
    solution_paths = Clipper2Lib::PolyTreeToPaths64(solution_tree); // This flattens the tree

    if (solution_paths.empty()) return {};

    // The following logic to find the "largest" path might not be correct if the
    // cleaning operation results in a valid polygon that is smaller but correct,
    // or if it splits the polygon. A true PolyTree traversal is better.
    // For this subtask, we'll assume the first path from the flattened tree is the main one,
    // and subsequent paths might be holes (if PolyTreeToPaths64 orders them that way, it usually doesn't directly).
    
    // A simplified approach for cleanPolygon:
    // 1. Take the subject paths.
    // 2. Union them. This will give a set of paths representing the cleaned regions.
    // 3. Convert these paths back to Polygon objects. If the original polygon was complex,
    //    this might result in multiple disjoint polygons or a polygon with new/modified holes.
    //    The current Paths64ToPolygons will make each path an outer boundary.
    //    This is often the desired result of "cleaning" a single complex shape.

    std::vector<Polygon> cleaned_polys = Paths64ToPolygons(solution_paths);
    if (cleaned_polys.empty()) return {};
    
    // If the goal is to return a single Polygon struct, and the clean operation
    // might have resolved the input into multiple disjoint polygons, we need to decide
    // which one to return (e.g., the largest by area).
    // Or, if the input was one polygon and its holes, we need to reconstruct that.
    // The PolyTree approach is best for reconstruction.
    // For now, if cleaned_polys has multiple, we return the one with largest area.
    if (cleaned_polys.size() > 1) {
        size_t largest_idx = 0;
        double max_area = 0.0;
        for (size_t i = 0; i < cleaned_polys.size(); ++i) {
            // Need a robust area calculation for our Polygon struct, or convert back to Path64 to use Clipper2Lib::Area
            Clipper2Lib::Path64 temp_path = PointsToPath64(cleaned_polys[i].outer);
            double area = std::abs(Clipper2Lib::Area(temp_path)); // Use absolute area
            if (area > max_area) {
                max_area = area;
                largest_idx = i;
            }
        }
        return cleaned_polys[largest_idx]; // Return the largest one. Holes are lost in this simplified return.
    }
    return cleaned_polys[0]; // Holes are lost if not reconstructed from PolyTree properly.
}

std::vector<Polygon> GeometryProcessor::cleanPolygons(const std::vector<Polygon>& polygons, Clipper2Lib::FillRule fillRule) {
    Clipper2Lib::Paths64 subj;
    for(const auto& poly : polygons) {
        Clipper2Lib::Paths64 p = PolygonToPaths64(poly);
        subj.insert(subj.end(), p.begin(), p.end());
    }
    
    if (subj.empty()) return {};

    // Using PolyTree to better handle complex results with holes
    Clipper2Lib::PolyTree64 solution_tree;
    Clipper2Lib::Clipper64 c;
    c.AddSubject(subj);
    c.Execute(Clipper2Lib::ClipType::Union, fillRule, solution_tree);

    // A proper PolyTree to std::vector<Polygon> conversion is needed here.
    // This is a placeholder for that more complex conversion.
    // For now, we'll use the simpler Paths64ToPolygons which flattens the structure.
    Clipper2Lib::Paths64 solution_paths;
    solution_paths = Clipper2Lib::PolyTreeToPaths64(solution_tree); // Flattens tree to paths
    
    return Paths64ToPolygons(solution_paths); // Each path becomes a separate polygon, holes not explicitly reconstructed.
}


std::vector<Polygon> GeometryProcessor::offsetPolygons(const std::vector<Polygon>& polygons, double delta, 
                                                       Clipper2Lib::JoinType jt, Clipper2Lib::EndType et) {
    Clipper2Lib::Paths64 subj_paths;
    for (const auto& poly : polygons) {
        Clipper2Lib::Paths64 p = PolygonToPaths64(poly);
        subj_paths.insert(subj_paths.end(), p.begin(), p.end());
    }

    if (subj_paths.empty()) return {};

    Clipper2Lib::Paths64 solution = Clipper2Lib::InflatePaths(subj_paths, delta * CLIPPER_SCALE, jt, et, 2.0); // Default miterLimit
    return Paths64ToPolygons(solution);
}

Polygon GeometryProcessor::simplifyPolygonRDP(const Polygon& poly, double epsilon) {
    if (poly.outer.empty()) return {};
    Clipper2Lib::Path64 path = PointsToPath64(poly.outer);
    // Epsilon for RDP in Clipper2 is a squared distance, but the interface RamerDouglasPeucker takes epsilon directly.
    // Let's assume epsilon * CLIPPER_SCALE is the correct scaling for the distance parameter.
    Clipper2Lib::Path64 simplified_path = Clipper2Lib::RamerDouglasPeucker(path, epsilon * CLIPPER_SCALE);
    
    Polygon result = Path64ToPolygon(simplified_path);
    
    for(const auto& hole_pts : poly.holes) {
        if (!hole_pts.empty()) {
            Clipper2Lib::Path64 hole_path = PointsToPath64(hole_pts);
            Clipper2Lib::Path64 simplified_hole = Clipper2Lib::RamerDouglasPeucker(hole_path, epsilon * CLIPPER_SCALE);
            if (!simplified_hole.empty()) {
                 std::vector<Point> hole_as_points = Path64ToPolygon(simplified_hole).outer; // Path64ToPolygon returns Polygon, take its outer
                 result.holes.push_back(hole_as_points);
            }
        }
    }
    return result;
}

Polygon GeometryProcessor::simplifyPolygonDeepnest(const Polygon& poly, double curveTolerance, bool isHole) {
    if (poly.outer.empty()) return {};

    // Step 1: RDP simplification (if curveTolerance is roughly equivalent to epsilon)
    Polygon simplified_rdp = simplifyPolygonRDP(poly, curveTolerance);
    if (simplified_rdp.outer.empty()) return {}; // RDP might eliminate the polygon if it's too small or simple

    // Step 2: Offset out and then back in (or vice-versa for holes) to smooth sharp angles from RDP
    double offset_delta = curveTolerance * CLIPPER_SCALE * 0.5; // Offset delta in Clipper scale
    
    Clipper2Lib::Paths64 paths_to_offset = PolygonToPaths64(simplified_rdp);
    if(paths_to_offset.empty()) return simplified_rdp; // Should not happen if simplified_rdp.outer wasn't empty

    Clipper2Lib::Paths64 offset_pass1_paths;
    if (isHole) { // Contract then expand
        offset_pass1_paths = Clipper2Lib::InflatePaths(paths_to_offset, -offset_delta, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon, 2.0);
    } else { // Expand then contract
        offset_pass1_paths = Clipper2Lib::InflatePaths(paths_to_offset, offset_delta, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon, 2.0);
    }

    if(offset_pass1_paths.empty()) return simplified_rdp; 

    Clipper2Lib::Paths64 offset_pass2_paths;
    if (isHole) { // Expand back
        offset_pass2_paths = Clipper2Lib::InflatePaths(offset_pass1_paths, offset_delta, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon, 2.0);
    } else { // Contract back
        offset_pass2_paths = Clipper2Lib::InflatePaths(offset_pass1_paths, -offset_delta, Clipper2Lib::JoinType::Round, Clipper2Lib::EndType::Polygon, 2.0);
    }
    
    if(offset_pass2_paths.empty()) return simplified_rdp;

    // Convert back to Polygon(s)
    std::vector<Polygon> final_polygons = Paths64ToPolygons(offset_pass2_paths);
    if (final_polygons.empty()) return simplified_rdp;

    // For simplifyPolygonDeepnest, we expect a single Polygon result.
    // If offsetting resulted in multiple disjoint shapes, we might take the largest.
    // Or, ideally, the PolyTree output from InflatePaths should be used to reconstruct holes properly.
    // For now, clean the first result (which might be the largest or only one).
    return cleanPolygon(final_polygons[0]); 
}

Point GeometryProcessor::getPolygonBoundsMin(const Polygon& poly) {
    if (poly.outer.empty()) {
        // std::cerr << "Warning: getPolygonBoundsMin called with an empty polygon." << std::endl; // Or qWarning if Qt is setup
        return {0.0, 0.0};
    }

    double min_x = std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();

    for (const auto& p : poly.outer) {
        if (p.x < min_x) {
            min_x = p.x;
        }
        if (p.y < min_y) {
            min_y = p.y;
        }
    }
    return {min_x, min_y};
}

Polygon GeometryProcessor::rotatePolygon(const Polygon& poly, double degrees) {
    Polygon rotatedPoly;
    double radians = degrees * M_PI / 180.0;
    double cos_rad = std::cos(radians);
    double sin_rad = std::sin(radians);

    // Rotate outer boundary
    rotatedPoly.outer.reserve(poly.outer.size());
    for (const auto& p : poly.outer) {
        rotatedPoly.outer.push_back({
            p.x * cos_rad - p.y * sin_rad,
            p.x * sin_rad + p.y * cos_rad
        });
    }

    // Rotate holes
    rotatedPoly.holes.reserve(poly.holes.size());
    for (const auto& hole_pts : poly.holes) {
        std::vector<Point> rotated_hole;
        rotated_hole.reserve(hole_pts.size());
        for (const auto& p : hole_pts) {
            rotated_hole.push_back({
                p.x * cos_rad - p.y * sin_rad,
                p.x * sin_rad + p.y * cos_rad
            });
        }
        rotatedPoly.holes.push_back(rotated_hole);
    }

    return rotatedPoly;
}

Clipper2Lib::Paths64 GeometryProcessor::minkowskiSum(const Polygon& polyA, const Polygon& polyB, bool isPathClosed) {
    // isPathClosed is not directly used in this Boost.Polygon based NFP calculation flow.
    // Path closure is inherent in the Polygon, Boost.Polygon, and Clipper2 models.
    (void)isPathClosed; // Mark as unused to prevent compiler warnings.

    // 1. Convert polyA and polyB to BoostMinkowski::PolygonDouble
    BoostMinkowski::PolygonDouble polyA_double = ToPolygonDouble(polyA);
    BoostMinkowski::PolygonDouble polyB_double = ToPolygonDouble(polyB);

    // 2. Declare variables for calculateMinkowskiSumRaw outputs
    double calculated_scale = 0.0;
    BoostMinkowski::PointDouble b_ref_original_coords = {0.0, 0.0};

    // 3. Call the refactored Minkowski sum function (NFP calculation: A + (-B))
    std::vector<boost::polygon::polygon_with_holes_data<int>> boost_result_polys =
        BoostMinkowski::calculateMinkowskiSumRaw(
            polyA_double,
            polyB_double,
            calculated_scale,
            b_ref_original_coords
        );

    // 4. Convert the Boost.Polygon result to Clipper2Lib::Paths64
    // The b_ref_original_coords is used to shift the result back, as NFP is A-B relative to origin of A.
    // The calculateMinkowskiSumRaw negates B and captures B's original reference point.
    // The result from Boost is A + (-B_shifted_to_origin), so we need to shift it by B's original reference point.
    Clipper2Lib::Paths64 final_clipper_paths = BoostPolygonsToPaths64(
        boost_result_polys,
        calculated_scale,
        b_ref_original_coords // This shift ensures the NFP is positioned correctly relative to A's origin.
    );
    
    // 5. Return the final Clipper2Lib::Paths64
    return final_clipper_paths;
}


Clipper2Lib::PointInPolygonResult GeometryProcessor::pointInPolygon(const Point& pt, const Polygon& poly) {
    if (poly.outer.empty()) return Clipper2Lib::PointInPolygonResult::IsOutside;

    Clipper2Lib::Point64 p(static_cast<long long>(pt.x * CLIPPER_SCALE), static_cast<long long>(pt.y * CLIPPER_SCALE));
    Clipper2Lib::Path64 outer_path = PointsToPath64(poly.outer);

    // Check orientation of outer_path. PointInPath behavior might depend on it,
    // or it might assume a specific winding (e.g. CCW for filled regions).
    // For robustness, ensure consistent orientation or test Clipper2's behavior.
    // If outer_path is CW, PointInPath might consider "inside" as "outside" for positive fill rules.
    // For now, assume PointInPath handles it, or that input paths are CCW.

    Clipper2Lib::PointInPolygonResult result = Clipper2Lib::PointInPolygon(p, outer_path);

    if (result == Clipper2Lib::PointInPolygonResult::IsInside) { // Only check holes if it's inside the outer boundary
        for (const auto& hole_pts : poly.holes) {
            if (hole_pts.empty()) continue;
            Clipper2Lib::Path64 hole_path = PointsToPath64(hole_pts);
            // Similar orientation consideration for holes (typically CW if outer is CCW).
            Clipper2Lib::PointInPolygonResult hole_result = Clipper2Lib::PointInPolygon(p, hole_path);
            
            if (hole_result == Clipper2Lib::PointInPolygonResult::IsInside) {
                return Clipper2Lib::PointInPolygonResult::IsOutside; // Point is inside a hole
            }
            if (hole_result == Clipper2Lib::PointInPolygonResult::IsOn) {
                return Clipper2Lib::PointInPolygonResult::IsOn; // Point is on the boundary of a hole
            }
        }
    }
    return result; // Return original result if not inside any hole (or if no holes)
}
