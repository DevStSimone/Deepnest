#include "nestingEngine.h"
#include "geometryUtils.h" // For GeometryUtils:: (e.g. boundingBox)
#include <QDebug>
#include <algorithm> // For std::sort, etc.
#include <limits>    // For std::numeric_limits
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <QFutureWatcher> // Useful for handling results, but can also use QFuture::results()
#include <QThreadPool>    // To potentially control max thread count
#include <QElapsedTimer>  // For basic performance timing

// Define a high value for "not placed" or error fitness
const double BAD_FITNESS_SCORE = -std::numeric_limits<double>::infinity(); // If higher is better
// const double BAD_FITNESS_SCORE = std::numeric_limits<double>::max(); // If lower is better


namespace Core {

// Structure to hold result of fitness calculation for QtConcurrent::mapped
struct FitnessResult {
    double fitness = BAD_FITNESS_SCORE; // Initialize to bad fitness
    SvgNest::NestSolution solution;
    int originalIndex; // Index of the individual in the original population vector
    // Store the chromosome itself if needed, or just its relevant parts for generating the solution later
    QVector<Gene> chromosomeForSolution; // To reconstruct solution if needed
};


NestingEngine::NestingEngine(const SvgNest::Configuration& config,
                             QList<InternalPart>& partsToPlace,
                             const QList<InternalSheet>& sheets)
    : config_(config),
      allParts_(partsToPlace), // Store reference
      sheets_(sheets),
      nfpGenerator_(config.clipperScale), // Initialize NfpGenerator with scale
      geneticAlgorithm_(config, allParts_), // Pass all available part instances
      stopRequested_(false),
      solutionsFoundCount_(0) {
    qDebug() << "NestingEngine created. Parts to place:" << allParts_.size() << "Sheets available:" << sheets_.size();
    // You can adjust the global QThreadPool if needed:
    // QThreadPool::globalInstance()->setMaxThreadCount(desired_max_threads);
}

NestingEngine::~NestingEngine() {
    qDebug() << "NestingEngine destroyed.";
}

QList<SvgNest::NestSolution> NestingEngine::runNesting() {
    qDebug() << "NestingEngine: Starting nesting process (parallel fitness eval)...";
    QElapsedTimer timer;
    timer.start();

    QList<SvgNest::NestSolution> allFoundSolutionsBestFirst; 
    solutionsFoundCount_ = 0;

    if (allParts_.isEmpty() || sheets_.isEmpty()) {
        qWarning() << "NestingEngine: No parts to place or no sheets available.";
        return allFoundSolutionsBestFirst;
    }

    geneticAlgorithm_.initializePopulation();

    int maxGenerations = config_.populationSize * 10; 
    if (config_.placementType == "simple") maxGenerations = 1;

    for (int gen = 0; gen < maxGenerations; ++gen) {
        if (stopRequested_) {
            qDebug() << "NestingEngine: Stop requested during GA generation" << gen;
            break;
        }
        qDebug() << "NestingEngine: GA Generation" << gen;

        QVector<Individual>& currentPopulation = const_cast<QVector<Individual>&>(geneticAlgorithm_.getPopulation());
        
        // Create a sequence of indices to map over, to avoid issues with capturing `this` directly
        // if lambda context or this-pointer stability with QtConcurrent becomes an issue (less so with modern C++).
        // Or, map over a copy of the population.
        QVector<Individual> populationCopy = currentPopulation; 
        // Add originalIndex to each individual copy for tracking
        for(int i=0; i < populationCopy.size(); ++i) {
            // This assumes Individual struct can hold an originalIndex, or we use a wrapper.
            // For simplicity, FitnessResult will store originalIndex.
        }


        std::function<FitnessResult(Individual&)> mapFunction = 
            [this, &populationCopy /* capture by reference if its lifetime is guaranteed, else copy */]
            (Individual& individual) -> FitnessResult {
            // The 'individual' here is an element from populationCopy.
            // Its original index is its current position in populationCopy.
            // This relies on QtConcurrent::mapped processing elements in order or results being re-orderable.
            // To be safe, explicitly find index if needed, or pass index with data.

            int originalIdx = -1; // Need a way to get original index if not mapping over indices.
                                  // For now, assume results order matches input order.

            if (this->stopRequested_) {
                return {BAD_FITNESS_SCORE, SvgNest::NestSolution(), originalIdx, QVector<Gene>()};
            }
            SvgNest::NestSolution sol;
            double fit = this->calculateFitness(individual, sol); // individual.fitness is set here
            
            // 'individual' is a copy, so its chromosome is safe to copy if needed.
            return {fit, sol, originalIdx, individual.chromosome}; 
        };

       qDebug() << "NestingEngine: Starting parallel fitness calculation for" << populationCopy.size() << "individuals.";
       // Using a blocking version of mapped for simplicity in this step.
       // For a responsive UI, QFutureWatcher would be used.
       QFuture<FitnessResult> future = QtConcurrent::mapped(populationCopy.begin(), populationCopy.end(), mapFunction);
       future.waitForFinished(); 
       qDebug() << "NestingEngine: Parallel fitness calculation finished.";

       QList<FitnessResult> results = future.results();
       for (int i = 0; i < results.size() && i < currentPopulation.size(); ++i) {
           // Assuming results are in the same order as populationCopy
           currentPopulation[i].fitness = results[i].fitness; 
           if (results[i].fitness != BAD_FITNESS_SCORE) {
               solutionsFoundCount_++;
               // The SvgNest object (owner of NestingWorker) is responsible for collecting solutions
               // via signals from NestingWorker. NestingEngine itself doesn't directly emit to SvgNest.
               // NestingWorker will get all solutions at the end from NestingEngine.
               // For now, we can store the best solutions from each generation if needed.
               allFoundSolutionsBestFirst.append(results[i].solution); // Collect all valid solutions
           }
       }

        if (stopRequested_) break;

        geneticAlgorithm_.runGeneration(); 
        
        // Optionally, log or store the best solution of this generation
        Individual bestThisGen = geneticAlgorithm_.getBestIndividual();
        if (bestThisGen.fitness != BAD_FITNESS_SCORE) {
            // The bestThisGen is now from the *new* population after selection/crossover/mutation.
            // Its SvgNest::NestSolution is not directly available here unless we re-calculate or store it.
            // This is fine, as the primary goal is to drive the GA with fitness.
        }
    }
    
    qDebug() << "NestingEngine: Nesting process finished. Total valid solutions considered:" << solutionsFoundCount_;
    qDebug() << "NestingEngine: Total time:" << timer.elapsed() << "ms";
    
    // Sort all collected solutions by fitness (descending, higher is better)
    std::sort(allFoundSolutionsBestFirst.begin(), allFoundSolutionsBestFirst.end(), 
        [](const SvgNest::NestSolution& a, const SvgNest::NestSolution& b) {
        return a.fitness > b.fitness;
    });

    return allFoundSolutionsBestFirst; 
}


// calculateFitness remains largely the same, but must be thread-safe regarding
// NestingEngine members if it accesses them.
// NfpCache is now mutex-protected. Other shared state? allParts_ and sheets_ are read-only here.
// config_ is read-only. stopRequested_ is an atomic or needs protection if written by another thread.
// For now, stopRequested_ is checked by the lambda.
double NestingEngine::calculateFitness(Individual& individual, SvgNest::NestSolution& outSolution) {
    outSolution.placements.clear();
    QList<SvgNest::PlacedPart> placedPartsList;
    int successfullyPlacedCount = 0;
    
    // Create a temporary, modifiable copy of parts for this individual's evaluation if needed
    // QList<InternalPart> partsForThisRun = allParts_; // If parts state changes during placement

    // Map to keep track of placed parts on sheets for obstacle NFP calculations
    QHash<int, QList<InternalPart>> partsOnSheetMap;


    for (const Gene& gene : individual.chromosome) {
        if (stopRequested_) return BAD_FITNESS_SCORE;

        InternalPart partToPlaceOriginal;
        bool foundPart = false;
        if(gene.sourceIndex >= 0 && gene.sourceIndex < allParts_.size() && allParts_[gene.sourceIndex].id == gene.partId) {
             partToPlaceOriginal = allParts_[gene.sourceIndex];
             foundPart = true;
        }
        if(!foundPart) {
             qWarning() << "NestingEngine: Could not find valid part for gene ID" << gene.partId << "at sourceIndex" << gene.sourceIndex;
             continue;
        }

        InternalPart partToPlaceTransformed = transformPart(partToPlaceOriginal, gene.rotation);
        
        bool placedThisPart = false;
        for (int sheetIdx = 0; sheetIdx < sheets_.size(); ++sheetIdx) {
            if (stopRequested_) return BAD_FITNESS_SCORE;
            
            const QList<InternalPart>& obstaclesOnThisSheet = partsOnSheetMap.value(sheetIdx);

            CandidatePosition bestPos = findBestPositionForPart(
                partToPlaceTransformed, gene.rotation, sheets_[sheetIdx], obstaclesOnThisSheet, config_.placementType
            );

            if (bestPos.position != QPointF(-1,-1)) { 
                SvgNest::PlacedPart pp;
                pp.partId = gene.partId; 
                pp.sheetIndex = sheetIdx;
                pp.position = bestPos.position;
                pp.rotation = gene.rotation;
                
                placedPartsList.append(pp);
                successfullyPlacedCount++;
                
                // Add this part to the list of obstacles for the current sheet for subsequent placements
                InternalPart placedPartCopy = partToPlaceTransformed; // It's already transformed
                // Translate its geometry to the placed position to act as an obstacle
                QTransform translateToPlace;
                translateToPlace.translate(bestPos.position.x(), bestPos.position.y());
                placedPartCopy.outerBoundary = translateToPlace.map(placedPartCopy.outerBoundary);
                QList<QPolygonF> translatedHoles;
                for(const QPolygonF& hole : placedPartCopy.holes) {
                    translatedHoles.append(translateToPlace.map(hole));
                }
                placedPartCopy.holes = translatedHoles;
                placedPartCopy.bounds = placedPartCopy.outerBoundary.boundingRect(); // Update bounds to placed location

                partsOnSheetMap[sheetIdx].append(placedPartCopy);
                
                placedThisPart = true;
                break; 
            }
        }
    }

    outSolution.placements = placedPartsList;
    individual.fitness = evaluateSolutionFitness(placedPartsList, individual.chromosome.size()); // Set fitness on the individual
    return individual.fitness;
}


InternalPart NestingEngine::transformPart(const InternalPart& part, double rotation) {
    if (rotation == 0) return part;
    InternalPart transformedPart = part; // Make a copy
    QTransform t;
    // Rotate around the part's own origin (0,0) as its geometry is defined relative to that.
    // If parts have a different pivot point, that logic would be here or in InternalPart.
    t.rotate(rotation); 

    transformedPart.outerBoundary = t.map(part.outerBoundary);
    transformedPart.holes.clear();
    for (const QPolygonF& hole : part.holes) {
        transformedPart.holes.append(t.map(hole));
    }
    if (!transformedPart.outerBoundary.isEmpty()) {
        transformedPart.bounds = transformedPart.outerBoundary.boundingRect();
    } else {
        transformedPart.bounds = QRectF();
    }
    return transformedPart;
}

double NestingEngine::evaluateSolutionFitness(const QList<SvgNest::PlacedPart>& placements, int totalPartsAttempted) {
    if (totalPartsAttempted == 0) return BAD_FITNESS_SCORE;
    double fitness = static_cast<double>(placements.size()) / static_cast<double>(totalPartsAttempted);
    if (placements.size() < totalPartsAttempted) {
        fitness -= (totalPartsAttempted - placements.size()); 
    } else {
        fitness += 1.0; 
        // TODO: Add more sophisticated fitness metrics like bounds, sheet usage etc.
    }
    return fitness;
}


CandidatePosition NestingEngine::findBestPositionForPart(
    const InternalPart& partToPlaceTransformed, 
    double partRotationVal, 
    const InternalSheet& targetSheet,
    const QList<InternalPart>& staticObstacles, 
    const QString& placementStrategy) {

    if (!partToPlaceTransformed.isValid() || !targetSheet.isValid()) {
        return {QPointF(-1,-1), -1, 0.0};
    }

    // For NFP generation, partToPlaceTransformed is already rotated.
    // Sheet and obstacles are assumed to be in their fixed, 0-rotation state on the sheet.
    QList<QPolygonF> nfpSheet = getNfpInside(partToPlaceTransformed, 0 /*rot for partA is effectively baked in*/, false, 
                                             targetSheet, 0, false);
    if (nfpSheet.isEmpty()) {
        return {QPointF(-1,-1), -1, 0.0};
    }

    QList<QList<QPolygonF>> nfpObstaclesList;
    for (const InternalPart& obstacle : staticObstacles) {
        if (stopRequested_) return {QPointF(-1,-1), -1, 0.0};
        // Obstacle is already placed, so its geometry is static. PartToPlaceTransformed orbits it.
        // Obstacle rotation is 0 because its geometry is already in sheet coordinates.
        QList<QPolygonF> nfpObs = getNfp(partToPlaceTransformed, 0 /*baked in*/, false, 
                                         obstacle, 0, false, 
                                         false /*partB (obstacle) is static*/);
        if (!nfpObs.isEmpty()) {
            nfpObstaclesList.append(nfpObs);
        }
    }
    
    QList<CandidatePosition> candidates = findCandidatePositions(partToPlaceTransformed, nfpSheet, nfpObstaclesList);
    if (candidates.isEmpty()) {
         return {QPointF(-1,-1), -1, 0.0};
    }

    CandidatePosition bestPosition = {QPointF(-1,-1), -1, 0.0};
    if (placementStrategy == "gravity" || placementStrategy == "bottomleft") { 
        double minY = std::numeric_limits<double>::max();
        double minX_at_minY = std::numeric_limits<double>::max();
        for (const auto& cand : candidates) {
            if (cand.position.y() < minY) {
                minY = cand.position.y();
                minX_at_minY = cand.position.x();
                bestPosition = cand;
            } else if (cand.position.y() == minY) {
                if (cand.position.x() < minX_at_minY) {
                    minX_at_minY = cand.position.x();
                    bestPosition = cand;
                }
            }
        }
    } else { 
         if(!candidates.isEmpty()) bestPosition = candidates.first(); 
    }
    
    bestPosition.partRotation = partRotationVal; 
    return bestPosition;
}

QList<CandidatePosition> NestingEngine::findCandidatePositions(
    const InternalPart& partToPlaceTransformed,
    const QList<QPolygonF>& nfpForPartAndSheet, 
    const QList<QList<QPolygonF>>& nfPsForPartAndPlacedObstacles) 
{
    QList<CandidatePosition> validPositions;
    if (nfpForPartAndSheet.isEmpty() || nfpForPartAndSheet.first().isEmpty()) {
        return validPositions;
    }

    const QPolygonF& mainPlacementRegion = nfpForPartAndSheet.first();
    for (const QPointF& potentialPos : mainPlacementRegion) {
        bool overlapsObstacle = false;
        for (const QList<QPolygonF>& nfpObstacleSet : nfPsForPartAndPlacedObstacles) {
            for (const QPolygonF& nfpObsPoly : nfpObstacleSet) {
                if (GeometryUtils::isPointInPolygon(potentialPos, nfpObsPoly, Qt::OddEvenFill)) { 
                    overlapsObstacle = true;
                    break;
                }
            }
            if (overlapsObstacle) break;
        }

        if (!overlapsObstacle) {
            validPositions.append({potentialPos, 0, 0.0}); 
        }
    }
    return validPositions;
}


QList<QPolygonF> NestingEngine::getNfp(const InternalPart& partA, double rotationA, bool flippedA,
                                       const InternalPart& partB, double rotationB, bool flippedB,
                                       bool partAIsStaticInKey) { 
    
    // The parts pA_for_nfp, pB_for_nfp are transformed based on rotationA, rotationB for geometry calculation.
    // The key uses original part IDs and gene-defined rotations/flips.
    InternalPart pA_for_nfp = transformPart(partA, rotationA); 
    InternalPart pB_for_nfp = transformPart(partB, rotationB);

    QString cacheKey;
    if (partAIsStaticInKey) { // partA is static, partB orbits partA
         cacheKey = nfpCache_.generateKey(partB.id, rotationB, flippedB,   
                                          partA.id, rotationA, flippedA,   
                                          true); // True because partA (the second one in key) is static
    } else { // partA orbits partB, partB is static
         cacheKey = nfpCache_.generateKey(partA.id, rotationA, flippedA,   
                                          partB.id, rotationB, flippedB,   
                                          false); // False because partA (the first one in key) is not static
    }

    Geometry::CachedNfp cachedNfp;
    if (nfpCache_.findNfp(cacheKey, cachedNfp)) {
        return cachedNfp.nfpPolygons;
    }

    QList<QPolygonF> nfp;
    if (partAIsStaticInKey) { 
         nfp = nfpGenerator_.calculateNfp(pB_for_nfp, pA_for_nfp, config_.placementType == "deepnest", false);
    } else { 
         nfp = nfpGenerator_.calculateNfp(pA_for_nfp, pB_for_nfp, config_.placementType == "deepnest", false);
    }
    
    nfpCache_.storeNfp(cacheKey, Geometry::CachedNfp(nfp));
    return nfp;
}

QList<QPolygonF> NestingEngine::getNfpInside(const InternalPart& partA, double rotationA, bool flippedA,
                                             const InternalPart& containerB, double rotationB, bool flippedB) {
    
    InternalPart pA_for_nfp = transformPart(partA, rotationA);
    InternalPart pB_container_for_nfp = transformPart(containerB, rotationB);

    // Key for "A fitting inside B (container)": A is dynamic, B is the static frame.
    // generateKey(dynamicId, dynamicRot, dynamicFlip, staticId, staticRot, staticFlip, is_first_part_static_flag)
    // Here, partA is dynamic (first part in key), containerB is static (second part in key).
    // So, the "is_first_part_static_flag" should be false.
    QString cacheKey = nfpCache_.generateKey(partA.id, rotationA, flippedA,          
                                             containerB.id, rotationB, flippedB,    
                                             false); 
    
    Geometry::CachedNfp cachedNfp;
    if (nfpCache_.findNfp(cacheKey, cachedNfp)) {
        return cachedNfp.nfpPolygons;
    }
    
    QList<QPolygonF> nfp = nfpGenerator_.calculateNfpInside(pA_for_nfp, pB_container_for_nfp, config_.placementType == "deepnest", false);
    
    nfpCache_.storeNfp(cacheKey, Geometry::CachedNfp(nfp));
    return nfp;
}


} // namespace Core
