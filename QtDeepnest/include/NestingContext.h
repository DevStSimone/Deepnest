#ifndef NESTING_CONTEXT_H
#define NESTING_CONTEXT_H

#include <QObject>
#include <QList>
#include <QTimer> // For managing worker dispatch if not using QThreadPool immediately
#include "DataStructures.h"
#include "Config.h"
#include "GeneticAlgorithm.h"
#include "NfpCache.h"       // Added
#include "NfpGenerator.h"   // Added
#include "NestingWorker.h"  // Added


class NestingContext : public QObject {
    Q_OBJECT
public:
    explicit NestingContext(QObject *parent = nullptr);
    ~NestingContext();

    void startNesting(const QList<Part>& parts, const AppConfig& config);
    void stopNesting();
    void resetNesting();
    
    const QList<NestResult>& getNests() const { return m_nests; }

signals:
     void nestProgress(double percentage, int individualId); // Overall progress
    void newBestNest(const NestResult& nest); // When a new best solution is found
    void nestingFinished();

public slots:
    void handleWorkerResult(const NestResult& result, int individualId); // Add individualId
    void launchNextTask(); // Slot to be called to dispatch next GA individual
    void handleWorkerProgress(double percentage, int individualId);

private:
    QList<Part> m_partsToNest; // All unique parts with their quantities (after processing for GA)
    std::vector<int> m_expandedPartIndices; // Indices into m_partsToNest, expanded by quantity, for GA individuals
    
    AppConfig m_currentConfig;
    GeneticAlgorithm* m_ga;
    NfpCache m_nfpCache;         // Added
    NfpGenerator m_nfpGenerator; // Added
    QList<NestResult> m_nests; // Stores top N results

    QList<Part> m_placeablePartsForGA;
    QList<Part> m_sheetPartsForWorker;
    bool m_isNesting;
    int m_threadsLaunched; // Track how many "threads" (tasks) are out
    int m_threadsCompleted; // Track completed tasks for a generation
    int m_maxConcurrentWorkers; // From config.threads (note: config.threads was removed from AppConfig, will use m_currentConfig.threads if it's re-added or a default)

    QTimer* m_workerDispatchTimer; // To periodically call launchNextTask

    // Placeholder for actual worker thread management
    // QList<NestingWorkerThread*> m_workerThreads; 

    void prepareAdamParts(const QList<Part>& inputParts); // Takes original parts from UI/SVG
    void sortAndTrimNests();
};

#endif // NESTING_CONTEXT_H
