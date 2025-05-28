#include "NfpCache.h"
#include <QMutexLocker>

NfpCache::NfpCache() {
    // Constructor can be empty, QMap and QMutex are initialized by their own constructors.
}

bool NfpCache::has(const NfpKey& key) {
    QMutexLocker locker(&m_mutex);
    return m_cache.contains(key);
}

std::vector<Polygon> NfpCache::get(const NfpKey& key) {
    QMutexLocker locker(&m_mutex);
    // QMap::value() returns a default-constructed value if key is not found.
    // For std::vector<Polygon>, this would be an empty vector.
    return m_cache.value(key); 
}

// The header had 'voidinsert', correcting to 'insert'
void NfpCache::insert(const NfpKey& key, const std::vector<Polygon>& nfp) {
    QMutexLocker locker(&m_mutex);
    m_cache.insert(key, nfp);
}

void NfpCache::clear() {
    QMutexLocker locker(&m_mutex);
    m_cache.clear();
}
