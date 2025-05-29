#include "svgNest.h"
#include "nestingWorker.h" // Required for instantiation
#include <QDebug>          // For logging

SvgNest::SvgNest(QObject *parent) : QObject(parent), workerThread_(nullptr), worker_(nullptr) {
    qDebug() << "SvgNest instance created.";
}

SvgNest::~SvgNest() {
    qDebug() << "SvgNest instance being destroyed.";
    // stopNesting() should ensure thread is properly joined and worker is cleaned up.
    // It's important that stopNesting() can be called multiple times or if already stopped.
    // Call stopNesting unconditionally; it has internal checks.
    stopNesting();
}

void SvgNest::setConfiguration(const Configuration& config) {
    currentConfig_ = config;
    qDebug() << "SvgNest configuration updated.";
}

SvgNest::Configuration SvgNest::getConfiguration() const {
    return currentConfig_;
}

void SvgNest::addPart(const QString& id, const QPainterPath& path, int quantity) {
    if (quantity <= 0) {
        qWarning() << "Attempted to add part with id" << id << "and quantity" << quantity << ". Quantity must be positive.";
        return;
    }
    partsToNest_.insert(id, qMakePair(path, quantity));
    qDebug() << "Part added:" << id << "Quantity:" << quantity;
}

void SvgNest::addSheet(const QPainterPath& sheetPath) {
    if (sheetPath.isEmpty()) {
        qWarning() << "Attempted to add an empty sheet path.";
        return;
    }
    sheets_.append(sheetPath);
    qDebug() << "Sheet added. Total sheets:" << sheets_.size();
}

void SvgNest::clearParts() {
    partsToNest_.clear();
    qDebug() << "All parts cleared.";
}

void SvgNest::clearSheets() {
    sheets_.clear();
    qDebug() << "All sheets cleared.";
}

void SvgNest::startNestingAsync() {
    if (workerThread_ && workerThread_->isRunning()) {
        qWarning() << "Nesting is already in progress. Please stop the current process before starting a new one.";
        return;
    }

    // Clean up previous instances if they somehow exist and weren't cleaned up
    // This ensures that if stopNesting wasn't called or didn't fully complete (e.g. due to event loop issues),
    // we explicitly attempt to clean up before creating new objects.
    if (worker_) {
        qDebug() << "Warning: worker_ was not null at startNestingAsync. Deleting now.";
        delete worker_; // This is risky if it's still processing events. Better to rely on stopNesting.
        worker_ = nullptr;
    }
    if (workerThread_) {
        qDebug() << "Warning: workerThread_ was not null at startNestingAsync. Deleting now.";
        delete workerThread_; // Also risky.
        workerThread_ = nullptr;
    }
    // A more robust way is to call stopNesting() here, which handles graceful shutdown.
    // However, stopNesting() itself might be asynchronous due to wait() and deleteLater.
    // For this iteration, let's assume prior calls or destructor handled it.
    // The destructor now calls stopNesting().
    
    if (partsToNest_.isEmpty()) {
        qWarning() << "No parts to nest. Aborting nesting start.";
        emit nestingFinished(QList<SvgNest::NestSolution>()); // Emit with empty list
        return;
    }
    if (sheets_.isEmpty()) {
        qWarning() << "No sheets to nest on. Aborting nesting start.";
        emit nestingFinished(QList<SvgNest::NestSolution>()); // Emit with empty list
        return;
    }

    qDebug() << "Starting nesting process asynchronously...";
    worker_ = new NestingWorker(partsToNest_, sheets_, currentConfig_);
    workerThread_ = new QThread();
    worker_->moveToThread(workerThread_);

    // Lifecycle connections
    connect(workerThread_, &QThread::started, worker_, &NestingWorker::process);
    connect(worker_, &NestingWorker::finished, this, &SvgNest::handleWorkerFinished); 
    
    // Cleanup connections:
    connect(worker_, &NestingWorker::finished, worker_, &QObject::deleteLater);
    connect(workerThread_, &QThread::finished, workerThread_, &QObject::deleteLater);
    // When thread actually finishes, nullify our pointers to prevent dangling access.
    // This lambda is crucial for pointer hygiene.
    connect(workerThread_, &QThread::finished, this, [this](){
        qDebug() << "Worker thread QThread::finished signal caught in SvgNest lambda. Nullifying SvgNest's pointers.";
        worker_ = nullptr; 
        workerThread_ = nullptr; 
    });

    // Data connections
    connect(worker_, &NestingWorker::progress, this, &SvgNest::handleWorkerProgress);
    connect(worker_, &NestingWorker::newSolution, this, &SvgNest::handleWorkerNewSolution);
    
    workerThread_->start();
    qDebug() << "Nesting worker thread started.";
}

