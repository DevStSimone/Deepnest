#ifndef BOOST_MINKOWSKI_H
#define BOOST_MINKOWSKI_H

#include <vector>
#include <boost/polygon/polygon.hpp> // General Boost.Polygon header
#include <boost/polygon/point_data.hpp>
#include <boost/polygon/polygon_set_data.hpp>
#include <boost/polygon/polygon_with_holes_data.hpp>
#include <limits> // Required for std::numeric_limits

namespace BoostMinkowski {

    struct PointDouble {
        double x;
        double y;
    };

    struct PolygonDouble {
        std::vector<PointDouble> outer;
        std::vector<std::vector<PointDouble>> holes;
    };

    // Forward declaration for types used in detail namespace, if any were to be exposed.
    // For now, assume detail types are self-contained or use fundamental/Boost types.

    std::vector<boost::polygon::polygon_with_holes_data<int>>
    calculateMinkowskiSumRaw(
        const PolygonDouble& polyA_double,
        const PolygonDouble& polyB_double,
        double& out_calculated_scale,
        PointDouble& out_b_ref_point_original_coords
    );

    struct NFPResult {
        std::vector<boost::polygon::polygon_with_holes_data<int>> nfp_polys;
        double scale_used; 
        // Add an error state or message if needed in the future
    };

    std::vector<NFPResult>
    calculateMinkowskiSumBatchRaw(
        const std::vector<PolygonDouble>& listPolyA,
        const PolygonDouble& polyB,
        int num_threads,
        PointDouble& out_b_ref_point_original_coords // Output: B's original reference point, captured once
    );

} // namespace BoostMinkowski

#endif // BOOST_MINKOWSKI_H
