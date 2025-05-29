#ifndef NFPCACHE_H
#define NFPCACHE_H

#include <QString>
#include <QPolygonF>
#include <QList>
#include <QHash>
#include <QPair> // For QPair used in key generation
#include <QMutex>

namespace Geometry {

// Structure to store a cached No-Fit Polygon.
// An NFP can consist of multiple polygons (e.g., the main NFP and potentially others representing holes or sections).
// Typically, for a pair of polygons A and B, the NFP represents the area where A cannot translate
// without overlapping B, when A is placed relative to a reference point on B.
struct CachedNfp {
    QList<QPolygonF> nfpPolygons; // The NFP itself, could be multiple if complex
    // Add other relevant data if needed, e.g., source part IDs, rotations, etc. for debugging or advanced logic
    bool isValid = false; // Flag to indicate if this cache entry is valid / successfully computed

    CachedNfp() : isValid(false) {} // Default constructor
    CachedNfp(const QList<QPolygonF>& polygons) : nfpPolygons(polygons), isValid(true) {}
};

class NfpCache {
public:
    NfpCache();
    ~NfpCache();

    // Tries to retrieve an NFP from the cache.
    // Returns true if found and populates 'result', false otherwise.
    bool findNfp(const QString& key, CachedNfp& result) const;

    // Stores an NFP into the cache.
    void storeNfp(const QString& key, const CachedNfp& nfp);

    // Generates a unique key for an NFP based on part identifiers, rotations, and flip states.
    // The order of partAId and partBId might matter depending on NFP definition (e.g., NFP(A,B) vs NFP(B,A)).
    // For this implementation, we'll assume a canonical order or that the caller handles it.
    // To ensure NFP(A,B) and NFP(B,A) are distinct or canonicalized before keygen:
    // One common approach is to always sort part IDs alphabetically if the NFP is symmetric.
    // However, if NFP(A,B) is for placing A around B, it's different from NFP(B,A).
    // The `inside` flag (or similar context like "A inside B" vs "A around B") is also critical.
    static QString generateKey(const QString& partAId, double rotationA, bool flippedA,
                               const QString& partBId, double rotationB, bool flippedB,
                               bool partAIsStatic); // partAIsStatic could differentiate A inside B vs B inside A, or similar context

    void clear(); // Clears the cache
    int size() const; // Returns the number of items in the cache

private:
    QHash<QString, CachedNfp> cache_;
    mutable QMutex mutex_; // Added for thread-safety
};

} // namespace Geometry
#endif // NFPCACHE_H
