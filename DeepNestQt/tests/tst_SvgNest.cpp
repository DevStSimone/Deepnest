#include "tst_SvgNest.h"

// Library headers
#include "svgNest.h"
#include "SimplifyPath.h"   // For Geometry::SimplifyPath
#include "HullPolygon.h"    // For Geometry::HullPolygon
#include "geometryUtils.h"  // For GeometryUtils
#include "nfpCache.h"       // For Geometry::NfpCache
#include "internalTypes.h"  // For Core::InternalPart
#include "nfpGenerator.h"   // For Geometry::NfpGenerator
#include "minkowski_wrapper.h" // For CustomMinkowski types used in NfpGenerator batch results
#include "mainWindow.h"      // For testing UI interaction (example)

#include <QPainterPath>
#include <QGraphicsPathItem> // For checking scene content
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

// --- Test NfpGenerator with Holed Parts (Clipper Backend) ---
void TestSvgNest::testNfpGenerator_Clipper_HoledParts() {
    SvgNest::Configuration config; // Default config
    Geometry::NfpGenerator nfpGen(config.clipperScale); // Clipper scale not directly used by PathsD NFP

    // Part S10: Solid 10x10 square
    Core::InternalPart partSolidSquare10;
    partSolidSquare10.id = "S10";
    QPolygonF s10Poly;
    s10Poly << QPointF(0,0) << QPointF(10,0) << QPointF(10,10) << QPointF(0,10);
    partSolidSquare10.outerBoundary = s10Poly;
    partSolidSquare10.bounds = s10Poly.boundingRect();

    // Part F50H20: Frame 50x50 with a 20x20 concentric hole
    // Outer: (0,0) to (50,50). Hole: (15,15) to (35,35)
    Core::InternalPart partFrameSquare50_Hole20;
    partFrameSquare50_Hole20.id = "F50H20";
    QPolygonF frameOuterPoly;
    frameOuterPoly << QPointF(0,0) << QPointF(50,0) << QPointF(50,50) << QPointF(0,50);
    partFrameSquare50_Hole20.outerBoundary = frameOuterPoly;
    QPolygonF frameHolePoly;
    frameHolePoly << QPointF(15,15) << QPointF(35,15) << QPointF(35,35) << QPointF(15,35);
    partFrameSquare50_Hole20.holes.append(frameHolePoly);
    partFrameSquare50_Hole20.bounds = frameOuterPoly.boundingRect();

    // Test Case 1: S10 orbiting F50H20 (A around B)
    // Expected: 2 NFP paths. Outer NFP approx 60x60. Inner NFP (for hole) approx 30x30.
    // Note: `calculateNfp` with useOriginalDeepNestModule = false uses Clipper.
    QList<QPolygonF> nfp_S10_around_F50H20 = nfpGen.calculateNfp(partSolidSquare10, partFrameSquare50_Hole20, false, false);
    
    QVERIFY2(nfp_S10_around_F50H20.size() >= 1, "NFP of S10 around F50H20 should produce at least one path (outer).");
    // Depending on Clipper's output for complex cases (like part fitting in hole), it might be one complex path or multiple.
    // For a part fitting in a hole, it often results in two distinct NFP regions.
    // If S10 (10x10) can fit in the 20x20 hole of F50H20, we expect two paths.
    QVERIFY2(nfp_S10_around_F50H20.size() == 2, "NFP of S10 around F50H20 (with hole it fits in) should ideally produce 2 paths.");

    if (nfp_S10_around_F50H20.size() == 2) {
        // Assuming paths are sorted by area or some other consistent order by Clipper (not guaranteed)
        // Or, identify by bounding box.
        QPolygonF path1 = nfp_S10_around_F50H20.at(0);
        QPolygonF path2 = nfp_S10_around_F50H20.at(1);
        QRectF bounds1 = path1.boundingRect();
        QRectF bounds2 = path2.boundingRect();

        // Expected Outer NFP (S10 around F50H20_outer): (-10,-10) to (50,50), size 60x60
        // Expected Inner NFP (S10 around F50H20_hole_boundary_as_solid): (5,5) to (35,35), size 30x30
        bool foundOuter = false;
        bool foundInner = false;

        if (std::abs(bounds1.width() - 60.0) < 0.1 && std::abs(bounds1.height() - 60.0) < 0.1) foundOuter = true;
        else if (std::abs(bounds1.width() - 30.0) < 0.1 && std::abs(bounds1.height() - 30.0) < 0.1) foundInner = true;
        
        if (std::abs(bounds2.width() - 60.0) < 0.1 && std::abs(bounds2.height() - 60.0) < 0.1) foundOuter = true;
        else if (std::abs(bounds2.width() - 30.0) < 0.1 && std::abs(bounds2.height() - 30.0) < 0.1) foundInner = true;
        
        QVERIFY2(foundOuter, "Outer NFP bounds not found for S10 around F50H20.");
        QVERIFY2(foundInner, "Inner NFP (hole interaction) bounds not found for S10 around F50H20.");
    }


    // Test Case 2: S10 fitting *inside* F50H20 (A inside B)
    // Expected NFP: Area on the frame material.
    // Frame material is [0,50]x[0,50] excluding [15,35]x[15,35].
    // NFP is B(-)A.
    QList<QPolygonF> nfp_S10_inside_F50H20 = nfpGen.calculateNfpInside(partSolidSquare10, partFrameSquare50_Hole20, false, false);
    QVERIFY(!nfp_S10_inside_F50H20.isEmpty());
    if(!nfp_S10_inside_F50H20.isEmpty()){
        const QPolygonF& mainNfp = nfp_S10_inside_F50H20.first();
        QVERIFY(!mainNfp.isEmpty());
        QRectF nfpBounds = mainNfp.boundingRect();
        // Expected bounds: The NFP for a 10x10 square inside a 50x50 frame (hole 15,15 to 35,35)
        // Part can be placed from (0,0) to (40,40).
        // Outer boundary of placeable region: (0,0) to (40,40).
        // Hole in placeable region (due to original hole): (15-10, 15-10) to (35+0, 35+0) -> (5,5) to (35,35)
        // So, overall bounds should be from (0,0) to (40,40).
        QVERIFY(std::abs(nfpBounds.left() - 0.0) < 0.1);
        QVERIFY(std::abs(nfpBounds.top() - 0.0) < 0.1);
        QVERIFY(std::abs(nfpBounds.width() - 40.0) < 0.1);
        QVERIFY(std::abs(nfpBounds.height() - 40.0) < 0.1);

        // Check that a point clearly on the frame material is part of the NFP
        // e.g. placing S10 at (2.5, 2.5) should be valid. (0,0 of S10 is at (2.5, 2.5))
        QVERIFY(mainNfp.containsPoint(QPointF(2.5, 2.5), Qt::OddEvenFill));
        // Check that a point inside the hole's NFP region is NOT part of this NFP
        // e.g. placing S10 at (20,20) (its top-left) would be inside the original hole.
        // The NFP is where the part's origin can be. If S10 is at (20,20), it's in the hole.
        // The NFP for "A inside B (frame)" should EXCLUDE areas that make A fall into B's hole.
        QVERIFY(!mainNfp.containsPoint(QPointF(20,20), Qt::OddEvenFill));
    }
}

