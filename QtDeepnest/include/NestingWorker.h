#ifndef NESTING_WORKER_H
#define NESTING_WORKER_H

#include <QObject>
#include <QRunnable>
#include <QList>
#include "DataStructures.h"
#include "Config.h"
#include "GeneticAlgorithm.h" // For Individual struct
#include "NfpCache.h"
#include "NfpGenerator.h"
#include "GeometryProcessor.h" // For rotatePolygon, getPolygonBounds etc.

class NestingWorker : public QObject, public QRunnable {
    Q_OBJECT
public:
    NestingWorker(
        int individualId, // To identify which GA individual this worker is for
        const Individual& individualConfig, // The specific part order and rotations
        const QList<Part>& allUniqueParts,   // List of all unique part definitions (processed for GA: spacing, simplification)
        const QList<Part>& sheetParts,     // List of parts designated as sheets (processed for GA: spacing, simplification)
        const AppConfig& appConfig,
        NfpCache* nfpCache,           // Pointer, shared among workers
        NfpGenerator* nfpGenerator    // Pointer, assumed thread-safe or one per worker
    );

    void run() override;

signals:
    void resultReady(const NestResult& result, int individualId);
    void progressUpdated(double percentage, int individualId); // For finer-grained progress within a worker

private:
    // Helper to convert a Part (defined by an index in m_allUniqueParts and a rotation from m_individualConfig)
    // to its actual geometry and ID.
    Part getPartInstanceForPlacement(int expandedPartListIndex);
    Polygon getTransformedPartGeometry(int uniquePartListIndex, double rotationStep);
    
    // NFP calculation logic
    std::vector<Polygon> getInnerNfp(const Part& sheet, const Part& partToPlace, const AppConfig& config, int partToPlaceRotationIndex);
    std::vector<Polygon> getOuterNfp(const Part& placedPartInstance, int placedPartRotationStep,
                                     const Part& currentPartInstance, int currentPartRotationStep,
                                     const AppConfig& config);
    
    // Part placement logic
    NestResult placeParts();

    // Helper to find best placement position (simplified)
    bool findBestPlacement(const std::vector<Polygon>& nfpPaths, Point& outPosition);


    // Member variables
    int m_individualId;
    Individual m_individualConfig; // Contains indices into an expanded list of parts and their rotations
    QList<Part> m_allUniqueParts;   // All unique parts (non-sheets) that the indices in m_individualConfig refer to.
                                   // These are already processed (spacing, simplification).
    QList<Part> m_sheetPartsList;   // Available sheets, already processed (spacing, simplification).
    AppConfig m_appConfig;
    NfpCache* m_nfpCache;           // Shared, needs thread-safe access.
    NfpGenerator* m_nfpGenerator;   // If not thread-safe, each worker might need its own or use a shared one with mutex.
                                    // For now, assume it can be shared or is used carefully.
    
    // Temporary state for placeParts
    std::vector<Part> m_partsToPlaceThisRun; // This will be constructed inside placeParts from m_individualConfig
    QList<Part> m_availableSheetsThisRun; // This will be a copy from m_sheetPartsList
};

#endif // NESTING_WORKER_H
