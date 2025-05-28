#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QGraphicsView>
#include <QDockWidget>
#include <QWidget>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QFileDialog>      // Added
#include <QGraphicsScene>   // Added
#include <QPainterPath>     // Added
#include <QInputDialog>
#include <QMenuBar>
#include <QWheelEvent>
#include <QSettings>       // Added
#include <QFormLayout>     // Added
#include <QSpinBox>        // Added
#include <QDoubleSpinBox>  // Added
#include <QComboBox>       // Added
#include <QCheckBox>       // Added
#include <QPushButton>     // Added
#include <QLabel>          // Added
#include <QScrollArea>     
#include <QGraphicsPathItem> // Added

#include "SvgParser.h"
#include "DataStructures.h"
#include "NestingContext.h" // Added for m_nestingContext signal connection

// Forward declaration for Ui::MainWindow if using .ui file
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void wheelEvent(QWheelEvent *event) override; // For zoom on svgDisplayView

private:
    // Ui::MainWindow *ui; // Uncomment if using .ui file

    // UI Elements
    QListWidget *partListWidget;
    QGraphicsView *svgDisplayView;
    QDockWidget *configDockWidget;
    QWidget *configPlaceholderWidget; // Placeholder for configDockWidget
    QGraphicsView *nestDisplayView;

    // Central widget and layout
    QWidget *centralWidget;
    QHBoxLayout *mainLayout; // Main layout for the central widget
    // QVBoxLayout *leftLayout; // Not explicitly used in this structure, partListWidget directly in mainLayout
    QTabWidget *rightTabWidget; // Tab widget for svgDisplayView and nestDisplayView

    // Member Variables
    SvgParser m_svgParser;
    QList<Part> m_importedParts;
    QGraphicsScene* m_svgScene;
    Part* m_currentlySelectedPart;

    // Config Panel related
    QSettings* m_settings;
    QComboBox* m_unitsComboBox;
    QDoubleSpinBox* m_scaleSpinBox; // For SVG import scale
    QDoubleSpinBox* m_spacingSpinBox;
    QDoubleSpinBox* m_curveToleranceSpinBox;
    QSpinBox* m_rotationsSpinBox;
    QSpinBox* m_threadsSpinBox;
    QSpinBox* m_populationSizeSpinBox;
    QSpinBox* m_mutationRateSpinBox; // Represent as integer percentage e.g. 10 for 10%
    QComboBox* m_placementTypeComboBox;
    QCheckBox* m_mergeLinesCheckBox;
    QCheckBox* m_simplifyCheckBox;
    QDoubleSpinBox* m_endpointToleranceSpinBox;
    QPushButton* m_resetDefaultsButton;

    // Labels for units, to be updated
    QLabel* m_spacingUnitLabel;
    QLabel* m_curveToleranceUnitLabel;
    QLabel* m_endpointToleranceUnitLabel;
    QLabel* m_scaleUnitLabel;

    // Nesting result display members
    QListWidget* m_nestListWidget;
    QGraphicsScene* m_nestScene; 
    QList<NestResult> m_currentNests;
    const NestResult* m_selectedNest;
    QTabWidget* m_mainDisplayTabs; 
    QWidget* m_nestingResultsTab;   
    QPushButton* m_exportNestSvgButton; // Added


private slots:
    void onImportSvgClicked();
    void onAddRectangleClicked();
    void onPartSelectionChanged();
    void onPartItemChanged(QListWidgetItem* item);

    // Config Panel slots
    void onSettingChanged(); 
    void onUnitsChanged(int index); 
    void onResetDefaultsClicked();

    // Nesting result slots
    void onNestsUpdated(const QList<NestResult>& nests); 
    void onNestListSelectionChanged(); 
    void onStartNestingClicked(); 
    void onExportNestSvgClicked(); // Added

private:
    void updatePartList();
    void displayPartGeometry(const Part& part);
    void setupPartListContextMenu(); 
    void markSelectedPartAsSheet(bool isSheet);

    // Config Panel helpers
    void setupConfigPanel();
    void loadSettings();
    void saveSettings();
    void applyDefaults(); 
    void updateConfigDisplayUnits();

    // Nesting UI helpers
    void setupNestingUI(); 
    void updateNestListWidget();
    void displayNestResult(const NestResult& nest);
    void switchToNestingViewTab(); // Helper to switch tab
};
#endif // MAINWINDOW_H
