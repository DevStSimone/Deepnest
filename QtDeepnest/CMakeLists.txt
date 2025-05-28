cmake_minimum_required(VERSION 3.16)
project(QtDeepnest VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Attempt to find Qt5 first
find_package(Qt5 COMPONENTS Widgets Graphics Svg)
set(QT_VERSION 5)

if(NOT Qt5_FOUND)
  message(STATUS "Qt5 not found, trying Qt6.")
  find_package(Qt6 COMPONENTS Widgets Graphics Svg REQUIRED)
  set(QT_VERSION 6)
else()
  message(STATUS "Qt5 found.")
endif()

# Set the CMAKE_PREFIX_PATH to the Qt installation directory if it's not found automatically
# You might need to adjust this path based on your Qt installation
# For example: set(CMAKE_PREFIX_PATH "/path/to/your/Qt/version/platform")

if (QT_VERSION EQUAL 6 AND Qt6_FOUND)
    message(STATUS "Configuring for Qt 6.")
    qt_standard_project_setup()
    qt_add_executable(QtDeepnest
        src/main.cpp
        src/mainwindow.cpp
        # Add Clipper2 source files
        ../Clipper2Lib/src/clipper.engine.cpp
        ../Clipper2Lib/src/clipper.offset.cpp
        ../Clipper2Lib/src/clipper.rectclip.cpp
    )
    qt_import_plugins(QtDeepnest) # Specific to Qt 6
    target_link_libraries(QtDeepnest PRIVATE Qt::Widgets Qt::Graphics Qt::Svg)
    # Add include directory for Clipper2
    target_include_directories(QtDeepnest PRIVATE ../Clipper2Lib/include)
elif(QT_VERSION EQUAL 5 AND Qt5_FOUND)
    message(STATUS "Configuring for Qt 5.")
    # Qt 5 specific setup
    set(CMAKE_INCLUDE_CURRENT_DIR ON) # Required for Qt 5 UI tools
    # qt5_add_resources(RESOURCES resources.qrc) # No resources.qrc file defined yet
    # qt5_wrap_ui(UIS_H mainwindow.ui) # Not using .ui file for MainWindow

    add_executable(QtDeepnest
        src/main.cpp
        src/mainwindow.cpp
        # Add Clipper2 source files
        ../Clipper2Lib/src/clipper.engine.cpp
        ../Clipper2Lib/src/clipper.offset.cpp
        ../Clipper2Lib/src/clipper.rectclip.cpp
        # ${UIS_H} # Not using .ui file
    )
    target_link_libraries(QtDeepnest PRIVATE Qt5::Widgets Qt5::Graphics Qt5::Svg)
    # Add include directory for Clipper2
    target_include_directories(QtDeepnest PRIVATE ../Clipper2Lib/include)
else()
    message(FATAL_ERROR "Neither Qt5 nor Qt6 could be configured successfully.")
endif()

# Define source group for better organization in IDEs
source_group("Source Files" FILES src/main.cpp src/mainwindow.cpp)
source_group("Header Files" FILES include/mainwindow.h)
source_group("Clipper2Lib/Source Files" FILES ../Clipper2Lib/src/clipper.engine.cpp ../Clipper2Lib/src/clipper.offset.cpp ../Clipper2Lib/src/clipper.rectclip.cpp)
source_group("Clipper2Lib/Header Files" FILES ../Clipper2Lib/include/clipper2/clipper.h)

install(TARGETS QtDeepnest
    RUNTIME DESTINATION bin
)
