#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include <vector>
#include <QString> // For Part ID or name if needed

// Forward declaration if Point and Polygon are in a different header (e.g. GeometryTypes.h or NfpGenerator.h)
// For now, assume they are defined here or included from NfpGenerator.h if that's already created.
// If NfpGenerator.h is not created yet by a previous step, define Point and Polygon here.
// To avoid redefinition, we can use include guards or ensure they are in a common header.
// For this subtask, let's assume Point and Polygon from NfpGenerator.h will be used.
// If NfpGenerator.h is not available, the worker should define them as follows:
/*
struct Point {
    double x;
    double y;
};

struct Polygon {
    std::vector<Point> outer;
    std::vector<std::vector<Point>> holes;
};
*/
// Make sure to include the header that defines Point and Polygon if they are separate.
// For now, let's assume NfpGenerator.h (which should contain Point and Polygon) will be included by files using Part.
#include "NfpGenerator.h" // Assuming Point and Polygon are here.

struct Part {
    QString id; // Using QString for ID/name, can be changed later
    Polygon geometry;
    int quantity;
    bool isSheet;
    QString sourceFilename; // To store the original filename

    Part() : quantity(1), isSheet(false) {}
};

struct PlacedPart {
    QString partId; 
    Point position;
    double rotation;
};

struct NestSheet {
    QString sheetPartId; // ID of the Part used as a sheet
    std::vector<PlacedPart> placements;
    double width; // Width of the sheet
    double height; // Height of the sheet
};

struct NestResult {
    std::vector<NestSheet> sheets;
    double fitness;
    double totalAreaUsed;
    double mergedLength;
    int partsPlacedCount;

    NestResult() : fitness(0.0), totalAreaUsed(0.0), mergedLength(0.0), partsPlacedCount(0) {}
};

#endif // DATA_STRUCTURES_H
