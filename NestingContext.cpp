#include "NestingContext.h"
#include "GeometryProcessor.h" 
#include "NestingWorker.h"     
#include <QThreadPool>         
#include <QDebug>
#include <QRandomGenerator>   
#include <QThread> // For QThread::idealThreadCount()
#include <numeric> // For std::iota

NestingContext::NestingContext(QObject *parent) 
    : QObject(parent), m_ga(nullptr), m_isNesting(false), 
      m_threadsLaunched(0), m_threadsCompleted(0), m_maxConcurrentWorkers(0) {
    m_workerDispatchTimer = new QTimer(this);
    connect(m_workerDispatchTimer, &QTimer::timeout, this, &NestingContext::launchNextTask);
    
    m_maxConcurrentWorkers = QThread::idealThreadCount();
    if (m_maxConcurrentWorkers <= 0) {
        m_maxConcurrentWorkers = 2; 
    }
    // m_nfpCache and m_nfpGenerator are default constructed as members.
}

NestingContext::~NestingContext() {
    stopNesting(); 
    delete m_ga;
    QThreadPool::globalInstance()->waitForDone(); 
}

void NestingContext::prepareAdamParts(const QList<Part>& inputParts) {
    m_partsInputOriginal = inputParts; 
    m_partsToNest.clear(); 
    m_expandedPartIndices.clear();
    m_placeablePartsForGA.clear();
    m_sheetPartsForWorker.clear();

    for (const Part& part_input : m_partsInputOriginal) { 
        Part processedPart = part_input; 
        Polygon currentGeometry = part_input.geometry; 

        if (part_input.isSheet) {
            // Process Sheet
            if (m_currentConfig.simplify) {
                // Simplify sheet's outer boundary. 'isHole=true' for simplifyPolygonDeepnest implies contract-then-expand logic.
                Polygon tempSheetPolyForSimp = {currentGeometry.outer, {}}; // Create Polygon with only outer for simplification
                Polygon simplifiedOuter = GeometryProcessor::simplifyPolygonDeepnest(tempSheetPolyForSimp, m_currentConfig.curveTolerance, true);
                currentGeometry.outer = simplifiedOuter.outer;
                currentGeometry.holes.clear(); // Sheets typically don't have holes, or they are simplified away.
            }
            // Offset sheet (shrink)
            std::vector<Polygon> offsetResult = GeometryProcessor::offsetPolygons({currentGeometry}, -0.5 * m_currentConfig.spacing);
            if (!offsetResult.empty() && !offsetResult[0].outer.empty()) {
                processedPart.geometry = offsetResult[0]; 
            } else {
                qWarning() << "Warning: Offsetting sheet" << part_input.id << "resulted in empty geometry. Using original/simplified:" << polygonToString(currentGeometry);
                processedPart.geometry = currentGeometry; 
            }
            m_sheetPartsForWorker.append(processedPart);
        } else {
            // Process Non-Sheet Part
            if (m_currentConfig.simplify) {
                Polygon simplifiedPartGeom;
                // Simplify outer boundary (isHole = false for expand-then-contract)
                Polygon tempOuterPolyForSimp = {currentGeometry.outer, {}};
                Polygon outerSimplified = GeometryProcessor::simplifyPolygonDeepnest(tempOuterPolyForSimp, m_currentConfig.curveTolerance, false);
                simplifiedPartGeom.outer = outerSimplified.outer;

                // Simplify holes (isHole = true for contract-then-expand)
                for (const auto& hole_pts : currentGeometry.holes) {
                    if (hole_pts.empty()) continue;
                    Polygon tempHolePolyForSimp = {hole_pts, {}}; 
                    Polygon simplifiedHole = GeometryProcessor::simplifyPolygonDeepnest(tempHolePolyForSimp, m_currentConfig.curveTolerance, true);
                    if(!simplifiedHole.outer.empty()){
                         simplifiedPartGeom.holes.push_back(simplifiedHole.outer);
                    }
                }
                currentGeometry = simplifiedPartGeom;
            }
            // Offset part (expand)
            std::vector<Polygon> offsetResult = GeometryProcessor::offsetPolygons({currentGeometry}, 0.5 * m_currentConfig.spacing);
            if (!offsetResult.empty() && !offsetResult[0].outer.empty()) {
                processedPart.geometry = offsetResult[0]; 
            } else {
                qWarning() << "Warning: Offsetting part" << part_input.id << "resulted in empty geometry. Using original/simplified:" << polygonToString(currentGeometry);
                processedPart.geometry = currentGeometry; 
            }
            m_placeablePartsForGA.append(processedPart);
            for(int q=0; q < processedPart.quantity; ++q){
                m_expandedPartIndices.push_back(m_placeablePartsForGA.size() - 1); 
            }
        }
        m_partsToNest.append(processedPart); 
    }
}


void NestingContext::startNesting(const QList<Part>& parts, const AppConfig& config) {
    if (m_isNesting) {
        qWarning() << "Nesting is already in progress. Stop current nesting first.";
        return;
    }

    resetNesting(); 
    m_nfpCache.clear(); 

    m_currentConfig = config;
    if (m_currentConfig.threads > 0) { 
         m_maxConcurrentWorkers = m_currentConfig.threads;
    } else {
        m_maxConcurrentWorkers = QThread::idealThreadCount() > 0 ? QThread::idealThreadCount() : 2;
    }

    prepareAdamParts(parts); 

    if (m_placeablePartsForGA.isEmpty() || m_expandedPartIndices.empty()) { 
        qWarning() << "No placeable parts available for nesting after preparation.";
        emit nestingFinished();
        return;
    }
    if (m_sheetPartsForWorker.isEmpty()){
        qWarning() << "No sheets available for nesting.";
        emit nestingFinished();
        return;
    }
    
    delete m_ga; 
    m_ga = new GeneticAlgorithm(m_placeablePartsForGA, m_currentConfig); 
    m_ga->initializePopulation(m_placeablePartsForGA); 

    m_nests.clear();
    m_threadsLaunched = 0; // Not strictly needed if relying on QThreadPool::activeThreadCount for dispatch decisions
    m_threadsCompleted = 0; 
    m_isNesting = true;

    qInfo() << "Nesting started. Max workers:" << m_maxConcurrentWorkers << "Population size:" << m_ga->getPopulation().size();
    m_workerDispatchTimer->start(50); 
}

