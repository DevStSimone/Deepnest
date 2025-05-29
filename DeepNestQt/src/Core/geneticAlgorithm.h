#ifndef GENETICALGORITHM_H
#define GENETICALGORITHM_H

#include "internalTypes.h" // For Core::InternalPart
#include "svgNest.h"       // For SvgNest::Configuration (GA parameters)
#include <QVector>
#include <QList>
#include <QString>
#include <functional> // For std::function a_fitness_callback

namespace Core {

// Represents a gene in the chromosome. Each gene corresponds to a part to be placed.
// It needs to store the part's identifier and its properties for this specific solution,
// like rotation, and potentially which source part it is if multiple instances of the same part type exist.
struct Gene {
    QString partId;      // Original ID of the part
    int     sourceIndex; // If multiple items of the same part ID, this distinguishes them (e.g., part_A_0, part_A_1)
    double  rotation;    // Rotation applied to this part
    bool    isPlaced;    // Has this part been successfully placed in the current solution?
    // QPointF position; // Position - usually determined by the nesting engine, not directly by GA genes

    Gene(QString id = "", int srcIdx = 0, double rot = 0.0)
        : partId(id), sourceIndex(srcIdx), rotation(rot), isPlaced(false) {}
};

// Represents an individual in the population (a potential solution).
// A chromosome is a sequence of genes (parts with their transformations/order).
struct Individual {
    QVector<Gene> chromosome; // Order of parts and their rotations/properties
    double fitness;           // Fitness score of this individual (lower is better, or higher, depending on metric)
    // SvgNest::NestSolution detailedSolution; // Optionally, the full placement details if needed by GA

    Individual() : fitness(0.0) {}

    // Comparison for sorting (e.g., if higher fitness is better)
    bool operator<(const Individual& other) const {
        return fitness > other.fitness; // Assuming higher fitness is better for std::sort
    }
};


class GeneticAlgorithm {
public:
    GeneticAlgorithm(const SvgNest::Configuration& config,
                     const QList<InternalPart>& partsAvailable); // Parts to be arranged

    ~GeneticAlgorithm();

    void initializePopulation();
    void runGeneration(); // Runs one cycle of selection, crossover, mutation

    const QVector<Individual>& getPopulation() const { return population_; }
    Individual getBestIndividual() const; // Returns the best individual from the current population

    // The fitness function is external, provided by NestingEngine.
    // It takes an Individual (its chromosome), attempts to place parts, and returns its fitness.
    // typedef std::function<double(Individual&)> FitnessCallback;
    // void setFitnessCallback(FitnessCallback cb) { fitnessCallback_ = cb; }
    
    // Simpler: Fitness is calculated by NestingEngine and set on Individual before selection.

private:
    SvgNest::Configuration config_;
    QList<InternalPart> availableParts_; // Master list of parts to choose from for genes
    QVector<Individual> population_;
    int generationCount_;

    // --- Core GA Operations ---
    void populate(); // Creates the initial population
    Individual createRandomIndividual();

    void evaluatePopulation(); // Placeholder: In DeepNest, fitness evaluation is external.
                               // Here, it means ensuring each individual's fitness is up-to-date.

    void selection();       // Selects individuals for the next generation
    Individual tournamentSelection();

    void crossover();       // Creates new individuals from selected parents
    QPair<Individual, Individual> orderedCrossover(const Individual& parent1, const Individual& parent2);

    void mutation();        // Applies mutations to individuals
    void mutateIndividual(Individual& individual);
    void scrambleMutation(Individual& individual); // Example mutation type
    void rotationMutation(Individual& individual); // Example mutation type


    // Helper to get a flat list of all part instances based on quantity
    QList<Gene> getAllPartGenes() const;
};

} // namespace Core
#endif // GENETICALGORITHM_H
