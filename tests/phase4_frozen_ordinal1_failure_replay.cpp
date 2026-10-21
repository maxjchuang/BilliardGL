#include "automation_json.h"
#include "deterministic_hash.h"
#include "training/phase4_final_ownership_instrumentation.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace billiardgl;
using namespace billiardgl::training;

void expect(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

std::string read(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("read_failed:" + path);
    return std::string(std::istreambuf_iterator<char>(input),
        std::istreambuf_iterator<char>());
}

CompoundVisibilityTrack track(const std::string& value)
{
    return value == "predicted" ? CompoundVisibilityTrack::Predicted :
        CompoundVisibilityTrack::Actual;
}

CompoundComponentRole role(const std::string& value)
{
    if (value == "outer_halo_edge")
        return CompoundComponentRole::OuterHaloEdge;
    if (value == "solid_in_predicted_dash_gap")
        return CompoundComponentRole::SolidInPredictedDashGap;
    if (value == "tick") return CompoundComponentRole::Tick;
    if (value == "endpoint") return CompoundComponentRole::Endpoint;
    return CompoundComponentRole::Core;
}

}  // namespace

int main(int argc, char** argv)
{
    try {
        expect(argc == 6,
            "usage: replay <d1> <ppm> <replay> <failure> <output-jsonl>");
        const std::string d1Bytes = read(argv[1]);
        const std::string ppmBytes = read(argv[2]);
        const std::string replayBytes = read(argv[3]);
        const std::string failureBytes = read(argv[4]);
        expect(sha256Hex(ppmBytes) ==
                "f3626f44ead6cfe4729e6ff3b76168747154b2452d0ca2afd70e1520b0e09744",
            "frozen PPM drift");
        expect(sha256Hex(failureBytes) ==
                "2dbd499249df1e44631669165a891f94d232f4fa436c9fd0910716de35d2717f",
            "frozen failure receipt drift");
        const json::ParseResult parsed = json::parse(d1Bytes);
        expect(parsed.ok, "frozen CPU package dump parses");
        const json::Value& d1 = parsed.value;
        expect(d1.at("source_probe_count").asInt() == 95 &&
            d1.at("ordered_source_probe_digest").asString() ==
                "d533924980efc85aeb94e4f1d31831d93e2bfce744be3b66481d08c56d10289a",
            "immutable frozen source inventory drift");

        Phase4CaptureTelemetryInput input;
        input.captureId = "camera_player_low_1080";
        input.planOrdinalOneBased = 1;
        input.machinePass = false;
        input.machineErrorCode =
            "diagnostic_missing_integrated_final_ownership";
        input.ppmPath = argv[2];
        input.ppmSha256 = sha256Hex(ppmBytes);
        input.integratedFinalOwnership = false;
        input.legacyPpmDiagnosticOnly = true;
        input.legacyPpmContrastOk = false;
        input.legacyPpmVisibleProbeCount = 46;
        input.legacyPpmHiddenProbeCount = 49;
        input.legacyPpmVisibleProbeCoverage = 46.0 / 95.0;
        input.legacyIntegratedDecisionDiscrepancy = false;
        input.sourceAndBinaryBindings = {
            {"design_gate_sha256",
                "c3b6f032deaac6bd39d32755d4ab423915e8b70b08b6da18626707b8d7134056"},
            {"diagnostic_manifest_sha256",
                "fb041c6e96c83f30df78742ead8a59251e3425f8b62999dd16390b349bdf7349"},
            {"root_report_sha256",
                "54d0bef6af28085f6e87456645802d4eb3e2537c6ca1869a57fece27141ab50b"},
            {"ta_gate_sha256",
                "ad95db70125384a1de289792002aba05c3557d587a70ea849d5d168f415e57fa"},
            {"frozen_failure_sha256", sha256Hex(failureBytes)},
            {"frozen_replay_sha256", sha256Hex(replayBytes)},
            {"frozen_counts_bound_not_redistributed", "95/46/49"},
            {"ownership_evidence_label", "ANALYTIC_NON_FINAL"},
        };

        CompoundVisibilityAudit& audit = input.audit;
        audit.ok = false;
        audit.errorCode = input.machineErrorCode;
        const auto& probes = d1.at("source_probes").asArray();
        expect(probes.size() == 95, "frozen source rows missing");
        for (const json::Value& value : probes) {
            const std::string id = value.at("source_probe_id").asString();
            expect(id.size() == 64, "immutable source ID malformed");
            const CompoundVisibilityTrack probeTrack =
                track(value.at("track").asString());
            const CompoundComponentRole probeRole =
                role(value.at("component_role").asString());
            const auto& coordinate =
                value.at("physical_coordinate_px").asArray();
            const int x = coordinate[0].asInt();
            const int y = coordinate[1].asInt();
            audit.orderedSourceProbeIds.push_back(id);
            ++audit.sourceProbeCount;
            CompoundTrackVisibilityCounts& counts =
                probeTrack == CompoundVisibilityTrack::Predicted ?
                    audit.predicted : audit.actual;
            ++counts.source;
            ++counts.hidden;
            ++audit.hiddenProbeCount;
            ++audit.componentSourceCounts[
                compoundComponentRoleName(probeRole)];
            ++audit.componentHiddenReasonCounts[
                std::string(compoundComponentRoleName(probeRole)) +
                "|no_same_track_final_owner"];
            audit.hiddenProbes.push_back({id, probeTrack, probeRole,
                CompoundHiddenReason::NoSameTrackFinalOwner, x, y});

            FinalOwnershipCandidateTelemetry candidate;
            candidate.sourceProbeId = id;
            candidate.xPhysicalPx = x;
            candidate.yPhysicalPx = y;
            candidate.ownerTrack = "none";
            candidate.ownerComponent = "none";
            candidate.ownerPrimitiveId = "none";
            candidate.eligibilityOrRejectionReason =
                "analytic_non_final_no_integrated_sample_payload";
            input.ownershipCandidates.push_back(candidate);
            FinalOwnershipArbitrationTelemetry arbitration;
            arbitration.physicalSampleOrPixelId =
                "unavailable:" + id;
            arbitration.claimingSourceProbeIds = {id};
            arbitration.winnerSourceProbeId = "";
            arbitration.deterministicSortKey = "not_run";
            arbitration.loserReason =
                "integrated_final_ownership_missing";
            input.arbitrationRecords.push_back(arbitration);
        }
        audit.orderedSourceProbeDigest =
            d1.at("ordered_source_probe_digest").asString();
        audit.orderedVisibleProbeDigest = sha256Hex("");
        audit.predicted.coverage = 0.0;
        audit.actual.coverage = 0.0;
        audit.visibleProbeCoverage = 0.0;
        audit.unownedEvidenceCount = 95;

        bool expectedFailure = false;
        try {
            preservePhase4TelemetryThenEnforceMachineGate(argv[5], input);
        } catch (const std::runtime_error& failure) {
            expectedFailure = std::string(failure.what()).find(
                "phase4_overlay_machine_gate_failed") == 0;
        }
        expect(expectedFailure,
            "read-only replay must preserve then fail closed");
        const json::ParseResult row = json::parse(read(argv[5]));
        expect(row.ok && row.value.at("machine_status").asString() ==
                "FAILED" &&
            row.value.at("source_probe_count").asInt() == 95 &&
            row.value.at("ordered_source_probe_ids").asArray().size() == 95 &&
            row.value.at("hidden_probe_id_track_role_reason_coordinates")
                .asArray().size() == 95 &&
            row.value.at("ownership_candidate_records").asArray().size() == 95 &&
            row.value.at("exclusive_arbitration_records").asArray().size() == 95 &&
            !row.value.at("integrated_final_ownership").asBool(),
            "complete non-final failure row was not preserved");
        std::cout << "frozen ordinal1 read-only failure replay PASS: "
                  << "95 immutable IDs; 46/49 bound, not redistributed; "
                  << "integrated ownership explicitly non-final\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "frozen ordinal1 replay failed: " << error.what() << '\n';
        return 1;
    }
}
