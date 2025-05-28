#include "mainwindow.h" 
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QThread>
#include <QLabel> // For placeholder widget
#include <QMenuBar> // For menu creation
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox> 
#include <QFileInfo>   
#include <QScrollArea>     
#include <QFormLayout>     
#include <QSpinBox>        
#include <QDoubleSpinBox>  
#include <QComboBox>       
#include <QCheckBox>       
#include <QPushButton>
#include <QSplitter>
#include <QTextStream> 
#include <cmath>       

#include "NestingContext.h" 
#include "GeometryProcessor.h" 

// Conversion factor
const double INCH_TO_MM = 25.4;

// Helper function to convert QPainterPath to SVG 'd' string
QString pathToSvgD(const QPainterPath& path, bool forceClose = false) {
    QString d;
    if (path.isEmpty()) return d;

    for (int i = 0; i < path.elementCount(); ++i) {
        QPainterPath::Element el = path.elementAt(i);
        switch (el.type) {
            case QPainterPath::MoveToElement:
                d += QString("M %1 %2 ").arg(el.x).arg(el.y);
                break;
            case QPainterPath::LineToElement:
                d += QString("L %1 %2 ").arg(el.x).arg(el.y);
                break;
            case QPainterPath::CurveToElement: 
                d += QString("L %1 %2 ").arg(el.x).arg(el.y); 
                break;
            case QPainterPath::CurveToDataElement:
                break; 
            default:
                break; 
        }
    }
    if (forceClose && !d.isEmpty() && !d.endsWith("Z ", Qt::CaseInsensitive)) {
        d += "Z ";
    }
    return d.trimmed();
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), 
      m_currentlySelectedPart(nullptr), 
      m_selectedNest(nullptr),
      m_nestListWidget(nullptr), 
      m_nestScene(nullptr),
      m_mainDisplayTabs(nullptr),
      m_nestingResultsTab(nullptr),
      m_exportNestSvgButton(nullptr)
{
    m_settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, "QtDeepnestOrg", "QtDeepnest", this);
    m_nestingContext = new NestingContext(this); 

    partListWidget = new QListWidget(this);
    partListWidget->setObjectName("partListWidget");

    svgDisplayView = new QGraphicsView(this);
    svgDisplayView->setObjectName("svgDisplayView");
    m_svgScene = new QGraphicsScene(this);
    svgDisplayView->setScene(m_svgScene);
    svgDisplayView->setRenderHint(QPainter::Antialiasing);
    svgDisplayView->setDragMode(QGraphicsView::ScrollHandDrag);

    configDockWidget = new QDockWidget("Configuration", this);
    configDockWidget->setObjectName("configDockWidget");
    addDockWidget(Qt::LeftDockWidgetArea, configDockWidget);
    setupConfigPanel(); 

    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    mainLayout = new QHBoxLayout(centralWidget); 

    m_mainDisplayTabs = new QTabWidget(this); 

    QWidget* setupTabWidget = new QWidget();
    QHBoxLayout* setupTabLayout = new QHBoxLayout(setupTabWidget);
    setupTabLayout->addWidget(partListWidget, 1); 
    setupTabLayout->addWidget(svgDisplayView, 3); 
    m_mainDisplayTabs->addTab(setupTabWidget, "Setup");

    nestDisplayView = new QGraphicsView(this); 
    nestDisplayView->setObjectName("nestDisplayView");
    m_nestScene = new QGraphicsScene(this); 
    nestDisplayView->setScene(m_nestScene);
    nestDisplayView->setRenderHint(QPainter::Antialiasing);
    nestDisplayView->setDragMode(QGraphicsView::ScrollHandDrag);
    
    setupNestingUI(); 

    mainLayout->addWidget(m_mainDisplayTabs); 
    centralWidget->setLayout(mainLayout);

    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *importSvgAction = new QAction(tr("&Import SVG..."), this);
    fileMenu->addAction(importSvgAction);
    connect(importSvgAction, &QAction::triggered, this, &MainWindow::onImportSvgClicked);

    QMenu *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    QAction *addRectangleAction = new QAction(tr("&Add Rectangle..."), this);
    toolsMenu->addAction(addRectangleAction);
    connect(addRectangleAction, &QAction::triggered, this, &MainWindow::onAddRectangleClicked);
    
    QAction *startNestingAction = new QAction(tr("&Start Nesting"), this); 
    toolsMenu->addAction(startNestingAction);
    connect(startNestingAction, &QAction::triggered, this, &MainWindow::onStartNestingClicked);

    connect(partListWidget, &QListWidget::itemSelectionChanged, this, &MainWindow::onPartSelectionChanged);
    connect(partListWidget, &QListWidget::itemChanged, this, &MainWindow::onPartItemChanged);
    
    connect(m_nestingContext, &NestingContext::nestsChanged, this, &MainWindow::onNestsUpdated);
    connect(m_nestingContext, &NestingContext::newBestNest, this, [this](const NestResult& nest){
        qInfo() << "New best nest reported with fitness:" << nest.fitness;
    });
    connect(m_nestingContext, &NestingContext::nestProgress, this, [this](double percentage, int individualId){
        // Log or update progress bar
    });
     connect(m_nestingContext, &NestingContext::nestingFinished, this, [this](){
        QMessageBox::information(this, "Nesting Finished", "The nesting process has completed.");
    });

    setupPartListContextMenu(); 
    setWindowTitle("QtDeepnest");
    resize(1024, 768);
    loadSettings(); 
}

MainWindow::~MainWindow() {}

