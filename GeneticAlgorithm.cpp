#include "GeneticAlgorithm.h"
#include "GeometryProcessor.h" // For area calculation (if needed for sorting)
#include <algorithm>    // For std::sort, std::shuffle, std::transform
#include <numeric>      // For std::iota
#include <QDebug>       // For debug messages

// Helper to calculate area of a Polygon (simplified, assumes outer is simple, ignores holes for GA sorting)
double calculatePolygonArea(const Polygon& polygon) {
    if (polygon.outer.empty()) return 0.0;
    // Using Clipper2's Area function after conversion
    Clipper2Lib::Path64 path = GeometryProcessor::PointsToPath64(polygon.outer);
    return std::abs(Clipper2Lib::Area(path) / (GeometryProcessor::CLIPPER_SCALE * GeometryProcessor::CLIPPER_SCALE));
}


GeneticAlgorithm::GeneticAlgorithm(const QList<Part>& partsToNest, const AppConfig& config)
    : m_appConfig(config), m_randomEngine(std::random_device{}()), m_currentIndividualIdCounter(0) {
    // partsToNest in the constructor are the source parts (potentially including sheets,
    // but GA should only operate on placeable parts).
    // The actual expansion and filtering of placeable parts happen in initializePopulation.
}

std::vector<int> GeneticAlgorithm::expandPartsByQuantity(const QList<Part>& parts) {
    std::vector<int> expanded_indices;
    for (int i = 0; i < parts.size(); ++i) {
        // Assuming parts that are sheets are NOT part of the GA's items to place.
        // This filtering should ideally happen before calling the GA constructor or initializePopulation.
        // For now, we assume partsToNest passed to initializePopulation are already filtered.
        if (!parts[i].isSheet) { // Only include non-sheet parts for placement
            for (int j = 0; j < parts[i].quantity; ++j) {
                expanded_indices.push_back(i); // Store original index
            }
        }
    }
    return expanded_indices;
}

void GeneticAlgorithm::initializePopulation(const QList<Part>& partsToNest) {
    m_population.clear();
    m_currentIndividualIdCounter = 0;

    // m_expandedPartSourceIndices stores the indices of parts from the original partsToNest list,
    // expanded by quantity. This is what individuals will shuffle.
    m_expandedPartSourceIndices = expandPartsByQuantity(partsToNest);
    if(m_expandedPartSourceIndices.empty()){
        qWarning() << "GA: No placeable parts to initialize population.";
        return;
    }

    // Create "adam" individual - the initial sequence
    Individual adam;
    adam.partIndices.resize(m_expandedPartSourceIndices.size());
    std::iota(adam.partIndices.begin(), adam.partIndices.end(), 0); // Fill with 0, 1, 2... up to size-1
                                                                    // These indices refer to positions in m_expandedPartSourceIndices

    // Sort adam by part area (descending)
    // This requires knowing the original Part objects that m_expandedPartSourceIndices point to.
    std::sort(adam.partIndices.begin(), adam.partIndices.end(),
        [&](int a_idx_in_expanded, int b_idx_in_expanded) {
            // Get the original part index from m_expandedPartSourceIndices
            int original_part_idx_a = m_expandedPartSourceIndices[a_idx_in_expanded];
            int original_part_idx_b = m_expandedPartSourceIndices[b_idx_in_expanded];
            return calculatePolygonArea(partsToNest[original_part_idx_a].geometry) >
                   calculatePolygonArea(partsToNest[original_part_idx_b].geometry);
        });
    
    adam.rotations.resize(m_expandedPartSourceIndices.size());
    std::uniform_int_distribution<> rotation_dist(0, m_appConfig.rotations - 1);
    for (size_t i = 0; i < adam.rotations.size(); ++i) {
        adam.rotations[i] = rotation_dist(m_randomEngine); // Assign random initial rotations (0 to N-1)
    }
    
    adam.fitness = -1; // Mark as uncalculated initially
    adam.processing = false;
    adam.id = m_currentIndividualIdCounter++;
    m_population.push_back(adam);

    // Create initial population by mutating adam
    for (int i = 1; i < m_appConfig.populationSize; ++i) {
        Individual mutant = mutate(adam);
        mutant.id = m_currentIndividualIdCounter++;
        mutant.fitness = -1; // Mark as uncalculated
        mutant.processing = false;
        m_population.push_back(mutant);
    }
}


