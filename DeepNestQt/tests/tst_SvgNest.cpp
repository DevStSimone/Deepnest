#include "tst_SvgNest.h"

// Library headers
#include "svgNest.h"
#include "SimplifyPath.h"   // For Geometry::SimplifyPath
#include "HullPolygon.h"    // For Geometry::HullPolygon
#include "geometryUtils.h"  // For GeometryUtils
#include "nfpCache.h"       // For Geometry::NfpCache
#include "internalTypes.h"  // For Core::InternalPart (if directly testing conversion/NFP)

#include <QPainterPath>
#include <QPolygonF>
#include <QRectF>
#include <cmath> // For std::abs, M_PI_2 for rotations

TestSvgNest::TestSvgNest() : nestInstance(nullptr) {
}

TestSvgNest::~TestSvgNest() {
}

void TestSvgNest::init() {
    // Called before each test function.
    nestInstance = new SvgNest();
}

void TestSvgNest::cleanup() {
    // Called after each test function.
    delete nestInstance;
    nestInstance = nullptr;
}

// --- Test SvgNest API ---
void TestSvgNest::testSvgNestApi_data() {
    QTest::addColumn<double>("spacing");
    QTest::addColumn<int>("rotations");

    QTest::newRow("default_config") << 0.0 << 4;
    QTest::newRow("custom_config") << 5.0 << 8;
}

void TestSvgNest::testSvgNestApi() {
    QFETCH(double, spacing); // QFETCH_GLOBAL is not needed here as data is per test function
    QFETCH(int, rotations);

    SvgNest::Configuration config = nestInstance->getConfiguration();
    // Default might not match test data exactly, so set it.
    config.spacing = spacing;
    config.rotations = rotations;
    nestInstance->setConfiguration(config);

    SvgNest::Configuration retrievedConfig = nestInstance->getConfiguration();
    QCOMPARE(retrievedConfig.spacing, spacing);
    QCOMPARE(retrievedConfig.rotations, rotations);

    QPainterPath partPath;
    partPath.addRect(0, 0, 10, 10);
    nestInstance->addPart("P1", partPath, 1);
    // QCOMPARE(nestInstance->getPartsCount(), 1); // Assuming SvgNest exposes part count

    QPainterPath sheetPath;
    sheetPath.addRect(0, 0, 100, 100);
    nestInstance->addSheet(sheetPath);
    // QCOMPARE(nestInstance->getSheetsCount(), 1); // Assuming SvgNest exposes sheet count

    nestInstance->clearParts();
    // QCOMPARE(nestInstance->getPartsCount(), 0);
    nestInstance->clearSheets();
    // QCOMPARE(nestInstance->getSheetsCount(), 0);
}

// --- Test SimplifyPath (RDP) ---
void TestSvgNest::testSimplifyPath_data() {
    QTest::addColumn<QPolygonF>("inputPolygon");
    QTest::addColumn<double>("epsilon");
    QTest::addColumn<QPolygonF>("expectedPolygon");

    QPolygonF line; // A line (should not be changed much by RDP)
    line << QPointF(0,0) << QPointF(10,0) << QPointF(20,0) << QPointF(30,0);
    QPolygonF expectedLine = QPolygonF() << QPointF(0,0) << QPointF(30,0); // RDP simplifies collinear points

    QPolygonF square;
    square << QPointF(0,0) << QPointF(10,0) << QPointF(10,10) << QPointF(0,10) << QPointF(0,0);
    
    QPolygonF noisySquare; // A square with a slightly perturbed midpoint on one edge
    noisySquare << QPointF(0,0) << QPointF(5,0.1) << QPointF(10,0) << QPointF(10,10) << QPointF(0,10) << QPointF(0,0);


    QTest::newRow("collinear_points") << line << 0.1 << expectedLine;
    QTest::newRow("square_no_change") << square << 0.05 << square; // Epsilon too small to change square
    QTest::newRow("noisy_square_simplify") << noisySquare << 0.5 << square; // Epsilon large enough to remove noise
}

void TestSvgNest::testSimplifyPath() {
    QFETCH(QPolygonF, inputPolygon);
    QFETCH(double, epsilon);
    QFETCH(QPolygonF, expectedPolygon);

    QPolygonF simplified = Geometry::SimplifyPath::simplify(inputPolygon, epsilon);
    QCOMPARE(simplified, expectedPolygon);
}

// --- Test Convex Hull ---
void TestSvgNest::testConvexHull_data() {
    QTest::addColumn<QPolygonF>("inputPoints");
    QTest::addColumn<QPolygonF>("expectedHull");

    QPolygonF square;
    square << QPointF(0,0) << QPointF(10,0) << QPointF(10,10) << QPointF(0,10);
    QPolygonF expectedSquareHull; // Monotone chain output is sorted, unique, and CCW
    expectedSquareHull << QPointF(0,0) << QPointF(10,0) << QPointF(10,10) << QPointF(0,10);


    QPolygonF pointsInside;
    pointsInside << QPointF(0,0) << QPointF(10,0) << QPointF(10,10) << QPointF(0,10) /*box*/
                 << QPointF(5,5) << QPointF(2,3); // Internal points
    
    QPolygonF triangle;
    triangle << QPointF(0,0) << QPointF(10,0) << QPointF(5,5);
    QPolygonF expectedTriangleHull;
    expectedTriangleHull << QPointF(0,0) << QPointF(10,0) << QPointF(5,5);


    QTest::newRow("square") << square << expectedSquareHull;
    QTest::newRow("points_inside_square") << pointsInside << expectedSquareHull;
    QTest::newRow("triangle") << triangle << expectedTriangleHull;

}

