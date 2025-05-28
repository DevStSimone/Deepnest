#ifndef NFP_CACHE_H
#define NFP_CACHE_H

#include <QMap>
#include <QMutex>
#include <QString>
#include "DataStructures.h" // For Polygon

struct NfpKey {
    QString partAId; // Or int partASourceId
    QString partBId; // Or int partBSourceId
    int rotationA;   // Representing discrete rotation steps (e.g., 0, 1, 2, 3 for 0, 90, 180, 270 deg)
    int rotationB;
    bool forInnerNfp; // True if this is an NFP for a hole of partA against partB

    bool operator<(const NfpKey& other) const {
        if (partAId != other.partAId) return partAId < other.partAId;
        if (partBId != other.partBId) return partBId < other.partBId;
        if (rotationA != other.rotationA) return rotationA < other.rotationA;
        if (rotationB != other.rotationB) return rotationB < other.rotationB;
        return forInnerNfp < other.forInnerNfp;
    }
};

class NfpCache {
public:
    NfpCache();
    bool has(const NfpKey& key);
    std::vector<Polygon> get(const NfpKey& key);
    void insert(const NfpKey& key, const std::vector<Polygon>& nfp); // Changed from 'voidinsert'
    void clear();

private:
    QMap<NfpKey, std::vector<Polygon>> m_cache;
    QMutex m_mutex; // For thread-safe access
};

#endif // NFP_CACHE_H
