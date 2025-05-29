#ifndef TST_SVGNEST_H
#define TST_SVGNEST_H

#include <QObject>
#include <QtTest/QtTest>

// Forward declare classes from the library to be tested
class SvgNest;
namespace Geometry {
    class SimplifyPath;
    class HullPolygon;
    // NfpCache, etc.
}
namespace Core {
    // GeneticAlgorithm, etc.
}


class TestSvgNest : public QObject {
    Q_OBJECT

public:
    TestSvgNest();
    ~TestSvgNest();

private slots:
    // Initialization and cleanup for each test function
    void init();
    void cleanup();

    // Test cases
    void testSvgNestApi_data();
    void testSvgNestApi();

    void testSimplifyPath_data();
    void testSimplifyPath();
    
    void testConvexHull_data();
    void testConvexHull();
    
    void testGeometryUtilsArea_data();
    void testGeometryUtilsArea();

    void testNfpCache_data();
    void testNfpCache();
    
    // Placeholder for more complex tests
    // void testSimpleNestingRun(); // Integration-like test

private:
    SvgNest* nestInstance;
};

#endif // TST_SVGNEST_H
