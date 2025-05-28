#include "NfpGenerator.h"
#include <algorithm> // For std::max, std::min, std::fabs
#include <limits>    // For std::numeric_limits

#include <QDebug> // Added for logging

// Bring in Boost.Polygon using directives for convenience
using namespace boost::polygon::operators;

NfpGenerator::NfpGenerator() {}

// Private helper functions adapted from minkowski.cc
void NfpGenerator::convolve_two_segments(std::vector<BoostPoint>& figure, const BoostEdge& a, const BoostEdge& b) {
    using namespace boost::polygon;
    figure.clear();
    figure.push_back(BoostPoint(a.first));  // p0 + q0
    figure.push_back(BoostPoint(a.first));  // p0 + q1
    figure.push_back(BoostPoint(a.second)); // p1 + q1
    figure.push_back(BoostPoint(a.second)); // p1 + q0
    // Perform convolution: (p + q)
    // This order matches the example in the prompt (derived from original minkowski.cc)
    convolve(figure[0], b.second); 
    convolve(figure[1], b.first);  
    convolve(figure[2], b.first);
    convolve(figure[3], b.second);
}

// Template methods convolve_two_point_sequences and convolve_point_sequence_with_polygons
// are moved to NfpGenerator.h as per task instructions for template definitions.

void NfpGenerator::convolve_two_polygon_sets(BoostPolygonSet& result, const BoostPolygonSet& a, const BoostPolygonSet& b) {
  using namespace boost::polygon;
  result.clear();
  std::vector<BoostPolygonWithHoles> a_polygons;
  std::vector<BoostPolygonWithHoles> b_polygons;
  a.get(a_polygons);
  b.get(b_polygons);

  for(std::size_t ai = 0; ai < a_polygons.size(); ++ai) {
    convolve_point_sequence_with_polygons(result, begin_points(a_polygons[ai]), 
                                          end_points(a_polygons[ai]), b_polygons);
    for(polygon_with_holes_traits<BoostPolygonWithHoles>::iterator_holes_type itrh = begin_holes(a_polygons[ai]);
        itrh != end_holes(a_polygons[ai]); ++itrh) {
      convolve_point_sequence_with_polygons(result, begin_points(*itrh), 
                                            end_points(*itrh), b_polygons);
    }
    // The original code also had a part that convolved individual points of one polygon set
    // with the other polygon set. This is generally handled by Boost.Polygon's polygon set
    // convolution, but the original implementation might have had specific reasons for it.
    // For a direct port, we include it.
    for(std::size_t bi = 0; bi < b_polygons.size(); ++bi) {
      if (size(a_polygons[ai]) > 0 && size(b_polygons[bi]) > 0) { // Ensure polygons are not empty
        BoostPolygonWithHoles tmp_poly_a = a_polygons[ai];
        result.insert(convolve(tmp_poly_a, *(begin_points(b_polygons[bi]))));
      
        BoostPolygonWithHoles tmp_poly_b = b_polygons[bi];
        result.insert(convolve(tmp_poly_b, *(begin_points(a_polygons[ai]))));
      }
    }
  }
}


BoostPolygonSet NfpGenerator::toBoostPolygonSet(const Polygon& poly, double scale_factor) {
    BoostPolygonSet bset;
    BoostPolygonWithHoles bpoly_with_holes;

    // Outer polygon
    std::vector<BoostPoint> bpoints_outer;
    for (const auto& pt : poly.outer) {
        bpoints_outer.push_back(BoostPoint(static_cast<int>(pt.x * scale_factor), static_cast<int>(pt.y * scale_factor)));
    }
    if (!bpoints_outer.empty()) {
        boost::polygon::set_points(bpoly_with_holes, bpoints_outer.begin(), bpoints_outer.end());
    }

    // Holes
    std::vector<BoostPolygonWithHoles::hole_type> bholes;
    for (const auto& hole_pts_vec : poly.holes) {
        std::vector<BoostPoint> bpoints_hole;
        for (const auto& pt : hole_pts_vec) {
            bpoints_hole.push_back(BoostPoint(static_cast<int>(pt.x * scale_factor), static_cast<int>(pt.y * scale_factor)));
        }
        if (!bpoints_hole.empty()) {
            // Holes in Boost.Polygon must have clockwise winding order.
            // The input Polygon struct doesn't specify winding order.
            // Assuming input is correct or Boost handles it. If issues arise,
            // winding order check/reversal might be needed here.
            BoostPolygonWithHoles::hole_type hole_poly;
            boost::polygon::set_points(hole_poly, bpoints_hole.begin(), bpoints_hole.end());
            bholes.push_back(hole_poly);
        }
    }
    if (!bholes.empty()) {
        boost::polygon::set_holes(bpoly_with_holes, bholes.begin(), bholes.end());
    }
    
    // Only insert if the polygon is valid (has points)
    if (!bpoints_outer.empty()) {
        bset.insert(bpoly_with_holes);
    }
    return bset;
}

