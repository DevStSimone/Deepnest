#include "nestingWorker.h"
#include "Core/internalTypes.h" // For Core::InternalPart, Core::InternalSheet
#include "Core/nestingEngine.h"  // For Core::NestingEngine
#include "Geometry/geometryUtils.h" // For GeometryUtils::signedArea for orientation
#include "Geometry/SimplifyPath.h" // For path simplification
#include <QTransform> // For path conversions
#include <QDebug>
#include <QThread>     // For QThread::currentThreadId()
#include <algorithm> // For std::reverse, std::sort

// Note: QCoreApplication include was removed as it's not used for msleep or processEvents here.
// If processEvents were to be used, it would be added back.

NestingWorker::NestingWorker(const QHash<QString, QPair<QPainterPath, int>>& rawParts,
                             const QList<QPainterPath>& rawSheets,
                             const SvgNest::Configuration& config)
    : partsRaw_(rawParts), sheetsRaw_(rawSheets), config_(config), stopRequested_(false) {
    qDebug() << "NestingWorker instance created. Raw parts:" << partsRaw_.size() << "Raw sheets:" << sheetsRaw_.size();
}

NestingWorker::~NestingWorker() {
    qDebug() << "NestingWorker instance being destroyed.";
}

void NestingWorker::requestStop() {
    qDebug() << "NestingWorker stop requested.";
    stopRequested_ = true;
    // Note: The NestingEngine, if running a long process, should ideally check this flag.
}

// Helper to ensure correct polygon orientation for Clipper2.
// Outer polygons: CCW (negative signed area in Qt's Y-down system).
// Hole polygons: CW (positive signed area in Qt's Y-down system).
void ensureCorrectOrientation(QPolygonF& polygon, bool isHole) {
    if (polygon.size() < 3) return; 
    double area = GeometryUtils::signedArea(polygon);
    if (isHole) { 
        if (area < 0) { 
            //qDebug() << "Reversing hole to CW (positive area in Qt Y-down)";
            std::reverse(polygon.begin(), polygon.end());
        }
    } else { 
        if (area > 0) { 
            //qDebug() << "Reversing outer boundary to CCW (negative area in Qt Y-down)";
            std::reverse(polygon.begin(), polygon.end());
        }
    }
}


Core::InternalPart NestingWorker::convertPathToInternalPart(const QString& id, const QPainterPath& painterPath, double curveTolerance) {
    Core::InternalPart part;
    part.id = id;

    QList<QPolygonF> subPaths = painterPath.toSubpathPolygons(QTransform(), curveTolerance);
    if (subPaths.isEmpty()) {
        qWarning() << "Part ID" << id << ": toSubpathPolygons resulted in no paths.";
        return part; 
    }

    int outerPathIndex = -1;
    double maxAbsArea = -1.0;

    for (int i = 0; i < subPaths.size(); ++i) {
        if (subPaths[i].size() < 3) continue; 
        double currentAbsArea = std::abs(GeometryUtils::signedArea(subPaths[i]));
        if (currentAbsArea > maxAbsArea) {
            maxAbsArea = currentAbsArea;
            outerPathIndex = i;
        }
    }
    
    if (outerPathIndex == -1) {
         qWarning() << "Part ID" << id << ": Could not determine outer path from subpaths.";
         return part; 
    }

    part.outerBoundary = subPaths[outerPathIndex];
    ensureCorrectOrientation(part.outerBoundary, false);

    for (int i = 0; i < subPaths.size(); ++i) {
        if (i == outerPathIndex) continue;
        if (subPaths[i].size() < 3) continue;
        QPolygonF holeCandidate = subPaths[i];
        ensureCorrectOrientation(holeCandidate, true);
        part.holes.append(holeCandidate);
    }
    
    part.bounds = part.outerBoundary.boundingRect();
    return part;
}

Core::InternalSheet NestingWorker::convertPathToInternalSheet(const QPainterPath& painterPath, double curveTolerance) {
    Core::InternalSheet sheet;
    QList<QPolygonF> subPaths = painterPath.toSubpathPolygons(QTransform(), curveTolerance);
     if (subPaths.isEmpty()) {
        qWarning() << "Sheet conversion: toSubpathPolygons resulted in no paths.";
        return sheet;
    }
    int outerPathIndex = -1;
    double maxAbsArea = -1.0;
    for (int i = 0; i < subPaths.size(); ++i) {
        if (subPaths[i].size() < 3) continue;
        double currentAbsArea = std::abs(GeometryUtils::signedArea(subPaths[i]));
        if (currentAbsArea > maxAbsArea) {
            maxAbsArea = currentAbsArea;
            outerPathIndex = i;
        }
    }
    if (outerPathIndex == -1) {
         qWarning() << "Sheet conversion: Could not determine outer path from subpaths.";
         return sheet;
    }
    sheet.outerBoundary = subPaths[outerPathIndex];
    ensureCorrectOrientation(sheet.outerBoundary, false);

    for (int i = 0; i < subPaths.size(); ++i) {
        if (i == outerPathIndex) continue;
        if (subPaths[i].size() < 3) continue;
        QPolygonF holeCandidate = subPaths[i];
        ensureCorrectOrientation(holeCandidate, true);
        sheet.holes.append(holeCandidate);
    }
    sheet.bounds = sheet.outerBoundary.boundingRect();
    return sheet;
}

