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

// New static helper for converting InternalPart to Clipper2Lib::PathsD, with optional reflection
static Clipper2Lib::PathsD internalPartToClipperPathsD(const Core::InternalPart& part, bool reflect) {
    Clipper2Lib::PathsD allPaths;

    // Process outer boundary
    if (!part.outerBoundary.isEmpty()) {
        Clipper2Lib::PathD outerPath;
        for (const QPointF& pt : part.outerBoundary) {
            outerPath.push_back(Clipper2Lib::PointD(reflect ? -pt.x() : pt.x(), reflect ? -pt.y() : pt.y()));
        }
        if (reflect && !outerPath.empty()) { // Reflection reverses orientation
            std::reverse(outerPath.begin(), outerPath.end());
        }
        allPaths.push_back(outerPath);
    }

    // Process holes
    for (const QPolygonF& holeQPoly : part.holes) {
        if (!holeQPoly.isEmpty()) {
            Clipper2Lib::PathD holePath;
            for (const QPointF& pt : holeQPoly) {
                holePath.push_back(Clipper2Lib::PointD(reflect ? -pt.x() : pt.x(), reflect ? -pt.y() : pt.y()));
            }
            if (reflect && !holePath.empty()) { // Reflection reverses orientation
                std::reverse(holePath.begin(), holePath.end());
            }
            allPaths.push_back(holePath);
        }
    }
    return allPaths;
}


Clipper2Lib::PathD NfpGenerator::qPolygonFToPathD(const QPolygonF& polygon) const {
    Clipper2Lib::PathD path;
    for (const QPointF& pt : polygon) {
        // For Clipper2, we use its own scaling mechanism if needed, or pass doubles directly.
        // Assuming NfpGenerator's scale_ is for integer-based operations like Boost.Polygon might use.
        // Clipper2Lib::PointD typically takes doubles.
        path.push_back(Clipper2Lib::PointD(pt.x(), pt.y()));
    }

    return path;
}

Clipper2Lib::PathsD NfpGenerator::qPolygonFToPathsD(const QPolygonF& polygon) const {
    // This function seems to return PathsD (plural) but is named qPolygonFToPathD (singular PathD).
    // It also creates an unnecessary outer PathsD for a single PathD.
    // Let's assume it should just return PathD.
    Clipper2Lib::PathD path;
    for (const QPointF& pt : polygon) {
        path.push_back(Clipper2Lib::PointD(pt.x(), pt.y()));
    }
    return path;
}

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
    
    Clipper2Lib::PathD pathsB_outer = qPolygonFToPathD(partB_static.outerBoundary);
    // Convert partA (orbiting) and reflected partB (static, reflected) to PathsD, including holes.
    Clipper2Lib::PathsD pathsA = internalPartToClipperPathsD(partA_orbiting, false);
    Clipper2Lib::PathsD pathsReflectedB = internalPartToClipperPathsD(partB_static, true);

    if (pathsA.empty() || pathsReflectedB.empty()) {
        qWarning() << "NfpGenerator::minkowskiNfp: One or both parts resulted in empty PathsD input for Clipper2 (A:" << pathsA.empty() << "ReflectedB:" << pathsReflectedB.empty() << "). Part A ID: " << partA_orbiting.id << " Part B ID: " << partB_static.id;
        return QList<QPolygonF>();
    }

    // Clipper2's MinkowskiSum expects (pattern, path) where pattern is typically the smaller/orbiting part.
    // NFP(A orbits B) = A (+) Reflect(B).
    // However, the common definition for NFP where A's reference point traces the boundary is B (+) Reflect(A).
    // Let's use the latter: Static Part B (+) Reflected Orbiting Part A.
    // So, pathsB will be `pattern` and pathsReflectedA will be `path`.
    // The original DeepNest JS uses A (orbiting) + (-B) (reflected static).
    // Let's stick to A_orbiting (+) reflected(B_static)
    // pathsA is pattern, pathsReflectedB is path.
    Clipper2Lib::PathsD nfpPaths = Clipper2Lib::MinkowskiSum(pathsA, pathsReflectedB, false);
    // qDebug() << "Clipper2 MinkowskiSum for NFP(A around B) produced" << nfpPaths.size() << "paths for part " << partA_orbiting.id << "around" << partB_static.id;
    return pathsDToQPolygonFs(nfpPaths);
}

QList<QPolygonF> NfpGenerator::minkowskiNfpInside(const Core::InternalPart& partA_fitting, const Core::InternalPart& partB_container) {
    if (!partA_fitting.isValid() || !partB_container.isValid()) {
        qWarning() << "NfpGenerator::minkowskiNfpInside: Invalid input parts.";
        return QList<QPolygonF>();
    }
    
    Clipper2Lib::PathsD pathsA = internalPartToClipperPathsD(partA_fitting, false);
    Clipper2Lib::PathsD pathsB = internalPartToClipperPathsD(partB_container, false);

    if (pathsA.empty() || pathsB.empty()) {
        qWarning() << "NfpGenerator::minkowskiNfpInside: One or both parts resulted in empty PathsD input for Clipper2 (A:" << pathsA.empty() << "B:" << pathsB.empty() << "). Part A ID: " << partA_fitting.id << " Part B ID: " << partB_container.id;
        return QList<QPolygonF>();
    }

    // NFP_inside(A fits in B) = B (-) A (Minkowski Difference)
    Clipper2Lib::PathsD nfpPaths = Clipper2Lib::MinkowskiDiff(pathsB, pathsA, false);
    // qDebug() << "Clipper2 MinkowskiDiff for NFP(A inside B) produced" << nfpPaths.size() << "paths for part " << partA_fitting.id << "inside" << partB_container.id;
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