void MainWindow::setupNestingUI() {
    m_nestingResultsTab = new QWidget(); 
    QVBoxLayout* mainNestingLayout = new QVBoxLayout(m_nestingResultsTab);

    m_nestListWidget = new QListWidget(this); 
    connect(m_nestListWidget, &QListWidget::itemSelectionChanged, this, &MainWindow::onNestListSelectionChanged);

    m_exportNestSvgButton = new QPushButton("Export Selected Nest to SVG", this);
    connect(m_exportNestSvgButton, &QPushButton::clicked, this, &MainWindow::onExportNestSvgClicked);
    m_exportNestSvgButton->setEnabled(false); 

    QSplitter *splitter = new QSplitter(Qt::Horizontal); 
    splitter->addWidget(m_nestListWidget);
    splitter->addWidget(nestDisplayView); 
    splitter->setStretchFactor(0, 1); 
    splitter->setStretchFactor(1, 3); 

    mainNestingLayout->addWidget(splitter); 
    mainNestingLayout->addWidget(m_exportNestSvgButton); 

    m_nestingResultsTab->setLayout(mainNestingLayout); 
    m_mainDisplayTabs->addTab(m_nestingResultsTab, "Nesting Output");
}

void MainWindow::onStartNestingClicked() {
    if (m_importedParts.isEmpty()) {
        QMessageBox::information(this, "Start Nesting", "No parts imported to nest.");
        return;
    }
    AppConfig currentConfig; 
    // Retrieve base scale: SVG units per Inch
    currentConfig.svgImportScale = m_settings->value("Configuration/scaleUnitsPerInch", 72.0).toDouble();
    
    bool isMM = (m_unitsComboBox->currentText() == "Millimeters");
    
    // Spacing, curveTolerance, endpointTolerance are stored in Inches in settings,
    // but AppConfig expects them in the current UI unit for direct use by NestingContext/Worker.
    // This is a point of potential confusion: AppConfig fields should ideally be unit-agnostic or clearly defined.
    // For now, assume AppConfig fields should hold values in the *current display units*.
    // So, we retrieve the "Inches" value from settings and convert it to current display units for AppConfig.
    
    double spacingInches = m_settings->value("Configuration/spacingInches", 0.0).toDouble();
    currentConfig.spacing = isMM ? spacingInches * INCH_TO_MM : spacingInches;

    double curveToleranceInches = m_settings->value("Configuration/curveToleranceInches", 0.01).toDouble();
    currentConfig.curveTolerance = isMM ? curveToleranceInches * INCH_TO_MM : curveToleranceInches;
    
    double endpointToleranceInches = m_settings->value("Configuration/endpointToleranceInches", 0.005).toDouble();
    currentConfig.endpointTolerance = isMM ? endpointToleranceInches * INCH_TO_MM : endpointToleranceInches;

    currentConfig.rotations = m_rotationsSpinBox->value();
    currentConfig.threads = m_threadsSpinBox->value(); 
    currentConfig.populationSize = m_populationSizeSpinBox->value();
    currentConfig.mutationRate = m_mutationRateSpinBox->value(); 
    currentConfig.placementType = m_placementTypeComboBox->currentText().toLower().replace(" ", "");
    currentConfig.mergeLines = m_mergeLinesCheckBox->isChecked();
    currentConfig.simplify = m_simplifyCheckBox->isChecked();
    
    m_nestingContext->startNesting(m_importedParts, currentConfig);
    switchToNestingViewTab(); 
}

void MainWindow::switchToNestingViewTab() {
    if (m_mainDisplayTabs && m_nestingResultsTab) {
        m_mainDisplayTabs->setCurrentWidget(m_nestingResultsTab);
    }
}

void MainWindow::onNestsUpdated(const QList<NestResult>& nests) {
    m_currentNests = nests; 
    updateNestListWidget();

    bool selectionStillValid = false;
    int newSelectionIndex = -1;

    if (m_selectedNest && !m_currentNests.isEmpty()) {
        for (int i = 0; i < m_currentNests.size(); ++i) {
            if (std::abs(m_currentNests[i].fitness - m_selectedNest->fitness) < 1e-6 &&
                m_currentNests[i].sheets.size() == m_selectedNest->sheets.size() &&
                m_currentNests[i].partsPlacedCount == m_selectedNest->partsPlacedCount) {
                newSelectionIndex = i;
                selectionStillValid = true;
                break;
            }
        }
    }

    if (selectionStillValid && newSelectionIndex != -1) {
        m_selectedNest = &m_currentNests[newSelectionIndex];
        m_nestListWidget->setCurrentRow(newSelectionIndex); 
    } else if (!m_currentNests.isEmpty()) {
        m_selectedNest = &m_currentNests.first();
        m_nestListWidget->setCurrentRow(0);
        if (m_nestListWidget->currentRow() == 0) { 
            displayNestResult(*m_selectedNest);
        }
    } else {
        m_selectedNest = nullptr;
        m_nestScene->clear();
    }
    if(m_exportNestSvgButton) m_exportNestSvgButton->setEnabled(m_selectedNest != nullptr);
}

void MainWindow::updateNestListWidget() {
    m_nestListWidget->clear();
    for (int i = 0; i < m_currentNests.size(); ++i) {
        const NestResult& nest = m_currentNests[i];
        QListWidgetItem* item = new QListWidgetItem(
            QString("Nest %1 (Fit: %2, Sheets: %3, Parts: %4)")
            .arg(i + 1).arg(nest.fitness, 0, 'f', 2).arg(nest.sheets.size()).arg(nest.partsPlacedCount), 
            m_nestListWidget);
        item->setData(Qt::UserRole, i); 
    }
}

