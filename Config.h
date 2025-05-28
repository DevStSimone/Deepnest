#ifndef CONFIG_H
#define CONFIG_H

#include <QString>

struct AppConfig {
    // From DeepNest config
    double clipperScale = 10000000.0; // Used by Clipper1, might be different for Clipper2 if handled externally
    double curveTolerance = 0.3;    // Max bound for bezier->line segment conversion
    double spacing = 0.0;           // Space between parts
    int rotations = 4;              // Number of part rotations to try
    int populationSize = 10;        // GA population size
    double mutationRate = 10.0;     // GA mutation rate (percentage, e.g., 10 for 10%)
    // int threads = 4; // Will be handled by QThreadPool or number of QThreads
    QString placementType = "gravity"; // 'gravity', 'box', 'convexhull' (currently only gravity/box seems most relevant from JS analysis)
    bool mergeLines = true;
    double timeRatio = 0.5;         // Optimization ratio for material vs. time (related to merged lines)
    double svgImportScale = 72.0;   // Default DPI for SVG import scaling
    bool simplify = false;          // Use rough approximation for parts
    double endpointTolerance = 0.005; // For merging line endpoints

    // Application specific or derived
    // double unitScale; // Factor to convert UI units (e.g. inches) to internal units if necessary
};

#endif // CONFIG_H
