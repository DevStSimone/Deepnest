#include "mainwindow.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QLabel> // For placeholder widget
#include <QMenuBar> // For menu creation
#include <QFileDialog>
#include <QGraphicsScene>
#include <QPainterPath>
#include <QInputDialog>
#include <QMessageBox> // For error messages or info
#include <QFileInfo>
#include <QScrollArea>     // Added for config panel
#include <QFormLayout>     // Added for config panel
#include <QSpinBox>        // Added for config panel
#include <QDoubleSpinBox>  // Added for config panel
#include <QComboBox>       // Added for config panel
#include <QCheckBox>       // Added for config panel
#include <QPushButton>     // Added for config panel
#include <QLabel>          // Added for config panel
#include <QSettings>       // Added for config panel

// Conversion factor
const double INCH_TO_MM = 25.4;


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_currentlySelectedPart(nullptr)
{
    m_settings = new QSettings(QSettings::IniFormat, QSettings::UserScope, "QtDeepnestOrg", "QtDeepnest", this);

    // Create UI Elements
    partListWidget = new QListWidget(this);
    partListWidget->setObjectName("partListWidget");

    svgDisplayView = new QGraphicsView(this);
    svgDisplayView->setObjectName("svgDisplayView");
    m_svgScene = new QGraphicsScene(this);
    svgDisplayView->setScene(m_svgScene);
    svgDisplayView->setRenderHint(QPainter::Antialiasing);
    svgDisplayView->setDragMode(QGraphicsView::ScrollHandDrag);


    nestDisplayView = new QGraphicsView(this);
    nestDisplayView->setObjectName("nestDisplayView");

    configDockWidget = new QDockWidget("Configuration", this);
    configDockWidget->setObjectName("configDockWidget");
    addDockWidget(Qt::LeftDockWidgetArea, configDockWidget);
    setupConfigPanel(); // Setup the actual config panel widgets

    // Central Widget and Layout
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    mainLayout = new QHBoxLayout(centralWidget);

    rightTabWidget = new QTabWidget(this);
    rightTabWidget->addTab(svgDisplayView, "SVG Display");
    rightTabWidget->addTab(nestDisplayView, "Nest Result");

    mainLayout->addWidget(partListWidget, 1); 
    mainLayout->addWidget(rightTabWidget, 3); 
    centralWidget->setLayout(mainLayout);

    // Setup Menu
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    QAction *importSvgAction = new QAction(tr("&Import SVG..."), this);
    fileMenu->addAction(importSvgAction);
    connect(importSvgAction, &QAction::triggered, this, &MainWindow::onImportSvgClicked);

    QMenu *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    QAction *addRectangleAction = new QAction(tr("&Add Rectangle..."), this);
    toolsMenu->addAction(addRectangleAction);
    connect(addRectangleAction, &QAction::triggered, this, &MainWindow::onAddRectangleClicked);

    // Connect signals
    connect(partListWidget, &QListWidget::itemSelectionChanged, this, &MainWindow::onPartSelectionChanged);
    connect(partListWidget, &QListWidget::itemChanged, this, &MainWindow::onPartItemChanged);
    
    setupPartListContextMenu(); 

    setWindowTitle("QtDeepnest");
    resize(1024, 768);
    
    loadSettings(); // Load settings after UI is set up
}

MainWindow::~MainWindow()
{
    // m_settings is child of MainWindow, Qt handles deletion
    // m_svgScene is child of MainWindow, Qt handles deletion
}


