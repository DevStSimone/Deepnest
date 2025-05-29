#ifndef NFPCACHE_H
#define NFPCACHE_H

#include <QString>
#include <QPolygonF> // Or a more complex structure for cached NFP
#include <QHash>
#include <QList>   // For QList<QPolygonF> if NFP can have holes


// Placeholder for the structure that will be cached
// struct CachedNfp {
//     QPolygonF nfp;
//     QList<QPolygonF> holes; // if applicable
// };

class NfpCache {
public:
    NfpCache();
    ~NfpCache();

    // bool findNfp(const QString& key, CachedNfp& result);
    // void storeNfp(const QString& key, const CachedNfp& nfp);
    // QString generateKey(const QString& partIdA, const QString& partIdB, double rotationA, double rotationB, bool flippedA, bool flippedB);

private:
    // QHash<QString, CachedNfp> cache_;
};

#endif // NFPCACHE_H