void MainWindow::onNestListSelectionChanged() {
    QList<QListWidgetItem*> selectedItems = m_nestListWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        m_selectedNest = nullptr;
        if(m_exportNestSvgButton) m_exportNestSvgButton->setEnabled(false);
        return;
    }
    QListWidgetItem* selectedItem = selectedItems.first();
    int nestIndex = selectedItem->data(Qt::UserRole).toInt();
    if (nestIndex >= 0 && nestIndex < m_currentNests.size()) {
        m_selectedNest = &m_currentNests[nestIndex];
        displayNestResult(*m_selectedNest);
    } else {
        m_selectedNest = nullptr;
        m_nestScene->clear();
    }
    if(m_exportNestSvgButton) m_exportNestSvgButton->setEnabled(m_selectedNest != nullptr);
}

void MainWindow::onExportNestSvgClicked() {
    if (!m_selectedNest) {
        QMessageBox::information(this, "Export SVG", "No nest selected to export.");
        return;
    }

    QString filePath = QFileDialog::getSaveFileName(this, tr("Export Nest as SVG"), "", tr("SVG files (*.svg)"));
    if (filePath.isEmpty()) return;

    QStringList svgElements;
    QRectF overallBounds; 
    qreal current_sheet_x_offset_for_bounds = 0;
    
    for (const auto& sheetLayout : m_selectedNest->sheets) {
        Part const* sheetPartDef = nullptr;
        for(const auto& p : m_importedParts) { if(p.id == sheetLayout.sheetPartId) { sheetPartDef = &p; break; } }
        if (!sheetPartDef || sheetPartDef->geometry.outer.empty()) continue;
        
        QPainterPath sheetGeomPath;
        sheetGeomPath.moveTo(sheetPartDef->geometry.outer[0].x, sheetPartDef->geometry.outer[0].y);
        for(size_t k=1; k < sheetPartDef->geometry.outer.size(); ++k) sheetGeomPath.lineTo(sheetPartDef->geometry.outer[k].x, sheetPartDef->geometry.outer[k].y);
        
        QRectF currentSheetRect = sheetGeomPath.boundingRect(); 
        currentSheetRect.translate(current_sheet_x_offset_for_bounds, 0); 
        overallBounds = overallBounds.united(currentSheetRect);
        current_sheet_x_offset_for_bounds += currentSheetRect.width() + 50; 
    }

    if (overallBounds.isEmpty()) overallBounds = QRectF(0,0,100,100); 
    overallBounds.adjust(-20, -20, 20, 20); 

    svgElements.append(QString("<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" "
                   "width=\"%1\" height=\"%2\" viewBox=\"%3 %4 %5 %6\">\n")
           .arg(overallBounds.width()).arg(overallBounds.height())
           .arg(overallBounds.left()).arg(overallBounds.top())
           .arg(overallBounds.width()).arg(overallBounds.height()));
    svgElements.append("<defs/>\n"); 

    qreal current_sheet_x_offset_drawing = overallBounds.left() + 20;
    qreal current_sheet_y_offset_drawing = overallBounds.top() + 20;

    for (const auto& sheetLayout : m_selectedNest->sheets) {
        Part const* sheetPartDef = nullptr;
        for(const auto& p : m_importedParts) { if(p.id == sheetLayout.sheetPartId) { sheetPartDef = &p; break; } }
        if (!sheetPartDef) continue;

        svgElements.append(QString("  <g transform=\"translate(%1 %2)\">\n").arg(current_sheet_x_offset_drawing).arg(current_sheet_y_offset_drawing));
        QPainterPath sheetPath;
        if (!sheetPartDef->geometry.outer.empty()) {
            sheetPath.moveTo(sheetPartDef->geometry.outer[0].x, sheetPartDef->geometry.outer[0].y);
            for(size_t i = 1; i < sheetPartDef->geometry.outer.size(); ++i) sheetPath.lineTo(sheetPartDef->geometry.outer[i].x, sheetPartDef->geometry.outer[i].y);
            sheetPath.closeSubpath(); 
            svgElements.append(QString("    <path d=\"%1\" style=\"fill:none;stroke:darkgray;stroke-width:1;\" />\n").arg(pathToSvgD(sheetPath, true)));
        }
        
        for (const auto& placedPart : sheetLayout.placements) {
            Part const* actualPartDef = nullptr;
            for(const auto& p : m_importedParts) { if(p.id == placedPart.partId) { actualPartDef = &p; break; } }
            if (!actualPartDef) continue;

            svgElements.append(QString("    <g transform=\"translate(%1,%2) rotate(%3)\">\n")
                   .arg(placedPart.position.x).arg(placedPart.position.y).arg(placedPart.rotation));

            QPainterPath partPath;
            if (!actualPartDef->geometry.outer.empty()) {
                partPath.moveTo(actualPartDef->geometry.outer[0].x, actualPartDef->geometry.outer[0].y);
                for(size_t i = 1; i < actualPartDef->geometry.outer.size(); ++i) partPath.lineTo(actualPartDef->geometry.outer[i].x, actualPartDef->geometry.outer[i].y);
                partPath.closeSubpath();
            }
            QString combinedPathData = pathToSvgD(partPath, true); 
            for (const auto& hole_geom : actualPartDef->geometry.holes) {
                QPainterPath holePath;
                if(!hole_geom.empty()){
                    holePath.moveTo(hole_geom[0].x, hole_geom[0].y);
                    for(size_t i = 1; i < hole_geom.size(); ++i) holePath.lineTo(hole_geom[i].x, hole_geom[i].y);
                    holePath.closeSubpath();
                    combinedPathData += " " + pathToSvgD(holePath, true); 
                }
            }
            
            svgElements.append(QString("      <path d=\"%1\" style=\"fill:#%2%3%4;stroke:black;stroke-width:0.5;fill-opacity:0.7;fill-rule:evenodd;\" />\n")
                            .arg(combinedPathData) 
                            .arg(QRandomGenerator::global()->bounded(180,240), 2, 16, QChar('0')) 
                            .arg(QRandomGenerator::global()->bounded(180,240), 2, 16, QChar('0')) 
                            .arg(QRandomGenerator::global()->bounded(180,240), 2, 16, QChar('0')));
            svgElements.append("    </g>\n"); 
        }
        svgElements.append("  </g>\n"); 
        QRectF currentSheetGeomBounds = sheetPath.boundingRect();
        current_sheet_x_offset_drawing += (currentSheetGeomBounds.width() > 0 ? currentSheetGeomBounds.width() : 200) + 50; 
    }
    svgElements.append("</svg>\n");

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        for(const QString& line : svgElements) stream << line;
        file.close();
        QMessageBox::information(this, "Export SVG", "Nest exported successfully.");
    } else {
        QMessageBox::warning(this, "Export SVG", "Could not write to file: " + filePath);
    }
}

