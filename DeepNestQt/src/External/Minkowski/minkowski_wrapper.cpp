#include "minkowski_wrapper.h"

#include <iostream> // For potential debugging
#include <vector>
#include <list>
#include <algorithm> // For std::reverse if needed for orientation
#include <limits>    // For std::numeric_limits

// Boost.Polygon headers - ensure these are available in the include path during compilation
#include <boost/polygon/polygon.hpp>
#include <boost/polygon/point_data.hpp>
#include <boost/polygon/polygon_set_data.hpp>
#include <boost/polygon/polygon_with_holes_data.hpp>
#include <boost/polygon/convenience_operators.hpp> // For operators like +=, -= on polygon_set_data

// Typedefs from original minkowski.cc, adapted for clarity within this C++ context
typedef boost::polygon::point_data<int> BoostPoint; // Using int as per original module
typedef boost::polygon::polygon_set_data<int> BoostPolygonSet;
typedef boost::polygon::polygon_with_holes_data<int> BoostPolygonWithHoles;
typedef std::pair<BoostPoint, BoostPoint> BoostEdge;

// Using namespace for Boost.Polygon operators to match original style
using namespace boost::polygon::operators;

namespace CustomMinkowski {

// --- Helper functions (potentially from original, made pure C++) ---
// These are the core convolution functions from the original file.
// They seem to be generic enough to be used directly with Boost.Polygon types.

void convolve_two_segments(std::vector<BoostPoint>& figure, const BoostEdge& a, const BoostEdge& b) {
  using namespace boost::polygon;
  figure.clear();
  figure.push_back(BoostPoint(a.first));
  figure.push_back(BoostPoint(a.first));
  figure.push_back(BoostPoint(a.second));
  figure.push_back(BoostPoint(a.second));
  convolve(figure[0], b.second);
  convolve(figure[1], b.first);
  convolve(figure[2], b.first);
  convolve(figure[3], b.second);
}

template <typename itrT1, typename itrT2>
void convolve_two_point_sequences(BoostPolygonSet& result, itrT1 ab, itrT1 ae, itrT2 bb, itrT2 be) {
  using namespace boost::polygon;
  if(ab == ae || bb == be)
    return;
  //BoostPoint first_a = *ab; // Unused
  BoostPoint prev_a = *ab;
  std::vector<BoostPoint> vec;
  BoostPolygonWithHoles poly; // Use BoostPolygonWithHoles here
  ++ab;
  for( ; ab != ae; ++ab) {
    //BoostPoint first_b = *bb; // Unused
    BoostPoint prev_b = *bb;
    itrT2 tmpb = bb;
    ++tmpb;
    for( ; tmpb != be; ++tmpb) {
      convolve_two_segments(vec, std::make_pair(prev_b, *tmpb), std::make_pair(prev_a, *ab));
      set_points(poly, vec.begin(), vec.end());
      result.insert(poly);
      prev_b = *tmpb;
    }
    prev_a = *ab;
  }
}

template <typename itrT>
void convolve_point_sequence_with_polygons(BoostPolygonSet& result, itrT b, itrT e, const std::vector<BoostPolygonWithHoles>& polygons) {
  using namespace boost::polygon;
  for(std::size_t i = 0; i < polygons.size(); ++i) {
    convolve_two_point_sequences(result, b, e, begin_points(polygons[i]), end_points(polygons[i]));
    for(polygon_with_holes_traits<BoostPolygonWithHoles>::iterator_holes_type itrh = begin_holes(polygons[i]);
        itrh != end_holes(polygons[i]); ++itrh) {
      convolve_two_point_sequences(result, b, e, begin_points(*itrh), end_points(*itrh));
    }
  }
}

void convolve_two_polygon_sets(BoostPolygonSet& result, const BoostPolygonSet& pa, const BoostPolygonSet& pb) {
  using namespace boost::polygon;
  result.clear();
  std::vector<BoostPolygonWithHoles> a_polygons;
  std::vector<BoostPolygonWithHoles> b_polygons;
  pa.get(a_polygons);
  pb.get(b_polygons);

  for(std::size_t ai = 0; ai < a_polygons.size(); ++ai) {
    convolve_point_sequence_with_polygons(result, begin_points(a_polygons[ai]), 
                                          end_points(a_polygons[ai]), b_polygons);
    for(polygon_with_holes_traits<BoostPolygonWithHoles>::iterator_holes_type itrh = begin_holes(a_polygons[ai]);
        itrh != end_holes(a_polygons[ai]); ++itrh) {
      convolve_point_sequence_with_polygons(result, begin_points(*itrh), 
                                            end_points(*itrh), b_polygons);
    }
    for(std::size_t bi = 0; bi < b_polygons.size(); ++bi) {
      if (a_polygons[ai].begin_points() == a_polygons[ai].end_points() || b_polygons[bi].begin_points() == b_polygons[bi].end_points()) continue;
      BoostPolygonWithHoles tmp_poly = a_polygons[ai];
      result.insert(convolve(tmp_poly, *(begin_points(b_polygons[bi]))));
      tmp_poly = b_polygons[bi];
      result.insert(convolve(tmp_poly, *(begin_points(a_polygons[ai]))));
    }
  }
}


// Helper to convert PolygonPath (vector<Point>) to BoostPoints (vector<BoostPoint>)
std::vector<BoostPoint> toBoostPoints(const PolygonPath& path, double scale) {
    std::vector<BoostPoint> boostPts;
    boostPts.reserve(path.size());
    for (const auto& p : path) {
        boostPts.emplace_back(static_cast<int>(p.x * scale), static_cast<int>(p.y * scale));
    }
    return boostPts;
}


bool CalculateNfp(
    const PolygonWithHoles& partA_orbiting, 
    const PolygonWithHoles& partB_static,
    NfpResultPolygons& nfp_result,
    double fixed_scale_for_boost_poly) 
{
    nfp_result.clear();
    if (fixed_scale_for_boost_poly <= 0) {
        // Invalid scale
        return false; 
    }

    BoostPolygonSet boost_set_A, boost_set_B, boost_set_C_result;

    // Convert partA_orbiting to BoostPolygonSet
    if (!partA_orbiting.outer.empty()) {
        std::vector<BoostPoint> outerA_pts = toBoostPoints(partA_orbiting.outer, fixed_scale_for_boost_poly);
        BoostPolygonWithHoles polyA_outer;
        boost::polygon::set_points(polyA_outer, outerA_pts.begin(), outerA_pts.end());
        boost_set_A += polyA_outer;

        for (const auto& hole_path : partA_orbiting.holes) {
            if (!hole_path.empty()) {
                std::vector<BoostPoint> holeA_pts = toBoostPoints(hole_path, fixed_scale_for_boost_poly);
                BoostPolygonWithHoles polyA_hole;
                boost::polygon::set_points(polyA_hole, holeA_pts.begin(), holeA_pts.end());
                boost_set_A -= polyA_hole; // Subtract holes
            }
        }
    }

    // Convert partB_static to BoostPolygonSet, applying reflection and shift logic
    // The original code negates B's coordinates and stores the first point's original value as a shift.
    // This is equivalent to reflecting B about its first point, then translating so that first point is at origin,
    // then negating all coordinates (reflecting about origin).
    // NFP(A, B) = A (+) Reflect(B about B_ref_point, then translate B_ref_point to origin)
    // Or, if B's reference point is (0,0) for its coordinates: NFP = A (+) Reflect(B about origin)
    
    double xshift = 0, yshift = 0;
    if (!partB_static.outer.empty()) {
        xshift = partB_static.outer[0].x; // Capture shift from original first point of B
        yshift = partB_static.outer[0].y;

        PolygonPath reflected_outerB;
        reflected_outerB.reserve(partB_static.outer.size());
        for(const auto& p : partB_static.outer) {
            // Reflect B: For NFP A(+)B, B is typically reflected about its reference point.
            // If B's reference point is (0,0) in its local coords, then this is just (-x, -y).
            // The original code did: x = -(inputscale * p.x); y = -(inputscale * p.y);
            // And then added xshift, yshift to the result.
            // This means the NFP is initially calculated for B reflected and at origin, then shifted.
            // So, for input to Boost, we need B's points as (-p.x, -p.y) relative to B's origin.
            reflected_outerB.push_back({-p.x, -p.y});
        }
        // Note: boost::polygon expects specific orientations (CW for holes, CCW for outer).
        // The PolygonWithHoles struct should ideally enforce this or it needs to be done here.
        // Assuming PolygonPath points are already in correct order (e.g. CCW for outer).
        // Reflection reverses orientation, so if outer was CCW, reflected is CW.
        // Boost might require CCW for the outer boundary of the *second* operand in Minkowski sum (convolution).
        // For now, let's assume the original code's orientation handling within boost::polygon is sufficient
        // once points are correctly provided.

        std::vector<BoostPoint> outerB_pts = toBoostPoints(reflected_outerB, fixed_scale_for_boost_poly);
        BoostPolygonWithHoles polyB_outer;
        boost::polygon::set_points(polyB_outer, outerB_pts.begin(), outerB_pts.end());
        boost_set_B += polyB_outer;

        for (const auto& hole_path_orig : partB_static.holes) {
            if (!hole_path_orig.empty()) {
                PolygonPath reflected_holeB;
                reflected_holeB.reserve(hole_path_orig.size());
                for(const auto& p_hole : hole_path_orig) {
                     reflected_holeB.push_back({-p_hole.x, -p_hole.y});
                }
                std::vector<BoostPoint> holeB_pts = toBoostPoints(reflected_holeB, fixed_scale_for_boost_poly);
                BoostPolygonWithHoles polyB_hole;
                boost::polygon::set_points(polyB_hole, holeB_pts.begin(), holeB_pts.end());
                boost_set_B -= polyB_hole; // Holes in reflected B are also subtracted
            }
        }
    }

    if (boost_set_A.empty() || boost_set_B.empty()) {
      //  std::cerr << "CustomMinkowski: One or both input polygon sets are empty after conversion." << std::endl;
        return false; // Cannot compute NFP if one part is empty
    }
    
    convolve_two_polygon_sets(boost_set_C_result, boost_set_A, boost_set_B);

    std::vector<BoostPolygonWithHoles> result_polys_with_holes;
    boost_set_C_result.get(result_polys_with_holes);

    if (result_polys_with_holes.empty()) {
       // std::cerr << "CustomMinkowski: NFP result is empty." << std::endl;
        return true; // Technically successful, but no NFP produced (e.g. if one part is a point and other is empty)
    }

    for(const auto& poly_wh : result_polys_with_holes) {
        PolygonPath current_nfp_outer_path;
        for(auto itr = poly_wh.begin(); itr != poly_wh.end(); ++itr) {
            current_nfp_outer_path.push_back({
                (static_cast<double>(itr->x()) / fixed_scale_for_boost_poly) + xshift,
                (static_cast<double>(itr->y()) / fixed_scale_for_boost_poly) + yshift
            });
        }
        // TODO: Handle holes in the NFP result if necessary.
        // The current NfpResultPolygons type is std::list<PolygonPath>, so it doesn't store holes of the NFP.
        // Most NFP applications use the outer boundary of the NFP.
        if (!current_nfp_outer_path.empty()) {
            nfp_result.push_back(current_nfp_outer_path);
        }
    }
    
    //std::cout << "CustomMinkowski: NFP calculation produced " << nfp_result.size() << " polygons." << std::endl;
    return !nfp_result.empty();
}

} // namespace CustomMinkowski
