#pragma once

#include "automation_json.h"
#include "deterministic_hash.h"
#include "game_runtime.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <vector>
#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>

namespace billiardgl::diagnostics {

// Local diagnostic evidence only. Never feeds simulation or rule decisions.
class PlaytestLogger {
public:
    enum class RetentionMode { FullEvidence, BoundedRolling };
    explicit PlaytestLogger(const std::string& directory,
        const std::string& version, std::size_t segmentBytes = 32 * 1024 * 1024,
        RetentionMode mode = RetentionMode::FullEvidence)
        : version_(version), limit_(segmentBytes), mode_(mode) {
        try {
            std::filesystem::create_directories(directory);
            const auto stamp = std::chrono::system_clock::now().time_since_epoch().count();
            for (unsigned n = 0; n < 1000; ++n) {
                const auto candidate = std::filesystem::path(directory) /
                    ("session-" + std::to_string(stamp) + "-" + std::to_string(n));
                if (std::filesystem::create_directory(candidate)) {
                    directory_ = candidate.string();
                    openSegment();
                    break;
                }
            }
        } catch (...) { fail("session_open_failed"); }
        if (directory_.empty()) fail("session_unique_directory_unavailable");
    }
    ~PlaytestLogger() { close(); }
    PlaytestLogger(const PlaytestLogger&) = delete;
    PlaytestLogger& operator=(const PlaytestLogger&) = delete;
    bool good() const noexcept { return !failed_ && file_ != nullptr && !std::ferror(file_); }
    bool fullEvidence() const noexcept { return mode_ == RetentionMode::FullEvidence; }
    const std::string& failureDetail() const noexcept { return failureDetail_; }
    void close() noexcept {
        if (closed_) return;
        closed_ = true;
        try { closeSegment(); } catch (...) { fail("segment_receipt_write_failed"); }
        try {
            auto receipt = json::Value::object();
            receipt["schema"] = json::Value("billiardgl_playtest_session_receipt_v2");
            receipt["status"] = json::Value(failed_ ? "FAILED_RECORDING" :
                fullEvidence() ? "FULL_EVIDENCE_CLOSED" : "PARTIAL_BOUNDED_CLOSED");
            receipt["retention_complete"] = json::Value(fullEvidence() && !failed_);
            receipt["next_event_sequence"] = json::Value(std::to_string(sequence_));
            receipt["segment_count"] = json::Value(std::to_string(segment_ + 1));
            receipt["last_segment_receipt_sha256"] = json::Value(previousReceiptHash_);
            receipt["failure_detail"] = json::Value(failureDetail_);
            if (!directory_.empty()) writeFresh(std::filesystem::path(directory_) / "session-closed.json",
                json::stringify(receipt) + "\n");
        } catch (...) { fail("session_receipt_write_failed"); }
    }
    std::string path() const { return directory_; }
    double recordMilliseconds() const noexcept { return recordMilliseconds_; }