void MainWindow::displayNestResult(const NestResult& nest) { 
    m_nestScene->clear();
    qreal current_x_offset = 0;
    qreal scene_max_y = 0; 

    for (const auto& nestSheet : nest.sheets) {
        Part sheetPartDef; 
        bool sheetDefFound = false;
        for(const auto& p : m_importedParts) { 
            if(p.id == nestSheet.sheetPartId) {
                sheetPartDef = p;
                sheetDefFound = true;
                break;
            }
        }

        if (!sheetDefFound) {
            qWarning() << "Sheet definition not found for ID:" << nestSheet.sheetPartId;
            continue; 
        }
        
        QPainterPath sheetDisplayPath; 
        if (!sheetPartDef.geometry.outer.empty()) {
            sheetDisplayPath.moveTo(sheetPartDef.geometry.outer[0].x, sheetPartDef.geometry.outer[0].y);
            for (size_t i = 1; i < sheetPartDef.geometry.outer.size(); ++i) {
                sheetDisplayPath.lineTo(sheetPartDef.geometry.outer[i].x, sheetPartDef.geometry.outer[i].y);
            }
            sheetDisplayPath.closeSubpath();
        }

        QGraphicsPathItem* sheetItem = m_nestScene->addPath(sheetDisplayPath, QPen(Qt::darkGray, 2), QBrush(Qt::white));
        sheetItem->setPos(current_x_offset, 0);
        QRectF sheetBounds = sheetItem->boundingRect(); 

        for (const auto& placedPart : nestSheet.placements) {
            Part originalPartDef;
            bool partDefFound = false;
            for(const auto& p : m_importedParts) { 
                if(p.id == placedPart.partId) {
                    originalPartDef = p;
                    partDefFound = true;
                    break;
                }
            }
            if (!partDefFound) {
                qWarning() << "Part definition not found for ID:" << placedPart.partId;
                continue;
            }

            QPainterPath partDisplayPath;
            if (!originalPartDef.geometry.outer.empty()) {
                partDisplayPath.moveTo(originalPartDef.geometry.outer[0].x, originalPartDef.geometry.outer[0].y);
                for (size_t i = 1; i < originalPartDef.geometry.outer.size(); ++i) {
                    partDisplayPath.lineTo(originalPartDef.geometry.outer[i].x, originalPartDef.geometry.outer[i].y);
                }
                partDisplayPath.closeSubpath();
            }
            for(const auto& hole : originalPartDef.geometry.holes){ 
                QPainterPath holePath;
                 if (!hole.empty()) {
                    holePath.moveTo(hole[0].x, hole[0].y);
                    for (size_t i = 1; i < hole.size(); ++i) {
                        holePath.lineTo(hole[i].x, hole[i].y);
                    }
                    holePath.closeSubpath();
                    partDisplayPath.addPath(holePath); 
                }
            }
            partDisplayPath.setFillRule(Qt::OddEvenFill); 

            QGraphicsPathItem* partItem = m_nestScene->addPath(partDisplayPath);
            partItem->setBrush(QColor(QRandomGenerator::global()->bounded(150,250), 
                                     QRandomGenerator::global()->bounded(150,250), 
                                     QRandomGenerator::global()->bounded(150,250), 180)); 
            partItem->setPen(QPen(Qt::black, 0)); 

            QTransform transform;
            transform.translate(placedPart.position.x, placedPart.position.y);
            transform.rotate(placedPart.rotation); 
            
            partItem->setTransform(transform);
            partItem->setPos(current_x_offset, 0); 
        }
        current_x_offset += (sheetBounds.width() > 0 ? sheetBounds.width() : 200) + 50; 
        if (sheetBounds.height() > scene_max_y) {
            scene_max_y = sheetBounds.height();
        }
    }
    
    if (m_nestScene->items().isEmpty()) {
        nestDisplayView->setSceneRect(-100, -100, 200, 200); 
    } else {
        QRectF fullBounds = m_nestScene->itemsBoundingRect();
        fullBounds.adjust(-20, -20, 20, 20); 
        nestDisplayView->setSceneRect(fullBounds);
        nestDisplayView->fitInView(fullBounds, Qt::KeepAspectRatio);
    }
}

