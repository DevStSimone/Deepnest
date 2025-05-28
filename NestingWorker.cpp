#include "NestingWorker.h"
#include "GeometryProcessor.h" 
#include <QRandomGenerator>   
#include <QDebug>
#include <limits> 
#include <cmath> 
#include <numeric> 
#include <stdexcept> // For std::exception

// Helper for debugging Polygon structure
QString polygonToString(const Polygon& poly) {
    QString s = QString("Outer (%1 pts): ").arg(poly.outer.size());
    if (!poly.outer.empty()) {
      s += QString("First pt: (%1,%2) ").arg(poly.outer.front().x).arg(poly.outer.front().y);
    }
    s += QString("Holes: %1").arg(poly.holes.size());
    return s;
}


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
    setAutoDelete(true); // QThreadPool will manage deletion
}

void NestingWorker::run() {
    qInfo().noquote() << QString("NestingWorker ID %1: Run started. Individual sequence size: %2.")
              .arg(m_individualId)
              .arg(m_individualConfig.partIndices.size());
    
    m_partsToPlaceThisRun.clear();
    for (size_t i = 0; i < m_individualConfig.partIndices.size(); ++i) {
        int uniquePartListIndex = m_individualConfig.partIndices[i]; 
        double rotationStep = m_individualConfig.rotations[i]; 
        
        if (uniquePartListIndex < 0 || uniquePartListIndex >= m_allUniqueParts.size()) {
            qWarning().noquote() << QString("NestingWorker ID %1: Error - Invalid part index %2 from individual config. Max unique parts: %3")
                         .arg(m_individualId).arg(uniquePartListIndex).arg(m_allUniqueParts.size());
            // Potentially emit error result and return early if critical
            NestResult errorResult; 
            errorResult.fitness = std::numeric_limits<double>::max(); 
            errorResult.partsPlacedCount = -1; // Indicate error
            emit resultReady(errorResult, m_individualId);
            return;
        }
        
        const Part& basePart = m_allUniqueParts[uniquePartListIndex];
        Polygon rotatedGeometry = getTransformedPartGeometry(uniquePartListIndex, rotationStep);

        Part partInstance = basePart; 
        partInstance.geometry = rotatedGeometry; 
        m_partsToPlaceThisRun.push_back(partInstance);
    }
    
    m_availableSheetsThisRun = m_sheetPartsList; 

    try {
        NestResult result = placeParts();
        // Fitness and parts placed count are logged inside placeParts completion message now.
        emit resultReady(result, m_individualId);
    } catch (const std::exception& e) {
        qCritical().noquote() << QString("NestingWorker ID %1: Exception in placeParts - %2")
                        .arg(m_individualId).arg(e.what());
        NestResult errorResult; 
        errorResult.fitness = std::numeric_limits<double>::max(); 
        errorResult.partsPlacedCount = -1; 
        emit resultReady(errorResult, m_individualId);
    } catch (...) {
        qCritical().noquote() << QString("NestingWorker ID %1: Unknown exception in placeParts.")
                        .arg(m_individualId);
        NestResult errorResult;
        errorResult.fitness = std::numeric_limits<double>::max();
        errorResult.partsPlacedCount = -1;
        emit resultReady(errorResult, m_individualId);
    }
    qInfo().noquote() << QString("NestingWorker ID %1: Run finished.").arg(m_individualId);
}

Polygon NestingWorker::getTransformedPartGeometry(int uniquePartListIndex, double rotationStep) {
    if (uniquePartListIndex < 0 || uniquePartListIndex >= m_allUniqueParts.size()) {
        qWarning().noquote() << QString("NestingWorker ID %1: Invalid part index %2 in getTransformedPartGeometry.")
                     .arg(m_individualId).arg(uniquePartListIndex);
        return Polygon();
    }
    const Part& basePart = m_allUniqueParts[uniquePartListIndex];
    double rotationDegrees = rotationStep * (360.0 / (m_appConfig.rotations == 0 ? 1.0 : static_cast<double>(m_appConfig.rotations)));
    return GeometryProcessor::rotatePolygon(basePart.geometry, rotationDegrees);
}