void NestingContext::stopNesting() {
    m_isNesting = false;
    if (m_workerDispatchTimer) {
        m_workerDispatchTimer->stop();
    }
    // This doesn't cancel already running QRunnables. They will complete.
    // QThreadPool::globalInstance()->clear(); // Avoid this, it's too aggressive.
    QThreadPool::globalInstance()->waitForDone(1000); 
    qInfo() << "Nesting stopping...";
}

void NestingContext::resetNesting() {
    stopNesting();
    delete m_ga;
    m_ga = nullptr;
    m_nests.clear();
    m_partsInputOriginal.clear();
    m_partsToNest.clear(); 
    m_expandedPartIndices.clear();
    m_placeablePartsForGA.clear();
    m_sheetPartsForWorker.clear();
    m_threadsLaunched = 0;
    m_threadsCompleted = 0;
}

void NestingContext::launchNextTask() {
    if (!m_isNesting || !m_ga) {
        if(m_workerDispatchTimer && m_workerDispatchTimer->isActive()) m_workerDispatchTimer->stop();
        return;
    }

    if (m_ga->allIndividualsProcessed() && QThreadPool::globalInstance()->activeThreadCount() == 0) { 
        qInfo() << "All individuals processed for current generation. Moving to next generation.";
        m_ga->nextGeneration();
        m_threadsCompleted = 0; 
        // TODO: Add termination conditions (max generations etc.)
        emit nestProgress(0.0, -1); // -1 individualId for overall/generation progress
    }

    while (QThreadPool::globalInstance()->activeThreadCount() < m_maxConcurrentWorkers) {
        Individual ind = m_ga->getNextIndividualToProcess();
        if (ind.id == -1) { 
            break; 
        }
        
        qInfo() << "Dispatching NestingWorker for individual ID:" << ind.id << ". Active threads:" << QThreadPool::globalInstance()->activeThreadCount();
        
        NestingWorker* worker = new NestingWorker(
            ind.id, 
            ind, 
            m_placeablePartsForGA, 
            m_sheetPartsForWorker,   
            m_currentConfig, 
            &m_nfpCache, 
            &m_nfpGenerator
        );

        connect(worker, &NestingWorker::resultReady, this, &NestingContext::handleWorkerResult, Qt::QueuedConnection);
        connect(worker, &NestingWorker::progressUpdated, this, &NestingContext::handleWorkerProgress, Qt::QueuedConnection);
        
        QThreadPool::globalInstance()->start(worker);
    }

    if (!m_isNesting && m_workerDispatchTimer->isActive()) { 
        m_workerDispatchTimer->stop();
    }
}

void NestingContext::handleWorkerResult(const NestResult& result, int individualId) {
    if (!m_ga) { 
        qWarning() << "GA is null. Ignoring worker result for ID:" << individualId;
        return; 
    }

    m_ga->updateIndividualFitness(individualId, result.fitness);
    m_threadsCompleted++;

    qInfo() << "Worker finished for individual ID:" << individualId << ". Fitness:" << result.fitness 
            << ". Active threads:" << QThreadPool::globalInstance()->activeThreadCount() 
            << ". Completed this gen:" << m_threadsCompleted;

    m_nests.append(result);
    sortAndTrimNests(); // This will also emit newBestNest if the best changes implicitly by adding a better one

    // Emit newBestNest explicitly if this result is the new best
    if (!m_nests.isEmpty() && result.fitness == m_nests.first().fitness) {
         emit newBestNest(m_nests.first());
    }
    // Emit updated list of top N nests
    emit nestsChanged(m_nests); 
    
    if (!m_ga->getPopulation().empty()) {
        // double overallProgress = static_cast<double>(m_threadsCompleted) / m_ga->getPopulation().size() * 100.0;
        // emit nestProgress(overallProgress, -1); // Overall progress for the generation
    }

    if (m_isNesting) { 
        launchNextTask();
    }
}

void NestingContext::handleWorkerProgress(double percentage, int individualId) {
    // This signal comes from the worker. The context can re-emit it.
    emit nestProgress(percentage, individualId);
}


void NestingContext::sortAndTrimNests() {
    std::sort(m_nests.begin(), m_nests.end(), [](const NestResult& a, const NestResult& b) {
        return a.fitness < b.fitness; 
    });

    const int MAX_SAVED_NESTS = 10; 
    if (m_nests.size() > MAX_SAVED_NESTS) {
        m_nests.erase(m_nests.begin() + MAX_SAVED_NESTS, m_nests.end());
    }
}

// Helper to print polygon for debugging (if needed)
QString polygonToString(const Polygon& poly) {
    QString s = QString("Outer (%1 pts): ").arg(poly.outer.size());
    // for(const auto& pt : poly.outer) s += QString("(%1,%2) ").arg(pt.x).arg(pt.y);
    // s += " Holes: " + QString::number(poly.holes.size());
    return s;
}