Individual GeneticAlgorithm::mutate(const Individual& individual) {
    Individual mutant = individual; // Start with a copy
    mutant.fitness = -1; // Reset fitness for the new mutant
    mutant.processing = false;

    // Part indices mutation (swap)
    std::uniform_real_distribution<> prob_dist(0.0, 100.0);
    if (prob_dist(m_randomEngine) < m_appConfig.mutationRate && mutant.partIndices.size() >= 2) {
        std::uniform_int_distribution<> index_dist(0, mutant.partIndices.size() - 1);
        int index1 = index_dist(m_randomEngine);
        int index2 = index_dist(m_randomEngine);
        while (index1 == index2) { // Ensure different indices for swap
            index2 = index_dist(m_randomEngine);
        }
        std::swap(mutant.partIndices[index1], mutant.partIndices[index2]);
    }

    // Rotation mutation
    if (m_appConfig.rotations > 1) { // Only mutate rotations if there's more than one option
        std::uniform_int_distribution<> rotation_val_dist(0, m_appConfig.rotations - 1);
        for (size_t i = 0; i < mutant.rotations.size(); ++i) {
            if (prob_dist(m_randomEngine) < m_appConfig.mutationRate) {
                mutant.rotations[i] = rotation_val_dist(m_randomEngine);
            }
        }
    }
    return mutant;
}

std::pair<Individual, Individual> GeneticAlgorithm::mate(const Individual& male, const Individual& female) {
    Individual child1, child2;
    child1.partIndices.resize(male.partIndices.size());
    child1.rotations.resize(male.rotations.size());
    child2.partIndices.resize(male.partIndices.size());
    child2.rotations.resize(male.rotations.size());

    // Single-point crossover for partIndices
    if (male.partIndices.size() > 1) {
        std::uniform_int_distribution<> crossover_point_dist(1, male.partIndices.size() - 1);
        int crossover_point = crossover_point_dist(m_randomEngine);

        // Child 1: male_head + female_tail
        std::copy(male.partIndices.begin(), male.partIndices.begin() + crossover_point, child1.partIndices.begin());
        std::copy(female.partIndices.begin() + crossover_point, female.partIndices.end(), child1.partIndices.begin() + crossover_point);

        // Child 2: female_head + male_tail
        std::copy(female.partIndices.begin(), female.partIndices.begin() + crossover_point, child2.partIndices.begin());
        std::copy(male.partIndices.begin() + crossover_point, male.partIndices.end(), child2.partIndices.begin() + crossover_point);

        // Fix duplicates by finding missing elements and replacing duplicates (simplified approach)
        // A more robust crossover (like PMX) would be better but is more complex.
        // This basic crossover can lead to invalid permutations.
        // For now, we'll assume this simple version and acknowledge it might need improvement.
        // To keep it simple as per deepnest.js initial approach, we might skip duplicate fixing here,
        // relying on mutation and selection to eventually filter out bad individuals, or assume
        // the evaluation can handle individuals with repeated/missing parts gracefully (e.g. by ignoring them).
        // Deepnest.js's original GA was also quite simple.
    } else if (male.partIndices.size() == 1) { // Handle single part case
        child1.partIndices[0] = male.partIndices[0];
        child2.partIndices[0] = female.partIndices[0]; // or male, doesn't matter much for one part
    }


    // Crossover for rotations (average or pick one parent's)
    // deepnest.js seems to just pick one parent's rotation or average, then mutate.
    // Here, let's do point-wise crossover for rotations.
    std::uniform_int_distribution<> parent_choice_dist(0, 1);
    for (size_t i = 0; i < male.rotations.size(); ++i) {
        if (parent_choice_dist(m_randomEngine) == 0) {
            child1.rotations[i] = male.rotations[i];
            child2.rotations[i] = female.rotations[i];
        } else {
            child1.rotations[i] = female.rotations[i];
            child2.rotations[i] = male.rotations[i];
        }
    }
    
    child1.fitness = -1; child1.processing = false;
    child2.fitness = -1; child2.processing = false;

    return {child1, child2};
}


Individual GeneticAlgorithm::selectRandomWeightedIndividual(const std::vector<Individual>& excluded_individuals) {
    // Filter out individuals that have fitness -1 (not evaluated) or are in excluded_individuals
    std::vector<const Individual*> candidates;
    double total_fitness_sum = 0;

    for (const auto& ind : m_population) {
        bool excluded = false;
        for(const auto& ex_ind : excluded_individuals){
            if(ind.id == ex_ind.id){
                excluded = true;
                break;
            }
        }
        if (!excluded && ind.fitness >= 0) { // Fitness must be non-negative (lower is better, so we need to invert for selection probability)
            candidates.push_back(&ind);
            // Assuming lower fitness is better. For roulette wheel, higher values are selected more.
            // So, we need to transform fitness. MaxFitness - fitness, or 1/fitness.
            // Let's use 1 / (1 + fitness) to avoid division by zero and handle positive fitness values.
            total_fitness_sum += (1.0 / (1.0 + ind.fitness));
        }
    }
    
    if (candidates.empty() || total_fitness_sum == 0) {
        // Fallback: if no candidates with valid fitness or sum is zero, return a random individual from population (if any)
        // This shouldn't happen in a typical run after initial evaluation.
        if (m_population.empty()) { 
            qWarning() << "selectRandomWeightedIndividual: Population is empty!";
            return Individual(); // Return invalid individual
        }
        std::uniform_int_distribution<> dist(0, m_population.size() - 1);
        return m_population[dist(m_randomEngine)];
    }

    std::uniform_real_distribution<> dist(0.0, total_fitness_sum);
    double random_value = dist(m_randomEngine);
    double current_sum = 0;

    for (const auto* ind_ptr : candidates) {
        current_sum += (1.0 / (1.0 + ind_ptr->fitness));
        if (current_sum >= random_value) {
            return *ind_ptr;
        }
    }
    // Fallback, should ideally not be reached if total_fitness_sum > 0
    return *candidates.back();
}


