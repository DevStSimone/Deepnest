#include "mainWindow.h"
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout> 
#include <QProgressBar>
#include <QTextEdit>
#include <QDebug>
#include <QPainterPath> 
#include <QFormLayout>    
#include <QGroupBox>      
#include <QScrollArea>    
#include <QSpinBox>       
#include <QDoubleSpinBox> 
#include <QComboBox>      
#include <QCheckBox>      
#include <QGraphicsPathItem> // For adding paths to the scene
#include <QPen>              // For styling path items

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    svgNestInstance_(new SvgNest(this)),
    startButton_(nullptr), 
    stopButton_(nullptr),
    progressBar_(nullptr),
    resultsTextEdit_(nullptr),
    // Initialize config pointers
    clipperScaleSpinBox_(nullptr), curveToleranceSpinBox_(nullptr), spacingSpinBox_(nullptr),
    rotationsSpinBox_(nullptr), populationSizeSpinBox_(nullptr), mutationRateSpinBox_(nullptr),
    placementTypeComboBox_(nullptr), mergeLinesCheckBox_(nullptr), timeRatioSpinBox_(nullptr),
    simplifyOnLoadCheckBox_(nullptr), configLayout_(nullptr), configWidget_(nullptr)
{
    setupUi(); 
    setupConnections();
    loadConfigurationToUi(); 
    updateButtonStates(false); 
}

MainWindow::~MainWindow() {
    // SvgNest and UI elements parented to MainWindow or its child widgets will be deleted by Qt.
}

