#include "NestingWorker.h"
#include "GeometryProcessor.h" 
#include <QRandomGenerator>   
#include <QDebug>
#include <limits> 
#include <cmath> // For std::cos, std::sin, M_PI, std::abs
#include <numeric> // For std::iota

NestingWorker::NestingWorker(
    int individualId,
    const Individual& individualConfig,
    const QList<Part>& allUniquePlaceableParts, 
    const QList<Part>& sheetParts,       
    const AppConfig& appConfig,
    NfpCache* nfpCache,
    NfpGenerator* nfpGenerator)
    : m_individualId(individualId),
      m_individualConfig(individualConfig),
      m_allUniqueParts(allUniquePlaceableParts), 
      m_sheetPartsList(sheetParts),
      m_appConfig(appConfig),
      m_nfpCache(nfpCache),
      m_nfpGenerator(nfpGenerator) {
    setAutoDelete(true); 
}

void NestingWorker::run() {
    qInfo() << "NestingWorker ID" << m_individualId << "started for individual sequence size:" << m_individualConfig.partIndices.size();
    
    m_partsToPlaceThisRun.clear();
    for (size_t i = 0; i < m_individualConfig.partIndices.size(); ++i) {
        int uniquePartListIndex = m_individualConfig.partIndices[i]; 
        double rotationStep = m_individualConfig.rotations[i];
        
        if (uniquePartListIndex < 0 || uniquePartListIndex >= m_allUniqueParts.size()) {
            qWarning() << "NestingWorker ID" << m_individualId << "Error: Invalid part index" << uniquePartListIndex;
            continue;
        }
        
        const Part& basePart = m_allUniqueParts[uniquePartListIndex];
        Polygon rotatedGeometry = getTransformedPartGeometry(uniquePartListIndex, rotationStep);

        Part partInstance = basePart; 
        partInstance.geometry = rotatedGeometry; 
        // Store rotation step with the instance for easier access in NFP functions
        // This requires Part struct to have a way to store this, e.g. a QVariantMap properties
        // For now, we'll pass rotationStep alongside partInstance when calling NFP functions.
        m_partsToPlaceThisRun.push_back(partInstance);
    }
    
    m_availableSheetsThisRun = m_sheetPartsList; 

    NestResult result = placeParts();
    qInfo() << "NestingWorker ID" << m_individualId << "finished. Fitness:" << result.fitness << "Parts placed:" << result.partsPlacedCount;
    emit resultReady(result, m_individualId);
}

Polygon NestingWorker::getTransformedPartGeometry(int uniquePartListIndex, double rotationStep) {
    if (uniquePartListIndex < 0 || uniquePartListIndex >= m_allUniqueParts.size()) {
        qWarning() << "NestingWorker: Invalid part index" << uniquePartListIndex << "in getTransformedPartGeometry.";
        return Polygon();
    }
    const Part& basePart = m_allUniqueParts[uniquePartListIndex];
    double rotationDegrees = rotationStep * (360.0 / static_cast<double>(m_appConfig.rotations)); // Ensure rotations > 0
    return GeometryProcessor::rotatePolygon(basePart.geometry, rotationDegrees);
}

// partInstance is already rotated. partRotationStep is the discrete step (0, 1, 2...).
std::vector<Polygon> NestingWorker::getInnerNfp(const Part& sheet, const Part& partInstance, int partRotationStep, const AppConfig& config) {
    NfpKey key;
    key.partAId = sheet.id; 
    key.partBId = partInstance.id; 
    key.rotationA = 0;  
    key.rotationB = partRotationStep;
    key.forInnerNfp = true;

    if (m_nfpCache->has(key)) {
        return m_nfpCache->get(key);
    }

    Point partReferenceShift = GeometryProcessor::getPolygonBoundsMin(partInstance.geometry);
    
    std::vector<Polygon> nfp = m_nfpGenerator->calculateNFP(
        sheet.geometry,          
        partInstance.geometry,    
        config.clipperScale,
        -partReferenceShift.x,   
        -partReferenceShift.y
    );

    m_nfpCache->insert(key, nfp);
    return nfp;
}