void GeneticAlgorithm::nextGeneration() {
    std::vector<Individual> new_population;
    new_population.reserve(m_appConfig.populationSize);

    // Elitism: Keep the best individual from the current population
    // Sort by fitness (lower is better)
    std::sort(m_population.begin(), m_population.end(), [](const Individual& a, const Individual& b) {
        if (a.fitness < 0 && b.fitness < 0) return false; // Both un-evaluated or invalid
        if (a.fitness < 0) return false; // a is worse (un-evaluated)
        if (b.fitness < 0) return true;  // b is worse (un-evaluated)
        return a.fitness < b.fitness;
    });

    if (!m_population.empty() && m_population[0].fitness >=0 ) { // Ensure the best has been evaluated
        Individual elite = m_population[0];
        elite.id = m_currentIndividualIdCounter++; // Assign new ID for the new generation
        elite.processing = false; // Reset processing flag
        // Fitness is retained
        new_population.push_back(elite);
    } else if (!m_population.empty()) {
        // If best was not evaluated, just take a copy of the first one (e.g. adam from first gen)
        Individual first_copy = m_population[0];
        first_copy.id = m_currentIndividualIdCounter++;
        first_copy.processing = false;
        first_copy.fitness = -1; // Mark as un-evaluated
        new_population.push_back(first_copy);
    }


    // Fill the rest of the population
    while (new_population.size() < static_cast<size_t>(m_appConfig.populationSize)) {
        Individual parent1 = selectRandomWeightedIndividual();
        Individual parent2 = selectRandomWeightedIndividual({parent1}); // Ensure parent2 is different if possible

        std::pair<Individual, Individual> children = mate(parent1, parent2);
        
        Individual child1_mutated = mutate(children.first);
        child1_mutated.id = m_currentIndividualIdCounter++;
        new_population.push_back(child1_mutated);

        if (new_population.size() < static_cast<size_t>(m_appConfig.populationSize)) {
            Individual child2_mutated = mutate(children.second);
            child2_mutated.id = m_currentIndividualIdCounter++;
            new_population.push_back(child2_mutated);
        }
    }

    m_population = new_population;
}


Individual GeneticAlgorithm::getNextIndividualToProcess() {
    for (auto& ind : m_population) {
        if (ind.fitness < 0 && !ind.processing) { // Fitness < 0 indicates not yet evaluated
            ind.processing = true;
            return ind;
        }
    }
    Individual invalid_ind; // Default constructor sets id = -1
    return invalid_ind; 
}

void GeneticAlgorithm::updateIndividualFitness(int individualId, double fitness) {
    for (auto& ind : m_population) {
        if (ind.id == individualId) {
            ind.fitness = fitness;
            ind.processing = false;
            return;
        }
    }
    qWarning() << "GA: Attempted to update fitness for non-existent individual ID:" << individualId;
}

bool GeneticAlgorithm::allIndividualsProcessed() const {
    for (const auto& ind : m_population) {
        if (ind.fitness < 0 && !ind.processing) { // Not evaluated and not currently processing
            return false;
        }
        // If an individual is still marked as processing, we are waiting for its result.
        // This function is more like "areAllTasksDispatchedOrCompletedForTheCurrentEvaluationRound"
        // Or, "areThereAnyUnassignedIndividualsForEvaluation"
        // If it's processing, it's not "done" from the perspective of moving to next gen,
        // but it's also not available to be picked by getNextIndividualToProcess.
        // The logic in NestingContext.handleWorkerResult will check if all _completed_ + _launched_ cover the population.
    }
    // This means: no individual is both un-evaluated (fitness < 0) AND not currently processing.
    // So, either all have fitness >= 0, or those with fitness < 0 are currently being processed.
    // This is the state where no new tasks can be dispatched from the current population.
    return true; 
}