std::vector<Polygon> NfpGenerator::fromBoostPolygons(const std::vector<BoostPolygonWithHoles>& boost_polys, double scale_factor, double x_shift, double y_shift) {
    std::vector<Polygon> result_polys;
    if (scale_factor == 0) { // Avoid division by zero
        // Handle error or return empty, depending on desired behavior
        return result_polys; 
    }

    for (const auto& bpwh : boost_polys) {
        Polygon current_poly;
        
        // Outer boundary
        for (auto it = boost::polygon::begin_points(bpwh); it != boost::polygon::end_points(bpwh); ++it) {
            current_poly.outer.push_back({
                (static_cast<double>((*it).get(boost::polygon::HORIZONTAL)) / scale_factor) + x_shift,
                (static_cast<double>((*it).get(boost::polygon::VERTICAL)) / scale_factor) + y_shift
            });
        }

        // Holes
        for (auto hole_it = boost::polygon::begin_holes(bpwh); hole_it != boost::polygon::end_holes(bpwh); ++hole_it) {
            std::vector<Point> current_hole;
            for (auto pt_it = boost::polygon::begin_points(*hole_it); pt_it != boost::polygon::end_points(*hole_it); ++pt_it) {
                current_hole.push_back({
                    (static_cast<double>((*pt_it).get(boost::polygon::HORIZONTAL)) / scale_factor) + x_shift,
                    (static_cast<double>((*pt_it).get(boost::polygon::VERTICAL)) / scale_factor) + y_shift
                });
            }
            if (!current_hole.empty()) {
                current_poly.holes.push_back(current_hole);
            }
        }
        
        // Only add if the polygon is valid (has an outer boundary)
        if (!current_poly.outer.empty()) {
            result_polys.push_back(current_poly);
        }
    }
    return result_polys;
}


std::vector<Polygon> NfpGenerator::calculateNFP(const Polygon& polyA, const Polygon& polyB, double scale, double xshift, double yshift) {
    if (scale == 0) {
        // Or throw an exception, or handle as an error appropriately
        return {}; 
    }

    BoostPolygonSet boost_poly_A = toBoostPolygonSet(polyA, scale);
    
    Polygon polyB_transformed = polyB; // Make a mutable copy
    // Negate points of polyB for Minkowski difference (A + (-B))
    for (auto& pt : polyB_transformed.outer) {
        pt.x = -pt.x;
        pt.y = -pt.y;
    }
    for (auto& hole_pts : polyB_transformed.holes) {
        for (auto& pt : hole_pts) {
            pt.x = -pt.x;
            pt.y = -pt.y;
        }
    }
    BoostPolygonSet boost_poly_B_negated = toBoostPolygonSet(polyB_transformed, scale);

    BoostPolygonSet minkowski_result_set;
    // Perform Minkowski sum: A + (-B_transformed) which is equivalent to A - B_original
    // The convolve_two_polygon_sets function implements the Minkowski sum.
    convolve_two_polygon_sets(minkowski_result_set, boost_poly_A, boost_poly_B_negated);
    
    std::vector<BoostPolygonWithHoles> minkowski_polys_with_holes;
    minkowski_result_set.get(minkowski_polys_with_holes); // Extract polygons from the set

    qDebug() << "NfpGenerator::calculateNFP - Input A polygons:" << boost_poly_A.size() 
             << "Input B_negated polygons:" << boost_poly_B_negated.size();

    // Perform Minkowski sum: A + (-B_transformed) which is equivalent to A - B_original
    // The convolve_two_polygon_sets function implements the Minkowski sum.
    convolve_two_polygon_sets(minkowski_result_set, boost_poly_A, boost_poly_B_negated);
    
    std::vector<BoostPolygonWithHoles> minkowski_polys_with_holes;
    minkowski_result_set.get(minkowski_polys_with_holes); // Extract polygons from the set

    qDebug() << "NfpGenerator::calculateNFP - Result set size (before conversion):" << minkowski_polys_with_holes.size();
    if (minkowski_polys_with_holes.empty() && (boost_poly_A.size() > 0 || boost_poly_B_negated.size() > 0)) {
        qWarning() << "NfpGenerator::calculateNFP - Minkowski sum resulted in empty set of polygons for non-empty inputs.";
    }

    // Convert back to our Polygon struct, applying inverse scale and shifts
    return fromBoostPolygons(minkowski_polys_with_holes, scale, xshift, yshift);
}
