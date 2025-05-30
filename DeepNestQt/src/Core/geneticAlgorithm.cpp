#include "geneticAlgorithm.h"
#include <random>      // For std::mt19937, std::uniform_int_distribution, etc.
#include <algorithm>   // For std::shuffle, std::sort, std::min/max
#include <QDebug>      // For logging

namespace Core {

// --- Random Number Generation Setup ---
static std::mt19937& getrng() {
    static std::mt19937 rng(std::random_device{}());
    return rng;
}

// Returns a random double between min and max (inclusive)
static double randomDouble(double min, double max) {
    std::uniform_real_distribution<double> dist(min, max);
    return dist(getrng());
}

// Returns a random integer between min and max (inclusive)
static int randomInt(int min, int max) {
    if (min > max) std::swap(min, max);
    std::uniform_int_distribution<int> dist(min, max);
    return dist(getrng());
}

// --- GeneticAlgorithm Implementation ---

GeneticAlgorithm::GeneticAlgorithm(const SvgNest::Configuration& config,
                                   const QList<InternalPart>& partsAvailable)
    : config_(config), availableParts_(partsAvailable), generationCount_(0) {
    qDebug() << "GeneticAlgorithm created. Population size:" << config_.populationSize
             << "Mutation rate:" << config_.mutationRate << "%"
             << "Rotations:" << config_.rotations;
}

GeneticAlgorithm::~GeneticAlgorithm() {
    qDebug() << "GeneticAlgorithm destroyed.";
}

QList<Gene> GeneticAlgorithm::getAllPartGenes() const {
    QList<Gene> allGenes;
    // The plan's SvgNest::addPart has 'quantity', but InternalPart doesn't directly store it.
    // This implies partsAvailable_ should already be expanded or SvgNest provides quantities.
    // For now, assume availableParts_ contains each instance. If not, this needs adjustment.
    // Or, the NestingWorker passes a list of part *instances* to GA.
    // Let's assume availableParts_ IS the list of all part instances to be placed.
    // NestingWorker ensures that 'availableParts_' is a flat list where each entry is a unique instance
    // (e.g., if Part 'A' has quantity 2, 'availableParts_' will contain two separate InternalPart objects for 'A').
    // Thus, 'sourceIndex' correctly becomes the direct index into the 'availableParts_' list for that specific instance.
    int currentSourceIndex = 0; 
    for (const auto& part : availableParts_) {
        // gene.partId stores the original ID (e.g., "PartA")
        // gene.sourceIndex stores the index of this specific instance in the availableParts_ list.
        allGenes.append(Gene(part.id, currentSourceIndex++)); 
    }
    return allGenes;
}


void GeneticAlgorithm::initializePopulation() {
    qDebug() << "Initializing GA population...";
    populate();
    generationCount_ = 0;
    // Fitness for the initial population will be calculated by NestingEngine.
}

Individual GeneticAlgorithm::createRandomIndividual() {
    Individual ind;
    ind.chromosome = QVector<Gene>::fromList(getAllPartGenes()); // Get all part instances

    // Shuffle the order of parts
    std::shuffle(ind.chromosome.begin(), ind.chromosome.end(), getrng());

    // Assign random rotations (if configured)
    if (config_.rotations > 0) {
        for (Gene& gene : ind.chromosome) {
            if (config_.rotations == 1) { // 0 degrees only
                gene.rotation = 0.0;
            } else {
                // Assuming rotations are steps: 0, 360/rotations, 2*360/rotations ...
                int rotationStep = randomInt(0, config_.rotations - 1);
                gene.rotation = rotationStep * (360.0 / config_.rotations);
            }
        }
    }
    ind.fitness = -1.0; // Mark as unevaluated
    return ind;
}

void GeneticAlgorithm::populate() {
    population_.clear();
    for (int i = 0; i < config_.populationSize; ++i) {
        population_.append(createRandomIndividual());
    }
    qDebug() << "GA population initialized with" << population_.size() << "individuals.";
}

void GeneticAlgorithm::runGeneration() {
    qDebug() << "GA Running Generation:" << generationCount_;

    // Fitness for all individuals in population_ should have been calculated by NestingEngine by this point.
    // 1. Selection
    selection();

    // 2. Crossover
    crossover();

    // 3. Mutation
    mutation();

    generationCount_++;
    // After this, NestingEngine will evaluate the new population_.
}


Individual GeneticAlgorithm::getBestIndividual() const {
    if (population_.isEmpty()) {
        qDebug() << "getBestIndividual called on empty population.";
        return Individual(); // Return empty/default individual
    }
    // Assumes fitness is already calculated and higher is better (operator< is defined for this)
    auto it = std::max_element(population_.constBegin(), population_.constEnd());
    return (it != population_.constEnd()) ? *it : Individual();
}


void GeneticAlgorithm::selection() {
    QVector<Individual> nextGenerationPopulation;
    nextGenerationPopulation.reserve(config_.populationSize);

    // Elitism: Keep the best N individuals
    int elitismCount = 0; 
    if (config_.populationSize > 10) { // Basic heuristic for elitism
        elitismCount = std::min(2, config_.populationSize / 10); 
    } else if (config_.populationSize > 0) {
        elitismCount = 1;
    }


    if (elitismCount > 0 && !population_.isEmpty()) {
         QVector<Individual> sortedPopulation = population_;
         // std::sort uses operator<, which is defined as `fitness > other.fitness`
         // so sorting ascending will put highest fitness (best) at the beginning.
         std::sort(sortedPopulation.begin(), sortedPopulation.end()); 
         for(int i=0; i < elitismCount && i < sortedPopulation.size(); ++i) {
             nextGenerationPopulation.append(sortedPopulation[i]);
         }
    }

    // Fill the rest of the population using tournament selection
    while (nextGenerationPopulation.size() < config_.populationSize) {
        nextGenerationPopulation.append(tournamentSelection());
    }
    population_ = nextGenerationPopulation;
}

Individual GeneticAlgorithm::tournamentSelection() {
    int tournamentSize = std::max(2, config_.populationSize / 10); 
    if (population_.isEmpty()) return Individual(); // Should not happen if called after populate

    Individual bestInTournament = population_[randomInt(0, population_.size() - 1)];

    for (int i = 1; i < tournamentSize; ++i) {
        const Individual& contender = population_[randomInt(0, population_.size() - 1)];
        // operator< is (fitness > other.fitness), so (a < b) means a.fitness > b.fitness
        // We want the one with higher fitness.
        if (contender.fitness > bestInTournament.fitness) { 
            bestInTournament = contender;
        }
    }
    return bestInTournament;
}

void GeneticAlgorithm::crossover() {
    QVector<Individual> offspringPopulation;
    offspringPopulation.reserve(population_.size());

    // Account for elites already added by selection (if selection does that directly)
    // Our current selection replaces the whole population, so elites are already there.
    
    // Shuffle for random pairing, but skip elites if they were added first and we want to preserve them.
    // For simplicity, our current selection already forms the new base population.
    // We can shuffle a list of indices to pick parents for crossover.
    
    QVector<int> parentIndices(population_.size());
    std::iota(parentIndices.begin(), parentIndices.end(), 0); // Fill with 0, 1, ..., n-1
    std::shuffle(parentIndices.begin(), parentIndices.end(), getrng());

    for (int i = 0; i < population_.size() - 1; i += 2) {
        Individual& parent1 = population_[parentIndices[i]];
        Individual& parent2 = population_[parentIndices[i+1]];

        if (randomDouble(0, 1) < 0.7) { // Crossover probability (e.g. 70%)
            QPair<Individual, Individual> children = orderedCrossover(parent1, parent2);
            offspringPopulation.append(children.first);
            offspringPopulation.append(children.second);
        } else {
            offspringPopulation.append(parent1);
            offspringPopulation.append(parent2);
        }
    }
    // Handle odd population size by adding the last shuffled parent if not crossed over
    if (population_.size() % 2 != 0) {
        if (offspringPopulation.size() < population_.size()) // Ensure we don't skip if all were crossed over
            offspringPopulation.append(population_[parentIndices.last()]);
    }
    
    // Ensure population size is maintained.
    // If elitism was separate, add elites first, then fill with offspring.
    // Since our selection already includes elites, this offspringPopulation replaces it.
    if (offspringPopulation.size() == config_.populationSize) {
         population_ = offspringPopulation;
    } else {
        // Fallback: if offspring count doesn't match, refill (crude, indicates issue in logic)
        // This can happen if elitism is handled outside crossover and crossover produces varying numbers.
        // For now, assume selection has already picked the base for next-gen, and crossover modifies *that*.
        // Or, if crossover produces children to *replace* parts of population, ensure sizes match.
        // A common strategy: elites + (pop_size - elites) children.
        // Our selection does a full replacement, so crossover generates a full new set (or modifies current).
        // The current crossover directly creates an offspring population, which should ideally be `config_.populationSize`.
        // If not, it's a bug in pairing/handling odd numbers.
        if(offspringPopulation.size() > config_.populationSize) {
             while(offspringPopulation.size() > config_.populationSize) offspringPopulation.pop_back();
        } else {
             while(offspringPopulation.size() < config_.populationSize && !population_.isEmpty()) {
                 offspringPopulation.append(population_[randomInt(0, population_.size()-1)]); // Fill with random from old
             }
        }
        population_ = offspringPopulation;
    }
}

// Ordered Crossover (OX1) - specific implementation details can vary.
QPair<Individual, Individual> GeneticAlgorithm::orderedCrossover(const Individual& parent1, const Individual& parent2) {
    Individual child1, child2;
    int chromoSize = parent1.chromosome.size();
    if (chromoSize == 0) return qMakePair(child1, child2);

    child1.chromosome.resize(chromoSize);
    child2.chromosome.resize(chromoSize);

    int start = randomInt(0, chromoSize - 1);
    int end = randomInt(0, chromoSize - 1);
    if (start == end && chromoSize > 1) { // Ensure segment has at least 1 element if possible
        end = (end + 1) % chromoSize;
    }
    if (start > end) std::swap(start, end);

    // Copy the segment from parents to children
    for (int i = start; i <= end; ++i) {
        child1.chromosome[i] = parent1.chromosome[i];
        child2.chromosome[i] = parent2.chromosome[i];
    }
    
    // Helper lambda to fill remaining genes
    auto fill_remaining = [&](Individual& child, const Individual& segmentParent, const Individual& fillParent) {
        QVector<bool> geneInChildSegment(chromoSize + availableParts_.size(), false); // Max possible unique ID if sourceIndex is global
        // Mark genes that are already in child's copied segment
        // This requires a robust way to identify unique genes if part IDs can repeat.
        // Assuming Gene (partId + sourceIndex) is unique for now.
        for (int i = start; i <= end; ++i) {
            // This check is tricky if sourceIndex is not globally unique or partId is not enough.
            // For OX1, you typically check against the *values* (partId+sourceIndex) of the genes.
            // Let's use a set-like structure for quick lookups of what's in the segment.
            // For simplicity, we'll iterate.
        }

        int fillParentIdx = 0;
        for (int i = 0; i < chromoSize; ++i) {
            int currentChildPos = (end + 1 + i) % chromoSize; // Start filling after the copied segment
            if (child.chromosome[currentChildPos].partId.isNull()) { // If not filled by segment
                // Find next gene from fillParent not already in child's segment
                while (fillParentIdx < chromoSize) {
                    const Gene& geneToConsider = fillParent.chromosome[fillParentIdx];
                    bool gene_already_in_segment = false;
                    for(int k=start; k <= end; ++k) {
                        if(child.chromosome[k].partId == geneToConsider.partId &&
                           child.chromosome[k].sourceIndex == geneToConsider.sourceIndex) {
                            gene_already_in_segment = true;
                            break;
                        }
                    }
                    fillParentIdx++; // Move to next gene in fillParent
                    if (!gene_already_in_segment) {
                        child.chromosome[currentChildPos] = geneToConsider;
                        break; 
                    }
                }
            }
        }
    };

    fill_remaining(child1, parent1, parent2);
    fill_remaining(child2, parent2, parent1);
    
    child1.fitness = -1; // Mark as unevaluated
    child2.fitness = -1;

    return qMakePair(child1, child2);
}


void GeneticAlgorithm::mutation() {
    double mutationThreshold = static_cast<double>(config_.mutationRate) / 100.0;
    for (Individual& individual : population_) {
        // Don't mutate elites if selection already preserved them and we want to keep them pristine for this gen
        // However, our current selection replaces the pop, so all are fair game.
        if (randomDouble(0, 1) < mutationThreshold) {
            mutateIndividual(individual);
        }
    }
}

void GeneticAlgorithm::mutateIndividual(Individual& individual) {
    int mutationType = randomInt(0, 1); 
    if (mutationType == 0 && individual.chromosome.size() >= 2) {
        scrambleMutation(individual);
    } else if (config_.rotations > 1 && !individual.chromosome.isEmpty()) { 
        rotationMutation(individual);
    }
    individual.fitness = -1; // Mark as unevaluated after mutation
}

void GeneticAlgorithm::scrambleMutation(Individual& individual) {
    int size = individual.chromosome.size();
    if (size < 2) return;
    int start = randomInt(0, size - 1); // Can be size-1 for a 1-element scramble
    int end = randomInt(0, size - 1);
    if (start == end && size > 1) { // Ensure at least 2 elements if possible for non-trivial scramble
         end = (end + 1 + randomInt(0, size-2)) % size; // Pick another distinct index
    }
    if (start > end) std::swap(start, end);
    if (end - start + 1 < 2 && size >=2) { // Ensure scramble segment is at least 2 if possible
        if (end < size -1 ) end++;
        else if (start > 0) start --;
    }
    if (end - start + 1 >= 2) { // only scramble if segment is 2 or more
      std::shuffle(individual.chromosome.begin() + start, individual.chromosome.begin() + end + 1, getrng());
    }
}

void GeneticAlgorithm::rotationMutation(Individual& individual) {
    if (config_.rotations <= 1 || individual.chromosome.isEmpty()) return;
    int geneIdx = randomInt(0, individual.chromosome.size() - 1);
    
    // Calculate current rotation step
    double anglePerStep = (360.0 / config_.rotations);
    int currentRotationStep = static_cast<int>(round(individual.chromosome[geneIdx].rotation / anglePerStep)) % config_.rotations;

    int newRotationStep = randomInt(0, config_.rotations - 1);
    // Ensure new rotation is different if possible and more than one rotation exists
    if (config_.rotations > 1 && newRotationStep == currentRotationStep) {
        newRotationStep = (currentRotationStep + 1) % config_.rotations; 
    }
    individual.chromosome[geneIdx].rotation = newRotationStep * anglePerStep;
}

} // namespace Core