void MainWindow::setupConfigPanel() {
    QWidget *configWidget = new QWidget();
    QFormLayout *formLayout = new QFormLayout(configWidget);
    configWidget->setLayout(formLayout);

    // Units ComboBox
    m_unitsComboBox = new QComboBox();
    m_unitsComboBox->addItems({"Inches", "Millimeters"});
    formLayout->addRow(tr("Units:"), m_unitsComboBox);
    connect(m_unitsComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onUnitsChanged);

    // Scale SpinBox (SVG Import Scale)
    m_scaleSpinBox = new QDoubleSpinBox();
    m_scaleSpinBox->setRange(1.0, 10000.0);
    m_scaleSpinBox->setDecimals(2);
    m_scaleSpinBox->setSingleStep(1.0);
    m_scaleUnitLabel = new QLabel(); // Units will be set by updateConfigDisplayUnits
    QHBoxLayout* scaleLayout = new QHBoxLayout();
    scaleLayout->addWidget(m_scaleSpinBox);
    scaleLayout->addWidget(m_scaleUnitLabel);
    formLayout->addRow(tr("SVG Import Scale:"), scaleLayout);
    connect(m_scaleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onSettingChanged);

    // Spacing SpinBox
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

    // Curve Tolerance SpinBox
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

    // Rotations SpinBox
    m_rotationsSpinBox = new QSpinBox();
    m_rotationsSpinBox->setRange(0, 360); // Or a smaller number like 0-16 if specific steps
    m_rotationsSpinBox->setSingleStep(1);
    formLayout->addRow(tr("Rotations:"), m_rotationsSpinBox);
    connect(m_rotationsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onSettingChanged);

    // Threads SpinBox
    m_threadsSpinBox = new QSpinBox();
    m_threadsSpinBox->setRange(1, QThread::idealThreadCount() > 0 ? QThread::idealThreadCount() * 2 : 8); // Allow more than ideal
    m_threadsSpinBox->setSingleStep(1);
    formLayout->addRow(tr("Threads:"), m_threadsSpinBox);
    connect(m_threadsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onSettingChanged);

    // Population Size SpinBox
    m_populationSizeSpinBox = new QSpinBox();
    m_populationSizeSpinBox->setRange(2, 1000);
    m_populationSizeSpinBox->setSingleStep(1);
    formLayout->addRow(tr("Population Size:"), m_populationSizeSpinBox);
    connect(m_populationSizeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onSettingChanged);
    
    // Mutation Rate SpinBox (percentage)
    m_mutationRateSpinBox = new QSpinBox();
    m_mutationRateSpinBox->setRange(0, 100);
    m_mutationRateSpinBox->setSuffix("%");
    formLayout->addRow(tr("Mutation Rate:"), m_mutationRateSpinBox);
    connect(m_mutationRateSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onSettingChanged);

    // Placement Type ComboBox
    m_placementTypeComboBox = new QComboBox();
    m_placementTypeComboBox->addItems({"Gravity", "Bounding Box", "Squeeze"});
    formLayout->addRow(tr("Placement Type:"), m_placementTypeComboBox);
    connect(m_placementTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onSettingChanged);
    
    // Merge Lines CheckBox
    m_mergeLinesCheckBox = new QCheckBox(tr("Merge Lines"));
    formLayout->addRow(m_mergeLinesCheckBox);
    connect(m_mergeLinesCheckBox, &QCheckBox::stateChanged, this, &MainWindow::onSettingChanged);

    // Simplify CheckBox
    m_simplifyCheckBox = new QCheckBox(tr("Simplify SVG"));
    formLayout->addRow(m_simplifyCheckBox);
    connect(m_simplifyCheckBox, &QCheckBox::stateChanged, this, &MainWindow::onSettingChanged);

    // Endpoint Tolerance SpinBox
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

    // Reset Defaults Button
    m_resetDefaultsButton = new QPushButton(tr("Reset to Defaults"));
    formLayout->addRow(m_resetDefaultsButton);
    connect(m_resetDefaultsButton, &QPushButton::clicked, this, &MainWindow::onResetDefaultsClicked);

    // Scroll Area for Config Panel
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(configWidget);
    configDockWidget->setWidget(scrollArea);
}


void MainWindow::applyDefaults() {
    // Block signals while setting defaults to avoid triggering onSettingChanged multiple times
    bool blocked = m_unitsComboBox->blockSignals(true);
    m_unitsComboBox->setCurrentText("Inches");
    m_unitsComboBox->blockSignals(blocked);

    // Scale: Stored as SVG units per Inch. Default 72 units/inch.
    // The display will be updated by updateConfigDisplayUnits.
    // No direct widget update here for scale, as it depends on units.
    
    blocked = m_spacingSpinBox->blockSignals(true);
    m_spacingSpinBox->setValue(0.0); // Stored as inches
    m_spacingSpinBox->blockSignals(blocked);

    blocked = m_curveToleranceSpinBox->blockSignals(true);
    m_curveToleranceSpinBox->setValue(0.01); // Stored as inches
    m_curveToleranceSpinBox->blockSignals(blocked);

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
    m_mutationRateSpinBox->setValue(10); // 10%
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
    m_endpointToleranceSpinBox->setValue(0.005); // Stored as inches
    m_endpointToleranceSpinBox->blockSignals(blocked);

    // After setting all programmatic changes, update display units and then save.
    updateConfigDisplayUnits(); 
}

