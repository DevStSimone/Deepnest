#include "mainWindow.h"
#include <QApplication>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    SvgNest::registerType();

    MainWindow w;
    w.show();
    return a.exec();
}