void MainWindow::setupUi() {
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    // --- Configuration Group ---
    if (!configWidget_) { 
        QGroupBox* configGroupBox = new QGroupBox("Configuration", this);
        configLayout_ = new QFormLayout();
        configWidget_ = new QWidget(); 

        clipperScaleSpinBox_ = new QDoubleSpinBox();
        clipperScaleSpinBox_->setRange(1000, 1000000000); clipperScaleSpinBox_->setDecimals(0);
        clipperScaleSpinBox_->setValue(10000000.0); // Default from SvgNest::Configuration
        clipperScaleSpinBox_->setEnabled(false); 
        configLayout_->addRow("Clipper Scale:", clipperScaleSpinBox_);

        curveToleranceSpinBox_ = new QDoubleSpinBox();
        curveToleranceSpinBox_->setRange(0.01, 10.0); curveToleranceSpinBox_->setSingleStep(0.01); curveToleranceSpinBox_->setDecimals(3);
        curveToleranceSpinBox_->setValue(0.3); // Default
        configLayout_->addRow("Curve Tolerance:", curveToleranceSpinBox_);

        spacingSpinBox_ = new QDoubleSpinBox();
        spacingSpinBox_->setRange(0.0, 1000.0); spacingSpinBox_->setSingleStep(0.1); spacingSpinBox_->setDecimals(2);
        spacingSpinBox_->setValue(0.0); // Default
        configLayout_->addRow("Spacing:", spacingSpinBox_);

        rotationsSpinBox_ = new QSpinBox();
        rotationsSpinBox_->setRange(0, 360); rotationsSpinBox_->setValue(4); // Default (0, 90, 180, 270)
        configLayout_->addRow("Rotations (0-360, steps):", rotationsSpinBox_);

        populationSizeSpinBox_ = new QSpinBox();
        populationSizeSpinBox_->setRange(2, 10000); populationSizeSpinBox_->setStepType(QAbstractSpinBox::AdaptiveDecimalStepType);
        populationSizeSpinBox_->setValue(10); // Default
        configLayout_->addRow("GA Population Size:", populationSizeSpinBox_);

        mutationRateSpinBox_ = new QSpinBox();
        mutationRateSpinBox_->setRange(0, 100); mutationRateSpinBox_->setSuffix("%");
        mutationRateSpinBox_->setValue(10); // Default
        configLayout_->addRow("GA Mutation Rate:", mutationRateSpinBox_);

        placementTypeComboBox_ = new QComboBox();
        placementTypeComboBox_->addItems({"gravity", "box", "convexhull"}); // As per SvgNest::Configuration plan
        configLayout_->addRow("Placement Type:", placementTypeComboBox_);

        mergeLinesCheckBox_ = new QCheckBox("Merge Collinear Lines");
        mergeLinesCheckBox_->setChecked(true); // Default
        configLayout_->addRow(mergeLinesCheckBox_);

        timeRatioSpinBox_ = new QDoubleSpinBox();
        timeRatioSpinBox_->setRange(0.0, 1.0); timeRatioSpinBox_->setSingleStep(0.01); timeRatioSpinBox_->setDecimals(2);
        timeRatioSpinBox_->setValue(0.5); // Default
        connect(mergeLinesCheckBox_, &QCheckBox::toggled, timeRatioSpinBox_, &QDoubleSpinBox::setEnabled);
        timeRatioSpinBox_->setEnabled(mergeLinesCheckBox_->isChecked()); // Initial state
        configLayout_->addRow("Time Ratio (for merging):", timeRatioSpinBox_);

        simplifyOnLoadCheckBox_ = new QCheckBox("Simplify Paths on Load");
        simplifyOnLoadCheckBox_->setChecked(false); // Default
        configLayout_->addRow(simplifyOnLoadCheckBox_);

        configWidget_->setLayout(configLayout_); 

        QScrollArea* scrollArea = new QScrollArea();
        scrollArea->setWidgetResizable(true);
        scrollArea->setWidget(configWidget_); 

        QVBoxLayout* configGroupLayout = new QVBoxLayout(); 
        configGroupLayout->addWidget(scrollArea);
        configGroupBox->setLayout(configGroupLayout);
        
        mainLayout->insertWidget(0, configGroupBox); 
    }

    // --- Controls Group ---
    // Ensure these are created if not already (e.g. if setupUi is called multiple times, though not typical)
    QGroupBox* controlsGroupBox = nullptr;
    QHBoxLayout* buttonLayout = nullptr; // To hold Start and Stop buttons

    // Find if controlsGroupBox already exists (e.g. if it was added by a previous partial setupUi call)
    QList<QGroupBox*> gboxes = centralWidget->findChildren<QGroupBox*>();
    for(QGroupBox* gb : gboxes) {
        if(gb->title() == "Controls") {
            controlsGroupBox = gb;
            break;
        }
    }
    if(!controlsGroupBox) {
        controlsGroupBox = new QGroupBox("Controls", this);
        QVBoxLayout* controlsLayout = new QVBoxLayout(); // This will be the main layout for controls group
        controlsGroupBox->setLayout(controlsLayout);
        mainLayout->addWidget(controlsGroupBox); // Add to main layout if new
    }
    QVBoxLayout* controlsLayout = qobject_cast<QVBoxLayout*>(controlsGroupBox->layout());
    if(!controlsLayout) { // Should not happen if created above
        controlsLayout = new QVBoxLayout();
        controlsGroupBox->setLayout(controlsLayout);
    }

    // Ensure buttons are created and part of a horizontal layout
    if (!startButton_) startButton_ = new QPushButton("Start Nesting", this);
    if (!stopButton_) stopButton_ = new QPushButton("Stop Nesting", this);
    
    // Check if buttonLayout already exists as a child of controlsLayout
    QLayoutItem* item = controlsLayout->itemAt(0); // Assuming button layout is first
    if (item && qobject_cast<QHBoxLayout*>(item->layout())) {
        buttonLayout = qobject_cast<QHBoxLayout*>(item->layout());
    } else {
        buttonLayout = new QHBoxLayout();
        controlsLayout->insertLayout(0, buttonLayout); // Insert at the top of controls
    }
    // Ensure buttons are in the layout (handles case where they might be re-parented or layout cleared)
    if(buttonLayout->indexOf(startButton_) == -1) buttonLayout->addWidget(startButton_);
    if(buttonLayout->indexOf(stopButton_) == -1) buttonLayout->addWidget(stopButton_);


    if (!progressBar_) {
        progressBar_ = new QProgressBar(this);
        progressBar_->setRange(0,100); progressBar_->setValue(0);
        controlsLayout->addWidget(progressBar_); // Add if new
    }

    // --- Results Group --- (Ensure it's created once)
    QGroupBox* resultsGroupBox = nullptr;
     for(QGroupBox* gb : gboxes) {
        if(gb->title() == "Results") {
            resultsGroupBox = gb;
            break;
        }
    }
    if(!resultsGroupBox) {
        resultsGroupBox = new QGroupBox("Results", this);
        QVBoxLayout* resultsLayout = new QVBoxLayout();
        resultsGroupBox->setLayout(resultsLayout);
        mainLayout->addWidget(resultsGroupBox);
    }
    QVBoxLayout* resultsLayout = qobject_cast<QVBoxLayout*>(resultsGroupBox->layout());
     if(!resultsLayout) { // Should not happen
        resultsLayout = new QVBoxLayout();
        resultsGroupBox->setLayout(resultsLayout);
    }

    if (!resultsTextEdit_) {
        resultsTextEdit_ = new QTextEdit(this);
        resultsTextEdit_->setReadOnly(true);
        resultsLayout->addWidget(resultsTextEdit_); // Add if new
    }

    // --- Graphics View for Nesting Result ---
    if (!graphicsScene_) {
        graphicsScene_ = new QGraphicsScene(this);
    }
    if (!graphicsView_) {
        graphicsView_ = new QGraphicsView(graphicsScene_, this);
        graphicsView_->setRenderHint(QPainter::Antialiasing);
        graphicsView_->setMinimumSize(300, 200); // Example minimum size
        if (resultsLayout) { // Should exist if resultsGroupBox exists
            resultsLayout->addWidget(graphicsView_);
        } else { // Fallback if resultsLayout somehow wasn't created, though unlikely
            mainLayout->addWidget(graphicsView_);
        }
    }
    
    // Connections for buttons (might be redundant if called multiple times, but connect is safe)
    connect(startButton_, &QPushButton::clicked, this, &MainWindow::onStartNestingClicked);
    connect(stopButton_, &QPushButton::clicked, this, &MainWindow::onStopNestingClicked);
}

