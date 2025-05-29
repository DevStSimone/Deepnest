#ifndef NFPGENERATOR_H
#define NFPGENERATOR_H

#include "internalTypes.h" // For Core::InternalPart
#include "clipper.h"       // From Clipper2 library (clipper.h is the main header)
#include "External/Minkowski/minkowski_wrapper.h" // Added for CustomMinkowski
#include <QList>
#include <QPolygonF>

// Forward declaration from SvgNest.h if needed, or include a slim config header
// For now, assume relevant config values (like clipperScale) are passed directly.
// struct SvgNestDeepNestConfiguration; // Placeholder

namespace Geometry {

class NfpGenerator {
public:
    // Constructor - takes scaling factor for Clipper library
    NfpGenerator(double clipperScale);
    ~NfpGenerator();

    // Calculates the No-Fit Polygon for partA (orbiting) around partB (static).
    // Returns a list of polygons representing the NFP. Usually one, but could be multiple.
    // 'useMinkowskiModule' is a placeholder for choosing between the original C++ module and Clipper2.
    // 'threadCount' is for the original C++ module if it supports threading.
    QList<QPolygonF> calculateNfp(
        const Core::InternalPart& partA, // Orbiting part
        const Core::InternalPart& partB, // Static part
        bool useOriginalDeepNestModule,  // True to attempt using DeepNest's C++ NFP code
        bool allowOriginalModuleMultithreading // If true, and original module is used, allow it to use threads
    );
    
    // Calculates NFP for partA (orbiting) trying to fit INSIDE partB (static part's outer boundary, considering its holes).
    // This is different from A around B.
    QList<QPolygonF> calculateNfpInside(
        const Core::InternalPart& partA,
        const Core::InternalPart& partB, // The "container" part
        bool useOriginalDeepNestModule,
        bool allowOriginalModuleMultithreading
    );


private:
    double scale_; // Scale factor for Clipper operations

    // Helper to convert QPolygonF to Clipper2 PathsD
    Clipper2Lib::PathsD qPolygonFToPathsD(const QPolygonF& polygon) const;
    Clipper2Lib::PathsD qPolygonFsToPathsD(const QList<QPolygonF>& polygons) const;
    // Helper to convert Clipper2 PathsD to QList<QPolygonF>
    QList<QPolygonF> pathsDToQPolygonFs(const Clipper2Lib::PathsD& paths) const;

    // --- Methods for NFP using Clipper2 (Minkowski Sum) ---
    // NFP(A, B) where A orbits B (B is static)
    // This is typically MinkowskiSum(B, Reflect(A, origin))
    QList<QPolygonF> minkowskiNfp(const Core::InternalPart& partA, const Core::InternalPart& partB);

    // NFP for A fitting INSIDE B.
    // This is more complex. For a convex B, it could be MinkowskiDiff(B_hole, A) for each hole of B,
    // and MinkowskiSum(Shrink(B_boundary, A_radius), Reflect(A_hole_shape, origin))
    // Or, B_boundary - MinkowskiSum(A_boundary, Reflect(hole_in_A)) and union with MinkowskiSum(hole_in_B, Reflect(A_boundary)).
    // DeepNest's original JS implementation has specific logic for this.
    // For now, a placeholder or simplified version.
    // A common approach for "inner NFP" is to consider the boundaries.
    // For placing A inside B's boundary: NFP = B_boundary - Reflect(A_boundary, A_reference_point)
    // where '-' is Minkowski Difference.
    // And for each hole H_b in B, NFP_Hb = Reflect(A_boundary, A_reference_point) + H_b (Minkowski Sum)
    QList<QPolygonF> minkowskiNfpInside(const Core::InternalPart& partA, const Core::InternalPart& partB);


    // --- Placeholders for original DeepNest NFP module integration ---
    QList<QPolygonF> originalModuleNfp(const Core::InternalPart& partA, const Core::InternalPart& partB, bool isInside, bool useThreads);
};

} // namespace Geometry
#endif // NFPGENERATOR_H