    void record(const std::string& kind, const GameRuntime& runtime,
        json::Value details = json::Value::object()) noexcept {
        if (!good()) return;
        struct Timer {
            double& total;
            std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
            ~Timer() { total += std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - start).count(); }
        } timer{recordMilliseconds_};
        try {
            if (kind == "session_start") metadata_ = details;
            json::Value row = json::Value::object();
            row["schema"] = json::Value("billiardgl_playtest_v1");
            row["kind"] = json::Value(kind);
            row["sequence"] = json::Value(std::to_string(sequence_));
            row["tick"] = json::Value(std::to_string(runtime.tick()));
            row["journal_generation"] = json::Value(std::to_string(runtime.eventJournalGeneration()));
            row["wall_time_ms"] = json::Value(std::to_string(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count()));
            row["monotonic_seconds"] = number(std::chrono::duration<double>(
                std::chrono::steady_clock::now() - start_).count());
            const std::string canonical = canonicalPhysicsProfileText(runtime.physicsProfile());
            if (profileCanonicalText_ != canonical) {
                profileId_ = runtime.physicsProfile().id;
                profileCanonicalText_ = canonical;
                profileHash_ = sha256Hex(canonical);
                profileReplayHash_ = sha256Hex(canonicalPhysicsProfileJson(runtime.physicsProfile()));
            }
            row["physics_profile_id"] = json::Value(profileId_);
            row["physics_profile_sha256"] = json::Value(profileHash_);
            row["physics_profile_canonical_text_sha256"] = json::Value(profileHash_);
            row["physics_profile_replay_canonical_json_sha256"] = json::Value(profileReplayHash_);
            row["details"] = std::move(details);
            auto rules = json::Value::object();
            rules["revision"] = json::Value(std::to_string(runtime.rulesAuthorityRevision()));
            // Authority hashes include ball position: revision-only caching would
            // report stale hashes while balls move. Sample only input boundaries
            // and session start, never the high-frequency physics/frame stream.
            const bool sampleAuthorityHash = kind == "input_before" ||
                kind == "input_after" || kind == "session_start";
            rules["current_authority_hash_sampled"] = json::Value(sampleAuthorityHash);
            rules["current_authority_hash"] = sampleAuthorityHash ?
                json::Value(runtime.rulesAuthorityStateHash()) : json::Value();
            rules["latest_committed_action"] = json::Value();
            if (!runtime.rulesAuthorityActions().empty()) {
                const auto& receipt = runtime.rulesAuthorityActions().back();
                auto action = json::Value::object();
                action["sequence"] = json::Value(std::to_string(receipt.sequence));
                action["tick"] = json::Value(std::to_string(receipt.tick));
                action["decision_id"] = json::Value(receipt.decisionId);
                action["action_id"] = json::Value(receipt.actionId);
                action["actor_player_id"] = json::Value(receipt.actorPlayerId);
                action["previous_revision"] = json::Value(std::to_string(receipt.previousStateRevision));
                action["committed_revision"] = json::Value(std::to_string(receipt.authoritativeStateRevision));
                action["ack_authority_hash"] = json::Value(receipt.authoritativeStateHash);
                rules["latest_committed_action"] = std::move(action);
            }
            row["rules_authority"] = std::move(rules);
            row["balls_moving"] = json::Value(runtime.state().ballsMoving);
            json::Value::Array balls;
            for (int id = 0; id < kBallCount; ++id) {
                const auto& b = runtime.state().balls[id];
                auto item = json::Value::object();
                item["id"] = json::Value(id);
                item["position_cm"] = point(b.position);
                item["velocity_cm_s"] = point(b.velocity);
                item["angular_velocity_rad_s"] = point(b.angularVelocity);
                item["speed_cm_s"] = number(b.speed);
                item["motion_state"] = json::Value(static_cast<int>(b.motionState));
                item["pocketed"] = json::Value(b.pocketed);
                item["out_of_play"] = json::Value(b.outOfPlay);
                item["physical_domain"] = json::Value(static_cast<int>(b.physicalDomain));
                item["pocket_phase"] = json::Value(static_cast<int>(b.pocketInteraction.phase));
                item["pocket_id"] = json::Value(b.pocketInteraction.pocketId);
                item["generation"] = json::Value(std::to_string(b.trajectoryGeneration));
                item["capture_sequence"] = json::Value(std::to_string(b.pocketInteraction.captureSequence));
                balls.push_back(std::move(item));
            }
            row["balls"] = json::Value(balls);
            const auto& shot = runtime.shotControl();
            auto controls = json::Value::object();
            controls["yaw_rad"] = number(shot.yawRadians);
            controls["power"] = number(shot.requestedPower);
            controls["tip_x"] = number(shot.tipOffsetX);
            controls["tip_y"] = number(shot.tipOffsetY);
            controls["aim_mode"] = json::Value(static_cast<int>(runtime.state().aim.mode));
            row["shot"] = controls;
            const auto& camera = runtime.cameraFrame();
            auto view = json::Value::object();
            view["eye"] = point(camera.pose.eye);
            view["target"] = point(camera.pose.target);
            view["state"] = json::Value(static_cast<int>(camera.state));
            row["camera"] = view;
            if (kind == "physics_step" && runtime.hasRecentPhysicsStep()) {
                const auto& record = runtime.latestPhysicsStep();
                const auto& t = record.telemetry;
                auto telemetry = json::Value::object();
                telemetry["attempt_sequence"] = json::Value(std::to_string(record.attemptSequence));
                telemetry["tick"] = json::Value(std::to_string(record.tick));
                telemetry["failure_code"] = json::Value(static_cast<int>(t.failureCode));
                telemetry["failure_detail"] = json::Value(t.failureDetail);
                telemetry["maximum_penetration_cm"] = number(t.maximumPenetrationCm);
                json::Value::Array contacts;
                for (const auto& c : t.contacts) {
                    auto item = json::Value::object();
                    item["kind"] = json::Value(static_cast<int>(c.kind));
                    item["first_ball"] = json::Value(c.firstBall);
                    item["second_ball"] = json::Value(c.secondBall);
                    item["solver_event"] = json::Value(c.solverEventId);
                    item["boundary_feature"] = json::Value(c.boundaryFeatureId);
                    item["normal"] = point(c.normal);
                    item["normal_impulse_ns"] = number(c.normalImpulseNs);
                    item["impulse_on_ball_ns"] = point(c.impulseOnBallNs);
                    item["impulse_on_second_ns"] = point(c.impulseOnSecondNs);
                    item["relative_velocity_before"] = point(c.relativeContactVelocityBeforeCmS);
                    item["relative_velocity_after"] = point(c.relativeContactVelocityAfterCmS);
                    item["energy_before_j"] = number(c.kineticEnergyBeforeJ);
                    item["energy_after_j"] = number(c.kineticEnergyAfterJ);
                    contacts.push_back(item);
                }
                telemetry["contacts"] = json::Value(contacts);
                json::Value::Array departures;
                for (const auto& e : t.tableDepartureEvents) {
                    auto item = json::Value::object();
                    item["kind"] = json::Value(static_cast<int>(e.kind));
                    item["ball"] = json::Value(e.ballIndex);
                    item["time_seconds"] = number(e.absoluteWholeTickTimeSeconds);
                    item["generation"] = json::Value(std::to_string(e.trajectoryGeneration));
                    item["solver_event"] = json::Value(e.solverEventId);
                    item["boundary_feature"] = json::Value(e.boundaryFeatureId);
                    item["pocket_id"] = json::Value(e.pocketId);
                    item["identity"] = json::Value(e.stableIdentity);
                    departures.push_back(item);
                }
                telemetry["table_departures"] = json::Value(departures);
                row["telemetry"] = telemetry;
            }
            if (!runtime.appliedSemanticActions().empty()) {
                // Include the last semantic receipt in every snapshot so that
                // each retained segment remains attributable after rotation.
                const auto& a = runtime.appliedSemanticActions().back();
                auto action = json::Value::object();
                action["sequence"] = json::Value(std::to_string(a.sequence));
                action["tick"] = json::Value(std::to_string(a.tick));
                action["type"] = json::Value(static_cast<int>(a.type));
                action["first"] = number(a.first);
                action["second"] = number(a.second);
                row["latest_semantic_action"] = action;
            }
            const std::string line = json::stringify(row) + "\n";
            if (bytes_ >= limit_) { closeSegment(); if (failed_) return; ++segment_; openSegment(); }
            if (!good()) return;
            const auto written = std::fwrite(line.data(), 1, line.size(), file_);
            bytes_ += written;
            if (written != line.size() || std::fflush(file_) != 0) {
                fail("event_write_or_flush_failed"); return;
            }
            ++sequence_; ++segmentRecords_;
        } catch (...) { fail("event_serialization_or_rotation_failed"); }
    }
private:
    static json::Value number(double value) {
        if (std::isfinite(value)) return json::Value(value);
        return json::Value(std::isnan(value) ? "NaN" : value > 0 ? "+Infinity" : "-Infinity");
    }
    template<class P> static json::Value point(const P& p) {
        return json::Value(json::Value::Array{number(p.x), number(p.y), number(p.z)});
    }
    void fail(const std::string& reason) noexcept {
        if (failureDetail_.empty()) failureDetail_ = reason;
        failed_ = true;
        std::fprintf(stderr, "Playtest evidence recording FAILED: %s\n", reason.c_str());
    }
    static std::string readBytes(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("playtest_segment_hash_open_failed");
        std::string bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        if (input.bad()) throw std::runtime_error("playtest_segment_hash_read_failed");
        return bytes;
    }
    static void writeFresh(const std::filesystem::path& path, const std::string& bytes) {
        std::FILE* output = std::fopen(path.string().c_str(), "wbx");
        if (!output) throw std::runtime_error("playtest_fresh_output_required");
        const auto count = std::fwrite(bytes.data(), 1, bytes.size(), output);
        const int flushed = std::fflush(output);
        const int closed = std::fclose(output);
        if (count != bytes.size() || flushed != 0 || closed != 0)
            throw std::runtime_error("playtest_fresh_write_failed");
    }
    void closeSegment() {
        if (!file_) return;
        if (std::fflush(file_) != 0) fail("segment_flush_failed");
        if (std::fclose(file_) != 0) fail("segment_close_failed");
        file_ = nullptr;
        auto receipt = json::Value::object();
        receipt["schema"] = json::Value("billiardgl_playtest_segment_receipt_v2");
        receipt["segment"] = json::Value(std::to_string(segment_));
        receipt["data_file"] = json::Value(segmentPath_.filename().string());
        receipt["data_sha256"] = json::Value(sha256Hex(readBytes(segmentPath_)));
        receipt["first_event_sequence"] = json::Value(std::to_string(segmentFirst_));
        receipt["next_event_sequence"] = json::Value(std::to_string(sequence_));
        receipt["event_count"] = json::Value(std::to_string(segmentRecords_));
        receipt["status"] = json::Value(failed_ ? "FAILED_PARTIAL_SEGMENT" : "CLOSED");
        receipt["previous_segment_receipt_sha256"] = json::Value(previousReceiptHash_);
        receipt["retention_mode"] = json::Value(fullEvidence() ? "full_evidence" : "bounded_rolling_partial");
        const std::string bytes = json::stringify(receipt) + "\n";
        writeFresh(std::filesystem::path(directory_) / ("segment-" + std::to_string(segment_) + ".closed.json"), bytes);
        previousReceiptHash_ = sha256Hex(bytes);
    }
    void openSegment() {
        if (!fullEvidence() && segment_ >= 4) {
            std::filesystem::remove(std::filesystem::path(directory_) / ("events-" + std::to_string(segment_ % 4) + ".jsonl"));
            std::filesystem::remove(std::filesystem::path(directory_) / ("segment-" + std::to_string(segment_ - 4) + ".closed.json"));
        }
        segmentPath_ = std::filesystem::path(directory_) /
            ("events-" + std::to_string(fullEvidence() ? segment_ : segment_ % 4) + ".jsonl");
        file_ = std::fopen(segmentPath_.string().c_str(), "wbx");
        if (!file_) { fail("segment_fresh_open_failed"); return; }
        segmentFirst_ = sequence_; segmentRecords_ = 0;
        auto header = json::Value::object();
        header["schema"] = json::Value("billiardgl_playtest_segment_v2");
        header["kind"] = json::Value("segment_start");
        header["segment"] = json::Value(std::to_string(segment_));
        header["first_event_sequence"] = json::Value(std::to_string(sequence_));
        header["previous_segment_receipt_sha256"] = json::Value(previousReceiptHash_);
        header["retention_mode"] = json::Value(fullEvidence() ? "full_evidence" : "bounded_rolling_partial");
        header["retention_policy_preserves_all_records"] = json::Value(fullEvidence());
        header["executable_version"] = json::Value(version_);
        header["session_metadata"] = metadata_;
        const auto line = json::stringify(header) + "\n";
        bytes_ = std::fwrite(line.data(), 1, line.size(), file_);
        if (bytes_ != line.size() || std::fflush(file_) != 0) fail("segment_header_write_failed");
    }
    std::string directory_, version_, profileId_, profileHash_, profileCanonicalText_, profileReplayHash_;
    std::FILE* file_ = nullptr;
    std::filesystem::path segmentPath_;
    RetentionMode mode_;
    std::string previousReceiptHash_, failureDetail_;
    std::uint64_t segmentFirst_ = 0, segmentRecords_ = 0;
    bool closed_ = false;
    std::size_t bytes_ = 0, limit_, segment_ = 0;
    std::uint64_t sequence_ = 0;
    bool failed_ = false;
    double recordMilliseconds_ = 0.0;
    json::Value metadata_ = json::Value::object();
    std::chrono::steady_clock::time_point start_ = std::chrono::steady_clock::now();
};
} // namespace billiardgl::diagnostics
