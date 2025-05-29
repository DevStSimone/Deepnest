#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "SvgNest.h" // Include the main header from the DeepNestQt library

// Forward declare UI class if using .ui file
// namespace Ui {
// class MainWindow;
// }

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onStartNestingClicked();
    void handleNestingProgress(int percentage);
    void handleNewSolution(const SvgNest::NestSolution& solution);
    void handleNestingFinished(const QList<SvgNest::NestSolution>& allSolutions);

private:
    // Ui::MainWindow *ui; // If using .ui file
    SvgNest* svgNestInstance_; // Instance of our nesting library

    // Example: Add some UI elements programmatically if not using .ui
    // QPushButton* startButton_;
    // QProgressBar* progressBar_;
    // QTextEdit* resultsTextEdit_;
    
    void setupUi(); // Helper to create UI if not using .ui file
    void setupConnections();
};

#endif // MAINWINDOW_H