void MainWindow::setupConnections() {
    connect(svgNestInstance_, &SvgNest::nestingProgress, this, &MainWindow::handleNestingProgress);
    connect(svgNestInstance_, &SvgNest::newSolutionFound, this, &MainWindow::handleNewSolution);
    connect(svgNestInstance_, &SvgNest::nestingFinished, this, &MainWindow::handleNestingFinished);
    connect(svgNestInstance_, &SvgNest::nestingFinished, this, [this](const QList<SvgNest::NestSolution>&){
        updateButtonStates(false);
    });
}

void MainWindow::loadConfigurationToUi() {
    if (!configWidget_) return; // Config UI not set up
    SvgNest::Configuration config = svgNestInstance_->getConfiguration();
    clipperScaleSpinBox_->setValue(config.clipperScale);
    curveToleranceSpinBox_->setValue(config.curveTolerance);
    spacingSpinBox_->setValue(config.spacing);
    rotationsSpinBox_->setValue(config.rotations);
    populationSizeSpinBox_->setValue(config.populationSize);
    mutationRateSpinBox_->setValue(config.mutationRate);
    placementTypeComboBox_->setCurrentText(config.placementType);
    mergeLinesCheckBox_->setChecked(config.mergeLines);
    timeRatioSpinBox_->setValue(config.timeRatio);
    timeRatioSpinBox_->setEnabled(config.mergeLines);
    simplifyOnLoadCheckBox_->setChecked(config.simplifyOnLoad);
}

void MainWindow::applyUiToConfiguration() {
    if (!configWidget_) return; // Config UI not set up
    SvgNest::Configuration config; 
    config.clipperScale = clipperScaleSpinBox_->value();
    config.curveTolerance = curveToleranceSpinBox_->value();
    config.spacing = spacingSpinBox_->value();
    config.rotations = rotationsSpinBox_->value();
    config.populationSize = populationSizeSpinBox_->value();
    config.mutationRate = mutationRateSpinBox_->value();
    config.placementType = placementTypeComboBox_->currentText();
    config.mergeLines = mergeLinesCheckBox_->isChecked();
    config.timeRatio = timeRatioSpinBox_->value();
    config.simplifyOnLoad = simplifyOnLoadCheckBox_->isChecked();
    svgNestInstance_->setConfiguration(config);
}

void MainWindow::onStartNestingClicked() {
    qDebug() << "Start Nesting Clicked";
    if(resultsTextEdit_) resultsTextEdit_->clear(); 
    if(progressBar_) progressBar_->setValue(0);
    applyUiToConfiguration(); 

    originalParts_.clear(); // Clear previously stored parts
    svgNestInstance_->clearParts();
    svgNestInstance_->clearSheets();
    
    QPainterPath part1; part1.addRect(0, 0, 50, 50);
    QString id1 = "square_1";
    svgNestInstance_->addPart(id1, part1, 2);
    originalParts_.insert(id1, part1);

    QPainterPath part2; part2.moveTo(0,0); part2.lineTo(30,0); part2.lineTo(15,30); part2.closeSubpath();
    QString id2 = "triangle_1";
    svgNestInstance_->addPart(id2, part2, 1);
    originalParts_.insert(id2, part2);

    QPainterPath sheet; sheet.addRect(0, 0, 200, 150);
    svgNestInstance_->addSheet(sheet);

    svgNestInstance_->startNestingAsync();
    updateButtonStates(true); 
}

