#include "nestingWorker.h"

NestingWorker::NestingWorker(const QHash<QString, QPair<QPainterPath, int>>& parts,
                             const QList<QPainterPath>& sheets,
                             const SvgNest::Configuration& config)
    : parts_(parts), sheets_(sheets), config_(config), stopRequested_(false) {
    // Constructor implementation
}

NestingWorker::~NestingWorker() {
    // Destructor implementation
}

void NestingWorker::process() {
    // Main processing logic will go here
    // For now, just emit a finished signal
    // QList<SvgNest::NestSolution> emptySolutions;
    // emit finished(emptySolutions);
}

void NestingWorker::requestStop() {
    stopRequested_ = true;
}