// Both part instances are already rotated. Their rotation steps are passed for the cache key.
std::vector<Polygon> NestingWorker::getOuterNfp(const Part& placedPartInstance, int placedPartRotationStep, 
                                                const Part& currentPartInstance, int currentPartRotationStep, 
                                                const AppConfig& config) {
    NfpKey key;
    key.partAId = placedPartInstance.id; 
    key.rotationA = placedPartRotationStep;
    key.partBId = currentPartInstance.id;  
    key.rotationB = currentPartRotationStep;
    key.forInnerNfp = false;

    if (m_nfpCache->has(key)) {
        return m_nfpCache->get(key);
    }
    
    // Outer NFP for (A, B) is A + MinkowskiSum(ReflectedOrigin(B))
    // Or, as per deepnest.js for this case (when B is not a hole of A): NFP = MinkowskiSum(A, B)
    // Where A is stationary (placedPartInstance) and B is orbiting (currentPartInstance).
    Clipper2Lib::Paths64 minkowski_paths = GeometryProcessor::minkowskiSum(placedPartInstance.geometry, currentPartInstance.geometry);
    std::vector<Polygon> nfp_polys = GeometryProcessor::Paths64ToPolygons(minkowski_paths);

    // Shift NFP to be relative to the origin/reference point of the orbiting part (currentPartInstance)
    Point currentPartReferenceShift = GeometryProcessor::getPolygonBoundsMin(currentPartInstance.geometry);
    std::vector<Polygon> final_nfp;
    final_nfp.reserve(nfp_polys.size());
    for (const auto& p : nfp_polys) {
        Polygon shifted_p;
        shifted_p.outer.reserve(p.outer.size());
        for (const auto& pt : p.outer) {
            shifted_p.outer.push_back({pt.x - currentPartReferenceShift.x, pt.y - currentPartReferenceShift.y});
        }
        for (const auto& hole_pts : p.holes) { 
            std::vector<Point> shifted_hole_pts;
            shifted_hole_pts.reserve(hole_pts.size());
            for (const auto& pt_h : hole_pts) {
                 shifted_hole_pts.push_back({pt_h.x - currentPartReferenceShift.x, pt_h.y - currentPartReferenceShift.y});
            }
            shifted_p.holes.push_back(shifted_hole_pts);
        }
        final_nfp.push_back(shifted_p);
    }
    
    m_nfpCache->insert(key, final_nfp);
    return final_nfp;
}

bool NestingWorker::findBestPlacement(const std::vector<Polygon>& nfpPaths, Point& outPosition) {
    double min_y = std::numeric_limits<double>::max();
    double min_x_at_min_y = std::numeric_limits<double>::max();
    bool found = false;

    for (const auto& nfpPoly : nfpPaths) {
        for (const auto& pt : nfpPoly.outer) { // Consider all points, not just vertices if NFP is complex
            if (!found || pt.y < min_y) {
                min_y = pt.y;
                min_x_at_min_y = pt.x;
                found = true;
            } else if (pt.y == min_y) {
                if (pt.x < min_x_at_min_y) {
                    min_x_at_min_y = pt.x;
                }
            }
        }
    }
    if(found){
        outPosition = {min_x_at_min_y, min_y};
    }
    return found; 
}


