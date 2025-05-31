#include "minkowski_thread_wrapper.h"
#include <boost/polygon/polygon.hpp>
#include <boost/polygon/point_data.hpp>
#include <boost/polygon/polygon_set_data.hpp>
#include <boost/polygon/polygon_with_holes_data.hpp>
// #include <boost/polygon/convenience_operators.hpp> // For operators like +=, -= on polygon_set_data

#include <boost/asio/io_service.hpp>
#include <boost/thread/thread.hpp>
#include <boost/bind/bind.hpp>
#include <boost/function.hpp>


#include <vector>
#include <list>
#include <algorithm> // For std::reverse, std::min, std::max
#include <limits>    // For std::numeric_limits
#include <cmath>     // For std::fabs
#include <iostream>  // For debugging (temporary)

// Typedefs for Boost.Polygon, consistent with minkowski_wrapper.cpp and original
typedef boost::polygon::point_data<int> BoostPoint;
typedef boost::polygon::polygon_set_data<int> BoostPolygonSet;
typedef boost::polygon::polygon_with_holes_data<int> BoostPolygonWithHoles;
typedef std::pair<BoostPoint, BoostPoint> BoostEdge;

// Using namespace for Boost.Polygon operators
using namespace boost::polygon::operators;

namespace CustomMinkowski {

// --- Start of Boost ASIO based thread_pool (adapted from original) ---
class thread_pool {
private:
    boost::asio::io_service io_service_;
    boost::asio::io_service::work work_;
    boost::thread_group threads_;
    std::size_t available_;
    boost::mutex mutex_; // Mutex for 'available_' count
    boost::condition_variable condition_; // To wait for all tasks to complete

    std::atomic<std::size_t> tasks_in_progress_;

public:
    thread_pool(std::size_t pool_size)
        : work_(io_service_),
          available_(pool_size), // This was used differently in original, more like max tasks at once
          tasks_in_progress_(0)
    {
        if (pool_size == 0) {
            pool_size = boost::thread::hardware_concurrency();
            if (pool_size == 0) pool_size = 2; // Fallback if hardware_concurrency is 0
        }
        for (std::size_t i = 0; i < pool_size; ++i) {
            threads_.create_thread(boost::bind(&boost::asio::io_service::run, &io_service_));
        }
    }

    ~thread_pool() {
        io_service_.stop();
        try {
            threads_.join_all();
        } catch (const std::exception&) {
            // Suppress exceptions during shutdown
        }
    }

    template <typename Task>
    void run_task(Task task) {
        tasks_in_progress_++;
        io_service_.post(boost::bind(&thread_pool::wrap_task, this, boost::function<void()>(task)));
    }

