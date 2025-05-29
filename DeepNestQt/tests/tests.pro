QT       += testlib core gui # gui for QPainterPath
TARGET = tst_DeepNestQt
CONFIG += console
CONFIG -= app_bundle

# Path to the DeepNestQt library source code (one level up from tests directory)
DEEPNESTQT_SRC_DIR = ../src

# Include paths for the library
INCLUDEPATH += $$DEEPNESTQT_SRC_DIR/SvgNest
INCLUDEPATH += $$DEEPNESTQT_SRC_DIR/Core
INCLUDEPATH += $$DEEPNESTQT_SRC_DIR/Geometry
INCLUDEPATH += $$DEEPNESTQT_SRC_DIR/External/Clipper2/Cpp/Clipper2Lib # For Clipper2 headers

# Source files of the library being tested
# This ensures the library code is compiled and linked with the tests.
# Alternatively, if the library is built as a separate target, link against it.
SOURCES += \
    $$DEEPNESTQT_SRC_DIR/SvgNest/svgNest.cpp \
    $$DEEPNESTQT_SRC_DIR/SvgNest/nestingWorker.cpp \
    $$DEEPNESTQT_SRC_DIR/SvgNest/placementTypes.cpp \
    $$DEEPNESTQT_SRC_DIR/Core/nestingEngine.cpp \
    $$DEEPNESTQT_SRC_DIR/Core/geneticAlgorithm.cpp \
    $$DEEPNESTQT_SRC_DIR/Core/internalTypes.cpp \
    $$DEEPNESTQT_SRC_DIR/Geometry/SimplifyPath.cpp \
    $$DEEPNESTQT_SRC_DIR/Geometry/HullPolygon.cpp \
    $$DEEPNESTQT_SRC_DIR/Geometry/geometryUtils.cpp \
    $$DEEPNESTQT_SRC_DIR/Geometry/nfpGenerator.cpp \
    $$DEEPNESTQT_SRC_DIR/Geometry/nfpCache.cpp \
    $$DEEPNESTQT_SRC_DIR/External/Minkowski/minkowski_wrapper.cpp \
    # Clipper2 sources
    $$DEEPNESTQT_SRC_DIR/External/Clipper2/Cpp/Clipper2Lib/clipper.engine.cpp \
    $$DEEPNESTQT_SRC_DIR/External/Clipper2/Cpp/Clipper2Lib/clipper.offset.cpp \
    $$DEEPNESTQT_SRC_DIR/External/Clipper2/Cpp/Clipper2Lib/clipper.rectclip.cpp

# Test source file
SOURCES += tst_SvgNest.cpp main_test.cpp

CONFIG += boost # Also add to test project for consistency

# Define where to find the test class header
HEADERS += tst_SvgNest.h # Will create this header for the test class