NestResult NestingWorker::placeParts() {
    NestResult nestResult;
    nestResult.fitness = 0.0; 
    double totalSheetUsedAreaApprox = 0; 
    double totalPartsAreaPlacedScaled = 0;

    std::vector<Part> remainingPartsToPlace = m_partsToPlaceThisRun; 
    QList<Part> availableSheets = m_availableSheetsThisRun; 

    if(availableSheets.isEmpty() && !remainingPartsToPlace.empty()){
        qWarning() << "NestingWorker ID" << m_individualId << ": No sheets available.";
        nestResult.fitness = std::numeric_limits<double>::max(); 
        return nestResult;
    }

    std::vector<int> p_config_indices(m_partsToPlaceThisRun.size());
    std::iota(p_config_indices.begin(), p_config_indices.end(), 0);

    for (int sheetIdx = 0; sheetIdx < availableSheets.size() && !remainingPartsToPlace.empty(); ++sheetIdx) {
        const Part& currentSheet = availableSheets[sheetIdx]; 
        NestSheet currentNestSheetResult;
        currentNestSheetResult.sheetPartId = currentSheet.id;

        std::vector<PlacedPart> partsPlacedOnThisSheet_data; 
        Clipper2Lib::Paths64 combinedForbiddenRegionsOnSheet_clipper; 

        std::vector<Part> tempPlacedPartsOnSheet_instances; 
        std::vector<int>  tempPlacedPartsOnSheet_rotationSteps;


        for (int i = 0; i < static_cast<int>(remainingPartsToPlace.size()); /* no i++ here */ ) {
            Part& partToPlace_Instance = remainingPartsToPlace[i]; 
            int originalIndividualConfigIndex = p_config_indices[i];
            int currentPartRotationStep = static_cast<int>(m_individualConfig.rotations[originalIndividualConfigIndex]);

            std::vector<Polygon> finalNfpForPlacement;
            Point placementPosition = {0,0};

            if (tempPlacedPartsOnSheet_instances.empty()) { 
                finalNfpForPlacement = getInnerNfp(currentSheet, partToPlace_Instance, currentPartRotationStep, m_appConfig);
            } else {
                Clipper2Lib::Paths64 sheetNfpForPart_ClipperPaths;
                std::vector<Polygon> sheetNfpForPart_Polygons = getInnerNfp(currentSheet, partToPlace_Instance, currentPartRotationStep, m_appConfig);
                for(const auto& p : sheetNfpForPart_Polygons) { 
                    sheetNfpForPart_ClipperPaths.push_back(GeometryProcessor::PointsToPath64(p.outer));
                }

                if (sheetNfpForPart_ClipperPaths.empty()) {
                    i++; 
                    continue;
                }
                
                Clipper2Lib::Paths64 forbiddenRegionsFromOtherParts_clipper;
                for(size_t k=0; k < tempPlacedPartsOnSheet_instances.size(); ++k) {
                    const Part& existingPlacedPart_Instance = tempPlacedPartsOnSheet_instances[k]; 
                    int existingPlacedPart_RotationStep = tempPlacedPartsOnSheet_rotationSteps[k];
                    Point existingPlacedPart_Position = partsPlacedOnThisSheet_data[k].position; 

                    std::vector<Polygon> outerNfp_Polygons = getOuterNfp(existingPlacedPart_Instance, existingPlacedPart_RotationStep, 
                                                                         partToPlace_Instance, currentPartRotationStep, 
                                                                         m_appConfig);
                    for(const auto& nfp_poly : outerNfp_Polygons) {
                        Clipper2Lib::Path64 nfp_path = GeometryProcessor::PointsToPath64(nfp_poly.outer);
                        Clipper2Lib::TranslatePath(nfp_path, 
                                                   static_cast<long long>(existingPlacedPart_Position.x * GeometryProcessor::CLIPPER_SCALE),
                                                   static_cast<long long>(existingPlacedPart_Position.y * GeometryProcessor::CLIPPER_SCALE));
                        forbiddenRegionsFromOtherParts_clipper = Clipper2Lib::Union(forbiddenRegionsFromOtherParts_clipper, Clipper2Lib::Paths64{nfp_path}, Clipper2Lib::FillRule::NonZero);
                    }
                }
                
                Clipper2Lib::Paths64 placeableRegions_paths = Clipper2Lib::Difference(sheetNfpForPart_ClipperPaths, forbiddenRegionsFromOtherParts_clipper, Clipper2Lib::FillRule::NonZero);
                finalNfpForPlacement = GeometryProcessor::Paths64ToPolygons(placeableRegions_paths);
            }

            if (findBestPlacement(finalNfpForPlacement, placementPosition)) {
                PlacedPart placedPartData;
                placedPartData.partId = partToPlace_Instance.id; 
                placedPartData.position = placementPosition;
                placedPartData.rotation = currentPartRotationStep * (360.0 / (m_appConfig.rotations == 0 ? 1.0 : static_cast<double>(m_appConfig.rotations)) );


                partsPlacedOnThisSheet_data.push_back(placedPartData);
                
                tempPlacedPartsOnSheet_instances.push_back(partToPlace_Instance); 
                tempPlacedPartsOnSheet_rotationSteps.push_back(currentPartRotationStep);

                nestResult.partsPlacedCount++;
                totalPartsAreaPlacedScaled += std::abs(Clipper2Lib::Area(GeometryProcessor::PointsToPath64(partToPlace_Instance.geometry.outer)));

                remainingPartsToPlace.erase(remainingPartsToPlace.begin() + i);
                p_config_indices.erase(p_config_indices.begin() + i); 
            } else {
                i++; 
            }
        } 
        
        if (!partsPlacedOnThisSheet_data.empty()) {
            currentNestSheetResult.placements = partsPlacedOnThisSheet_data;
            nestResult.sheets.push_back(currentNestSheetResult);
            totalSheetAreaUsedApprox += 1.0; 
        }
        double currentProgress = m_partsToPlaceThisRun.empty() ? 100.0 : (static_cast<double>(m_partsToPlaceThisRun.size() - remainingPartsToPlace.size()) / m_partsToPlaceThisRun.size()) * 100.0;
        emit progressUpdated(currentProgress); 

    } 

    double unplacedPenalty = remainingPartsToPlace.size() * m_appConfig.svgImportScale * 10000; 
    nestResult.fitness = unplacedPenalty + totalSheetAreaUsedApprox * m_appConfig.svgImportScale * 100; 
    double totalPartsAreaUnscaled = totalPartsAreaPlacedScaled / (GeometryProcessor::CLIPPER_SCALE * GeometryProcessor::CLIPPER_SCALE);
    nestResult.fitness -= totalPartsAreaUnscaled * 0.01; // Prefer solutions that place more area

    if (remainingPartsToPlace.empty()) {
        qInfo() << "NestingWorker ID" << m_individualId << "Placed all parts successfully.";
    } else {
        qInfo() << "NestingWorker ID" << m_individualId << "Failed to place" << remainingPartsToPlace.size() << "parts.";
    }

    return nestResult;
}