void MainWindow::loadSettings() {
    m_settings->beginGroup("Configuration");

    // Block signals to prevent onSettingChanged from firing during load
    bool blocked = m_unitsComboBox->blockSignals(true);
    m_unitsComboBox->setCurrentText(m_settings->value("units", "Inches").toString());
    m_unitsComboBox->blockSignals(blocked);

    // Scale is stored as SVG units per Inch. Display handled by updateConfigDisplayUnits.
    // No direct widget update here yet.

    blocked = m_spacingSpinBox->blockSignals(true);
    m_spacingSpinBox->setValue(m_settings->value("spacingInches", 0.0).toDouble());
    m_spacingSpinBox->blockSignals(blocked);

    blocked = m_curveToleranceSpinBox->blockSignals(true);
    m_curveToleranceSpinBox->setValue(m_settings->value("curveToleranceInches", 0.01).toDouble());
    m_curveToleranceSpinBox->blockSignals(blocked);

    blocked = m_rotationsSpinBox->blockSignals(true);
    m_rotationsSpinBox->setValue(m_settings->value("rotations", 4).toInt());
    m_rotationsSpinBox->blockSignals(blocked);

    blocked = m_threadsSpinBox->blockSignals(true);
    m_threadsSpinBox->setValue(m_settings->value("threads", QThread::idealThreadCount() > 0 ? QThread::idealThreadCount() : 4).toInt());
    m_threadsSpinBox->blockSignals(blocked);

    blocked = m_populationSizeSpinBox->blockSignals(true);
    m_populationSizeSpinBox->setValue(m_settings->value("populationSize", 10).toInt());
    m_populationSizeSpinBox->blockSignals(blocked);
    
    blocked = m_mutationRateSpinBox->blockSignals(true);
    m_mutationRateSpinBox->setValue(m_settings->value("mutationRate", 10).toInt());
    m_mutationRateSpinBox->blockSignals(blocked);

    blocked = m_placementTypeComboBox->blockSignals(true);
    m_placementTypeComboBox->setCurrentText(m_settings->value("placementType", "Gravity").toString());
    m_placementTypeComboBox->blockSignals(blocked);

    blocked = m_mergeLinesCheckBox->blockSignals(true);
    m_mergeLinesCheckBox->setChecked(m_settings->value("mergeLines", true).toBool());
    m_mergeLinesCheckBox->blockSignals(blocked);

    blocked = m_simplifyCheckBox->blockSignals(true);
    m_simplifyCheckBox->setChecked(m_settings->value("simplify", false).toBool());
    m_simplifyCheckBox->blockSignals(blocked);

    blocked = m_endpointToleranceSpinBox->blockSignals(true);
    m_endpointToleranceSpinBox->setValue(m_settings->value("endpointToleranceInches", 0.005).toDouble());
    m_endpointToleranceSpinBox->blockSignals(blocked);

    m_settings->endGroup();

    updateConfigDisplayUnits(); // This will correctly set scale and unit-dependent fields for display
}