void MainWindow::setupConfigPanel() { 
    QWidget *configWidget = new QWidget();
    QFormLayout *formLayout = new QFormLayout(configWidget);
    configWidget->setLayout(formLayout);

    m_unitsComboBox = new QComboBox();
    m_unitsComboBox->addItems({"Inches", "Millimeters"});
    formLayout->addRow(tr("Units:"), m_unitsComboBox);
    connect(m_unitsComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onUnitsChanged);

    m_scaleSpinBox = new QDoubleSpinBox();
    m_scaleSpinBox->setRange(1.0, 10000.0);
    m_scaleSpinBox->setDecimals(2);
    m_scaleSpinBox->setSingleStep(1.0);
    m_scaleUnitLabel = new QLabel(); 
    QHBoxLayout* scaleLayout = new QHBoxLayout();
    scaleLayout->addWidget(m_scaleSpinBox);
    scaleLayout->addWidget(m_scaleUnitLabel);
    formLayout->addRow(tr("SVG Import Scale:"), scaleLayout);
    connect(m_scaleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onSettingChanged);

    m_spacingSpinBox = new QDoubleSpinBox();
    m_spacingSpinBox->setRange(0.0, 1000.0);
    m_spacingSpinBox->setDecimals(4);
    m_spacingSpinBox->setSingleStep(0.01);
    m_spacingUnitLabel = new QLabel(); 
    QHBoxLayout* spacingLayout = new QHBoxLayout();
    spacingLayout->addWidget(m_spacingSpinBox);
    spacingLayout->addWidget(m_spacingUnitLabel);
    formLayout->addRow(tr("Spacing:"), spacingLayout);
    connect(m_spacingSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onSettingChanged);

    m_curveToleranceSpinBox = new QDoubleSpinBox();
    m_curveToleranceSpinBox->setRange(0.0001, 100.0);
    m_curveToleranceSpinBox->setDecimals(4);
    m_curveToleranceSpinBox->setSingleStep(0.001);
    m_curveToleranceUnitLabel = new QLabel();
    QHBoxLayout* curveToleranceLayout = new QHBoxLayout();
    curveToleranceLayout->addWidget(m_curveToleranceSpinBox);
    curveToleranceLayout->addWidget(m_curveToleranceUnitLabel);
    formLayout->addRow(tr("Curve Tolerance:"), curveToleranceLayout);
    connect(m_curveToleranceSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onSettingChanged);

    m_rotationsSpinBox = new QSpinBox();
    m_rotationsSpinBox->setRange(0, 360); 
    m_rotationsSpinBox->setSingleStep(1);
    formLayout->addRow(tr("Rotations:"), m_rotationsSpinBox);
    connect(m_rotationsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onSettingChanged);

    m_threadsSpinBox = new QSpinBox();
    m_threadsSpinBox->setRange(1, QThread::idealThreadCount() > 0 ? QThread::idealThreadCount() * 2 : 8); 
    m_threadsSpinBox->setSingleStep(1);
    formLayout->addRow(tr("Threads:"), m_threadsSpinBox);
    connect(m_threadsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onSettingChanged);

    m_populationSizeSpinBox = new QSpinBox();
    m_populationSizeSpinBox->setRange(2, 1000);
    m_populationSizeSpinBox->setSingleStep(1);
    formLayout->addRow(tr("Population Size:"), m_populationSizeSpinBox);
    connect(m_populationSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onSettingChanged);
    
    m_mutationRateSpinBox = new QSpinBox();
    m_mutationRateSpinBox->setRange(0, 100);
    m_mutationRateSpinBox->setSuffix("%");
    formLayout->addRow(tr("Mutation Rate:"), m_mutationRateSpinBox);
    connect(m_mutationRateSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onSettingChanged);

    m_placementTypeComboBox = new QComboBox();
    m_placementTypeComboBox->addItems({"Gravity", "Bounding Box", "Squeeze"});
    formLayout->addRow(tr("Placement Type:"), m_placementTypeComboBox);
    connect(m_placementTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onSettingChanged);
    
    m_mergeLinesCheckBox = new QCheckBox(tr("Merge Lines"));
    formLayout->addRow(m_mergeLinesCheckBox);
    connect(m_mergeLinesCheckBox, &QCheckBox::stateChanged, this, &MainWindow::onSettingChanged);

    m_simplifyCheckBox = new QCheckBox(tr("Simplify SVG"));
    formLayout->addRow(m_simplifyCheckBox);
    connect(m_simplifyCheckBox, &QCheckBox::stateChanged, this, &MainWindow::onSettingChanged);

    m_endpointToleranceSpinBox = new QDoubleSpinBox();
    m_endpointToleranceSpinBox->setRange(0.0001, 10.0);
    m_endpointToleranceSpinBox->setDecimals(4);
    m_endpointToleranceSpinBox->setSingleStep(0.001);
    m_endpointToleranceUnitLabel = new QLabel();
    QHBoxLayout* endpointToleranceLayout = new QHBoxLayout();
    endpointToleranceLayout->addWidget(m_endpointToleranceSpinBox);
    endpointToleranceLayout->addWidget(m_endpointToleranceUnitLabel);
    formLayout->addRow(tr("Endpoint Tolerance:"), endpointToleranceLayout);
    connect(m_endpointToleranceSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onSettingChanged);

    m_resetDefaultsButton = new QPushButton(tr("Reset to Defaults"));
    formLayout->addRow(m_resetDefaultsButton);
    connect(m_resetDefaultsButton, &QPushButton::clicked, this, &MainWindow::onResetDefaultsClicked);

    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(configWidget);
    configDockWidget->setWidget(scrollArea);
}

