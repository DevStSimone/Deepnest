#include "nfpGenerator.h"
#include "Clipper2/clipper.h"
#include "minkowski_wrapper.h" // Added for CustomMinkowski
#include <QDebug>
#include <algorithm> 

namespace Geometry {

NfpGenerator::NfpGenerator(double clipperScale) : scale_(clipperScale) {
    if (scale_ <= 0) {
        qWarning() << "NfpGenerator: Clipper scale factor must be positive. Using default 1000000.0.";
        scale_ = 1000000.0; // A common large scale factor for integer geometry
    }
}

NfpGenerator::~NfpGenerator() {}

// Helper function to convert Core::InternalPart to CustomMinkowski::PolygonWithHoles
CustomMinkowski::PolygonWithHoles internalPartToMinkowskiPolygon(const Core::InternalPart& part, double scale) {
    CustomMinkowski::PolygonWithHoles mPoly;
    // Outer boundary
    if (!part.outerBoundary.isEmpty()) {
        mPoly.outer.reserve(part.outerBoundary.size());
        for (const QPointF& pt : part.outerBoundary) {
            mPoly.outer.push_back({pt.x() /* * scale -> scaling done by CustomMinkowski::CalculateNfp */, 
                                   pt.y() /* * scale */});
        }
        // Note: Orientation for Boost.Polygon is typically CCW for outer, CW for holes.
        // Assuming InternalPart already provides this, or CalculateNfp handles it.
        // The original minkowski.cc does not seem to enforce/change orientation before Boost.
    }

    // Holes
    for (const QPolygonF& holeQPoly : part.holes) {
        if (!holeQPoly.isEmpty()) {
            CustomMinkowski::PolygonPath mHole;
            mHole.reserve(holeQPoly.size());
            for (const QPointF& pt : holeQPoly) {
                mHole.push_back({pt.x() /* * scale */, 
                                 pt.y() /* * scale */});
            }
            mPoly.holes.push_back(mHole);
        }
    }
    return mPoly;
}

// Helper function to convert CustomMinkowski::NfpResultPolygons to QList<QPolygonF>
// This version is kept if direct conversion to QPolygonF is needed elsewhere.
QList<QPolygonF> nfpResultPolygonsToQPolygonFs(const CustomMinkowski::NfpResultPolygons& resultPaths) {
    QList<QPolygonF> qPolygons;
    for (const CustomMinkowski::PolygonPath& mPath : resultPaths) {
        QPolygonF qPoly;
        qPoly.reserve(mPath.size());
        for (const CustomMinkowski::Point& pt : mPath) {
            qPoly.append(QPointF(pt.x, pt.y));
        }
        if (!qPoly.isEmpty()) {
            qPolygons.append(qPoly);
        }
    }
    return qPolygons;
}

// Static helper for NfpGenerator (Custom Minkowski input)
CustomMinkowski::PolygonWithHoles NfpGenerator::internalPartToMinkowskiPolygon(const Core::InternalPart& internalPart) {
    CustomMinkowski::PolygonWithHoles mPoly;

    // Outer boundary
    if (!internalPart.outerBoundary.isEmpty()) {
        mPoly.outer.reserve(internalPart.outerBoundary.size());
        for (const QPointF& pt : internalPart.outerBoundary) {
            mPoly.outer.push_back({pt.x(), pt.y()});
        }
    }

    // Holes
    for (const QPolygonF& holeQPoly : internalPart.holes) {
        if (!holeQPoly.isEmpty()) {
            CustomMinkowski::PolygonPath mHole;
            mHole.reserve(holeQPoly.size());
            for (const QPointF& pt : holeQPoly) {
                mHole.push_back({pt.x(), pt.y()});
            }
            mPoly.holes.push_back(mHole);
        }
    }
    return mPoly;
}

// Helper to convert QPolygonF to Clipper2Lib::PathD (no reflection)
Clipper2Lib::PathD NfpGenerator::qPolygonFToPathD(const QPolygonF& polygon) const {
    Clipper2Lib::PathD path;
    path.reserve(polygon.size());
    for (const QPointF& pt : polygon) {
        path.push_back(Clipper2Lib::PointD(pt.x(), pt.y()));
    }
    return path;
}

// Helper to convert QPolygonF to Clipper2Lib::PathD AND reflect
static Clipper2Lib::PathD qPolygonFToPathDReflected(const QPolygonF& polygon) {
    Clipper2Lib::PathD path;
    path.reserve(polygon.size());
    // Iterate in reverse for reflection to help maintain winding sense, though Clipper handles it
    for (int i = polygon.size() - 1; i >= 0; --i) {
        const QPointF& pt = polygon.at(i);
        path.push_back(Clipper2Lib::PointD(-pt.x(), -pt.y()));
    }
    return path;
}

// qPolygonFsToPathsD remains useful for collections if needed elsewhere.
Clipper2Lib::PathsD NfpGenerator::qPolygonFsToPathsD(const QList<QPolygonF>& polygons) const {
    Clipper2Lib::PathsD allPaths;
    for(const QPolygonF& polygon : polygons) {
        Clipper2Lib::PathD singlePath;
        for (const QPointF& pt : polygon) {
            singlePath.push_back(Clipper2Lib::PointD(pt.x(), pt.y()));
        }
        if (!singlePath.empty()) {
             allPaths.push_back(singlePath);
        }
    }
    return allPaths;
}


QList<QPolygonF> NfpGenerator::pathsDToQPolygonFs(const Clipper2Lib::PathsD& paths) const {
    QList<QPolygonF> qPolygons;
    for (const Clipper2Lib::PathD& path : paths) {
        QPolygonF qPolygon;
        for (const Clipper2Lib::PointD& pt : path) {
            qPolygon.append(QPointF(pt.x, pt.y));
        }
        if (!qPolygon.isEmpty()) {
             qPolygons.append(qPolygon);
        }
    }
    return qPolygons;
}

Core::InternalPart reflectPartAroundOrigin(const Core::InternalPart& part) {
    Core::InternalPart reflectedPart;
    reflectedPart.id = part.id + "_reflected";
    QPolygonF reflectedOuter;
    for(const QPointF& p : part.outerBoundary) {
        reflectedOuter.append(QPointF(-p.x(), -p.y()));
    }
    if(!reflectedOuter.isEmpty()) std::reverse(reflectedOuter.begin(), reflectedOuter.end());
    reflectedPart.outerBoundary = reflectedOuter;

    for(const QPolygonF& hole : part.holes) {
        QPolygonF reflectedHole;
        for(const QPointF& p : hole) {
            reflectedHole.append(QPointF(-p.x(), -p.y()));
        }
        if(!reflectedHole.isEmpty()) std::reverse(reflectedHole.begin(), reflectedHole.end());
        reflectedPart.holes.append(reflectedHole);
    }
    if(!reflectedPart.outerBoundary.isEmpty()) reflectedPart.bounds = reflectedPart.outerBoundary.boundingRect();
    return reflectedPart;
}


QList<QPolygonF> NfpGenerator::minkowskiNfp(const Core::InternalPart& partA_orbiting, const Core::InternalPart& partB_static) {
    if (!partA_orbiting.isValid() || !partB_static.isValid()) {
        qWarning() << "NfpGenerator::minkowskiNfp: Invalid input parts.";
        return QList<QPolygonF>();
    }
    
    if (!partA_orbiting.isValid() || !partB_static.isValid() || partA_orbiting.outerBoundary.isEmpty() || partB_static.outerBoundary.isEmpty()) {
        qWarning() << "NfpGenerator::minkowskiNfp: Invalid or empty outer boundary for input parts. Part A ID:" << partA_orbiting.id << "Part B ID:" << partB_static.id;
        return QList<QPolygonF>();
    }

    // NFP of A orbiting B = B_static (+) reflect(A_orbiting)
    Clipper2Lib::PathD Bo_path = qPolygonFToPathD(partB_static.outerBoundary);
    Clipper2Lib::PathD reflected_Ao_path = qPolygonFToPathDReflected(partA_orbiting.outerBoundary);

    // 1. Primary NFP from outer boundaries: NFP_primary = MinkowskiSum(B_o, reflect(A_o))
    Clipper2Lib::PathsD current_nfp_paths = Clipper2Lib::MinkowskiSum(Bo_path, reflected_Ao_path, true, 2);

    // 2. Effect of A's Holes (carving out from NFP_primary)
    // NFP_A_hole_effect_i = MinkowskiSum(B_o, reflect(A_hi))
    // NFP_after_A_holes = Difference(NFP_primary, Union(all NFP_A_hole_effect_i))
    if (!partA_orbiting.holes.isEmpty()) {
        Clipper2Lib::PathsD all_A_hole_effects_unioned;
        Clipper2Lib::ClipperD clipper_union_A_holes_effects(2); // Precision for union

        for (const QPolygonF& Ah_qpoly : partA_orbiting.holes) {
            if (Ah_qpoly.isEmpty()) continue;
            Clipper2Lib::PathD reflected_Ah_path = qPolygonFToPathDReflected(Ah_qpoly);
            Clipper2Lib::PathsD nfp_A_hole_effect_i = Clipper2Lib::MinkowskiSum(Bo_path, reflected_Ah_path, true, 2);
            clipper_union_A_holes_effects.AddSubject(nfp_A_hole_effect_i);
        }

        if (clipper_union_A_holes_effects.SubjectPathCount() > 0) {
             clipper_union_A_holes_effects.Execute(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::Positive, all_A_hole_effects_unioned);
        }

        if (!all_A_hole_effects_unioned.empty() && !current_nfp_paths.empty()) {
            Clipper2Lib::ClipperD clipper_diff_A_holes(2);
            clipper_diff_A_holes.AddSubject(current_nfp_paths);
            clipper_diff_A_holes.AddClip(all_A_hole_effects_unioned);
            clipper_diff_A_holes.Execute(Clipper2Lib::ClipType::Difference, Clipper2Lib::FillRule::Positive, current_nfp_paths);
        }
    }

    // 3. Effect of B's Holes (adding to the NFP)
    // NFP_B_hole_effect_j = MinkowskiSum(B_hj, reflect(A_o))
    // NFP_final = Union(NFP_after_A_holes, Union(all NFP_B_hole_effect_j))
    if (!partB_static.holes.isEmpty()) {
        Clipper2Lib::PathsD all_B_hole_effects_unioned;
        Clipper2Lib::ClipperD clipper_union_B_holes_effects(2);

        for (const QPolygonF& Bh_qpoly : partB_static.holes) {
            if (Bh_qpoly.isEmpty()) continue;
            Clipper2Lib::PathD Bh_path = qPolygonFToPathD(Bh_qpoly);
            Clipper2Lib::PathsD nfp_B_hole_effect_j = Clipper2Lib::MinkowskiSum(Bh_path, reflected_Ao_path, true, 2);
            clipper_union_B_holes_effects.AddSubject(nfp_B_hole_effect_j);
        }

        if (clipper_union_B_holes_effects.SubjectPathCount() > 0) {
            clipper_union_B_holes_effects.Execute(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::Positive, all_B_hole_effects_unioned);
        }

        if (!all_B_hole_effects_unioned.empty()) {
            if (current_nfp_paths.empty()) {
                 current_nfp_paths = all_B_hole_effects_unioned;
            } else {
                Clipper2Lib::ClipperD clipper_sum_B_holes(2);
                clipper_sum_B_holes.AddSubject(current_nfp_paths);
                clipper_sum_B_holes.AddClip(all_B_hole_effects_unioned); // Add as clip for union
                clipper_sum_B_holes.Execute(Clipper2Lib::ClipType::Union, Clipper2Lib::FillRule::Positive, current_nfp_paths);
            }
        }
    }

    return pathsDToQPolygonFs(current_nfp_paths);
}

QList<QPolygonF> NfpGenerator::minkowskiNfpInside(const Core::InternalPart& partA_fitting, const Core::InternalPart& partB_container) {
    // Full hole handling for "NFP Inside" is complex:
    // Outer NFP: B_outer (-) A_outer
    // For each hole H_b in B: NFP_H_b = H_b (+) reflect(A_outer) (area where A must be if its ref point is in H_b to avoid collision with H_b edge)
    // For each hole H_a in A: NFP_H_a = B_outer (-) H_a (area where A's ref point can be so H_a is inside B_outer)
    // Final = OuterNFP intersected with all NFP_H_a, and then unioned/differenced with NFP_H_b aspects.
    // This simplified version only handles outer boundaries.
    qWarning() << "NfpGenerator::minkowskiNfpInside (Clipper2) is using a simplified version (outer boundaries only) and does not fully support holes for 'inside' NFP context.";

    if (!partA_fitting.isValid() || !partB_container.isValid() || partA_fitting.outerBoundary.isEmpty() || partB_container.outerBoundary.isEmpty()) {
        qWarning() << "NfpGenerator::minkowskiNfpInside: Invalid or empty outer boundary for input parts. Part A ID:" << partA_fitting.id << "Part B ID:" << partB_container.id;
        return QList<QPolygonF>();
    }
    
    Clipper2Lib::PathD pathA_outer = qPolygonFToPathD(partA_fitting.outerBoundary);
    Clipper2Lib::PathD pathB_outer = qPolygonFToPathD(partB_container.outerBoundary);

    // NFP_inside(A fits in B) = B_outer (-) A_outer (Minkowski Difference)
    Clipper2Lib::PathsD nfpPaths = Clipper2Lib::MinkowskiDiff(pathB_outer, pathA_outer, true, 2); // isClosed=true, decimalPlaces=2
    // qDebug() << "Clipper2 MinkowskiDiff for NFP(A inside B) produced" << nfpPaths.size() << "paths for part " << partA_fitting.id << "inside" << partB_container.id; // Debug
    return pathsDToQPolygonFs(nfpPaths);
}


QList<QPolygonF> NfpGenerator::originalModuleNfp(const Core::InternalPart& partA_orbiting,
                                                     const Core::InternalPart& partB_static,
                                                     bool isInside, 
                                                     bool useThreads) {
    if (useThreads) {
        qWarning() << "NfpGenerator::originalModuleNfp: Multi-threaded version of custom Minkowski module is not available/integrated. Falling back to single-threaded if possible, or aborting custom module use.";
        // Depending on final design, could fallback to Clipper2 or error.
        // For now, if multi-thread custom is requested, we indicate it's not supported.
        // The actual single-threaded call below will proceed if useThreads was the *only* issue.
    }
    if (isInside) {
        qWarning() << "NfpGenerator::originalModuleNfp: 'isInside' NFP calculation is not supported by the current single-threaded CustomMinkowski::CalculateNfp wrapper. This call will compute A-around-B NFP.";
        // To correctly handle 'isInside' with the original module's logic, the wrapper might need
        // an 'isInside' flag, or a separate wrapped function if the core logic differs significantly.
        // Current CustomMinkowski::CalculateNfp is for A-around-B.
        // For now, we'll proceed, but the result will be for A-around-B, not A-inside-B.
    }

    qDebug() << "Using CustomMinkowski::CalculateNfp (refactored from original minkowski.cc)";

    // Convert InternalParts to CustomMinkowski::PolygonWithHoles.
    // The points within PolygonWithHoles are expected as doubles, scaling happens inside CalculateNfp.
    CustomMinkowski::PolygonWithHoles mPartA = internalPartToMinkowskiPolygon(partA_orbiting, 1.0 /* scale applied inside wrapper */);
    CustomMinkowski::PolygonWithHoles mPartB = internalPartToMinkowskiPolygon(partB_static, 1.0 /* scale applied inside wrapper */);
    
    CustomMinkowski::NfpResultPolygons mResult;
    // The fixed_scale_for_boost_poly was crucial but is now handled by dynamic scaling within CalculateNfp.
    // The this->scale_ (config_.clipperScale) is no longer passed as it was likely inappropriate.
    bool success = CustomMinkowski::CalculateNfp(mPartA, mPartB, mResult);

    if (!success) {
        qWarning() << "NfpGenerator::originalModuleNfp: CustomMinkowski::CalculateNfp reported failure or produced no NFP.";
        return QList<QPolygonF>();
    }
    
    qDebug() << "CustomMinkowski::CalculateNfp returned" << mResult.size() << "NFP paths.";
    // The minkowskiResultToQPolygonFs function expects results to be unscaled by the wrapper.
    return minkowskiResultToQPolygonFs(mResult, this->scale_ /* not used if wrapper unscales */);
}


QList<QPolygonF> NfpGenerator::calculateNfp(
    const Core::InternalPart& partA, 
    const Core::InternalPart& partB,
    bool useOriginalDeepNestModule,
    bool allowOriginalModuleMultithreading
) {
    if (useOriginalDeepNestModule) {
        // qDebug() << "NfpGenerator: Route to originalModuleNfp for NFP (A around B)."; // Debug
        return originalModuleNfp(partA, partB, false /*isInside=false*/, allowOriginalModuleMultithreading);
    } else {
        qDebug() << "NfpGenerator: Route to minkowskiNfp (Clipper2) for NFP (A around B).";
        return minkowskiNfp(partA, partB);
    }
}

QList<QPolygonF> NfpGenerator::calculateNfpInside(
    const Core::InternalPart& partA_fitting,
    const Core::InternalPart& partB_container,
    bool useOriginalDeepNestModule,
    bool allowOriginalModuleMultithreading
) {
    if (useOriginalDeepNestModule) {
        // qWarning() << "NfpGenerator: Route to originalModuleNfp for NFP (A inside B). 'isInside' specific logic might not be fully supported by current custom wrapper."; // Debug
        // The current originalModuleNfp will warn that 'isInside' is not truly handled.
        return originalModuleNfp(partA_fitting, partB_container, true /*isInside=true*/, allowOriginalModuleMultithreading);
    } else {
        // qDebug() << "NfpGenerator: Route to minkowskiNfpInside (Clipper2) for NFP (A inside B)."; // Debug
        return minkowskiNfpInside(partA_fitting, partB_container);
    }
}


QList<CustomMinkowski::NfpResultPolygons> NfpGenerator::generateNfpBatch_OriginalModule(
    const QList<QPair<Core::InternalPart, Core::InternalPart>>& partPairs,
    int threadCount
) {
    std::vector<CustomMinkowski::NfpTaskItem> tasks;
    tasks.reserve(partPairs.size());
    int taskIdCounter = 0;

    for (const auto& pair : partPairs) {
        CustomMinkowski::NfpTaskItem task;
        task.partA = internalPartToMinkowskiPolygon(pair.first);  // Orbiting part
        task.partB = internalPartToMinkowskiPolygon(pair.second); // Static part
        task.taskId = taskIdCounter++;
        tasks.push_back(task);
    }

    std::vector<CustomMinkowski::NfpBatchResultItem> batchResults;
    // The fixed_scale parameter is removed from CalculateNfp_Batch_MultiThreaded, so not passed here.
    bool batchCallSuccess = CustomMinkowski::CalculateNfp_Batch_MultiThreaded(tasks, batchResults, threadCount);

    QList<CustomMinkowski::NfpResultPolygons> finalResults;
    finalResults.reserve(batchResults.size());

    if (!batchCallSuccess) {
        qWarning() << "NfpGenerator::generateNfpBatch_OriginalModule: CalculateNfp_Batch_MultiThreaded call failed.";
        // Return list of empty results or handle error appropriately
        for (int i = 0; i < partPairs.size(); ++i) {
            finalResults.append(CustomMinkowski::NfpResultPolygons()); // Append empty NFP result
        }
        return finalResults;
    }

    // Sort results by taskId to ensure original order, though CalculateNfp_Batch_MultiThreaded
    // currently pre-sizes and writes to indices, so order should be maintained.
    // However, if it were to change, sorting is safer. For now, assume order is preserved.
    // std::sort(batchResults.begin(), batchResults.end(), [](const auto&a, const auto&b){ return a.taskId < b.taskId; });

    for (const auto& item : batchResults) {
        if (!item.success) {
            qWarning() << "NfpGenerator::generateNfpBatch_OriginalModule: Task ID" << item.taskId << "failed:" << QString::fromStdString(item.error_message);
            finalResults.append(CustomMinkowski::NfpResultPolygons()); // Append empty for failed task
        } else {
            finalResults.append(item.nfp);
            // Caching would happen here if this function were responsible for it.
            // Example:
            // if (item.success && pairIndex < partPairs.size()) { // Ensure index is valid
            //    const Core::InternalPart& partA = partPairs[item.taskId].first; // Assuming taskId matches original index
            //    const Core::InternalPart& partB = partPairs[item.taskId].second;
            //    // Need rotations and flips to generate a full cache key.
            //    // These are not available in InternalPart directly unless it's transformed.
            //    // QString cacheKey = nfpCache_.generateKey(partA.id, rotA, flipA, partB.id, rotB, flipB, false);
            //    // nfpCache_.storeNfp(cacheKey, Geometry::CachedNfp(nfpResultPolygonsToQPolygonFs(item.nfp)));
            // }
        }
    }
    return finalResults;
}

} // namespace Geometry