void MainWindow::saveSettings() {
    m_settings->beginGroup("Configuration");

    m_settings->setValue("units", m_unitsComboBox->currentText());

    // Scale: Stored as SVG units per Inch.
    // Convert the displayed value back to units/inch before saving.
    double currentScaleDisplay = m_scaleSpinBox->value();
    if (m_unitsComboBox->currentText() == "Millimeters") {
        m_settings->setValue("scaleUnitsPerInch", currentScaleDisplay * INCH_TO_MM);
    } else { // Inches
        m_settings->setValue("scaleUnitsPerInch", currentScaleDisplay);
    }

    // Store length-based values in inches
    if (m_unitsComboBox->currentText() == "Millimeters") {
        m_settings->setValue("spacingInches", m_spacingSpinBox->value() / INCH_TO_MM);
        m_settings->setValue("curveToleranceInches", m_curveToleranceSpinBox->value() / INCH_TO_MM);
        m_settings->setValue("endpointToleranceInches", m_endpointToleranceSpinBox->value() / INCH_TO_MM);
    } else { // Inches
        m_settings->setValue("spacingInches", m_spacingSpinBox->value());
        m_settings->setValue("curveToleranceInches", m_curveToleranceSpinBox->value());
        m_settings->setValue("endpointToleranceInches", m_endpointToleranceSpinBox->value());
    }

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
    // If any setting change requires immediate re-processing or UI updates elsewhere,
    // it can be triggered here. For now, just saving.
}

void MainWindow::onUnitsChanged(int index) {
    Q_UNUSED(index);
    // Save settings because the unit choice itself is a setting.
    // The actual value of unit-dependent spinboxes is not changed here,
    // but their display (and the unit labels) will be updated.
    saveSettings(); // Save the new unit preference
    updateConfigDisplayUnits();
}

void MainWindow::onResetDefaultsClicked() {
    applyDefaults(); // Sets widgets to defaults and calls updateConfigDisplayUnits
    saveSettings();  // Save these defaults
}

void MainWindow::updateConfigDisplayUnits() {
    bool isMillimeters = (m_unitsComboBox->currentText() == "Millimeters");
    QString lengthUnitSuffix = isMillimeters ? "mm" : "in";

    // Block signals to prevent saveSettings from being called recursively or with intermediate values.
    bool blocked;

    // Update Scale display
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

    // Update Spacing display
    blocked = m_spacingSpinBox->blockSignals(true);
    double spacingInches = m_settings->value("Configuration/spacingInches", 0.0).toDouble();
    m_spacingSpinBox->setValue(isMillimeters ? spacingInches * INCH_TO_MM : spacingInches);
    m_spacingUnitLabel->setText(lengthUnitSuffix);
    m_spacingSpinBox->blockSignals(blocked);

    // Update Curve Tolerance display
    blocked = m_curveToleranceSpinBox->blockSignals(true);
    double curveToleranceInches = m_settings->value("Configuration/curveToleranceInches", 0.01).toDouble();
    m_curveToleranceSpinBox->setValue(isMillimeters ? curveToleranceInches * INCH_TO_MM : curveToleranceInches);
    m_curveToleranceUnitLabel->setText(lengthUnitSuffix);
    m_curveToleranceSpinBox->blockSignals(blocked);

    // Update Endpoint Tolerance display
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

    double svgUnitsToDeviceUnitsScale = 1.0; // Default if not determined by SvgParser
    QDomElement rootElement = m_svgParser.load(svgString, svgUnitsToDeviceUnitsScale);
    if (rootElement.isNull()) {
        QMessageBox::warning(this, tr("Error"), tr("Could not parse SVG from file: ") + filePath);
        return;
    }
    
    // The prompt says to call applyTransformRecursive here. 
    // However, the SvgParser::polygonify method handles transform accumulation.
    // If applyTransformRecursive was meant to be destructive on the DOM, it would be called here.
    // For now, polygonify will handle transforms.
    // m_svgParser.applyTransformRecursive(rootElement, QTransform()); // Assuming identity at root

    // Unit conversion factor: for now, use 1.0, assuming SvgParser::load handles initial viewBox scaling
    // and we operate in "SVG user units" which are then scaled by svgUnitsToDeviceUnitsScale
    // for display purposes if needed, or a global application scale.
    // For geometric processing, we need a consistent unit. Let's assume 1.0 for now.
    double unitConversionFactor = 1.0; 
    QList<Part> newParts = m_svgParser.getParts(rootElement, unitConversionFactor);

    QFileInfo fileInfo(filePath);
    QString baseName = fileInfo.baseName();
    int partIndex = 0;
    for (Part& part : newParts) { // Iterate by reference to modify ID if needed
        if (part.id.isEmpty()) {
            part.id = QString("%1_part%2").arg(baseName).arg(partIndex++);
        }
        // Ensure unique ID if adding to existing parts
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
        item->setData(Qt::UserRole, i); // Store index of the part
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(part.isSheet ? Qt::Checked : Qt::Unchecked);
        partListWidget->addItem(item);
    }
}