void MainWindow::applyDefaults() { 
    bool blocked = m_unitsComboBox->blockSignals(true);
    m_unitsComboBox->setCurrentText("Inches");
    m_unitsComboBox->blockSignals(blocked);
    
    m_settings->setValue("Configuration/scaleUnitsPerInch", 72.0);

    blocked = m_spacingSpinBox->blockSignals(true);
    // m_spacingSpinBox->setValue(0.0); // Value will be set by updateConfigDisplayUnits
    m_spacingSpinBox->blockSignals(blocked);
    m_settings->setValue("Configuration/spacingInches", 0.0);


    blocked = m_curveToleranceSpinBox->blockSignals(true);
    // m_curveToleranceSpinBox->setValue(0.01); // Value will be set by updateConfigDisplayUnits
    m_curveToleranceSpinBox->blockSignals(blocked);
    m_settings->setValue("Configuration/curveToleranceInches", 0.01);


    blocked = m_rotationsSpinBox->blockSignals(true);
    m_rotationsSpinBox->setValue(4);
    m_rotationsSpinBox->blockSignals(blocked);

    blocked = m_threadsSpinBox->blockSignals(true);
    m_threadsSpinBox->setValue(QThread::idealThreadCount() > 0 ? QThread::idealThreadCount() : 4);
    m_threadsSpinBox->blockSignals(blocked);

    blocked = m_populationSizeSpinBox->blockSignals(true);
    m_populationSizeSpinBox->setValue(10);
    m_populationSizeSpinBox->blockSignals(blocked);

    blocked = m_mutationRateSpinBox->blockSignals(true);
    m_mutationRateSpinBox->setValue(10); 
    m_mutationRateSpinBox->blockSignals(blocked);
    
    blocked = m_placementTypeComboBox->blockSignals(true);
    m_placementTypeComboBox->setCurrentText("Gravity");
    m_placementTypeComboBox->blockSignals(blocked);

    blocked = m_mergeLinesCheckBox->blockSignals(true);
    m_mergeLinesCheckBox->setChecked(true);
    m_mergeLinesCheckBox->blockSignals(blocked);

    blocked = m_simplifyCheckBox->blockSignals(true);
    m_simplifyCheckBox->setChecked(false);
    m_simplifyCheckBox->blockSignals(blocked);

    blocked = m_endpointToleranceSpinBox->blockSignals(true);
    // m_endpointToleranceSpinBox->setValue(0.005); // Value will be set by updateConfigDisplayUnits
    m_endpointToleranceSpinBox->blockSignals(blocked);
    m_settings->setValue("Configuration/endpointToleranceInches", 0.005);


    updateConfigDisplayUnits(); 
}

void MainWindow::loadSettings() { 
    m_settings->beginGroup("Configuration");

    bool blocked = m_unitsComboBox->blockSignals(true);
    m_unitsComboBox->setCurrentText(m_settings->value("units", "Inches").toString());
    m_unitsComboBox->blockSignals(blocked);

    // Values for unit-dependent spinboxes are implicitly loaded by updateConfigDisplayUnits,
    // which reads the base "Inches" values from settings.
    // So, no need to set their values directly here before calling it.

    m_rotationsSpinBox->setValue(m_settings->value("rotations", 4).toInt());
    m_threadsSpinBox->setValue(m_settings->value("threads", QThread::idealThreadCount() > 0 ? QThread::idealThreadCount() : 4).toInt());
    m_populationSizeSpinBox->setValue(m_settings->value("populationSize", 10).toInt());
    m_mutationRateSpinBox->setValue(m_settings->value("mutationRate", 10).toInt());
    m_placementTypeComboBox->setCurrentText(m_settings->value("placementType", "Gravity").toString());
    m_mergeLinesCheckBox->setChecked(m_settings->value("mergeLines", true).toBool());
    m_simplifyCheckBox->setChecked(m_settings->value("simplify", false).toBool());
    
    m_settings->endGroup();
    updateConfigDisplayUnits(); 
}

void MainWindow::saveSettings() { 
    m_settings->beginGroup("Configuration");
    m_settings->setValue("units", m_unitsComboBox->currentText());

    double currentScaleDisplay = m_scaleSpinBox->value();
    if (m_unitsComboBox->currentText() == "Millimeters") {
        m_settings->setValue("scaleUnitsPerInch", currentScaleDisplay * INCH_TO_MM);
    } else { 
        m_settings->setValue("scaleUnitsPerInch", currentScaleDisplay);
    }

    // For other unit-dependent fields, save their current value in Inches
    // Convert from current display unit back to Inches if necessary
    bool isMillimeters = (m_unitsComboBox->currentText() == "Millimeters");
    m_settings->setValue("spacingInches", isMillimeters ? m_spacingSpinBox->value() / INCH_TO_MM : m_spacingSpinBox->value());
    m_settings->setValue("curveToleranceInches", isMillimeters ? m_curveToleranceSpinBox->value() / INCH_TO_MM : m_curveToleranceSpinBox->value());
    m_settings->setValue("endpointToleranceInches", isMillimeters ? m_endpointToleranceSpinBox->value() / INCH_TO_MM : m_endpointToleranceSpinBox->value());
    

    m_settings->setValue("rotations", m_rotationsSpinBox->value());
    m_settings->setValue("threads", m_threadsSpinBox->value());
    m_settings->setValue("populationSize", m_populationSizeSpinBox->value());
    m_settings->setValue("mutationRate", m_mutationRateSpinBox->value());
    m_settings->setValue("placementType", m_placementTypeComboBox->currentText());
    m_settings->setValue("mergeLines", m_mergeLinesCheckBox->isChecked());
    m_settings->setValue("simplify", m_simplifyCheckBox->isChecked());
    
    m_settings->endGroup();
}

void MainWindow::onSettingChanged() { 
    saveSettings();
}

void MainWindow::onUnitsChanged(int index) { 
    Q_UNUSED(index);
    // When units change, the displayed values of unit-dependent settings need to update.
    // The underlying stored value (e.g., spacingInches) doesn't change yet.
    // updateConfigDisplayUnits will read the stored "Inches" value and convert it to the new display unit.
    updateConfigDisplayUnits();
    saveSettings(); // Save the new unit preference itself, and also other settings which might now have different display values
}

void MainWindow::onResetDefaultsClicked() { 
    applyDefaults(); // This sets QSettings values for defaults and then calls updateConfigDisplayUnits.
    saveSettings();  // This ensures that even if applyDefaults didn't explicitly save all, they are saved.
}

