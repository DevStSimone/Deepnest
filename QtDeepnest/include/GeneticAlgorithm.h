#ifndef GENETIC_ALGORITHM_H
#define GENETIC_ALGORITHM_H

#include <vector>
#include <random>
#include "DataStructures.h" // For Part, Point, Polygon
#include "Config.h"         // For AppConfig

struct Individual {
    std::vector<int> partIndices; // Indices into the main list of parts to be placed
    std::vector<double> rotations;
    // std::vector<int> partSourceIds; // If needed to reference original part definitions after shuffling
    
    double fitness;
    bool processing; // Is this individual currently being processed by a worker?
    int id; // Unique ID for this individual in the population

    Individual() : fitness(0.0), processing(false), id(-1) {}
};

class GeneticAlgorithm {
public:
    GeneticAlgorithm(const QList<Part>& partsToNest, const AppConfig& config);

    void initializePopulation(const QList<Part>& partsToNest); // partsToNest here are the source parts with quantities
    void nextGeneration();
    Individual getNextIndividualToProcess(); // Gets an individual that needs fitness calculation
    void updateIndividualFitness(int individualId, double fitness);
    bool allIndividualsProcessed() const;
    const std::vector<Individual>& getPopulation() const { return m_population; }


private:
    Individual mutate(const Individual& individual);
    std::pair<Individual, Individual> mate(const Individual& male, const Individual& female);
    Individual selectRandomWeightedIndividual(const std::vector<Individual>& excluded_individuals = {});

    std::vector<Individual> m_population;
    const AppConfig& m_appConfig;
    std::mt19937 m_randomEngine; // For random number generation
    int m_currentIndividualIdCounter;
    std::vector<int> m_expandedPartSourceIndices;
    // Helper to get a list of part indices based on quantity
    std::vector<int> expandPartsByQuantity(const QList<Part>& parts);
};

#endif // GENETIC_ALGORITHM_H
