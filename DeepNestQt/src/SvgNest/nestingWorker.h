#ifndef NESTINGWORKER_H
#define NESTINGWORKER_H

#include <QObject>
#include <QHash>
#include <QString>
#include <QList>
#include <QPainterPath> // Required for QPair<QPainterPath, int>

// Forward declare SvgNest types to avoid circular dependency if full SvgNest.h is not needed
// However, SvgNest::Configuration and SvgNest::NestSolution are used directly by value
// or const reference, so including svgNest.h is cleaner here.
#include "svgNest.h" // Provides SvgNest::Configuration and SvgNest::NestSolution

class NestingWorker : public QObject {
    Q_OBJECT
public:
    NestingWorker(const QHash<QString, QPair<QPainterPath, int>>& rawParts,
                  const QList<QPainterPath>& rawSheets,
                  const SvgNest::Configuration& config);
    ~NestingWorker();

public slots:
    void process(); // Metodo principale eseguito dal thread
    void requestStop();

signals:
    void progress(int percentage);
    void newSolution(const SvgNest::NestSolution& solution);
    void finished(const QList<SvgNest::NestSolution>& allSolutions); // O solo la migliore

private:
    // Dati di input (copie o riferimenti costanti)
    QHash<QString, QPair<QPainterPath, int>> partsRaw_; // Renamed from parts_
    QList<QPainterPath> sheetsRaw_;  // Renamed from sheets_
    SvgNest::Configuration config_;
    bool stopRequested_ = false;

    // Converted internal representations
    QList<Core::InternalPart> internalParts_;
    QList<Core::InternalSheet> internalSheets_;

    // Helper for conversion
    Core::InternalPart convertPathToInternalPart(const QString& id, const QPainterPath& painterPath, double curveTolerance);
    Core::InternalSheet convertPathToInternalSheet(const QPainterPath& painterPath, double curveTolerance);
    void preprocessInputs(); // Main preprocessing call

    // Qui andrebbe la logica portata da DeepNest:
    // - Strutture dati interne per poligoni (con gestione fori)
    // - Oggetti per Algoritmo Genetico, NFP Generator (con Clipper2), NFP Cache
    // - Logica di `placeParts`
};

#endif // NESTINGWORKER_H