void MainWindow::updateConfigDisplayUnits() { 
    bool isMillimeters = (m_unitsComboBox->currentText() == "Millimeters");
    QString lengthUnitSuffix = isMillimeters ? " mm" : " in"; 

    bool blocked; // To prevent triggering onSettingChanged during programmatic value changes

    blocked = m_scaleSpinBox->blockSignals(true);
    double scaleUnitsPerInch = m_settings->value("Configuration/scaleUnitsPerInch", 72.0).toDouble();
    if (isMillimeters) {
        m_scaleSpinBox->setValue(scaleUnitsPerInch / INCH_TO_MM);
        m_scaleUnitLabel->setText("SVG units/mm");
    } else {
        m_scaleSpinBox->setValue(scaleUnitsPerInch);
        m_scaleUnitLabel->setText("SVG units/inch");
    }
    m_scaleSpinBox->blockSignals(blocked);

    blocked = m_spacingSpinBox->blockSignals(true);
    double spacingInches = m_settings->value("Configuration/spacingInches", 0.0).toDouble();
    m_spacingSpinBox->setValue(isMillimeters ? spacingInches * INCH_TO_MM : spacingInches);
    m_spacingUnitLabel->setText(lengthUnitSuffix);
    m_spacingSpinBox->blockSignals(blocked);

    blocked = m_curveToleranceSpinBox->blockSignals(true);
    double curveToleranceInches = m_settings->value("Configuration/curveToleranceInches", 0.01).toDouble();
    m_curveToleranceSpinBox->setValue(isMillimeters ? curveToleranceInches * INCH_TO_MM : curveToleranceInches);
    m_curveToleranceUnitLabel->setText(lengthUnitSuffix);
    m_curveToleranceSpinBox->blockSignals(blocked);

    blocked = m_endpointToleranceSpinBox->blockSignals(true);
    double endpointToleranceInches = m_settings->value("Configuration/endpointToleranceInches", 0.005).toDouble();
    m_endpointToleranceSpinBox->setValue(isMillimeters ? endpointToleranceInches * INCH_TO_MM : endpointToleranceInches);
    m_endpointToleranceUnitLabel->setText(lengthUnitSuffix);
    m_endpointToleranceSpinBox->blockSignals(blocked);
}


void MainWindow::onImportSvgClicked() { 
    QString filePath = QFileDialog::getOpenFileName(this, tr("Import SVG File"), "", tr("SVG files (*.svg)"));
    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Error"), tr("Could not open file: ") + filePath);
        return;
    }
    QString svgString = file.readAll();
    file.close();

    double internalSvgScaleFactor = 1.0; 
    QDomElement rootElement = m_svgParser.load(svgString, internalSvgScaleFactor);
    if (rootElement.isNull()) {
        QMessageBox::warning(this, tr("Error"), tr("Could not parse SVG from file: ") + filePath);
        return;
    }
    
    // Get the "SVG units per inch" scale from settings.
    double scaleUnitsPerInch = m_settings->value("Configuration/scaleUnitsPerInch", 72.0).toDouble();
    if (scaleUnitsPerInch <= 0) scaleUnitsPerInch = 72.0; // Avoid division by zero or invalid scale

    // The unitConversionFactor for SvgParser will convert SVG coordinates to inches.
    double unitConversionFactorForParser = 1.0 / scaleUnitsPerInch;
    
    QList<Part> newParts = m_svgParser.getParts(rootElement, unitConversionFactorForParser);

    QFileInfo fileInfo(filePath);
    QString baseName = fileInfo.baseName();
    int partIndex = 0;
    for (Part& part : newParts) { 
        if (part.id.isEmpty()) {
            part.id = QString("%1_part%2").arg(baseName).arg(partIndex++);
        }
        QString originalId = part.id;
        int duplicateCount = 0;
        bool idExists;
        do {
            idExists = false;
            for(const Part& existingPart : m_importedParts) {
                if (existingPart.id == part.id) {
                    idExists = true;
                    break;
                }
            }
            if (idExists) {
                part.id = QString("%1_%2").arg(originalId).arg(duplicateCount++);
            }
        } while (idExists);
        
        part.sourceFilename = fileInfo.fileName();
        m_importedParts.append(part);
    }
    updatePartList();
}

void MainWindow::updatePartList() { 
    partListWidget->clear();
    for (int i = 0; i < m_importedParts.size(); ++i) {
        const Part& part = m_importedParts[i];
        QListWidgetItem *item = new QListWidgetItem(part.id, partListWidget);
        item->setData(Qt::UserRole, i); 
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(part.isSheet ? Qt::Checked : Qt::Unchecked);
        partListWidget->addItem(item);
    }
}

void MainWindow::onPartSelectionChanged() { 
    QList<QListWidgetItem*> selectedItems = partListWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        m_currentlySelectedPart = nullptr;
        m_svgScene->clear(); 
        return;
    }

    QListWidgetItem *selectedItem = selectedItems.first();
    int partIndex = selectedItem->data(Qt::UserRole).toInt();
    if (partIndex >= 0 && partIndex < m_importedParts.size()) {
        m_currentlySelectedPart = &m_importedParts[partIndex]; 
        displayPartGeometry(*m_currentlySelectedPart);
    } else {
        m_currentlySelectedPart = nullptr;
        m_svgScene->clear();
    }
}

void MainWindow::onPartItemChanged(QListWidgetItem* item) { 
    if (!item) return;
    int partIndex = item->data(Qt::UserRole).toInt();
    if (partIndex >= 0 && partIndex < m_importedParts.size()) {
        m_importedParts[partIndex].isSheet = (item->checkState() == Qt::Checked);
    }
}