void MainWindow::onPartSelectionChanged() {
    QList<QListWidgetItem*> selectedItems = partListWidget->selectedItems();
    if (selectedItems.isEmpty()) {
        m_currentlySelectedPart = nullptr;
        m_svgScene->clear(); // Clear display if nothing selected
        return;
    }

    QListWidgetItem *selectedItem = selectedItems.first();
    int partIndex = selectedItem->data(Qt::UserRole).toInt();
    if (partIndex >= 0 && partIndex < m_importedParts.size()) {
        m_currentlySelectedPart = &m_importedParts[partIndex]; // Pointer to the part in the list
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
        // If this part is currently selected and displayed, you might want to update its display
        // or related UI elements that depend on the 'isSheet' status.
        // For now, just updating the data model.
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

    // Update action text when selection changes
    connect(partListWidget, &QListWidget::itemSelectionChanged, [this, markAsSheetAction]() {
        if (m_currentlySelectedPart) {
            markAsSheetAction->setText(m_currentlySelectedPart->isSheet ? "Unmark as Sheet" : "Mark as Sheet");
            markAsSheetAction->setEnabled(true);
        } else {
            markAsSheetAction->setText("Mark as Sheet");
            markAsSheetAction->setEnabled(false);
        }
    });
    markAsSheetAction->setEnabled(false); // Initially disabled
}

void MainWindow::markSelectedPartAsSheet(bool isSheet) {
    if (!m_currentlySelectedPart) return;

    m_currentlySelectedPart->isSheet = isSheet;

    // Update the checkbox in the list widget
    QList<QListWidgetItem*> selectedItems = partListWidget->selectedItems();
    if (!selectedItems.isEmpty()) {
        QListWidgetItem* item = selectedItems.first();
        // Ensure this item corresponds to m_currentlySelectedPart
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
    // Potentially update other UI elements or trigger reprocessing if needed
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
        outerPath.closeSubpath(); // Close the outer path
    }
    
    // For holes, they need to be sub-paths with an opposite winding order
    // or rely on fill rule settings. QPainterPath handles this with addPath.
    for (const auto& hole_vec : part.geometry.holes) {
        if (hole_vec.empty()) continue;
        QPainterPath holePath;
        holePath.moveTo(hole_vec[0].x, hole_vec[0].y);
        for (size_t i = 1; i < hole_vec.size(); ++i) {
            holePath.lineTo(hole_vec[i].x, hole_vec[i].y);
        }
        holePath.closeSubpath();
        outerPath.addPath(holePath); // Add hole to the main path for correct rendering (using EvenOddFill)
    }
    outerPath.setFillRule(Qt::OddEvenFill); // Important for holes to render correctly

    m_svgScene->addPath(outerPath, QPen(Qt::black), QBrush(Qt::lightGray));
    
    // Fit view, but ensure itemsBoundingRect is valid
    if (m_svgScene->itemsBoundingRect().isValid()) {
        svgDisplayView->fitInView(m_svgScene->itemsBoundingRect(), Qt::KeepAspectRatio);
    } else {
        // If no valid bounding rect (e.g. path was empty or degenerate), set a default view
        svgDisplayView->setSceneRect(-100, -100, 200, 200); // Example default
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
    // Check if the mouse is over the svgDisplayView
    if (svgDisplayView->underMouse()) { // Or check svgDisplayView->hasFocus() if preferred
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
            event->accept(); // Accept the event to prevent it from propagating further
        } else {
            // Default wheel event for scrolling if Ctrl is not pressed
            // QGraphicsView's default wheelEvent handles scrolling.
            // If svgDisplayView is a direct child or focus proxy, this might not be needed.
            // For simplicity, let QMainWindow handle it or pass to base.
            QMainWindow::wheelEvent(event);
        }
    } else {
        QMainWindow::wheelEvent(event); // Pass to base if not over the target view
    }
}
