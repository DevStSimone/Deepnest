TEMPLATE = lib
TARGET = DeepNestQt
QT += core concurrent
CONFIG += c++11 # Or a later standard if necessary for Clipper2 or other code

# Define the output directory for the library
DESTDIR = ../lib # Example: put the library in a 'lib' folder outside the project source
# For a more standard build process, especially when others might use the lib,
# you might let the build system decide or use $$[QT_INSTALL_LIBS] for system installs.
# For local/project use, a relative path like ../lib or just letting it be in build dir is fine.

# Specify header files
HEADERS += \
    src/SvgNest/svgNest.h \
    src/SvgNest/nestingWorker.h \
    src/SvgNest/placementTypes.h \
    src/Core/nestingEngine.h \
    src/Core/geneticAlgorithm.h \
    src/Core/internalTypes.h \
    src/Geometry/SimplifyPath.h \
    src/Geometry/HullPolygon.h \
    src/Geometry/geometryUtils.h \
    src/Geometry/nfpGenerator.h \
    src/Geometry/nfpCache.h

# Specify source files
SOURCES += \
    src/SvgNest/svgNest.cpp \
    src/SvgNest/nestingWorker.cpp \
    src/SvgNest/placementTypes.cpp \
    src/Core/nestingEngine.cpp \
    src/Core/geneticAlgorithm.cpp \
    src/Core/internalTypes.cpp \
    src/Geometry/SimplifyPath.cpp \
    src/Geometry/HullPolygon.cpp \
    src/Geometry/geometryUtils.cpp \
    src/Geometry/nfpGenerator.cpp \
    src/Geometry/nfpCache.cpp

# Include paths
INCLUDEPATH += \
    src/SvgNest \
    src/Core \
    src/Geometry \
    src/External/Clipper2/Cpp # Main include path for Clipper2 headers

# Clipper2 source files
# Note: If Clipper2 is header-only for some parts, those .cpp files might not exist or be needed.
# The worker reported clipper.core.cpp was not found, which is fine for header-only.
CLIPPER2_SRC_DIR = src/External/Clipper2/Cpp/Clipper2Lib
SOURCES += \
    $$CLIPPER2_SRC_DIR/clipper.engine.cpp \
    $$CLIPPER2_SRC_DIR/clipper.offset.cpp \
    $$CLIPPER2_SRC_DIR/clipper.rectclip.cpp \
    src/External/Minkowski/minkowski_wrapper.cpp
    # Add clipper.core.cpp here if it exists and is needed for compilation,
    # otherwise, if it's header-only, it's fine.

# Make sure headers from Clipper2Lib are accessible
INCLUDEPATH += $$CLIPPER2_SRC_DIR
INCLUDEPATH += src/External/Minkowski # Added for minkowski_wrapper.h

# For building a shared library (DLL/SO)
# CONFIG += sharedlib # Or staticlib if you prefer

CONFIG += boost # Attempt to make qmake auto-configure for Boost
# If specific Boost libraries are needed and CONFIG += boost isn't enough:
# win32: LIBS += -L"path/to/boost/libs" -llibboost_polygon-vcXXX-mt-gd-x64-1_XX
# unix: LIBS += -lboost_polygon

# If your library is intended to be used by other qmake projects, you might want to create a .pri file.
# For now, this .pro file will build the library itself.

# Platform-specific configurations (if any)
# macx {
#     # macOS specific settings
# }
# win32 {
#     # Windows specific settings
# }
# unix {
#     # Linux/Unix specific settings
# }
