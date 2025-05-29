#ifndef SVGNEST_H
#define SVGNEST_H

#include <QObject>
#include <QPainterPath>
#include <QHash>
#include <QString>
#include <QList>
#include <QVariantMap> // Per la configurazione
#include <QPointF>
#include <QPolygonF>
#include <QThread> // Per operazioni asincrone

// Eventuale forward declaration per classi interne
class NestingWorker; // Classe worker che gira in un thread separato

class SvgNest : public QObject {
    Q_OBJECT

public:
    // Struttura per la configurazione, ricalcando i parametri di DeepNest
    struct Configuration {
        double clipperScale = 10000000.0;
        double curveTolerance = 0.3; // Tolleranza per la conversione curve->polilinee
        double spacing = 0.0;        // Spaziatura tra le parti
        int rotations = 4;           // Numero di rotazioni da provare (es. 0, 90, 180, 270)
        int populationSize = 10;     // Dimensione popolazione per Algoritmo Genetico
        int mutationRate = 10;       // Percentuale tasso di mutazione per GA
        QString placementType = "gravity"; // Strategia di piazzamento: "gravity", "box", "convexhull"
        bool mergeLines = true;          // Unire linee collineari nell'output
        double timeRatio = 0.5;          // Bilanciamento tra uso materiale e tempo di taglio (per mergeLines)
        bool simplifyOnLoad = false;     // Semplificare i tracciati in input
        // Altri parametri rilevanti...
    };

    // Struttura per rappresentare una parte piazzata
    struct PlacedPart {
        QString partId;
        int sheetIndex; // Indice del foglio su cui è piazzata
        QPointF position; // Posizione (es. top-left del bounding box)
        double rotation;  // Rotazione applicata in gradi
        // QPainterPath placedShape; // Opzionale: la forma trasformata
    };

    // Struttura per una soluzione di nesting
    struct NestSolution {
        QList<PlacedPart> placements;
        double fitness; // Valore di fitness della soluzione
        // Altre metriche: utilizzo materiale, numero fogli, etc.
    };

    explicit SvgNest(QObject *parent = nullptr);
    ~SvgNest();

    void setConfiguration(const Configuration& config);
    Configuration getConfiguration() const;

    // API per aggiungere parti e fogli
    void addPart(const QString& id, const QPainterPath& path, int quantity = 1);
    void addSheet(const QPainterPath& sheetPath); // Per ora, un solo tipo di foglio, ma la logica interna può gestirne multipli
    void clearParts();
    void clearSheets();

    // Avvia il processo di nesting in modo asincrono
    void startNestingAsync();
    void stopNesting(); // Richiede l'interruzione del processo

signals:
    void nestingProgress(int percentage); // Percentuale di progresso (0-100)
    void newSolutionFound(const SvgNest::NestSolution& solution); // Nuova soluzione trovata dal GA
    void nestingFinished(const QList<SvgNest::NestSolution>& allSolutions); // Processo completato

private slots:
    void handleWorkerFinished(const QList<SvgNest::NestSolution>& allSolutions); // Slot per gestire la fine del lavoro del thread worker
    void handleWorkerProgress(int percentage);
    void handleWorkerNewSolution(const SvgNest::NestSolution& solution);

private:
    Configuration currentConfig_;
    QHash<QString, QPair<QPainterPath, int>> partsToNest_; // ID -> (Path, Quantità)
    QList<QPainterPath> sheets_; // Lista di fogli disponibili

    QThread* workerThread_;
    NestingWorker* worker_; // Oggetto che esegue il lavoro pesante in un thread separato

    // Metodi interni
    // void preprocessPaths(); // Converte QPainterPath in poligoni interni, applica semplificazioni
};

#endif // SVGNEST_H
