QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += core widgets xml

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
INCLUDEPATH += ./include ../../boost ./src/Clipper2Lib/include
SOURCES += \
    src/Clipper2Lib/src/clipper.engine.cpp \
    src/Clipper2Lib/src/clipper.offset.cpp \
    src/Clipper2Lib/src/clipper.rectclip.cpp \
    src/GeneticAlgorithm.cpp \
    src/GeometryProcessor.cpp \
    src/NestingContext.cpp \
    src/NestingWorker.cpp \
    src/NfpCache.cpp \
    src/NfpGenerator.cpp \
    src/SvgParser.cpp \
    src/boost_minkowski.cpp \
    src/main.cpp \
    src/mainwindow.cpp

HEADERS += \
    include/Config.h \
    include/DataStructures.h \
    include/GeneticAlgorithm.h \
    include/GeometryProcessor.h \
    include/NestingContext.h \
    include/NestingWorker.h \
    include/NfpCache.h \
    include/NfpGenerator.h \
    include/SvgParser.h \
    include/boost_minkowski.h \
    include/mainwindow.h \
    src/Clipper2Lib/include/clipper2/clipper.core.h \
    src/Clipper2Lib/include/clipper2/clipper.engine.h \
    src/Clipper2Lib/include/clipper2/clipper.export.h \
    src/Clipper2Lib/include/clipper2/clipper.h \
    src/Clipper2Lib/include/clipper2/clipper.minkowski.h \
    src/Clipper2Lib/include/clipper2/clipper.offset.h \
    src/Clipper2Lib/include/clipper2/clipper.rectclip.h \
    src/Clipper2Lib/include/clipper2/clipper.version.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
