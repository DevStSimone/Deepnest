#ifndef NFP_GENERATOR_H
#define NFP_GENERATOR_H

#include <vector>
#include <boost/polygon/polygon.hpp> // Include necessary boost headers
#include <utility> // For std::pair

// Definition of Point and Polygon structs
struct Point {
    double x;
    double y;
};

struct Polygon {
    std::vector<Point> outer;
    std::vector<std::vector<Point>> holes;
};

// Typedefs for Boost.Polygon
typedef boost::polygon::point_data<int> BoostPoint;
typedef boost::polygon::polygon_set_data<int> BoostPolygonSet;
typedef boost::polygon::polygon_with_holes_data<int> BoostPolygonWithHoles;
typedef std::pair<BoostPoint, BoostPoint> BoostEdge;


class NfpGenerator {
public:
    NfpGenerator();

    std::vector<Polygon> calculateNFP(
        const Polygon& polyA, 
        const Polygon& polyB,
        double scale,
        double xshift,
        double yshift
    );

private:
    // Helper functions adapted from minkowski.cc
    void convolve_two_segments(std::vector<BoostPoint>& figure, const BoostEdge& a, const BoostEdge& b);
    
    template <typename itrT1, typename itrT2>
    void convolve_two_point_sequences(BoostPolygonSet& result, itrT1 ab, itrT1 ae, itrT2 bb, itrT2 be) {
        using namespace boost::polygon;
        if(ab == ae || bb == be)
            return;
        
        // Ensure iterators provide BoostPoint directly or convertible to it
        BoostPoint prev_a = *ab;
        std::vector<BoostPoint> vec;
        BoostPolygonWithHoles poly; 
        
        itrT1 current_a = ab;
        ++current_a; // Start from the second point to form segments for 'a'

        for( ; current_a != ae; ++current_a) {
            BoostPoint prev_b = *bb;
            itrT2 current_b = bb;
            ++current_b; // Start from the second point to form segments for 'b'

            for( ; current_b != be; ++current_b) {
                // Create BoostEdge for current segments
                BoostEdge edge_a = std::make_pair(prev_a, *current_a);
                BoostEdge edge_b = std::make_pair(prev_b, *current_b);
                
                convolve_two_segments(vec, edge_a, edge_b); // Pass edges
                if (!vec.empty()) {
                     // Check orientation for Boost.Polygon: points must be in counter-clockwise order for outer, clockwise for holes.
                     // The convolve_two_segments output might need orientation check/fix if it's arbitrary.
                     // For simplicity, assuming convolve_two_segments provides correctly ordered points or that Boost handles it.
                    set_points(poly, vec.begin(), vec.end());
                    result.insert(poly);
                }
                prev_b = *current_b;
            }
            prev_a = *current_a;
        }
    }
    
    template <typename itrT>
    void convolve_point_sequence_with_polygons(BoostPolygonSet& result, itrT b, itrT e, const std::vector<BoostPolygonWithHoles>& polygons) {
        using namespace boost::polygon;
        for(std::size_t i = 0; i < polygons.size(); ++i) {
            convolve_two_point_sequences(result, b, e, begin_points(polygons[i]), end_points(polygons[i]));
            for(typename polygon_with_holes_traits<BoostPolygonWithHoles>::iterator_holes_type itrh = begin_holes(polygons[i]);
                itrh != end_holes(polygons[i]); ++itrh) {
                convolve_two_point_sequences(result, b, e, begin_points(*itrh), end_points(*itrh));
            }
        }
    }
    
    void convolve_two_polygon_sets(BoostPolygonSet& result, const BoostPolygonSet& a, const BoostPolygonSet& b);

    BoostPolygonSet toBoostPolygonSet(const Polygon& poly, double scale);
    std::vector<Polygon> fromBoostPolygons(const std::vector<BoostPolygonWithHoles>& polys, double scale, double xshift, double yshift);
};

#endif // NFP_GENERATOR_H
