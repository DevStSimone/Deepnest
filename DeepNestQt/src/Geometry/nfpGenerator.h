#ifndef NFPGENERATOR_H
#define NFPGENERATOR_H

#include <QPolygonF>
#include <QList> // For QList<QPolygonF> if NFP can have holes

// Forward declaration
// struct InternalPart; 

class NfpGenerator {
public:
    NfpGenerator();
    ~NfpGenerator();

    // QList<QPolygonF> calculateNfp(const InternalPart& partA, const InternalPart& partB, bool inside);
    // Placeholder - actual signature will depend on InternalPart definition and NFP library specifics

private:
    // Interface with Clipper2 and/or existing C++ NFP module
};

#endif // NFPGENERATOR_H
