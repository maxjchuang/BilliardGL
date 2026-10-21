#pragma once

#include "automation_json.h"
#include "training/trajectory_compound_visibility.h"

#include <array>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace billiardgl::training {

struct FinalOwnershipCandidateTelemetry {
    std::string sourceProbeId;
    int xPhysicalPx = 0;
    int yPhysicalPx = 0;
    std::size_t resolvedSampleCount = 0;
    double resolvedPixelEquivalentArea = 0.0;
    std::size_t exclusiveOwnedPixelCount = 0;
    std::size_t exclusiveLargestFourConnectedPixelCount = 0;
    std::size_t preArbitrationSupportPixelCount = 0;
    std::size_t preArbitrationLargestEightConnectedPixelCount = 0;
    std::string ownerTrack;
    std::string ownerComponent;
    std::string ownerPrimitiveId;
    std::string ownerVisualSublayer;
    double tangentDistancePx = 0.0;
    double normalDistancePx = 0.0;
    std::array<std::uint8_t, 3> finalSrgb8{{0, 0, 0}};
    std::array<std::uint8_t, 3> adjacentNonSelfSrgb8{{0, 0, 0}};
    double edgeContrastRatio = 0.0;
    std::string eligibilityOrRejectionReason;
};

struct FinalOwnershipArbitrationTelemetry {
    std::string physicalSampleOrPixelId;
    std::vector<std::string> claimingSourceProbeIds;
    std::string winnerSourceProbeId;
    std::string deterministicSortKey;
    std::string loserReason;
};

// Every component-aware four-connected final-LDR boundary alternative is
// preserved before classification. The selected pair is one member of this
// complete set; raw semantic-token differences alone are never sufficient.
struct FinalOwnershipBoundaryTelemetry {
    std::string sourceProbeId;
    int ownerXPhysicalPx = 0;
    int ownerYPhysicalPx = 0;
    int adjacentXPhysicalPx = 0;
    int adjacentYPhysicalPx = 0;
    int sampleIndex = 0;
    std::string ownerTrack;
    std::string ownerComponent;
    std::string ownerPrimitiveId;
    std::string ownerVisualSublayer;
    std::string adjacentTrack;
    std::string adjacentComponent;
    std::string adjacentPrimitiveId;
    std::string adjacentVisualSublayer;
    bool adjacentLaterUiOwned = false;
    std::array<std::uint8_t, 3> ownerFinalSrgb8{{0, 0, 0}};
    std::array<std::uint8_t, 3> adjacentFinalSrgb8{{0, 0, 0}};
    double edgeContrastRatio = 0.0;
    std::string boundaryClass;
    std::string deterministicSortKey;
    // Boundary evidence can be associated through the exact production
    // primitive stem or its final same-track owned island without transferring
    // exclusive resolved-area credit between probes.
    std::string associationMode;
    std::string associationSeedPrimitiveStem;
    std::string associatedOwnedIslandId;
    std::string boundaryAreaOwnerSourceProbeId;
    bool eligibleForRole = false;
    bool selected = false;
};

struct Phase4MachinePredicateTelemetry {
    std::string name;
    bool result = false;
    std::string measured;
    std::string expected;
};

struct Phase4CaptureTelemetryInput {
    std::string captureId;
    std::size_t planOrdinalOneBased = 0;
    bool machinePass = false;
    std::string machineErrorCode;
    std::string ppmPath;
    std::string ppmSha256;
    CompoundVisibilityAudit audit;
    std::vector<FinalOwnershipCandidateTelemetry> ownershipCandidates;
    std::vector<FinalOwnershipArbitrationTelemetry> arbitrationRecords;
    std::vector<FinalOwnershipBoundaryTelemetry> componentAwareBoundaryRecords;
    std::vector<Phase4MachinePredicateTelemetry> machinePredicateLedger;
    bool machinePredicateLedgerRequired = false;
    std::map<std::string, std::string> sourceAndBinaryBindings;
    bool integratedFinalOwnership = false;
    bool legacyPpmDiagnosticOnly = false;
    bool legacyPpmContrastOk = false;
    std::size_t legacyPpmVisibleProbeCount = 0;
    std::size_t legacyPpmHiddenProbeCount = 0;
    double legacyPpmVisibleProbeCoverage = 0.0;
    bool legacyIntegratedDecisionDiscrepancy = false;
    bool forceDurableFlushFailureForTest = false;
};

struct Phase4TelemetryWriteResult {
    bool ok = false;
    std::string errorCode;
    std::string canonicalRow;
};

json::Value buildPhase4CaptureTelemetryRow(
    const Phase4CaptureTelemetryInput& input);

Phase4TelemetryWriteResult appendPhase4CaptureTelemetryRowDurable(
    const std::string& path, const Phase4CaptureTelemetryInput& input);

// The row is durably appended before the unchanged machine decision is
// enforced. A failed machine decision therefore always remains diagnosable.
void preservePhase4TelemetryThenEnforceMachineGate(
    const std::string& path, const Phase4CaptureTelemetryInput& input);

}  // namespace billiardgl::training