    void wait_for_completion() {
        boost::unique_lock<boost::mutex> lock(mutex_);
        while (tasks_in_progress_ > 0) {
            condition_.wait(lock);
        }
    }

private:
    void wrap_task(boost::function<void()> task) {
        try {
            task();
        } catch (const std::exception& e) {
            std::cerr << "Exception in thread task: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "Unknown exception in thread task." << std::endl;
        }

        boost::unique_lock<boost::mutex> lock(mutex_);
        tasks_in_progress_--;
        if (tasks_in_progress_ == 0) {
            condition_.notify_all();
        }
    }
};
// --- End of thread_pool ---


// --- Core NFP Logic (convolve functions, adapted from original) ---
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
    // BoostPoint first_a = *ab; // Unused in original logic for this func
    BoostPoint prev_a = *ab;
    std::vector<BoostPoint> vec;
    BoostPolygonWithHoles poly;
    ++ab;
    for (; ab != ae; ++ab) {
        // BoostPoint first_b = *bb; // Unused
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


// Helper to convert CustomMinkowski::PolygonPath to std::vector<BoostPoint>
std::vector<BoostPoint> toBoostPoints(const PolygonPath& path, double scale) {
    std::vector<BoostPoint> boostPts;
    boostPts.reserve(path.size());
    for (const auto& p : path) {
        boostPts.emplace_back(static_cast<int>(p.x * scale), static_cast<int>(p.y * scale));
    }
    return boostPts;
}

// Helper to convert Boost path (iterator pair) to CustomMinkowski::PolygonPath
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

// Calculate bounds for dynamic scaling factor
void calculate_bounds(const PolygonWithHoles& poly, double& min_x, double& max_x, double& min_y, double& max_y) {
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

// This function encapsulates the logic for a single NFP pair calculation.
// It will be called by tasks in the thread pool.
NfpBatchResultItem ProcessSingleNfpTask(const NfpTaskItem& task) {
    NfpBatchResultItem result_item(task.taskId, false);

    // 1. Dynamic Scaling Factor Calculation (from original minkowski_thread_original.cc Worker::Do)
    double Aminx = std::numeric_limits<double>::max(), Amaxx = std::numeric_limits<double>::lowest();
    double Aminy = std::numeric_limits<double>::max(), Amaxy = std::numeric_limits<double>::lowest();
    calculate_bounds(task.partA, Aminx, Amaxx, Aminy, Amaxy);

    double Bminx = std::numeric_limits<double>::max(), Bmaxx = std::numeric_limits<double>::lowest();
    double Bminy = std::numeric_limits<double>::max(), Bmaxy = std::numeric_limits<double>::lowest();
    calculate_bounds(task.partB, Bminx, Bmaxx, Bminy, Bmaxy);

    if (Aminx > Amaxx || Bminx > Bmaxx) { // Check if parts are empty or invalid
        result_item.error_message = "Input part(s) have invalid bounds (possibly empty).";
        return result_item;
    }

    double Cmaxx = Amaxx + Bmaxx;
    double Cminx = Aminx + Bminx;
    double Cmaxy = Amaxy + Bmaxy;
    double Cminy = Aminy + Bminy;

    double maxxAbs = std::max(Cmaxx, std::fabs(Cminx));
    double maxyAbs = std::max(Cmaxy, std::fabs(Cminy));

    double max_coord_abs = std::max(maxxAbs, maxyAbs);
    if (max_coord_abs < 1.0) max_coord_abs = 1.0;

    double inputscale = (0.1 * static_cast<double>(std::numeric_limits<int>::max())) / max_coord_abs;
    if (inputscale <= 0) {
        result_item.error_message = "Calculated inputscale is invalid (<=0).";
        return result_item;
    }


    // 2. Convert inputs to Boost.Polygon types using `inputscale`
    // To compute NFP of A_orbiting (task.partA) around B_static (task.partB),
    // the formula is B_static (+) reflect(A_orbiting).
    // The final result is then shifted by B_static's reference point (task.partB.outer[0]).

    BoostPolygonSet boost_set_static_partB, boost_set_reflected_orbiting_partA;
    double xshift = 0, yshift = 0; // These shifts are from the static part (task.partB)

    // Prepare Static Part B (task.partB) - this will be the first operand for convolve_two_polygon_sets
    if (!task.partB.outer.empty()) {
        xshift = task.partB.outer[0].x; // Capture shift from original first point of static B
        yshift = task.partB.outer[0].y;

        std::vector<BoostPoint> outerB_pts = toBoostPoints(task.partB.outer, inputscale);
        BoostPolygonWithHoles polyB_outer;
        boost::polygon::set_points(polyB_outer, outerB_pts.begin(), outerB_pts.end());
        boost_set_static_partB += polyB_outer;

        for (const auto& hole_path : task.partB.holes) {
            if (!hole_path.empty()) {
                std::vector<BoostPoint> holeB_pts = toBoostPoints(hole_path, inputscale);
                BoostPolygonWithHoles polyB_hole;
                boost::polygon::set_points(polyB_hole, holeB_pts.begin(), holeB_pts.end());
                boost_set_static_partB -= polyB_hole; // Subtract holes
            }
        }
    }

    // Prepare Orbiting Part A (task.partA) - this will be reflected and become the second operand
    if (!task.partA.outer.empty()) {
        PolygonPath reflected_outerA;
        reflected_outerA.reserve(task.partA.outer.size());
        // Reflect A about its own reference point (assumed to be (0,0) in its local coordinates for reflection)
        for (const auto& p : task.partA.outer) {
            reflected_outerA.push_back({-p.x, -p.y});
        }
        std::vector<BoostPoint> outerA_refl_pts = toBoostPoints(reflected_outerA, inputscale);
        BoostPolygonWithHoles polyA_outer_refl;
        boost::polygon::set_points(polyA_outer_refl, outerA_refl_pts.begin(), outerA_refl_pts.end());
        boost_set_reflected_orbiting_partA += polyA_outer_refl;

        for (const auto& hole_path_orig : task.partA.holes) {
            if (!hole_path_orig.empty()) {
                PolygonPath reflected_holeA;
                reflected_holeA.reserve(hole_path_orig.size());
                for (const auto& p_hole : hole_path_orig) {
                     reflected_holeA.push_back({-p_hole.x, -p_hole.y});
                }
                std::vector<BoostPoint> holeA_refl_pts = toBoostPoints(reflected_holeA, inputscale);
                BoostPolygonWithHoles polyA_hole_refl;
                boost::polygon::set_points(polyA_hole_refl, holeA_refl_pts.begin(), holeA_refl_pts.end());
                boost_set_reflected_orbiting_partA -= polyA_hole_refl;
            }
        }
    }

    if (boost_set_static_partB.empty() || boost_set_reflected_orbiting_partA.empty()) {
        result_item.error_message = "One or both input polygon sets are empty after conversion to Boost types for NFP calculation.";
        result_item.success = true; // Not a fatal error for batch, but this pair yields no NFP.
        return result_item;
    }

    // 3. NFP Calculation: Static B (+) Reflected Orbiting A
    BoostPolygonSet boost_set_C_result;
    convolve_two_polygon_sets(boost_set_C_result, boost_set_static_partB, boost_set_reflected_orbiting_partA);

    // 4. Convert Result
    std::vector<BoostPolygonWithHoles> result_polys_with_holes;
    boost_set_C_result.get(result_polys_with_holes);

    for (const auto& poly_wh : result_polys_with_holes) {
        PolygonPath current_nfp_outer_path = fromBoostPathToPolygonPath(
            poly_wh.begin(), poly_wh.end(), inputscale, xshift, yshift);

        if (!current_nfp_outer_path.empty()) {
            result_item.nfp.push_back(current_nfp_outer_path);
        }
        // Note: Original code also extracted holes of the NFP.
        // Current `NfpResultPolygons` (list of PolygonPath) doesn't store NFP holes.
        // If NFP holes are needed, `NfpResultPolygons` type and this conversion step must be updated.
    }
    
    result_item.success = true; // Assume success if we got this far, even if nfp list is empty
    return result_item;
}


bool CalculateNfp_Batch_MultiThreaded(
    const std::vector<NfpTaskItem>& tasks,
    std::vector<NfpBatchResultItem>& results,
    double fixed_scale_for_boost_poly, // This parameter is now IGNORED due to dynamic scaling per task.
                                       // Kept for API compatibility if strictly needed, but ideally removed.
    int DANGER_requested_thread_count)
{
    if (tasks.empty()) {
        results.clear();
        return true;
    }

    results.resize(tasks.size()); // Pre-size results vector

    int num_threads = DANGER_requested_thread_count;
    if (num_threads <= 0) {
        num_threads = boost::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 2; // Default to 2 threads if detection fails
    }

    thread_pool pool(num_threads);

    for (size_t i = 0; i < tasks.size(); ++i) {
        // Need to ensure task object and results vector access are managed correctly.
        // We pass a copy of the task item to the lambda.
        // The lambda writes to a specific index in 'results'.
        // This assumes 'results' vector outlives the threads or results are copied before 'results' is destroyed.
        // And that concurrent writes to different indices of a std::vector are safe (they are).

        // Create a lambda or bind a function to be executed by the thread pool.
        // The task for the thread pool needs to capture necessary data by value or ensure lifetime.
        // The ProcessSingleNfpTask function will be wrapped.

        // IMPORTANT: The 'tasks' and 'results' vectors must remain valid for the duration of threads.
        // If CalculateNfp_Batch_MultiThreaded is a blocking call (waits for all threads), this is fine.

        // The task for the thread pool
        auto task_lambda = [i, &tasks, &results]() {
            results[i] = ProcessSingleNfpTask(tasks[i]);
        };
        pool.run_task(task_lambda);
    }

    pool.wait_for_completion(); // Wait for all tasks to finish

    return true; // Indicate batch processing was started and completed. Individual success in items.
}

} // namespace CustomMinkowski