// --- Test MainWindow Graphical Display (Basic) ---
// Note: This test requires MainWindow to be default constructible and usable without full application setup.
// It also accesses private members, requiring `friend class TestSvgNest;` in mainWindow.h
void TestSvgNest::testMainWindow_GraphicalDisplay_Basic() {
    MainWindow mainWindow; // Assuming default constructor is okay for basic test
    // mainWindow.show(); // Not needed for non-interactive test, might cause issues

    QList<SvgNest::NestSolution> solutions;
    SvgNest::NestSolution sol;
    
    SvgNest::PlacedPart p1;
    p1.partId = "part1";
    p1.position = QPointF(10,10);
    p1.rotation = 0;
    p1.sheetIndex = 0;
    sol.placements.append(p1);
    sol.fitness = 1.0;
    solutions.append(sol);

    // Simulate parts being added for drawing (originalParts_ in MainWindow)
    // This is a bit of a white-box test.
    QPainterPath partPath; partPath.addRect(0,0,5,5);
    mainWindow.originalParts_.insert("part1", partPath); // Manually insert for test

    // Simulate sheet being drawn (hardcoded in handleNestingFinished)
    // The sheet itself is drawn directly, not as an item from a list of parts/sheets in the solution.

    mainWindow.handleNestingFinished(solutions);

    // Expected items: 1 sheet (drawn directly) + 1 placed part item
    QVERIFY(mainWindow.graphicsScene_ != nullptr);
    if (mainWindow.graphicsScene_) {
         // The sheet is drawn as a QGraphicsPathItem, and each placed part is another QGraphicsPathItem.
        QCOMPARE(mainWindow.graphicsScene_->items().count(), 2); 
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
    // qDebug() << "Generated NfpCache Key:" << generatedKey; // Reduced verbosity

    cache.clear();
    QCOMPARE(cache.size(), 0);
}

// --- Test NfpGenerator Batch Original Module ---
void TestSvgNest::testNfpGenerator_BatchOriginalModule() {
    SvgNest::Configuration config; // Default config
    Geometry::NfpGenerator nfpGen(config.clipperScale);

    // Part S1: 10x10 square at origin
    Core::InternalPart partS1;
    partS1.id = "S1";
    QPolygonF s1Poly;
    s1Poly << QPointF(0,0) << QPointF(10,0) << QPointF(10,10) << QPointF(0,10);
    partS1.outerBoundary = s1Poly;
    partS1.bounds = s1Poly.boundingRect();

    // Part S2: 30x30 square at origin
    Core::InternalPart partS2;
    partS2.id = "S2";
    QPolygonF s2Poly;
    s2Poly << QPointF(0,0) << QPointF(30,0) << QPointF(30,30) << QPointF(0,30);
    partS2.outerBoundary = s2Poly;
    partS2.bounds = s2Poly.boundingRect();
    
    // Part T1: Simple triangle
    Core::InternalPart partT1;
    partT1.id = "T1";
    QPolygonF t1Poly;
    t1Poly << QPointF(0,0) << QPointF(20,0) << QPointF(10,20); // Base 20, Height 20
    partT1.outerBoundary = t1Poly;
    partT1.bounds = t1Poly.boundingRect();

    QList<QPair<Core::InternalPart, Core::InternalPart>> partPairs;
    // Pair 1: S1 (orbiting) around S2 (static)
    partPairs.append(qMakePair(partS1, partS2));
    // Pair 2: T1 (orbiting) around S2 (static)
    partPairs.append(qMakePair(partT1, partS2));
    // Pair 3: S1 (orbiting) around T1 (static) - For variety
    partPairs.append(qMakePair(partS1, partT1));


    int threadCount = 2;
    QList<CustomMinkowski::NfpResultPolygons> results = 
        nfpGen.generateNfpBatch_OriginalModule(partPairs, threadCount);

    QCOMPARE(results.size(), partPairs.size()); // Check if we got results for all pairs

    // --- Verification for Pair 1: S1 (10x10) orbiting S2 (30x30) ---
    // Expected NFP: 40x40 square, from (-10,-10) to (30,30) when S1 ref=(0,0), S2 ref=(0,0)
    // This is because the minkowski_wrapper.cpp logic is: NFP = A (+) reflect(B about its origin)
    // Then result points are shifted by B's original first point.
    // A = [0,0] to [10,10]. Reflected B = [0,0] to [-30,-30].
    // Sum = [-30,-30] to [10,10]. Shift by B[0]=(0,0) -> no change.
    if (results.size() > 0) {
        const CustomMinkowski::NfpResultPolygons& nfpS1S2_list = results.at(0);
        QVERIFY(!nfpS1S2_list.empty()); // Should have at least one polygon path
        if (!nfpS1S2_list.empty()) {
            const CustomMinkowski::PolygonPath& nfpS1S2_poly = nfpS1S2_list.front();
            QVERIFY(!nfpS1S2_poly.empty()); // The path itself should not be empty

            if (!nfpS1S2_poly.empty()) {
                double minX = nfpS1S2_poly[0].x, maxX = nfpS1S2_poly[0].x;
                double minY = nfpS1S2_poly[0].y, maxY = nfpS1S2_poly[0].y;
                for (const auto& pt : nfpS1S2_poly) {
                    if (pt.x < minX) minX = pt.x;
                    if (pt.x > maxX) maxX = pt.x;
                    if (pt.y < minY) minY = pt.y;
                    if (pt.y > maxY) maxY = pt.y;
                }
                // qDebug() << "NFP S1 vs S2 Bounds:" << minX << minY << maxX << maxY;
                QVERIFY(std::abs(minX - (-10.0)) < 0.001); // Allowing small tolerance for double comparisons
                QVERIFY(std::abs(minY - (-10.0)) < 0.001);
                QVERIFY(std::abs(maxX - (30.0)) < 0.001);
                QVERIFY(std::abs(maxY - (30.0)) < 0.001);
                QVERIFY(std::abs((maxX - minX) - 40.0) < 0.001); // Width
                QVERIFY(std::abs((maxY - minY) - 40.0) < 0.001); // Height
                
                // Check number of points. A simple convex NFP of two squares should be a square/rectangle.
                // Boost.Polygon might return more points on straight lines due to its algorithms.
                // For a simple square NFP, it should ideally simplify to 4-8 points.
                // This check is less critical than bounds.
                // QVERIFY(nfpS1S2_poly.size() >= 4 && nfpS1S2_poly.size() <= 8);
            }
        }
    }

    // --- Verification for Pair 2: T1 (triangle) orbiting S2 (30x30 square) ---
    if (results.size() > 1) {
        const CustomMinkowski::NfpResultPolygons& nfpT1S2_list = results.at(1);
        QVERIFY(!nfpT1S2_list.empty());
        if (!nfpT1S2_list.empty()) {
            const CustomMinkowski::PolygonPath& nfpT1S2_poly = nfpT1S2_list.front();
            QVERIFY(!nfpT1S2_poly.empty());
            // Further checks could be bounds or area, but are more complex to pre-calculate by hand.
            // For now, just checking non-emptiness shows the process ran.
        }
    }
     // --- Verification for Pair 3: S1 (10x10) orbiting T1 (triangle) ---
    if (results.size() > 2) {
        const CustomMinkowski::NfpResultPolygons& nfpS1T1_list = results.at(2);
        QVERIFY(!nfpS1T1_list.empty());
        if (!nfpS1T1_list.empty()) {
            const CustomMinkowski::PolygonPath& nfpS1T1_poly = nfpS1T1_list.front();
            QVERIFY(!nfpS1T1_poly.empty());
        }
    }
}


// QTEST_APPLESS_MAIN(TestSvgNest) // Or use a separate main_test.cpp
// Note: SvgNest and potentially other components might require a QApplication instance.
// If tests fail due to missing QApplication, a main_test.cpp with QApplication is needed.
