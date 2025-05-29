#include "nfpGenerator.h"
#include "Clipper2/clipper.h" // Ensure this is the correct include for Clipper2 operations like MinkowskiSum
#include <QDebug>
#include <algorithm> // Required for std::reverse
#include <internalTypes.h>

namespace Geometry {

NfpGenerator::NfpGenerator(double clipperScale) : scale_(clipperScale) {
    if (scale_ <= 0) {
        qWarning() << "Clipper scale factor must be positive. Using default 1.0.";
        scale_ = 1.0; // Or some other sensible default or throw error
    }
}

NfpGenerator::~NfpGenerator() {}

Clipper2Lib::PathsD NfpGenerator::qPolygonFToPathsD(const QPolygonF& polygon) const {
    Clipper2Lib::PathsD paths;
    Clipper2Lib::PathD path;
    for (const QPointF& pt : polygon) {
        path.push_back(Clipper2Lib::PointD(pt.x() * scale_, pt.y() * scale_));
    }
    if (!path.empty()) {
        // Clipper2 often expects open paths for Minkowski sums, but QPolygonF might be closed.
        // Depending on Clipper2's exact requirement, ensure path is not explicitly closed if not needed.
        // QPolygonF iterators skip the duplicate closing point if it's closed.
        paths.push_back(path);
    }
    return paths;
}

Clipper2Lib::PathD NfpGenerator::qPolygonFToPathD(const QPolygonF& polygon) const {
    //Clipper2Lib::PathsD paths;
    Clipper2Lib::PathD path;
    for (const QPointF& pt : polygon) {
        path.push_back(Clipper2Lib::PointD(pt.x() * scale_, pt.y() * scale_));
    }
    // Clipper2 often expects open paths for Minkowski sums, but QPolygonF might be closed.
    // Depending on Clipper2's exact requirement, ensure path is not explicitly closed if not needed.
    // QPolygonF iterators skip the duplicate closing point if it's closed.
    //    paths.push_back(path);

    return path;
}

Clipper2Lib::PathsD NfpGenerator::qPolygonFsToPathsD(const QList<QPolygonF>& polygons) const {
    Clipper2Lib::PathsD paths;
    for(const QPolygonF& polygon : polygons) {
        Clipper2Lib::PathD path;
        for (const QPointF& pt : polygon) {
            path.push_back(Clipper2Lib::PointD(pt.x() * scale_, pt.y() * scale_));
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
            qPolygon.append(QPointF(pt.x / scale_, pt.y / scale_));
        }
        if (!qPolygon.isEmpty()) {
             // Ensure closure if it was originally closed, or based on Clipper2 output conventions
             // if (qPolygon.first() == qPolygon.last()) qPolygon.removeLast(); // If Clipper returns explicitly closed
             qPolygons.append(qPolygon);
        }
    }
    return qPolygons;
}

// Reflects a polygon's points around the origin (0,0)
// Assumes partA's reference point for placement is its origin.
Core::InternalPart reflectPartAroundOrigin(const Core::InternalPart& part) {
    Core::InternalPart reflectedPart;
    reflectedPart.id = part.id + "_reflected";

    QPolygonF reflectedOuter;
    for(const QPointF& p : part.outerBoundary) {
        reflectedOuter.append(QPointF(-p.x(), -p.y()));
    }
    // Path orientation might flip, Clipper handles this.
    std::reverse(reflectedOuter.begin(), reflectedOuter.end()); // Preserve winding order for outer path
    reflectedPart.outerBoundary = reflectedOuter;

    for(const QPolygonF& hole : part.holes) {
        QPolygonF reflectedHole;
        for(const QPointF& p : hole) {
            reflectedHole.append(QPointF(-p.x(), -p.y()));
        }
        std::reverse(reflectedHole.begin(), reflectedHole.end()); // Preserve winding order for holes
        reflectedPart.holes.append(reflectedHole);
    }
    return reflectedPart;
}


QList<QPolygonF> NfpGenerator::minkowskiNfp(const Core::InternalPart& partA_orbiting, const Core::InternalPart& partB_static) {
    if (!partA_orbiting.isValid() || !partB_static.isValid()) {
        qWarning() << "NfpGenerator::minkowskiNfp: Invalid input parts.";
        return QList<QPolygonF>();
    }

    // NFP(A, B) = MinkowskiSum(B_boundary, Reflect(A_boundary, A_origin))
    // And union with MinkowskiSum(B_boundary, Reflect(A_hole_i, A_origin)) for each hole in A (these become like extensions of B)
    // And subtract MinkowskiSum(B_hole_j, Reflect(A_boundary, A_origin)) for each hole in B (these become inaccessible areas)
    // This is a complex definition. The common one for "A cannot hit B" is simpler:
    // NFP = MinkowskiSum( StaticPart_Boundary, Reflected_OrbitingPart_Boundary )
    // Holes complicate this. DeepNest JS uses a "no-fit-polygon" function that takes outer paths and holes.

    // Simplified: Minkowski sum of B's outer boundary with reflected A's outer boundary.
    // This forms the basic NFP. Holes need to be incorporated.
    // Clipper2's MinkowskiSum function takes Paths, so it can handle a shape and its holes.
    
    Clipper2Lib::PathD pathsB_outer = qPolygonFToPathD(partB_static.outerBoundary);
    // For NFP, we need to consider holes of B as "solid" parts of B's NFP contribution,
    // and holes of A as "empty" parts of A's NFP contribution.

    // Reflect partA around its origin (assuming (0,0) is its reference point for placement)
    Core::InternalPart reflectedA = reflectPartAroundOrigin(partA_orbiting);
    Clipper2Lib::PathD pathsReflectedA_outer = qPolygonFToPathD(reflectedA.outerBoundary);

    // Calculate MinkowskiSum(B_outer, ReflectedA_outer)
    Clipper2Lib::PathsD nfpPaths = Clipper2Lib::MinkowskiSum(pathsB_outer, pathsReflectedA_outer, false, 2); // false for open paths typically

    // This is a simplified NFP. A full NFP considering holes would be:
    // NFP_primary = MinkowskiSum(B_outer, ReflectedA_outer)
    // For each hole H_b in B: NFP_hole_contribution = MinkowskiSum(H_b, ReflectedA_outer)
    //   These are areas within B's original footprint that A cannot enter.
    //   The overall NFP might be NFP_primary - Union(NFP_hole_contributions related to B's holes)
    // For each hole H_a in A: ReflectedH_a.
    //   NFP_hole_A_contribution = MinkowskiSum(B_outer, ReflectedH_a)
    //   These describe where B can "pass through" A.
    //   The overall NFP is often Union(NFP_primary, NFP_hole_A_contributions)
    // The exact composition depends on DeepNest's specific NFP definition.
    // The @deepnest/calculate-nfp module handles this complexity.
    
    // For now, returning the NFP of outer boundaries as a starting point.
    // A robust Minkowski NFP needs to handle holes correctly, often by treating the part as a union of its solid areas.
    // Clipper2's boolean operations (Union, Difference) on PathsD would be used to combine these.
    // Example: Subject = B_outer U B_holes_inverted, Clip = A_outer_reflected U A_holes_reflected_inverted
    // This is highly dependent on the specific NFP definition used by DeepNest.

    qDebug() << "Generated NFP using MinkowskiSum of outer boundaries (simplified).";
    return pathsDToQPolygonFs(nfpPaths);
}

QList<QPolygonF> NfpGenerator::minkowskiNfpInside(const Core::InternalPart& partA, const Core::InternalPart& partB_container) {
     if (!partA.isValid() || !partB_container.isValid()) {
        qWarning() << "NfpGenerator::minkowskiNfpInside: Invalid input parts.";
        return QList<QPolygonF>();
    }
    
    // NFP for A to fit INSIDE B_container.
    // Primary NFP: MinkowskiDiff(B_container_outerBoundary, A_outerBoundary_reflected_around_A_origin)
    // This is where the reference point of A can be, such that A fits within B's outer boundary.
    // Clipper2Lib::PathsD nfpPaths = Clipper2Lib::MinkowskiDiff(pathsB_outer, pathsReflectedA_outer, false);

    // However, the common definition for "inner NFP" (where A's reference point can be so A is inside B)
    // is often NFP_inside = B_container_outer - A_outer (Minkowski difference, A not reflected, using B's frame)
    // OR NFP_inside = B_container_outer - Reflect(A_outer, A_reference_point) (if A's position is its reference point)
    // Let's use B_outerBoundary - Reflect(A_outerBoundary, A_ref_point=origin)
    // And then consider holes:
    //  - For each hole H_b in B_container: A must not overlap H_b. So, A must be outside MinkowskiSum(H_b, Reflect(A_outer)).
    //    This means the NFP_inside must be DIFFERENCED with these NFP_Hb regions.
    //  - For each hole H_a in A: These make A "smaller". The NFP_inside is effectively enlarged.
    //    This is complex. NFP_inside_final = Union (NFP_for(A_outer - H_a_i) inside B)

    Clipper2Lib::PathsD pathsB_outer = qPolygonFToPathsD(partB_container.outerBoundary);
    Core::InternalPart reflectedA = reflectPartAroundOrigin(partA); // A's origin is placement ref point.
    Clipper2Lib::PathsD pathsReflectedA_outer = qPolygonFToPathsD(reflectedA.outerBoundary);

    // Tentative: NFP where A's ref point can be so A is within B's outer boundary
    // Using Negative FillRule for difference as per Clipper2 examples for "shrinking"
    Clipper2Lib::PathsD nfpOuterPaths = Clipper2Lib::Difference(pathsB_outer, pathsReflectedA_outer, Clipper2Lib::FillRule::Negative);

    // Now, consider holes in B. A must not enter these.
    // For each hole H_b in B, the NFP for A orbiting H_b is MinkowskiSum(H_b, ReflectedA_outer)
    // These are forbidden regions for A's reference point.
    // So, nfpOuterPaths should be differenced with Union of these (NFP_Hb).
    Clipper2Lib::PathsD forbiddenRegionsFromBHoles;
    if (!partB_container.holes.isEmpty()) {
        for (const QPolygonF& bHole_qpoly : partB_container.holes) {
            Clipper2Lib::PathD bHole_paths = qPolygonFToPathD(bHole_qpoly);
            Clipper2Lib::PathsD nfp_for_bHole = Clipper2Lib::MinkowskiSum(bHole_paths, pathsReflectedA_outer[0], false, 2);
            // Union all forbidden regions
            if (forbiddenRegionsFromBHoles.empty()) {
                forbiddenRegionsFromBHoles = nfp_for_bHole;
            } else {
                forbiddenRegionsFromBHoles = Clipper2Lib::Union(forbiddenRegionsFromBHoles, nfp_for_bHole, Clipper2Lib::FillRule::NonZero);
            }
        }
        // Subtract the union of forbidden regions from the initial NFP
        nfpOuterPaths = Clipper2Lib::Difference(nfpOuterPaths, forbiddenRegionsFromBHoles, Clipper2Lib::FillRule::NonZero);
    }
    
    // This simplified model does not yet properly account for A's holes allowing it to overlap B's material.
    // The @deepnest/calculate-nfp module is designed to handle these complexities.
    qDebug() << "Generated NFP for INSIDE placement using Minkowski (simplified for holes).";
    return pathsDToQPolygonFs(nfpOuterPaths);
}


QList<QPolygonF> NfpGenerator::originalModuleNfp(const Core::InternalPart& partA, const Core::InternalPart& partB, bool isInside, bool useThreads) {
    qWarning() << "NfpGenerator::originalModuleNfp: Integration with original DeepNest C++ NFP module is not yet implemented.";
    // TODO: Implement interaction with the existing C++ NFP code (minkowski.cc / minkowski_thread.cc)
    // This would involve:
    // 1. Understanding how to pass polygon data (partA, partB, including holes) to these functions.
    // 2. Understanding the output format.
    // 3. Mapping InternalPart structure to the required input.
    // 4. Calling the appropriate function (single or multi-threaded based on 'useThreads').
    // 5. Converting the result back to QList<QPolygonF>.
    return QList<QPolygonF>(); // Return empty list as placeholder
}


QList<QPolygonF> NfpGenerator::calculateNfp(
    const Core::InternalPart& partA,
    const Core::InternalPart& partB,
    bool useOriginalDeepNestModule,
    bool allowOriginalModuleMultithreading
) {
    if (useOriginalDeepNestModule) {
        // TODO: Add logic to check if the original module is available/loaded.
        qDebug() << "Attempting to use original DeepNest NFP module (external).";
        return originalModuleNfp(partA, partB, false /*isInside=false*/, allowOriginalModuleMultithreading);
    } else {
        qDebug() << "Using Clipper2 (Minkowski sum) for NFP calculation (A around B).";
        return minkowskiNfp(partA, partB);
    }
}

QList<QPolygonF> NfpGenerator::calculateNfpInside(
    const Core::InternalPart& partA,
    const Core::InternalPart& partB_container,
    bool useOriginalDeepNestModule,
    bool allowOriginalModuleMultithreading
) {
    if (useOriginalDeepNestModule) {
        qDebug() << "Attempting to use original DeepNest NFP module (internal).";
        return originalModuleNfp(partA, partB_container, true /*isInside=true*/, allowOriginalModuleMultithreading);
    } else {
        qDebug() << "Using Clipper2 (Minkowski difference/sum) for NFP calculation (A inside B_container).";
        return minkowskiNfpInside(partA, partB_container);
    }
}

} // namespace Geometry
