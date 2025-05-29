#include "nfpCache.h"
#include <QStringBuilder> // For efficient string concatenation
#include <algorithm>    // For std::min/max if canonicalizing key order

namespace Geometry {

NfpCache::NfpCache() {
    // Constructor
}

NfpCache::~NfpCache() {
    // Destructor
}

bool NfpCache::findNfp(const QString& key, CachedNfp& result) const {
    QMutexLocker locker(&mutex_);
    if (cache_.contains(key)) {
        result = cache_.value(key);
        return result.isValid; // Only return true if the cached NFP is marked valid
    }
    return false;
}

void NfpCache::storeNfp(const QString& key, const CachedNfp& nfp) {
    QMutexLocker locker(&mutex_);
    // Ensure we are storing a valid NFP, or a placeholder indicating a calculation attempt.
    // The CachedNfp struct has an 'isValid' flag.
    cache_.insert(key, nfp);
}

// Generates a cache key.
// partAIsStatic: true if partA is the static part and partB is orbiting (e.g. NFP for placing B around A).
// This parameter helps distinguish NFP(A,B) from NFP(B,A) contextually.
// Rotations are converted to int after multiplying by a factor to handle precision issues with doubles in keys.
// Flipped states are boolean.
QString NfpCache::generateKey(const QString& partAId, double rotationA, bool flippedA,
                              const QString& partBId, double rotationB, bool flippedB,
                              bool partAIsStatic) { // True if A is static, B orbits. False if B is static, A orbits.
    // Normalize rotation values to handle potential floating point inaccuracies if they represent degrees.
    // E.g., multiply by 100 and convert to int to capture up to 2 decimal places of precision.
    // Or, ensure rotations are always passed with a fixed, limited precision.
    // For simplicity, we'll assume rotations are passed as discrete, comparable values (e.g., 0, 90, 180.123).
    // A common practice is to use integer representation of angles (e.g. degrees * 1000).
    
    // Using QString::number with sufficient precision for doubles, or a custom formatting.
    // Using 'g' format with enough precision, or 'f' with fixed.
    QString rotAStr = QString::number(rotationA, 'f', 4);
    QString rotBStr = QString::number(rotationB, 'f', 4);

    // The order of A and B in the key should be consistent if NFP(A,B) vs NFP(B,A) might be looked up.
    // If partAIsStatic distinguishes this, then direct concatenation is fine.
    // If NFP(A,B) should be the same cache entry as NFP(B,A) under some conditions (e.g. they are symmetric),
    // then part IDs could be sorted. However, typically the "static" part defines the context.
    
    QString keyString = partAId % "_" % rotAStr % "_" % (flippedA ? "t" : "f") % "vs" %
                        partBId % "_" % rotBStr % "_" % (flippedB ? "t" : "f") %
                        (partAIsStatic ? "_Astatic" : "_Bstatic");
    return keyString;
}

void NfpCache::clear() {
    QMutexLocker locker(&mutex_);
    cache_.clear();
}

int NfpCache::size() const {
    QMutexLocker locker(&mutex_); // For thread-safe size reading
    return cache_.size();
}

} // namespace Geometry
