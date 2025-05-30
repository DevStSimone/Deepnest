#include "minkowski_wrapper.h"

#include <iostream> // For potential debugging
#include <vector>
#include <list>
#include <algorithm> // For std::reverse, std::min, std::max
#include <limits>    // For std::numeric_limits
#include <cmath>     // For std::fabs

// Boost.Polygon headers
#include <boost/polygon/polygon.hpp>
#include <boost/polygon/point_data.hpp>
#include <boost/polygon/polygon_set_data.hpp>
#include <boost/polygon/polygon_with_holes_data.hpp>
// #include <boost/polygon/convenience_operators.hpp>

// Typedefs from original minkowski.cc, adapted for clarity within this C++ context
typedef boost::polygon::point_data<int> BoostPoint;
typedef boost::polygon::polygon_set_data<int> BoostPolygonSet;
typedef boost::polygon::polygon_with_holes_data<int> BoostPolygonWithHoles;
typedef std::pair<BoostPoint, BoostPoint> BoostEdge;

// Using namespace for Boost.Polygon operators to match original style
using namespace boost::polygon::operators;

namespace CustomMinkowski {

// --- Core NFP Logic (convolve functions, identical to thread_wrapper) ---
void convolve_two_segments(std::vector<BoostPoint>& figure, const BoostEdge& a, const BoostEdge& b) {
  figure.clear();
  figure.push_back(BoostPoint(a.first));
  figure.push_back(BoostPoint(a.first));
  figure.push_back(BoostPoint(a.second));
  figure.push_back(BoostPoint(a.second));
  boost::polygon::convolve(figure[0], b.second);
  boost::polygon::convolve(figure[1], b.first);
  boost::polygon::convolve(figure[2], b.first);
  boost::polygon::convolve(figure[3], b.second);
}

template <typename itrT1, typename itrT2>
void convolve_two_point_sequences(BoostPolygonSet& result, itrT1 ab, itrT1 ae, itrT2 bb, itrT2 be) {
  if (ab == ae || bb == be) return;
  BoostPoint prev_a = *ab;
  std::vector<BoostPoint> vec;
  BoostPolygonWithHoles poly;
  ++ab;
  for (; ab != ae; ++ab) {
    BoostPoint prev_b = *bb;
    itrT2 tmpb = bb;
    ++tmpb;
    for (; tmpb != be; ++tmpb) {
      convolve_two_segments(vec, std::make_pair(prev_b, *tmpb), std::make_pair(prev_a, *ab));
      boost::polygon::set_points(poly, vec.begin(), vec.end());
      result.insert(poly);
      prev_b = *tmpb;
    }
    prev_a = *ab;
  }
}

template <typename itrT>
void convolve_point_sequence_with_polygons(BoostPolygonSet& result, itrT b, itrT e, const std::vector<BoostPolygonWithHoles>& polygons) {
  for (std::size_t i = 0; i < polygons.size(); ++i) {
    convolve_two_point_sequences(result, b, e, boost::polygon::begin_points(polygons[i]), boost::polygon::end_points(polygons[i]));
    for (boost::polygon::polygon_with_holes_traits<BoostPolygonWithHoles>::iterator_holes_type itrh = boost::polygon::begin_holes(polygons[i]);
         itrh != boost::polygon::end_holes(polygons[i]); ++itrh) {
      convolve_two_point_sequences(result, b, e, boost::polygon::begin_points(*itrh), boost::polygon::end_points(*itrh));
    }
  }
}

void convolve_two_polygon_sets(BoostPolygonSet& result, const BoostPolygonSet& pa, const BoostPolygonSet& pb) {
  result.clear();
  std::vector<BoostPolygonWithHoles> a_polygons;
  std::vector<BoostPolygonWithHoles> b_polygons;
  pa.get(a_polygons);
  pb.get(b_polygons);

  for (std::size_t ai = 0; ai < a_polygons.size(); ++ai) {
    convolve_point_sequence_with_polygons(result, boost::polygon::begin_points(a_polygons[ai]),
                                          boost::polygon::end_points(a_polygons[ai]), b_polygons);
    for (boost::polygon::polygon_with_holes_traits<BoostPolygonWithHoles>::iterator_holes_type itrh = boost::polygon::begin_holes(a_polygons[ai]);
         itrh != boost::polygon::end_holes(a_polygons[ai]); ++itrh) {
      convolve_point_sequence_with_polygons(result, boost::polygon::begin_points(*itrh),
                                            boost::polygon::end_points(*itrh), b_polygons);
    }
    for (std::size_t bi = 0; bi < b_polygons.size(); ++bi) {
      if (a_polygons[ai].begin() == a_polygons[ai].end() || b_polygons[bi].begin() == b_polygons[bi].end()) continue;
      BoostPolygonWithHoles tmp_poly = a_polygons[ai];
      result.insert(boost::polygon::convolve(tmp_poly, *(boost::polygon::begin_points(b_polygons[bi]))));
      tmp_poly = b_polygons[bi];
      result.insert(boost::polygon::convolve(tmp_poly, *(boost::polygon::begin_points(a_polygons[ai]))));
    }
  }
}
// --- End of Core NFP Logic ---

// --- Helper functions (from minkowski_thread_wrapper.cpp) ---
std::vector<BoostPoint> toBoostPoints(const PolygonPath& path, double scale) {
    std::vector<BoostPoint> boostPts;
    boostPts.reserve(path.size());
    for (const auto& p : path) {
        boostPts.emplace_back(static_cast<int>(p.x * scale), static_cast<int>(p.y * scale));
    }
    return boostPts;
}

PolygonPath fromBoostPathToPolygonPath(boost::polygon::polygon_traits<BoostPolygonWithHoles>::iterator_type begin,
                                       boost::polygon::polygon_traits<BoostPolygonWithHoles>::iterator_type end,
                                       double inverse_scale, double x_shift, double y_shift) {
    PolygonPath path;
    for (auto itr = begin; itr != end; ++itr) {
        path.push_back({
            (static_cast<double>(itr->x()) / inverse_scale) + x_shift,
            (static_cast<double>(itr->y()) / inverse_scale) + y_shift
        });
    }
    return path;
}

void calculate_bounds_for_poly(const PolygonWithHoles& poly, double& min_x, double& max_x, double& min_y, double& max_y) {
    for (const auto& p : poly.outer) {
        min_x = std::min(min_x, p.x); max_x = std::max(max_x, p.x);
        min_y = std::min(min_y, p.y); max_y = std::max(max_y, p.y);
    }
    for (const auto& hole : poly.holes) {
        for (const auto& p : hole) {
            min_x = std::min(min_x, p.x); max_x = std::max(max_x, p.x);
            min_y = std::min(min_y, p.y); max_y = std::max(max_y, p.y);
        }
    }
}
// --- End of Helper Functions ---


// Main function for single-threaded NFP calculation, now with dynamic scaling.
bool CalculateNfp(
    const PolygonWithHoles& partA_orbiting, 
    const PolygonWithHoles& partB_static,
    NfpResultPolygons& nfp_result,
    double fixed_scale_for_boost_poly)  // This parameter is now IGNORED. Dynamic scaling is used.
{
    nfp_result.clear();

    // 1. Dynamic Scaling Factor Calculation
    double Aminx = std::numeric_limits<double>::max(), Amaxx = std::numeric_limits<double>::lowest();
    double Aminy = std::numeric_limits<double>::max(), Amaxy = std::numeric_limits<double>::lowest();
    calculate_bounds_for_poly(partA_orbiting, Aminx, Amaxx, Aminy, Amaxy);

    double Bminx = std::numeric_limits<double>::max(), Bmaxx = std::numeric_limits<double>::lowest();
    double Bminy = std::numeric_limits<double>::max(), Bmaxy = std::numeric_limits<double>::lowest();
    calculate_bounds_for_poly(partB_static, Bminx, Bmaxx, Bminy, Bmaxy);

    if (Aminx > Amaxx || Bminx > Bmaxx) { // Check if parts are empty or invalid
        // std::cerr << "CustomMinkowski::CalculateNfp: Input part(s) have invalid bounds (possibly empty)." << std::endl;
        return false; // Or true with empty nfp_result if that's preferred for "valid but no NFP"
    }

    double Cmaxx = Amaxx + Bmaxx;
    double Cminx = Aminx + Bminx;
    double Cmaxy = Amaxy + Bmaxy;
    double Cminy = Aminy + Bminy;

    double maxxAbs = std::max(Cmaxx, std::fabs(Cminx));
    double maxyAbs = std::max(Cmaxy, std::fabs(Cminy));

    double max_coord_abs = std::max(maxxAbs, maxyAbs);
    if (max_coord_abs < 1.0) max_coord_abs = 1.0;

    // Defensive check for max_coord_abs being zero or extremely small, which could make inputscale huge or inf
    if (max_coord_abs < 1e-9) { // If max coordinate is essentially zero
      //  std::cerr << "CustomMinkowski::CalculateNfp: Max coordinate value is near zero, cannot determine scale reliably." << std::endl;
        return false; // Cannot compute NFP if parts are effectively points at origin.
    }


    double inputscale = (0.1 * static_cast<double>(std::numeric_limits<int>::max())) / max_coord_abs;
    if (inputscale <= 0 || !std::isfinite(inputscale)) {
      //  std::cerr << "CustomMinkowski::CalculateNfp: Calculated inputscale is invalid: " << inputscale << std::endl;
        return false; 
    }

    // 2. Convert inputs to Boost.Polygon types using `inputscale`
    BoostPolygonSet boost_set_A, boost_set_B;

    // Part A (orbiting)
    if (!partA_orbiting.outer.empty()) {
        std::vector<BoostPoint> outerA_pts = toBoostPoints(partA_orbiting.outer, inputscale);
        BoostPolygonWithHoles polyA_outer;
        boost::polygon::set_points(polyA_outer, outerA_pts.begin(), outerA_pts.end());
        boost_set_A += polyA_outer;
        for (const auto& hole_path : partA_orbiting.holes) {
            if (!hole_path.empty()) {
                std::vector<BoostPoint> holeA_pts = toBoostPoints(hole_path, inputscale);
                BoostPolygonWithHoles polyA_hole;
                boost::polygon::set_points(polyA_hole, holeA_pts.begin(), holeA_pts.end());
                boost_set_A -= polyA_hole;
            }
        }
    }

    // Part B (static), reflected and shifted
    double xshift = 0, yshift = 0;
    if (!partB_static.outer.empty()) {
        xshift = partB_static.outer[0].x;
        yshift = partB_static.outer[0].y;

        PolygonPath reflected_outerB;
        reflected_outerB.reserve(partB_static.outer.size());
        for (const auto& p : partB_static.outer) {
            reflected_outerB.push_back({-p.x, -p.y});
        }

        std::vector<BoostPoint> outerB_pts = toBoostPoints(reflected_outerB, inputscale);
        BoostPolygonWithHoles polyB_outer;
        boost::polygon::set_points(polyB_outer, outerB_pts.begin(), outerB_pts.end());
        boost_set_B += polyB_outer;

        for (const auto& hole_path_orig : partB_static.holes) {
            if (!hole_path_orig.empty()) {
                PolygonPath reflected_holeB;
                reflected_holeB.reserve(hole_path_orig.size());
                for (const auto& p_hole : hole_path_orig) {
                    reflected_holeB.push_back({-p_hole.x, -p_hole.y});
                }
                std::vector<BoostPoint> holeB_pts = toBoostPoints(reflected_holeB, inputscale);
                BoostPolygonWithHoles polyB_hole;
                boost::polygon::set_points(polyB_hole, holeB_pts.begin(), holeB_pts.end());
                boost_set_B -= polyB_hole;
            }
        }
    }

    if (boost_set_A.empty() || boost_set_B.empty()) {
      //  std::cerr << "CustomMinkowski::CalculateNfp: One or both input polygon sets are empty after conversion." << std::endl;
        return true; // Successfully processed, but result is empty NFP.
    }
    
    // 3. NFP Calculation
    BoostPolygonSet boost_set_C_result;
    convolve_two_polygon_sets(boost_set_C_result, boost_set_A, boost_set_B);

    // 4. Convert Result
    std::vector<BoostPolygonWithHoles> result_polys_with_holes;
    boost_set_C_result.get(result_polys_with_holes);

    if (result_polys_with_holes.empty()) {
       // std::cerr << "CustomMinkowski::CalculateNfp: NFP result polygon list is empty from Boost." << std::endl;
        // This is not an error, it just means the NFP is empty.
    }

    for(const auto& poly_wh : result_polys_with_holes) {
        PolygonPath current_nfp_outer_path = fromBoostPathToPolygonPath(
            poly_wh.begin(), poly_wh.end(), inputscale, xshift, yshift);

        if (!current_nfp_outer_path.empty()) {
            nfp_result.push_back(current_nfp_outer_path);
        }
        // Note: Holes in the NFP result are not handled by current NfpResultPolygons type.
    }
    
    // std::cout << "CustomMinkowski::CalculateNfp: NFP calculation produced " << nfp_result.size() << " polygons." << std::endl;
    return true; // Success, even if nfp_result is empty.
}

} // namespace CustomMinkowski
