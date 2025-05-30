#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsView>
#include <QGraphicsScene>
#include "SvgNest.h" // Include the main header from the DeepNestQt library

// Forward declare UI class if using .ui file
// namespace Ui {
// class MainWindow;
// }

// Forward declarations for Qt UI elements to be added
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QPushButton;     // Already forward declared effectively by usage
class QProgressBar;    // Already forward declared effectively by usage
class QTextEdit;       // Already forward declared effectively by usage
class QFormLayout;
class QWidget;         // Already forward declared effectively by usage
class QGraphicsView;
class QGraphicsScene;

class TestSvgNest; // Forward declaration for friend class

class MainWindow : public QMainWindow {
    friend class TestSvgNest; // Grant access to test suite
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartNestingClicked();
    void onStopNestingClicked(); 
    void handleNestingProgress(int percentage);
    void handleNewSolution(const SvgNest::NestSolution& solution);
    void handleNestingFinished(const QList<SvgNest::NestSolution>& allSolutions);
    void updateButtonStates(bool nestingInProgress); 

private:
    SvgNest* svgNestInstance_;

    QPushButton* startButton_;
    QPushButton* stopButton_; 
    QProgressBar* progressBar_;
    QTextEdit* resultsTextEdit_;

    // Configuration UI Elements
   QDoubleSpinBox* clipperScaleSpinBox_;
   QDoubleSpinBox* curveToleranceSpinBox_;
   QDoubleSpinBox* spacingSpinBox_;
   QSpinBox* rotationsSpinBox_;
   QSpinBox* populationSizeSpinBox_;
   QSpinBox* mutationRateSpinBox_;
   QComboBox* placementTypeComboBox_;
   QCheckBox* mergeLinesCheckBox_;
   QDoubleSpinBox* timeRatioSpinBox_;
   QCheckBox* simplifyOnLoadCheckBox_;
   QFormLayout* configLayout_;
   QWidget* configWidget_; // To hold the configLayout and be target of setEnabled

    // For displaying nesting results graphically
    QGraphicsView* graphicsView_ = nullptr;
    QGraphicsScene* graphicsScene_ = nullptr;
    QHash<QString, QPainterPath> originalParts_;

    void setupUi();
    void setupConnections();
   void loadConfigurationToUi();   // Load SvgNest config to UI
   void applyUiToConfiguration();  // Apply UI values to SvgNest config
};

#endif // MAINWINDOW_H