std::vector<Polygon> NestingWorker::getInnerNfp(const Part& sheet, const Part& partInstance, int partRotationStep, const AppConfig& config) {
    NfpKey key;
    key.partAId = sheet.id; 
    key.partBId = partInstance.id; 
    key.rotationA = 0;  
    key.rotationB = partRotationStep;
    key.forInnerNfp = true;

    qDebug().noquote() << QString("NestingWorker ID %1: GetInnerNFP Cache Check - Sheet: %2, Part: %3, RotStep: %4")
               .arg(m_individualId).arg(sheet.id).arg(partInstance.id).arg(partRotationStep);
    if (m_nfpCache->has(key)) {
        qDebug().noquote() << QString("NestingWorker ID %1: GetInnerNFP Cache HIT - Sheet: %2, Part: %3, RotStep: %4")
                   .arg(m_individualId).arg(sheet.id).arg(partInstance.id).arg(partRotationStep);
        return m_nfpCache->get(key);
    }
    qDebug().noquote() << QString("NestingWorker ID %1: GetInnerNFP Cache MISS - Sheet: %2, Part: %3, RotStep: %4. Calculating...")
               .arg(m_individualId).arg(sheet.id).arg(partInstance.id).arg(partRotationStep);

    Point partReferenceShift = GeometryProcessor::getPolygonBoundsMin(partInstance.geometry);
    
    std::vector<Polygon> nfp = m_nfpGenerator->calculateNFP(
        sheet.geometry,          
        partInstance.geometry,    
        config.clipperScale,
        -partReferenceShift.x,   
        -partReferenceShift.y
    );
    qDebug().noquote() << QString("NestingWorker ID %1: GetInnerNFP Calculated - Sheet: %2, Part: %3, RotStep: %4. NFP Polygons: %5")
               .arg(m_individualId).arg(sheet.id).arg(partInstance.id).arg(partRotationStep).arg(nfp.size());
    if (nfp.empty()){
         qWarning().noquote() << QString("NestingWorker ID %1: GetInnerNFP - Calculation resulted in EMPTY NFP for Sheet: %2, Part: %3, RotStep: %4")
               .arg(m_individualId).arg(sheet.id).arg(partInstance.id).arg(partRotationStep);
    }

    m_nfpCache->insert(key, nfp);
    return nfp;
}

std::vector<Polygon> NestingWorker::getOuterNfp(const Part& placedPartInstance, int placedPartRotationStep, 
                                                const Part& currentPartInstance, int currentPartRotationStep, 
                                                const AppConfig& config) {
    NfpKey key;
    key.partAId = placedPartInstance.id; 
    key.rotationA = placedPartRotationStep;
    key.partBId = currentPartInstance.id;  
    key.rotationB = currentPartRotationStep;
    key.forInnerNfp = false;

    qDebug().noquote() << QString("NestingWorker ID %1: GetOuterNFP Cache Check - Placed: %2 (Rot %3), Current: %4 (Rot %5)")
               .arg(m_individualId).arg(placedPartInstance.id).arg(placedPartRotationStep)
               .arg(currentPartInstance.id).arg(currentPartRotationStep);
    if (m_nfpCache->has(key)) {
         qDebug().noquote() << QString("NestingWorker ID %1: GetOuterNFP Cache HIT - Placed: %2 (Rot %3), Current: %4 (Rot %5)")
                   .arg(m_individualId).arg(placedPartInstance.id).arg(placedPartRotationStep)
                   .arg(currentPartInstance.id).arg(currentPartRotationStep);
        return m_nfpCache->get(key);
    }
     qDebug().noquote() << QString("NestingWorker ID %1: GetOuterNFP Cache MISS - Placed: %2 (Rot %3), Current: %4 (Rot %5). Calculating...")
               .arg(m_individualId).arg(placedPartInstance.id).arg(placedPartRotationStep)
               .arg(currentPartInstance.id).arg(currentPartRotationStep);
    
    Clipper2Lib::Paths64 minkowski_paths = GeometryProcessor::minkowskiSum(placedPartInstance.geometry, currentPartInstance.geometry);
    std::vector<Polygon> nfp_polys = GeometryProcessor::Paths64ToPolygons(minkowski_paths);

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
        if (shifted_p.outer.empty() && !p.outer.empty()) {
            qWarning().noquote() << QString("NestingWorker ID %1: OuterNFP shift resulted in empty outer polygon for non-empty NFP. Part: %2, RefShift: (%3,%4)")
                .arg(m_individualId).arg(currentPartInstance.id).arg(currentPartReferenceShift.x).arg(currentPartReferenceShift.y);
        }
        final_nfp.push_back(shifted_p);
    }
    
    qDebug().noquote() << QString("NestingWorker ID %1: GetOuterNFP Calculated - Placed: %2 (Rot %3), Current: %4 (Rot %5). NFP Polygons: %6")
               .arg(m_individualId).arg(placedPartInstance.id).arg(placedPartRotationStep)
               .arg(currentPartInstance.id).arg(currentPartRotationStep).arg(final_nfp.size());
    if (final_nfp.empty() && !minkowski_paths.empty()){
         qWarning().noquote() << QString("NestingWorker ID %1: OuterNFP calculation resulted in EMPTY polygon list for non-empty minkowski_paths. Placed: %2, Current: %3")
               .arg(m_individualId).arg(placedPartInstance.id).arg(currentPartInstance.id);
    }
    m_nfpCache->insert(key, final_nfp);
    return final_nfp;
}

