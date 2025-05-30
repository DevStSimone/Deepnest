#ifndef MINKOWSKI_THREAD_WRAPPER_H
#define MINKOWSKI_THREAD_WRAPPER_H

#include "minkowski_wrapper.h" // For Point, PolygonPath, PolygonWithHoles, NfpResultPolygons
#include <vector>
#include <string> // For part IDs

namespace CustomMinkowski {

struct NfpTaskItem {
    PolygonWithHoles partA; // The orbiting/moving part
    PolygonWithHoles partB; // The static part
    int taskId;             // To identify the result
    // Add any other per-pair parameters if needed, e.g., specific scaling or flags
    // For now, assume global scaling will be handled by the main batch function.
};

struct NfpBatchResultItem {
    int taskId;
    NfpResultPolygons nfp; // The resulting NFP (list of polygons)
    bool success;
    std::string error_message; // Optional error message

    NfpBatchResultItem(int id = -1, bool succ = false) : taskId(id), success(succ) {}
};

// Calculates NFPs for a batch of tasks using multiple threads.
// Parameters:
// - tasks: A vector of NfpTaskItem, each defining a pair of polygons for NFP.
// - results: Output vector to store the NFP result for each corresponding task.
// - fixed_scale_for_boost_poly: The single scale factor to be used for all Boost.Polygon operations.
// - DANGER_thread_count: Number of threads to use. 0 for default (e.g. hardware_concurrency).
//                          The "DANGER_" prefix is because the original thread pool was global and might not be
//                          safe to reconfigure per call easily. A new pool per call is safer.
// Returns true if the batch processing was initiated successfully, false otherwise.
// The actual success of each NFP is in NfpBatchResultItem.success.
bool CalculateNfp_Batch_MultiThreaded(
    const std::vector<NfpTaskItem>& tasks,
    std::vector<NfpBatchResultItem>& results,
    double fixed_scale_for_boost_poly,
    int DANGER_requested_thread_count 
);

} // namespace CustomMinkowski
#endif // MINKOWSKI_THREAD_WRAPPER_H