void TestSvgNest::testConvexHull() {
    QFETCH(QPolygonF, inputPoints);
    QFETCH(QPolygonF, expectedHull);
    
    QPolygonF hull = Geometry::HullPolygon::convexHull(inputPoints);
    // The HullPolygon::convexHull sorts points and returns unique points in CCW order.
    // Expected hull data should match this format.
    QCOMPARE(hull, expectedHull);
}

// --- Test GeometryUtils Area ---
void TestSvgNest::testGeometryUtilsArea_data() {
    QTest::addColumn<QPolygonF>("polygon");
    QTest::addColumn<double>("expectedArea");

    QPolygonF square; // CCW by default from QPolygonF constructor if points are sequential
    square << QPointF(0,0) << QPointF(10,0) << QPointF(10,10) << QPointF(0,10);
    QTest::newRow("square_10x10") << square << 100.0;

    QPolygonF triangle; // Base 10, height 5
    triangle << QPointF(0,0) << QPointF(10,0) << QPointF(5,5);
    QTest::newRow("triangle_b10_h5") << triangle << 25.0; // 0.5 * 10 * 5
    
    QPolygonF cw_square; // Clockwise square
    cw_square << QPointF(0,0) << QPointF(0,10) << QPointF(10,10) << QPointF(10,0);
    QTest::newRow("cw_square_10x10") << cw_square << 100.0; // Area is absolute
}

void TestSvgNest::testGeometryUtilsArea() {
    QFETCH(QPolygonF, polygon);
    QFETCH(double, expectedArea);
    QCOMPARE(GeometryUtils::area(polygon), expectedArea);
    
    // Test signedArea based on known orientation (Qt Y-down: CCW is negative, CW is positive)
    if (QTest::currentDataTag() == "square_10x10") { // Assuming CCW
        QVERIFY(GeometryUtils::signedArea(polygon) < 0);
    } else if (QTest::currentDataTag() == "cw_square_10x10") { // CW
        QVERIFY(GeometryUtils::signedArea(polygon) > 0);
    }
}

// --- Test NfpCache ---
void TestSvgNest::testNfpCache_data() {
    QTest::addColumn<QString>("key1");
    QTest::addColumn<QList<QPolygonF>>("nfp1_polys");
    QTest::addColumn<QString>("key2");

    QList<QPolygonF> nfpData1;
    QPolygonF poly1;
    poly1 << QPointF(0,0) << QPointF(1,0) << QPointF(0,1);
    nfpData1.append(poly1);

    QTest::newRow("cache_ops") << "keyA" << nfpData1 << "keyB";
}

void TestSvgNest::testNfpCache() {
    QFETCH(QString, key1);
    QFETCH(QList<QPolygonF>, nfp1_polys);
    QFETCH(QString, key2);

    Geometry::NfpCache cache;
    QCOMPARE(cache.size(), 0);

    Geometry::CachedNfp nfp1_write(nfp1_polys);
    nfp1_write.isValid = true; // Mark as valid for testing findNfp
    cache.storeNfp(key1, nfp1_write);
    QCOMPARE(cache.size(), 1);

    Geometry::CachedNfp nfp1_read;
    QVERIFY(cache.findNfp(key1, nfp1_read));
    QCOMPARE(nfp1_read.nfpPolygons, nfp1_polys);
    QVERIFY(nfp1_read.isValid);

    Geometry::CachedNfp nfp2_read;
    QVERIFY(!cache.findNfp(key2, nfp2_read)); // Key2 not in cache

    QString generatedKey = Geometry::NfpCache::generateKey("partA", 0.0, false, "partB", 90.0, true, false);
    QVERIFY(!generatedKey.isEmpty());
    // Example: "partA_0.0000_fvs_partB_90.0000_t_Bstatic"
    qDebug() << "Generated NfpCache Key:" << generatedKey;

    cache.clear();
    QCOMPARE(cache.size(), 0);
}

// QTEST_APPLESS_MAIN(TestSvgNest) // Or use a separate main_test.cpp
// Note: SvgNest and potentially other components might require a QApplication instance.
// If tests fail due to missing QApplication, a main_test.cpp with QApplication is needed.I have already created `DeepNestQt/tests/tests.pro` and `DeepNestQt/tests/tst_SvgNest.h` in the previous turns. I have also created `DeepNestQt/tests/tst_SvgNest.cpp` in the previous turn.

Now, I will create `DeepNestQt/tests/main_test.cpp`.
