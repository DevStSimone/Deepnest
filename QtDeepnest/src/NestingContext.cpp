#include "NestingContext.h"
#include "GeometryProcessor.h" 
#include "NestingWorker.h"     // Added
#include <QThreadPool>         // Added
#include <QDebug>
#include <QRandomGenerator>   

NestingContext::NestingContext(QObject *parent) 
    : QObject(parent), m_ga(nullptr), m_isNesting(false), 
      m_threadsLaunched(0), m_threadsCompleted(0), m_maxConcurrentWorkers(0) {
    m_workerDispatchTimer = new QTimer(this);
    connect(m_workerDispatchTimer, &QTimer::timeout, this, &NestingContext::launchNextTask);
    
    // Note: m_currentConfig.threads was removed from AppConfig struct per previous prompts.
    // Using a default or re-add 'threads' to AppConfig if it's meant to be configurable.
    // For now, using QThread::idealThreadCount() or a fallback.
    m_maxConcurrentWorkers = QThread::idealThreadCount();
    if (m_maxConcurrentWorkers <= 0) {
        m_maxConcurrentWorkers = 2; // Fallback to 2 threads if idealThreadCount is not informative
    }
    // m_nfpCache and m_nfpGenerator are default constructed as members.
}

NestingContext::~NestingContext() {
    stopNesting(); 
    delete m_ga;
    QThreadPool::globalInstance()->waitForDone(); // Wait for all tasks to complete
}

void NestingContext::prepareAdamParts(const QList<Part>& inputParts) {
    m_partsToNest.clear(); 
    m_expandedPartIndices.clear();
    m_placeablePartsForGA.clear();
    m_sheetPartsForWorker.clear();

    for (const Part& part_input : inputParts) {
        Part processedPart = part_input; 

        double offsetDelta = m_currentConfig.spacing * 0.5;
        if (part_input.isSheet) {
            offsetDelta = -offsetDelta; 
        }
        
        if (offsetDelta != 0) { 
            std::vector<Polygon> offsetResult = GeometryProcessor::offsetPolygons({processedPart.geometry}, offsetDelta);
            if (!offsetResult.empty()) {
                processedPart.geometry = offsetResult[0];
            } else {
                qWarning() << "Warning: Offsetting part" ;//<< part_input.id << "resulted in empty geometry. Using original.";
            }
        }

        if (m_currentConfig.simplify) {
            processedPart.geometry = GeometryProcessor::simplifyPolygonDeepnest(processedPart.geometry, m_currentConfig.curveTolerance, part_input.isSheet);
        }
        
        m_partsToNest.append(processedPart); 
    }

    for(int i=0; i < m_partsToNest.size(); ++i){
        const Part& p = m_partsToNest[i];
        if(!p.isSheet){ 
            m_placeablePartsForGA.append(p); // Store the processed placeable part
            for(int q=0; q < p.quantity; ++q){
                // m_expandedPartIndices should store indices relative to m_placeablePartsForGA list for the GA
                m_expandedPartIndices.push_back(m_placeablePartsForGA.size() - 1); 
            }
        } else {
            m_sheetPartsForWorker.append(p); // Store processed sheet part
        }
    }
}