void NestingWorker::preprocessInputs() {
    qDebug() << "NestingWorker: Preprocessing inputs...";
    internalParts_.clear();
    internalSheets_.clear();

    // Convert Parts
    for (auto it = partsRaw_.constBegin(); it != partsRaw_.constEnd(); ++it) {
        const QString& partId = it.key();
        const QPainterPath& path = it.value().first;
        int quantity = it.value().second;

        if (path.isEmpty()) { qWarning() << "Skipping empty QPainterPath for part ID:" << partId; continue; }

        Core::InternalPart basePart = convertPathToInternalPart(partId, path, config_.curveTolerance);
        if (!basePart.isValid()) { qWarning() << "Failed to convert part ID:" << partId << "to a valid InternalPart."; continue; }
        
        if (config_.simplifyOnLoad && config_.curveTolerance > 0) {
            basePart.outerBoundary = Geometry::SimplifyPath::simplify(basePart.outerBoundary, config_.curveTolerance);
            QList<QPolygonF> simplifiedHoles;
            for(const QPolygonF& hole : basePart.holes) {
                QPolygonF simplifiedHole = Geometry::SimplifyPath::simplify(hole, config_.curveTolerance);
                if(simplifiedHole.size() >= 3) simplifiedHoles.append(simplifiedHole);
            }
            basePart.holes = simplifiedHoles;
            if(!basePart.outerBoundary.isEmpty()) basePart.bounds = basePart.outerBoundary.boundingRect(); else basePart.bounds = QRectF();
        }

        for (int i = 0; i < quantity; ++i) {
            internalParts_.append(basePart);
        }
    }
    qDebug() << "Converted" << internalParts_.size() << "total part instances.";

    // Convert Sheets
    int sheetCounter = 0;
    for (const QPainterPath& sheetPath : sheetsRaw_) {
         if (sheetPath.isEmpty()) { qWarning() << "Skipping empty QPainterPath for a sheet."; continue; }
        Core::InternalSheet sheet = convertPathToInternalSheet(sheetPath, config_.curveTolerance);
        if (!sheet.isValid()) { qWarning() << "Failed to convert a sheet path to a valid InternalSheet."; continue; }
        sheet.id = "sheet_" + QString::number(sheetCounter++);
        
        if (config_.simplifyOnLoad && config_.curveTolerance > 0) {
             sheet.outerBoundary = Geometry::SimplifyPath::simplify(sheet.outerBoundary, config_.curveTolerance);
             QList<QPolygonF> simplifiedHoles;
             for(const QPolygonF& hole : sheet.holes) {
                 QPolygonF simplifiedHole = Geometry::SimplifyPath::simplify(hole, config_.curveTolerance);
                 if(simplifiedHole.size() >=3) simplifiedHoles.append(simplifiedHole);
             }
             sheet.holes = simplifiedHoles;
             if(!sheet.outerBoundary.isEmpty()) sheet.bounds = sheet.outerBoundary.boundingRect(); else sheet.bounds = QRectF();
        }
        internalSheets_.append(sheet);
    }
    qDebug() << "Converted" << internalSheets_.size() << "sheets.";
}


void NestingWorker::process() {
    qDebug() << "NestingWorker process started in thread:" << QThread::currentThreadId();
    QList<SvgNest::NestSolution> allSolutions;

    preprocessInputs();

    if (internalParts_.isEmpty() || internalSheets_.isEmpty()) {
        qWarning() << "NestingWorker: No valid internal parts or sheets after preprocessing. Aborting.";
        emit finished(allSolutions);
        return;
    }
    
    Core::NestingEngine nestingEngine(config_, internalParts_, internalSheets_);
    // For NestingEngine to be interruptible by this worker's stopRequested_ flag,
    // it needs to be designed to check this flag. One way is to pass a pointer/reference:
    // nestingEngine.setStopFlag(&stopRequested_); (This method needs to be added to NestingEngine)
    // For now, assume NestingEngine::runNesting will complete its current task.
    // If stopRequested_ is set before calling runNesting, we can abort early.
    if (stopRequested_) {
        qDebug() << "NestingWorker: Stop requested before starting NestingEngine.";
        emit finished(allSolutions);
        return;
    }

    qDebug() << "NestingWorker: Starting main nesting logic via NestingEngine.";
    allSolutions = nestingEngine.runNesting(); // This is a blocking call.

    if (stopRequested_) {
        qDebug() << "NestingWorker: Process completed or interrupted due to stop request after NestingEngine attempt.";
        // Solutions found up to the point of interruption (if any) are in allSolutions.
    } else {
        qDebug() << "NestingWorker: Nesting process completed normally via NestingEngine.";
        emit progress(100); 
    }
    
    if (!allSolutions.isEmpty()) {
        // NestingEngine is expected to sort solutions, best first.
        // Emit the best one as a 'newSolution' signal if not stopped early and solutions exist.
        if(!stopRequested_ || (stopRequested_ && !allSolutions.isEmpty())) { // Emit if solutions exist even if stopped
             qDebug() << "NestingWorker: Emitting best solution found by engine as a 'newSolution' signal.";
             emit newSolution(allSolutions.first());
        }
    }

    emit finished(allSolutions);
    qDebug() << "NestingWorker emitted finished signal. Total solutions from engine:" << allSolutions.size();
}
