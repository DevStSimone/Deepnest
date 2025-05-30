#ifndef NESTINGENGINE_H
#define NESTINGENGINE_H

#include "internalTypes.h"       // For Core::InternalPart, Core::InternalSheet
#include "geneticAlgorithm.h"    // For Core::GeneticAlgorithm, Core::Individual
#include "nfpGenerator.h"        // For Geometry::NfpGenerator
#include "nfpCache.h"            // For Geometry::NfpCache
#include "svgNest.h"             // For SvgNest::Configuration, SvgNest::NestSolution, SvgNest::PlacedPart
#include <QList>
#include <QVector>
#include <QObject> // For tr, if any translated strings are used (unlikely in core logic)
#include <QtConcurrent/QtConcurrent> // For QtConcurrent::mapped
#include <QFuture>                 // For QFuture
#include <QMutex>                  // For protecting shared resources if any (e.g. solutions list)


namespace Core {

// Structure to hold information about a possible placement position for a part
struct CandidatePosition {
    QPointF position;
    double sheetIndex; // Which sheet this position is on
    double partRotation; // Rotation of the part at this position (already applied to NFP context)
    // Add other metrics if needed, e.g., score for this position by placement strategy
};


class NestingEngine {
public:
    NestingEngine(const SvgNest::Configuration& config,
                  QList<InternalPart>& partsToPlace, // Note: non-const, as placement might mark parts
                  const QList<InternalSheet>& sheets);
    ~NestingEngine();

    // Main entry point to run the nesting process
    // Returns all solutions found, or just the best one.
    // The GeneticAlgorithm will run for a number of generations.
    QList<SvgNest::NestSolution> runNesting();
    
    // Allows NestingWorker to request a stop
    void requestStop() { stopRequested_ = true; }


    // Fitness callback for the Genetic Algorithm
    // This method is called by the GA (or by NestingEngine itself after GA creates individuals)
    // to evaluate a given individual (i.e., a sequence of parts with rotations).
    // It attempts to place the parts according to the individual's chromosome
    // and returns a fitness score. The SvgNest::NestSolution is also populated.
    double calculateFitness(Individual& individual, SvgNest::NestSolution& outSolution);


private:
    SvgNest::Configuration config_;
    QList<InternalPart>& allParts_; // Reference to list of all part instances to be placed
    QList<InternalSheet> sheets_;   // Available sheets

    Geometry::NfpCache nfpCache_;
    Geometry::NfpGenerator nfpGenerator_;
    GeneticAlgorithm geneticAlgorithm_;

    bool stopRequested_;
    int solutionsFoundCount_; // Counter for unique solutions

    // --- Core Placement Logic ---
    // Attempts to place all parts defined in an individual's chromosome.
    // Returns a list of placed parts and updates the individual's fitness.
    QList<SvgNest::PlacedPart> placePartsForIndividual(const QVector<Gene>& chromosome, const QList<InternalSheet>& targetSheets);

    // Finds the best position for a single part on a given sheet, considering already placed parts.
    // `currentPlacedParts` are those already on `targetSheet`.
    CandidatePosition findBestPositionForPart(
        const InternalPart& partToPlace,
        double partRotation, // Rotation already applied to partToPlace's geometry for NFP context
        const InternalSheet& targetSheet,
        const QList<InternalPart>& staticParts, // Parts already placed on the sheet (these form obstacles)
        const QString& placementStrategy
    );
    
    // Helper to get the NFP for two parts (A orbiting B)
    QList<QPolygonF> getNfp(const InternalPart& partA, double rotationA, bool flippedA,
                            const InternalPart& partB, double rotationB, bool flippedB,
                            bool partAIsStatic); // partA is static, partB orbits

    // Helper to get NFP for partA to fit inside partB (container)
    QList<QPolygonF> getNfpInside(const InternalPart& partA, double rotationA, bool flippedA,
                                  const InternalPart& containerB, double rotationB, bool flippedB);

    // Helper to transform an InternalPart (e.g., by rotation)
    InternalPart transformPart(const InternalPart& part, double rotation);
    
    // Function to convert list of placed parts to a fitness score
    double evaluateSolutionFitness(const QList<SvgNest::PlacedPart>& placements, int totalParts);

    // Placeholder for actual geometric operations for placement strategies
    QList<CandidatePosition> findCandidatePositions(
        const InternalPart& partToPlaceTransformed, // Part to place, already rotated
        const QList<QPolygonF>& nfpForPartAndSheet, // NFP of (SheetBoundary - PartToPlace)
        const QList<QList<QPolygonF>>& nfPsForPartAndPlacedObstacles // List of NFPs (PlacedObstacle_i - PartToPlace)
    );
};

} // namespace Core
#endif // NESTINGENGINE_H