void NestingContext::startNesting(const QList<Part>& parts, const AppConfig& config) {
    if (m_isNesting) {
        qWarning() << "Nesting is already in progress. Stop current nesting first.";
        return;
    }

    resetNesting(); 
    m_nfpCache.clear(); // Clear NFP cache at the start of a new nesting process

    m_currentConfig = config;
    // Update m_maxConcurrentWorkers from config if it exists there, otherwise use constructor's default
    // if (m_currentConfig.contains("threads")) m_maxConcurrentWorkers = m_currentConfig.threads;
    if (m_currentConfig.threads > 0) { // Assuming 'threads' is back in AppConfig for this to work
         m_maxConcurrentWorkers = m_currentConfig.threads;
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
    m_ga = new GeneticAlgorithm(m_placeablePartsForGA, m_currentConfig); // GA works with unique placeable parts
    m_ga->initializePopulation(m_placeablePartsForGA); // This populates GA's internal list to permute

    m_nests.clear();
    m_threadsLaunched = 0;
    m_threadsCompleted = 0; 
    m_isNesting = true;

    qInfo() << "Nesting started. Max workers:" << m_maxConcurrentWorkers << "Population size:" << m_ga->getPopulation().size();
    m_workerDispatchTimer->start(50); // Check more frequently initially
}

void NestingContext::stopNesting() {
    m_isNesting = false;
    if (m_workerDispatchTimer) {
        m_workerDispatchTimer->stop();
    }
    // QThreadPool::globalInstance()->clear(); // This is too aggressive.
    // We need a way to cancel specific NestingWorker tasks if they support it.
    // For now, rely on m_isNesting flag to prevent new work and let existing tasks finish.
    QThreadPool::globalInstance()->waitForDone(1000); // Wait up to 1s for current tasks
    qInfo() << "Nesting stopping...";
}

void NestingContext::resetNesting() {
    stopNesting();
    delete m_ga;
    m_ga = nullptr;
    m_nests.clear();
    m_partsToNest.clear(); 
    m_expandedPartIndices.clear();
    m_placeablePartsForGA.clear();
    m_sheetPartsForWorker.clear();
    m_threadsLaunched = 0;
    m_threadsCompleted = 0;
}

void NestingContext::launchNextTask() {
    if (!m_isNesting || !m_ga) {
        m_workerDispatchTimer->stop(); // Stop timer if nesting should not proceed
        return;
    }

    if (m_ga->allIndividualsProcessed() && m_threadsLaunched == 0) {
        qInfo() << "All individuals processed for current generation. Moving to next generation.";
        m_ga->nextGeneration();
        m_threadsCompleted = 0; 
        emit nestProgress(0.0, -1); // Reset overall progress, -1 for generation change
    }

    while (QThreadPool::globalInstance()->activeThreadCount() < m_maxConcurrentWorkers) {
        Individual ind = m_ga->getNextIndividualToProcess();
        if (ind.id == -1) { 
            break; 
        }

        m_threadsLaunched++; // Increment when task is created and submitted
        
        qInfo() << "Dispatching NestingWorker for individual ID:" << ind.id << ". Active threads:" << QThreadPool::globalInstance()->activeThreadCount();
        
        NestingWorker* worker = new NestingWorker(
            ind.id, 
            ind, 
            m_placeablePartsForGA, // Pass the list of unique, processed, placeable parts
            m_sheetPartsForWorker,   // Pass the list of processed sheets
            m_currentConfig, 
            &m_nfpCache, 
            &m_nfpGenerator
        );

        connect(worker, &NestingWorker::resultReady, this, &NestingContext::handleWorkerResult);
        connect(worker, &NestingWorker::progressUpdated, this, &NestingContext::nestProgress);
        
        QThreadPool::globalInstance()->start(worker);
    }

    if (!m_isNesting && m_workerDispatchTimer->isActive()) { // Ensure timer stops if nesting stopped externally
        m_workerDispatchTimer->stop();
    }
}

void NestingContext::handleWorkerResult(const NestResult& result, int individualId) {
    if (!m_isNesting || !m_ga) { 
        qWarning() << "Nesting stopped or GA is null. Ignoring worker result for ID:" << individualId;
        // If a worker finishes after stopNesting, m_threadsLaunched might be stale.
        // However, QThreadPool manages worker lifetime.
        return; 
    }

    m_ga->updateIndividualFitness(individualId, result.fitness);
    // m_threadsLaunched is decremented when the QRunnable (worker) finishes and is deleted.
    // Here, we are reacting to the result signal. QThreadPool manages active thread count.
    m_threadsCompleted++;

    qInfo() << "Worker finished for individual ID:" << individualId << ". Fitness:" << result.fitness 
            << ". Active threads:" << QThreadPool::globalInstance()->activeThreadCount() 
            << ". Completed this gen:" << m_threadsCompleted;

    m_nests.append(result);
    sortAndTrimNests();

    if (!m_nests.isEmpty()) {
        emit newBestNest(m_nests.first());
    }
    
    // Overall progress: % of individuals evaluated in current generation
    if (!m_ga->getPopulation().empty()) {
        double overallProgress = static_cast<double>(m_threadsCompleted) / m_ga->getPopulation().size() * 100.0;
        // Emitting overall progress via the nestProgress signal from context (not per worker)
        // emit nestProgress(overallProgress, -1); // -1 to indicate overall, not specific worker
    }


    // Try to launch more tasks if current gen not done, or if ready for next gen.
    if (m_isNesting) { // Check again, as state might have changed during worker execution
        launchNextTask();
    }
}

void NestingContext::handleWorkerProgress(double percentage, int individualId) {
    // Forward worker's progress, perhaps prefixing with individual ID or averaging
    // For now, emit directly, but NestingContext's nestProgress might need adjustment for this detail
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