void MainWindow::setupPartListContextMenu() { 
    partListWidget->setContextMenuPolicy(Qt::ActionsContextMenu);
    QAction *markAsSheetAction = new QAction("Mark as Sheet", this);
    connect(markAsSheetAction, &QAction::triggered, [this, markAsSheetAction]() {
        if (m_currentlySelectedPart) {
            bool shouldBeSheet = !m_currentlySelectedPart->isSheet;
            markSelectedPartAsSheet(shouldBeSheet);
            markAsSheetAction->setText(shouldBeSheet ? "Unmark as Sheet" : "Mark as Sheet");
        }
    });
    partListWidget->addAction(markAsSheetAction);

    connect(partListWidget, &QListWidget::itemSelectionChanged, [this, markAsSheetAction]() {
        if (m_currentlySelectedPart) {
            markAsSheetAction->setText(m_currentlySelectedPart->isSheet ? "Unmark as Sheet" : "Mark as Sheet");
            markAsSheetAction->setEnabled(true);
        } else {
            markAsSheetAction->setText("Mark as Sheet");
            markAsSheetAction->setEnabled(false);
        }
    });
    markAsSheetAction->setEnabled(false); 
}

void MainWindow::markSelectedPartAsSheet(bool isSheet) { 
    if (!m_currentlySelectedPart) return;
    m_currentlySelectedPart->isSheet = isSheet;
    QList<QListWidgetItem*> selectedItems = partListWidget->selectedItems();
    if (!selectedItems.isEmpty()) {
        QListWidgetItem* item = selectedItems.first();
        int itemPartIndex = item->data(Qt::UserRole).toInt();
        int currentPartIndex = -1;
        for(int i=0; i < m_importedParts.size(); ++i){
            if(&m_importedParts[i] == m_currentlySelectedPart){
                currentPartIndex = i;
                break;
            }
        }
        if(itemPartIndex == currentPartIndex) {
             item->setCheckState(isSheet ? Qt::Checked : Qt::Unchecked);
        }
    }
}

void MainWindow::displayPartGeometry(const Part& part) { 
    m_svgScene->clear();
    if (part.geometry.outer.empty()) {
        return;
    }
    QPainterPath outerPath;
    if (!part.geometry.outer.empty()) {
        outerPath.moveTo(part.geometry.outer[0].x, part.geometry.outer[0].y);
        for (size_t i = 1; i < part.geometry.outer.size(); ++i) {
            outerPath.lineTo(part.geometry.outer[i].x, part.geometry.outer[i].y);
        }
        outerPath.closeSubpath(); 
    }
    for (const auto& hole_vec : part.geometry.holes) {
        if (hole_vec.empty()) continue;
        QPainterPath holePath;
        holePath.moveTo(hole_vec[0].x, hole_vec[0].y);
        for (size_t i = 1; i < hole_vec.size(); ++i) {
            holePath.lineTo(hole_vec[i].x, hole_vec[i].y);
        }
        holePath.closeSubpath();
        outerPath.addPath(holePath); 
    }
    outerPath.setFillRule(Qt::OddEvenFill); 
    m_svgScene->addPath(outerPath, QPen(Qt::black), QBrush(Qt::lightGray));
    if (m_svgScene->itemsBoundingRect().isValid()) {
        svgDisplayView->fitInView(m_svgScene->itemsBoundingRect(), Qt::KeepAspectRatio);
    } else {
        svgDisplayView->setSceneRect(-100, -100, 200, 200); 
    }
}

void MainWindow::onAddRectangleClicked() { 
    bool okW, okH;
    double width = QInputDialog::getDouble(this, tr("Add Rectangle"), tr("Width:"), 100.0, 1.0, 10000.0, 2, &okW);
    if (!okW) return;
    double height = QInputDialog::getDouble(this, tr("Add Rectangle"), tr("Height:"), 100.0, 1.0, 10000.0, 2, &okH);
    if (!okH) return;

    Polygon rectPoly;
    rectPoly.outer.push_back({0, 0});
    rectPoly.outer.push_back({width, 0});
    rectPoly.outer.push_back({width, height});
    rectPoly.outer.push_back({0, height});

    Part newPart;
    newPart.geometry = rectPoly;
    int rectCount = 1;
    for(const auto& p : m_importedParts){
        if(p.id.startsWith("Rectangle_")) rectCount++;
    }
    newPart.id = QString("Rectangle_%1").arg(rectCount);
    newPart.sourceFilename = "Generated";
    m_importedParts.append(newPart);
    updatePartList();
}

void MainWindow::wheelEvent(QWheelEvent *event) { 
    if (svgDisplayView->underMouse()) { 
        if (event->modifiers() & Qt::ControlModifier) {
            const QGraphicsView::ViewportAnchor anchor = svgDisplayView->transformationAnchor();
            svgDisplayView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
            int angle = event->angleDelta().y();
            double scaleFactor;
            if (angle > 0) {
                scaleFactor = 1.15;
            } else {
                scaleFactor = 1.0 / 1.15;
            }
            svgDisplayView->scale(scaleFactor, scaleFactor);
            svgDisplayView->setTransformationAnchor(anchor);
            event->accept(); 
        } else {
            QMainWindow::wheelEvent(event);
        }
    } else if (nestDisplayView && nestDisplayView->underMouse()){ 
         if (event->modifiers() & Qt::ControlModifier) {
            const QGraphicsView::ViewportAnchor anchor = nestDisplayView->transformationAnchor();
            nestDisplayView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
            int angle = event->angleDelta().y();
            double scaleFactor;
            if (angle > 0) {
                scaleFactor = 1.15;
            } else {
                scaleFactor = 1.0 / 1.15;
            }
            nestDisplayView->scale(scaleFactor, scaleFactor);
            nestDisplayView->setTransformationAnchor(anchor);
            event->accept();
        } else {
            QMainWindow::wheelEvent(event);
        }
    }
    else {
        QMainWindow::wheelEvent(event); 
    }
}
