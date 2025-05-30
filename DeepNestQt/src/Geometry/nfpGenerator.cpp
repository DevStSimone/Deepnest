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
QList<QPolygonF> minkowskiResultToQPolygonFs(const CustomMinkowski::NfpResultPolygons& resultPaths, double scale_to_reverse_if_needed_by_wrapper) {
    QList<QPolygonF> qPolygons;
    // The scale_to_reverse_if_needed_by_wrapper is if the CustomMinkowski::CalculateNfp returns scaled coordinates.
    // Based on its current refactored signature, it takes scaled inputs and should return unscaled (double) outputs.
    // So, direct conversion. If it returned scaled ints, we'd divide by scale_to_reverse_if_needed_by_wrapper.
    // The current wrapper's CalculateNfp takes double points and a scale, and it's assumed it handles scaling internally
    // and the points in NfpResultPolygons are already in the original double coordinate space.
    // However, the provided `minkowski_wrapper.cpp` implementation for CalculateNfp *does* do the scaling/unscaling.
    // The points in NfpResultPolygons are already unscaled and shifted by xshift/yshift.

    for (const CustomMinkowski::PolygonPath& mPath : resultPaths) {
        QPolygonF qPoly;
        qPoly.reserve(mPath.size());
        for (const CustomMinkowski::Point& pt : mPath) {
            qPoly.append(QPointF(pt.x , pt.y )); // Direct use, as they should be unscaled by wrapper
        }
        if (!qPoly.isEmpty()) {
            qPolygons.append(qPoly);
        }
    }
    return qPolygons;
}

Clipper2Lib::PathD NfpGenerator::qPolygonFToPathD(const QPolygonF& polygon) const {
    Clipper2Lib::PathsD paths;
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
    Clipper2Lib::PathsD paths;
    Clipper2Lib::PathD path;
    for (const QPointF& pt : polygon) {
        // For Clipper2, we use its own scaling mechanism if needed, or pass doubles directly.
        // Assuming NfpGenerator's scale_ is for integer-based operations like Boost.Polygon might use.
        // Clipper2Lib::PointD typically takes doubles.
        path.push_back(Clipper2Lib::PointD(pt.x(), pt.y()));
    }
    if (!path.empty()) {
        paths.push_back(path);
    }
    return paths;
}

Clipper2Lib::PathsD NfpGenerator::qPolygonFsToPathsD(const QList<QPolygonF>& polygons) const {
    Clipper2Lib::PathsD paths;
    for(const QPolygonF& polygon : polygons) {
        Clipper2Lib::PathD path;
        for (const QPointF& pt : polygon) {
            path.push_back(Clipper2Lib::PointD(pt.x(), pt.y()));
        }
        if (!path.empty()) {
             paths.push_back(path);
        }
    }
    return paths;
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
    Core::InternalPart reflectedA = reflectPartAroundOrigin(partA_orbiting);
    Clipper2Lib::PathD pathsReflectedA_outer = qPolygonFToPathD(reflectedA.outerBoundary);

    // TODO: Properly handle holes for Minkowski sum with Clipper2.
    // This typically involves treating the part as (Outer - Holes).
    // For NFP(A,B) = B (+) reflect(A):
    // B_shape = B_outer - Union(B_holes)
    // ReflectedA_shape = ReflectedA_outer - Union(ReflectedA_holes)
    // NFP = MinkowskiSum(B_shape, ReflectedA_shape)
    // Clipper2's MinkowskiSum works on Paths. If a part is (Outer - Holes), it should be represented as such.
    // For now, using outer boundaries only is a simplification.

    Clipper2Lib::PathsD nfpPaths = Clipper2Lib::MinkowskiSum(pathsB_outer, pathsReflectedA_outer, false); 
    //qDebug() << "Clipper2 MinkowskiSum for NFP(A around B) produced" << nfpPaths.size() << "paths.";
    return pathsDToQPolygonFs(nfpPaths);
}

QList<QPolygonF> NfpGenerator::minkowskiNfpInside(const Core::InternalPart& partA_fitting, const Core::InternalPart& partB_container) {
     if (!partA_fitting.isValid() || !partB_container.isValid()) {
        qWarning() << "NfpGenerator::minkowskiNfpInside: Invalid input parts.";
        return QList<QPolygonF>();
    }
    
    // NFP_inside(A, B) = B_outer (-) A_outer (Minkowski Difference)
    // This is for A's reference point. A is NOT reflected for this definition.
    Clipper2Lib::PathD pathsB_outer = qPolygonFToPathD(partB_container.outerBoundary);
    Clipper2Lib::PathD pathsA_outer = qPolygonFToPathD(partA_fitting.outerBoundary); // partA is not reflected

    // TODO: Properly handle holes for Inner NFP.
    // Inner NFP = (B_outer (-) A_outer) intersected with (For each hole H_b in B: H_b (+) Reflect(A_outer))
    // This is complex. The current is a simplification.

    Clipper2Lib::PathsD nfpPaths = Clipper2Lib::MinkowskiDiff(pathsB_outer, pathsA_outer, false);
    //qDebug() << "Clipper2 MinkowskiDiff for NFP(A inside B) produced" << nfpPaths.size() << "paths.";
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
    // The fixed_scale_for_boost_poly is crucial. The original minkowski.cc calculated this dynamically.
    // The refactored wrapper now takes it as an argument.
    // We use this->scale_ which is config_.clipperScale. This might be very different from the dynamic
    // scale used by the original module. This is a potential point of incompatibility or precision issues.
    bool success = CustomMinkowski::CalculateNfp(mPartA, mPartB, mResult, this->scale_);

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
        qDebug() << "NfpGenerator: Route to originalModuleNfp for NFP (A around B).";
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
        qWarning() << "NfpGenerator: Route to originalModuleNfp for NFP (A inside B). 'isInside' specific logic might not be fully supported by current custom wrapper.";
        // The current originalModuleNfp will warn that 'isInside' is not truly handled.
        return originalModuleNfp(partA_fitting, partB_container, true /*isInside=true*/, allowOriginalModuleMultithreading);
    } else {
        qDebug() << "NfpGenerator: Route to minkowskiNfpInside (Clipper2) for NFP (A inside B).";
        return minkowskiNfpInside(partA_fitting, partB_container);
    }
}

} // namespace Geometry