void SvgNest::stopNesting() {
    qDebug() << "SvgNest::stopNesting called.";
    if (workerThread_ && workerThread_->isRunning()) {
        qDebug() << "Worker thread is running. Requesting stop, quitting and waiting for thread.";
        if (worker_) { // Worker might be null if thread started but worker creation failed (unlikely here)
            worker_->requestStop(); 
        }
        workerThread_->quit(); 
        if (!workerThread_->wait(5000)) { // Wait for 5 seconds
            qWarning() << "Worker thread did not quit gracefully within 5s, terminating...";
            workerThread_->terminate();
            workerThread_->wait(); // Wait again after termination to ensure OS resources are freed
        }
        qDebug() << "Worker thread finished processing stop/termination.";
    } else {
        qDebug() << "Worker thread is not running (or already null). No action needed to stop it.";
    }

    // After thread is stopped (or if it was never running/already finished):
    // The QThread::finished lambda is responsible for nullifying worker_ and workerThread_.
    // If the thread never started, worker_ and workerThread_ might still exist if startNestingAsync()
    // was interrupted before thread->start() but after new QThread().
    // However, current startNestingAsync logic doesn't have such an exit path.
    // If worker_ or workerThread_ are not null here, and the thread is confirmed stopped,
    // it implies the QThread::finished signal (and thus our lambda) might not have been processed yet
    // if stopNesting is called from a context where the event loop isn't spinning (e.g. destructor of main app).
    // For robustness, especially in destructor contexts, explicitly delete if objects exist and thread is stopped.
    // However, direct deletion is risky if deleteLater is pending.
    // Relying on the lambda and deleteLater connections is generally safer during normal app lifetime.
    // The destructor of SvgNest will call this. If the application is shutting down,
    // the event loop might not process further deleteLater events.
    
    // If thread is definitively finished and pointers are not null, it might mean the lambda hasn't run.
    // This check is more for scenarios like immediate destruction after stop.
    if (workerThread_ && workerThread_->isFinished() && worker_ ) {
         // If worker_ is not null, and thread is finished, but lambda hasn't nulled worker_ yet.
         // This is tricky. deleteLater is preferred.
    }
    if (workerThread_ && workerThread_->isFinished() && workerThread_ ) {
        // same for workerThread_ itself.
    }
    // For now, we trust the lambda and deleteLater will handle cleanup if event loop is active.
    // If called from SvgNest destructor during app shutdown, direct deletion might be needed if events aren't processed.
    // But Qt's object parenting usually handles child object deletion. NestingWorker is not parented to SvgNest.
    // QThread is a QObject but manages a system thread.
    // The explicit `delete worker_; worker_ = nullptr;` etc. was removed from start of startNestingAsync
    // to rely on this function and the connected lambdas/deleteLater for proper cleanup.

    qDebug() << "SvgNest::stopNesting completed. Pointers should be nullified by QThread::finished lambda if thread ran.";
}

// Slot signature now matches NestingWorker::finished signal
void SvgNest::handleWorkerFinished(const QList<SvgNest::NestSolution>& allSolutions) {
    qDebug() << "NestingWorker::finished signal received by SvgNest::handleWorkerFinished. Solutions count:" << allSolutions.size();
    
    // At this point, worker_ and workerThread_ pointers in SvgNest might not yet be nullified by the lambda,
    // as this slot is called directly by the worker's signal. The lambda is connected to QThread::finished.
    // The worker object itself will be deleted shortly via its QObject::deleteLater slot.
    // The thread object will be deleted shortly via its QObject::deleteLater slot.

    emit nestingFinished(allSolutions); 
    qDebug() << "SvgNest emitted its own nestingFinished signal with solutions from worker.";
}

void SvgNest::handleWorkerProgress(int percentage) {
    emit nestingProgress(percentage);
}

void SvgNest::handleWorkerNewSolution(const SvgNest::NestSolution& solution) {
    emit newSolutionFound(solution);
}