void MainWindow::onStopNestingClicked() {
    qDebug() << "Stop Nesting Clicked";
    svgNestInstance_->stopNesting();
}

void MainWindow::updateButtonStates(bool nestingInProgress) {
    if (startButton_) startButton_->setEnabled(!nestingInProgress);
    if (stopButton_) stopButton_->setEnabled(nestingInProgress);
    if (configWidget_) configWidget_->setEnabled(!nestingInProgress);
}

void MainWindow::handleNestingProgress(int percentage) {
    // qDebug() << "Nesting Progress:" << percentage << "%"; // Optional: keep for debugging
    if (progressBar_) { // Check if progressBar_ is not null
        progressBar_->setValue(percentage);
    }
}

void MainWindow::handleNewSolution(const SvgNest::NestSolution& solution) {
    // qDebug() << "New Solution Found. Fitness:" << solution.fitness; // Optional
    if (resultsTextEdit_) { // Check if resultsTextEdit_ is not null
       resultsTextEdit_->append(QString("New intermediate solution found. Fitness: %1. Parts placed: %2")
                                .arg(solution.fitness)
                                .arg(solution.placements.size()));
       // Optionally, scroll to the bottom to show the latest message
       // resultsTextEdit_->verticalScrollBar()->setValue(resultsTextEdit_->verticalScrollBar()->maximum());
    }
}

void MainWindow::handleNestingFinished(const QList<SvgNest::NestSolution>& allSolutions) {
    if (graphicsScene_) {
        graphicsScene_->clear();
    }

    // qDebug() << "Nesting Finished. Total solutions found:" << allSolutions.size(); // Optional
    if (resultsTextEdit_) { // Check if resultsTextEdit_ is not null
       resultsTextEdit_->append("\nNesting process finished.");
       if (!allSolutions.isEmpty()) {
           const SvgNest::NestSolution& bestSolution = allSolutions.first();
           resultsTextEdit_->append(QString("Best solution details: Fitness: %1. Parts placed: %2.")
                                    .arg(bestSolution.fitness)
                                    .arg(bestSolution.placements.size()));

           if (graphicsScene_) {
               // Draw the sheet (using the hardcoded one for now)
               QPainterPath sheetPath; 
               sheetPath.addRect(0, 0, 200, 150); // Matches the one in onStartNestingClicked
               QPen sheetPen(Qt::black);
               sheetPen.setWidth(2); // Make sheet outline a bit thicker
               graphicsScene_->addPath(sheetPath, sheetPen);

               // Draw placed parts
               for (const SvgNest::PlacedPart& p : bestSolution.placements) {
                   QPainterPath originalPath = originalParts_.value(p.partId);
                   if (originalPath.isEmpty()) {
                       qWarning() << "Could not find original path for part ID:" << p.partId;
                       continue;
                   }

                   QGraphicsPathItem* item = new QGraphicsPathItem(originalPath);
                   item->setPos(p.position);
                   item->setRotation(p.rotation); 
                   // Note: Rotation is around (0,0) of the item's coordinate system.
                   // If parts are not defined with origin at a sensible rotation center,
                   // this might look off. For simple shapes like rects from (0,0) it's usually fine.
                   item->setBrush(Qt::cyan); // Example fill color
                   graphicsScene_->addItem(item);
               }
               if(graphicsView_) graphicsView_->viewport()->update(); // Ensure redraw
           }

       } else {
           resultsTextEdit_->append("No valid solutions were found.");
       }
       // resultsTextEdit_->verticalScrollBar()->setValue(resultsTextEdit_->verticalScrollBar()->maximum());
    }
    if (progressBar_) { // Check if progressBar_ is not null
        // If stopNesting was called, progress might not be 100%.
        // SvgNest::nestingFinished implies completion, regardless of how it finished.
        // If nesting was stopped, SvgNest should ideally tell us if it was premature.
        // For now, assume 100% if finished normally.
        // If stopNesting leads to an empty allSolutions or specific "stopped" state,
        // we might not want to set to 100%. SvgNest::nestingProgress should handle intermediate states.
        // Let's assume SvgNest will emit progress(100) if it completes fully.
        // If nesting is stopped, the progress bar will show its last value.
        // However, the prompt implies setting it to 100 on finish.
         progressBar_->setValue(100);
    }
    // updateButtonStates(false); // This is already connected to the nestingFinished signal
}
