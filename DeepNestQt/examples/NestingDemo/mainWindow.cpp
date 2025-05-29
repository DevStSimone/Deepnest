#include "mainWindow.h"
// #include "ui_mainWindow.h" // If using .ui file

#include <QPushButton>    // For example UI
#include <QVBoxLayout>    // For example UI
#include <QProgressBar>   // For example UI
#include <QTextEdit>      // For example UI
#include <QDebug>         // For logging
#include <QPainterPath>   // For creating example shapes

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent)/*, ui(new Ui::MainWindow)*/ { // Uncomment ui part if using .ui file
    // if (ui) ui->setupUi(this); // If using .ui file
    
    svgNestInstance_ = new SvgNest(this);
    setupUi(); // Programmatic UI setup
    setupConnections();
}

MainWindow::~MainWindow() {
    // delete ui; // If using .ui file
    // svgNestInstance_ is a child of MainWindow, Qt handles its deletion.
}

void MainWindow::setupUi() {
    // This is a placeholder for programmatic UI creation.
    // Replace with .ui file or more sophisticated UI code as needed.
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(centralWidget);

    QPushButton* startButton = new QPushButton("Start Nesting", this);
    QProgressBar* progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    QTextEdit* resultsTextEdit = new QTextEdit(this);
    resultsTextEdit->setReadOnly(true);

    layout->addWidget(startButton);
    layout->addWidget(progressBar);
    layout->addWidget(resultsTextEdit);
    setCentralWidget(centralWidget);

    // Connect button click to slot
    connect(startButton, &QPushButton::clicked, this, &MainWindow::onStartNestingClicked);
    
    // Store pointers if needed for updates, e.g., make progressBar_ a member
    // this->progressBar_ = progressBar; 
    // this->resultsTextEdit_ = resultsTextEdit;
}

void MainWindow::setupConnections() {
    connect(svgNestInstance_, &SvgNest::nestingProgress, this, &MainWindow::handleNestingProgress);
    connect(svgNestInstance_, &SvgNest::newSolutionFound, this, &MainWindow::handleNewSolution);
    connect(svgNestInstance_, &SvgNest::nestingFinished, this, &MainWindow::handleNestingFinished);
}

void MainWindow::onStartNestingClicked() {
    qDebug() << "Start Nesting Clicked";
    // resultsTextEdit_->clear(); // Clear previous results
    // progressBar_->setValue(0);

    // 1. Configure SvgNest (example configuration)
    SvgNest::Configuration config;
    config.rotations = 4;
    config.populationSize = 5; // Small for quick demo
    config.spacing = 1.0;
    // ... set other config parameters ...
    svgNestInstance_->setConfiguration(config);

    // 2. Clear previous parts/sheets
    svgNestInstance_->clearParts();
    svgNestInstance_->clearSheets();

    // 3. Add example parts (QPainterPath)
    QPainterPath part1;
    part1.addRect(0, 0, 50, 50); // A square
    svgNestInstance_->addPart("square_1", part1, 2); // Add 2 squares

    QPainterPath part2;
    part2.moveTo(0,0);
    part2.lineTo(30,0);
    part2.lineTo(15,30);
    part2.closeSubpath(); // A triangle
    svgNestInstance_->addPart("triangle_1", part2, 1);

    // 4. Add example sheet (QPainterPath)
    QPainterPath sheet;
    sheet.addRect(0, 0, 200, 150); // A rectangular sheet
    svgNestInstance_->addSheet(sheet);

    // 5. Start nesting
    svgNestInstance_->startNestingAsync();
}

void MainWindow::handleNestingProgress(int percentage) {
    qDebug() << "Nesting Progress:" << percentage << "%";
    // if (progressBar_) progressBar_->setValue(percentage);
}

void MainWindow::handleNewSolution(const SvgNest::NestSolution& solution) {
    qDebug() << "New Solution Found. Fitness:" << solution.fitness;
    // if (resultsTextEdit_) {
    //    resultsTextEdit_->append(QString("New solution found. Fitness: %1. Parts placed: %2")
    //                             .arg(solution.fitness)
    //                             .arg(solution.placements.size()));
    // }
}

void MainWindow::handleNestingFinished(const QList<SvgNest::NestSolution>& allSolutions) {
    qDebug() << "Nesting Finished. Total solutions found:" << allSolutions.size();
    // if (resultsTextEdit_) {
    //    resultsTextEdit_->append("Nesting finished.");
    //    if (!allSolutions.isEmpty()) {
    //        resultsTextEdit_->append(QString("Best solution fitness: %1")
    //                                 .arg(allSolutions.first().fitness)); // Assuming first is best or sorted
    //    }
    // }
    // if (progressBar_) progressBar_->setValue(100); // Mark as complete
}