bool NestingWorker::findBestPlacement(const std::vector<Polygon>& nfpPaths, Point& outPosition) {
    double min_y = std::numeric_limits<double>::max();
    double min_x_at_min_y = std::numeric_limits<double>::max();
    bool found = false;

    for (const auto& nfpPoly : nfpPaths) {
        for (const auto& pt : nfpPoly.outer) { 
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
    qInfo().noquote() << QString("NestingWorker ID %1: placeParts started. Parts to place: %2, Sheets available: %3")
              .arg(m_individualId).arg(m_partsToPlaceThisRun.size()).arg(m_availableSheetsThisRun.size());

    NestResult nestResult;
    nestResult.fitness = 0.0; 
    double totalSheetAreaUsedApprox = 0; 
    double totalPartsAreaPlacedScaled = 0;

    std::vector<Part> remainingPartsToPlace = m_partsToPlaceThisRun; 
    QList<Part> availableSheets = m_availableSheetsThisRun; 

    if(availableSheets.isEmpty() && !remainingPartsToPlace.empty()){
        qWarning().noquote() << QString("NestingWorker ID %1: No sheets available to place %2 parts.")
                     .arg(m_individualId).arg(remainingPartsToPlace.size());
        nestResult.fitness = std::numeric_limits<double>::max(); 
        return nestResult;
    }

    std::vector<int> p_config_indices(m_partsToPlaceThisRun.size());
    std::iota(p_config_indices.begin(), p_config_indices.end(), 0);

    for (int sheetIdx = 0; sheetIdx < availableSheets.size() && !remainingPartsToPlace.empty(); ++sheetIdx) {
        const Part& currentSheet = availableSheets[sheetIdx]; 
        qDebug().noquote() << QString("NestingWorker ID %1: Trying sheet %2 (ID: %3)")
                   .arg(m_individualId).arg(sheetIdx).arg(currentSheet.id);
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
            qDebug().noquote() << QString("NestingWorker ID %1: Attempting to place Part ID %2 (Instance %3 of %4 in sequence), RotationStep %5")
                       .arg(m_individualId).arg(partToPlace_Instance.id)
                       .arg(originalIndividualConfigIndex+1).arg(m_individualConfig.partIndices.size())
                       .arg(currentPartRotationStep);


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
                    qDebug().noquote() << QString("NestingWorker ID %1: Part %2 (Rot %3) - InnerNFP with sheet resulted in empty paths. Cannot place on this sheet with current config.")
                               .arg(m_individualId).arg(partToPlace_Instance.id).arg(currentPartRotationStep);
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
                        if (nfp_poly.outer.empty()) continue;
                        Clipper2Lib::Path64 nfp_path = GeometryProcessor::PointsToPath64(nfp_poly.outer);
                        Clipper2Lib::TranslatePath(nfp_path, 
                                                   static_cast<long long>(existingPlacedPart_Position.x * GeometryProcessor::CLIPPER_SCALE),
                                                   static_cast<long long>(existingPlacedPart_Position.y * GeometryProcessor::CLIPPER_SCALE));
                        forbiddenRegionsFromOtherParts_clipper = Clipper2Lib::Union(forbiddenRegionsFromOtherParts_clipper, Clipper2Lib::Paths64{nfp_path}, Clipper2Lib::FillRule::NonZero);
                    }
                }
                
                Clipper2Lib::Paths64 placeableRegions_paths = Clipper2Lib::Difference(sheetNfpForPart_ClipperPaths, forbiddenRegionsFromOtherParts_clipper, Clipper2Lib::FillRule::NonZero);
                finalNfpForPlacement = GeometryProcessor::Paths64ToPolygons(placeableRegions_paths);

                if(finalNfpForPlacement.empty() && !sheetNfpForPart_ClipperPaths.empty()){
                     qDebug().noquote() << QString("NestingWorker ID %1: Part %2 (Rot %3) - FinalNFP empty after difference. SheetNFP items: %4, Forbidden items from %5 placed parts: %6")
                                .arg(m_individualId).arg(partToPlace_Instance.id).arg(currentPartRotationStep)
                                .arg(sheetNfpForPart_ClipperPaths.size()).arg(tempPlacedPartsOnSheet_instances.size()).arg(forbiddenRegionsFromOtherParts_clipper.size());
                }
            }

            if (findBestPlacement(finalNfpForPlacement, placementPosition)) {
                 qDebug().noquote() << QString("NestingWorker ID %1: Placing Part ID %2 (Rot %3) at (%4, %5)")
                           .arg(m_individualId).arg(partToPlace_Instance.id).arg(currentPartRotationStep)
                           .arg(placementPosition.x).arg(placementPosition.y);
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
                 qDebug().noquote() << QString("NestingWorker ID %1: Part %2 (Rot %3) - No valid placement found on current sheet. FinalNFP paths: %4")
                           .arg(m_individualId).arg(partToPlace_Instance.id).arg(currentPartRotationStep).arg(finalNfpForPlacement.size());
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
    nestResult.fitness -= totalPartsAreaUnscaled * 0.01;
    
    qInfo().noquote() << QString("NestingWorker ID %1: placeParts finished. Fitness: %2. Parts placed: %3/%4. Sheets used: %5.")
              .arg(m_individualId).arg(nestResult.fitness)
              .arg(nestResult.partsPlacedCount).arg(m_partsToPlaceThisRun.size())
              .arg(nestResult.sheets.size());

    return nestResult;
}
