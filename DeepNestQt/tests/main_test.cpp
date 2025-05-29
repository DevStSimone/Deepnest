#include "tst_SvgNest.h"
#include <QtTest/QtTest>
#include <QApplication> // Needed for QPainterPath and other Qt resources

int main(int argc, char *argv[])
{
    QApplication app(argc, argv); // QApplication instance for tests
    TestSvgNest tc;
    return QTest::qExec(&tc, argc, argv);
}
