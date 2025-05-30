QT       += core gui widgets concurrent
TARGET = NestingDemo
TEMPLATE = app

# The following assumes the library DeepNestQt is built and its .pro file can be included,
# or that the library is installed in a location where Qt can find it.
# For a local development setup, you might directly link to the library's source or build output.

# Option 1: Include the library's .pri file (if you create one for DeepNestQt)
# include(../../DeepNestQt.pri) # Adjust path as necessary

# Option 2: Link against the library directly (adjust paths based on where DeepNestQt.pro puts the lib)
# This assumes DeepNestQt.pro has DESTDIR = ../../lib (relative to DeepNestQt.pro)
# or some other known location.
# For this example, let's assume the library is built in a folder 'lib' alongside 'examples'
# and the DeepNestQt project root is two levels up from NestingDemo.

DEEPNESTQT_LIB_PATH = C:/kimera/nesting/Deepnest2/DeepNestQt/build/lib #../../build/lib # Adjust if DESTDIR in DeepNestQt.pro is different
LIBS += -L$$DEEPNESTQT_LIB_PATH -lDeepNestQt

# Include path for DeepNestQt headers
# This assumes headers are accessible from the 'src' directory of the DeepNestQt project
DEEPNESTQT_SRC_PATH = ../../src/SvgNest
INCLUDEPATH += $$DEEPNESTQT_SRC_PATH

# If DeepNestQt is built as a subproject (e.g. using SUBDIRS template), qmake handles dependencies.
# For now, this manual linking is a common approach.

SOURCES += \
    main.cpp \
    mainWindow.cpp

HEADERS += \
    mainWindow.h

# FORMS +=
#   mainWindow.ui # If you use Qt Designer
