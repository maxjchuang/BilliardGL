import argparse
import csv
import hashlib
import io
import json
import math
import tempfile
from pathlib import Path

from .extract_confirmation_scalars import (
    NORMALIZED_HEADER,
    RAW_HEADER,
    normalized_bytes,
)


DATASET_ID = "cross_2016_newtons_cradle"
DATASET_VERSION = "1.0.0"
DOI = "10.1088/0031-9120/51/6/065020"
AUTHOR_URL = "https://www.oxfordcroquet.com/tech/cross2/"
MEDIA_URL = "https://iopscience.iop.org/article/10.1088/0031-9120/51/6/065020/data"
MEDIA_SHA256 = "sha256:e6d00c57364dff30c4047ac3ca9443dfde740b4d6719e416b49bec78a2dc4226"
SOURCE_FPS = 600.0
PLAYBACK_FPS = 30.0
STABLE_FRAMES = tuple(range(80, 96))
WPA_TRANSFER_DIAMETER_CM = 5.715
APPARENT_DIAMETER_PX = 127.5
PIXELS_PER_CM = APPARENT_DIAMETER_PX / WPA_TRANSFER_DIAMETER_CM
COORDINATE_STANDARD_UNCERTAINTY_PX = 1.0
TIME_STANDARD_UNCERTAINTY_SECONDS = 0.5 / SOURCE_FPS
BACK_X_PX = (
    370.083415, 373.042023, 375.897632, 378.842082,
    381.603553, 384.488854, 387.442101, 390.423742,
    393.165720, 396.096117, 398.913323, 401.713233,
    404.620773, 407.703512, 410.417877, 413.463451,
)
BACK_Y_PX = (
    257.329577, 257.305556, 257.136639, 257.067162,
    256.778043, 256.846161, 256.981575, 256.931506,
    257.193932, 257.326478, 257.517856, 257.554806,
    257.554557, 257.386967, 257.541663, 257.401239,
)
FRONT_LEFT_EDGE_PX = (
    442.0, 445.0, 448.0, 452.0, 454.0, 458.0, 460.0, 462.0,
    466.0, 469.0, 472.0, 474.0, 477.0, 480.0, 484.0, 486.0,
)
FRONT_X_PX = tuple(value + APPARENT_DIAMETER_PX / 2.0
                   for value in FRONT_LEFT_EDGE_PX)
FRONT_Y_PX = (256.5,) * len(STABLE_FRAMES)
RAW_FRAME_HEADER = (
    "source_frame_number", "source_time_seconds", "playback_time_seconds",
    "ball_id", "center_x_px", "center_y_px", "pixels_per_cm",
    "position_x_cm", "position_y_cm", "velocity_x_cm_s",
    "velocity_standard_uncertainty_cm_s",
    "coordinate_standard_uncertainty_px",
    "time_standard_uncertainty_seconds", "stable_window", "review_status",
)
JSON_NAMES = (
    "split.json", "extraction.json", "scenario_template.json",
    "expected_model_mismatches.json", "expected_reference_limitations.json",
    "source_access_audit.json",
)


def _canonical_json(document):
    return (json.dumps(document, ensure_ascii=False, indent=2,
                       sort_keys=True, allow_nan=False) + "\n").encode("utf-8")


def _sha256(content):
    return "sha256:" + hashlib.sha256(content).hexdigest()


def _csv_bytes(header, rows):
    output = io.StringIO(newline="")
    writer = csv.DictWriter(output, fieldnames=header, lineterminator="\n")
    writer.writeheader()
    writer.writerows(rows)
    return output.getvalue().encode("utf-8")


def _ols(values):
    times = [frame / SOURCE_FPS for frame in STABLE_FRAMES]
    mean_time = sum(times) / len(times)
    mean_value = sum(values) / len(values)
    denominator = sum((value - mean_time) ** 2 for value in times)
    slope = sum((time - mean_time) * (value - mean_value)
                for time, value in zip(times, values)) / denominator
    intercept = mean_value - slope * mean_time
    residuals = tuple(value - (intercept + slope * time)
                      for time, value in zip(times, values))
    residual_rmse = math.sqrt(
        sum(value * value for value in residuals) / (len(values) - 2))
    coordinate_slope_uncertainty = (
        COORDINATE_STANDARD_UNCERTAINTY_PX / math.sqrt(denominator))
    return {
        "coordinate_slope_standard_uncertainty_px_s":
            coordinate_slope_uncertainty,
        "intercept_px": intercept,
        "residual_rmse_px": residual_rmse,
        "residuals_px": residuals,
        "slope_px_s": slope,
    }


def extracted_scalars():
    back = _ols(BACK_X_PX)
    front = _ols(FRONT_X_PX)
    back_speed = back["slope_px_s"] / PIXELS_PER_CM
    front_speed = front["slope_px_s"] / PIXELS_PER_CM
    back_uncertainty = (
        back["coordinate_slope_standard_uncertainty_px_s"] / PIXELS_PER_CM)
    front_uncertainty = (
        front["coordinate_slope_standard_uncertainty_px_s"] / PIXELS_PER_CM)
    ratio = back_speed / front_speed
    ratio_uncertainty = abs(ratio) * math.sqrt(
        (back_uncertainty / back_speed) ** 2
        + (front_uncertainty / front_speed) ** 2)
    return {
        "back": back,
        "back_speed_cm_s": back_speed,
        "back_speed_standard_uncertainty_cm_s": back_uncertainty,
        "front": front,
        "front_speed_cm_s": front_speed,
        "front_speed_standard_uncertainty_cm_s": front_uncertainty,
        "ratio": ratio,
        "ratio_standard_uncertainty": ratio_uncertainty,
    }


def _raw_frame_rows():
    values = extracted_scalars()
    rows = []
    for ball_id, xs, ys in (
            ("back", BACK_X_PX, BACK_Y_PX),
            ("front", FRONT_X_PX, FRONT_Y_PX)):
        speed = values[f"{ball_id}_speed_cm_s"]
        uncertainty = values[
            f"{ball_id}_speed_standard_uncertainty_cm_s"]
        origin = xs[0]
        y_origin = ys[0]
        for frame, x, y in zip(STABLE_FRAMES, xs, ys):
            rows.append({
                "source_frame_number": frame,
                "source_time_seconds": format(frame / SOURCE_FPS, ".17g"),
                "playback_time_seconds": format(frame / PLAYBACK_FPS, ".17g"),
                "ball_id": ball_id,
                "center_x_px": format(x, ".17g"),
                "center_y_px": format(y, ".17g"),
                "pixels_per_cm": format(PIXELS_PER_CM, ".17g"),
                "position_x_cm": format((x - origin) / PIXELS_PER_CM, ".17g"),
                "position_y_cm": format((y - y_origin) / PIXELS_PER_CM, ".17g"),
                "velocity_x_cm_s": format(speed, ".17g"),
                "velocity_standard_uncertainty_cm_s":
                    format(uncertainty, ".17g"),
                "coordinate_standard_uncertainty_px": "1",
                "time_standard_uncertainty_seconds":
                    format(TIME_STANDARD_UNCERTAINTY_SECONDS, ".17g"),
                "stable_window": "true",
                "review_status": "REVIEWED",
            })
    return rows


def _scalar_row(point_id, role, metric, source_value, source_unit,
                normalized_value, normalized_unit, *, uncertainty="0",
                tolerance="0", locator="Supplementary video 4582, frames 80-95",
                applicability="DIRECT"):
    return {
        "point_id": point_id,
        "role": role,
        "series_id": "cross_frozen_cue_pair",
        "group_id": "equal_speed_release",
        "case_id": "cross_cue_frozen_pair",
        "metric": metric,
        "source_value": str(source_value),
        "source_unit": source_unit,
        "normalized_value": str(normalized_value),
        "normalized_unit": normalized_unit,
        "measurement_uncertainty": str(uncertainty),
        "digitization_uncertainty": "0",
        "conversion_uncertainty": "0",
        "coverage_factor": "2",
        "engineering_absolute_tolerance": str(tolerance),
        "engineering_relative_tolerance": "0",
        "source_locator": locator,
        "pool_applicability": applicability,
    }


def _scalar_rows():
    values = extracted_scalars()
    rows = [
        _scalar_row(
            "cross_equal_speed_ratio", "confirmation_target",
            "back_to_front_speed_ratio", values["ratio"], "dimensionless",
            1.0, "dimensionless",
            uncertainty=values["ratio_standard_uncertainty"], tolerance=0.05),
        _scalar_row(
            "cross_observed_back_speed", "source_observation",
            "back_ball_speed", values["back_speed_cm_s"], "cm/s",
            values["back_speed_cm_s"], "cm/s",
            uncertainty=values["back_speed_standard_uncertainty_cm_s"]),
        _scalar_row(
            "cross_observed_front_speed", "source_observation",
            "front_ball_speed", values["front_speed_cm_s"], "cm/s",
            values["front_speed_cm_s"], "cm/s",
            uncertainty=values["front_speed_standard_uncertainty_cm_s"]),
        _scalar_row(
            "cross_source_frozen_contact", "source_apparatus_fact",
            "balls_initially_in_contact", 1, "dimensionless", 1,
            "dimensionless", locator="Author manuscript section 1 and figure 1(b)",
            applicability="NOT_APPLICABLE"),
        _scalar_row(
            "cross_source_camera_rate", "source_apparatus_fact",
            "source_frame_rate", 600, "frame/s", 600, "frame/s",
            locator="Author manuscript section 2",
            applicability="NOT_APPLICABLE"),
    ]
    for ball_id in ("back", "front"):
        fit = values[ball_id]
        rows.extend((
            _scalar_row(
                f"cross_{ball_id}_ols_slope", "extraction_intermediate",
                "ols_slope", fit["slope_px_s"], "px/s",
                fit["slope_px_s"], "px/s",
                locator=f"OLS of {ball_id} ball frames 80-95",
                applicability="NOT_APPLICABLE"),
            _scalar_row(
                f"cross_{ball_id}_ols_intercept", "extraction_intermediate",
                "ols_intercept", fit["intercept_px"], "px",
                fit["intercept_px"], "px",
                locator=f"OLS of {ball_id} ball frames 80-95",
                applicability="NOT_APPLICABLE"),
            _scalar_row(
                f"cross_{ball_id}_ols_residual_rmse", "extraction_intermediate",
                "ols_residual_rmse", fit["residual_rmse_px"], "px",
                fit["residual_rmse_px"], "px",
                locator=f"OLS of {ball_id} ball frames 80-95",
                applicability="NOT_APPLICABLE"),
            _scalar_row(
                f"cross_{ball_id}_coordinate_slope_uncertainty",
                "extraction_intermediate", "coordinate_slope_uncertainty",
                fit["coordinate_slope_standard_uncertainty_px_s"], "px/s",
                fit["coordinate_slope_standard_uncertainty_px_s"], "px/s",
                locator="one-pixel standard uncertainty propagated through OLS",
                applicability="NOT_APPLICABLE"),
        ))
        for frame, residual in zip(STABLE_FRAMES, fit["residuals_px"]):
            rows.append(_scalar_row(
                f"cross_{ball_id}_residual_frame_{frame:03d}",
                "extraction_intermediate", "ols_residual", residual, "px",
                residual, "px", locator=(
                    f"Supplementary video 4582, {ball_id} ball source frame "
                    f"{frame}, OLS residual"),
                applicability="NOT_APPLICABLE"))
    rows.extend((
        _scalar_row(
            "cross_ratio_coordinate_uncertainty", "extraction_intermediate",
            "ratio_coordinate_uncertainty",
            values["ratio_standard_uncertainty"], "dimensionless",
            values["ratio_standard_uncertainty"], "dimensionless",
            locator="one-pixel coordinate uncertainty propagated through both OLS slopes",
            applicability="NOT_APPLICABLE"),
        _scalar_row(
            "cross_ratio_common_time_offset_uncertainty",
            "extraction_intermediate", "ratio_common_time_offset_uncertainty",
            0, "dimensionless", 0, "dimensionless", locator=(
                "shared half-frame absolute time offset cancels algebraically "
                "from the back/front slope ratio"),
            applicability="NOT_APPLICABLE"),
    ))
    return rows


def _documents(raw, normalized):
    values = extracted_scalars()
    raw_hash = _sha256(raw)
    normalized_hash = _sha256(normalized)
    return {
        "split.json": {
            "calibration_groups": [],
            "dataset_id": DATASET_ID,
            "dataset_version": DATASET_VERSION,
            "holdout_groups": ["equal_speed_release"],
            "schema_version": 1,
        },
        "extraction.json": {
            "date": "2026-07-16",
            "inputs": [{"file_id": "raw_extracted", "sha256": raw_hash}],
            "method": (
                "Frame-by-frame center digitization of the complete registered "
                "stable release window in supplementary video 4582; ordinary "
                "least squares independently fits each ball position versus "
                "the published 600 frame/s source time base"
            ),
            "operator": "Codex primary-source video extraction",
            "output_sha256": normalized_hash,
            "review": {
                "date": "2026-07-16",
                "method": (
                    "Visual contact-sheet review at frames 80, 85, 90, and 95 "
                    "plus residual review of every committed coordinate"
                ),
                "reviewed_by": "Codex frame-track visual cross-check",
            },
            "rounding_policy": (
                "Preserve digitized centers to six decimal places and all "
                "derived IEEE-754 values with 17 significant digits"
            ),
            "schema_version": 2,
            "script": {
                "module": (
                    "tools.physics_validation."
                    "extract_cross_2016_newtons_cradle"
                ),
                "version": DATASET_VERSION,
            },
            "source_sha256": MEDIA_SHA256,
            "tool": {
                "name": "deterministic reviewed center-track normalizer",
                "version": DATASET_VERSION,
            },
            "transformations": [{
                "formula": "t = source_frame / 600; x_cm = x_px / pixels_per_cm; velocity = OLS(x_cm, t)",
                "id": "stable_window_velocity",
                "input_unit": "source frame, pixel",
                "output_unit": "cm/s",
            }, {
                "formula": "ratio = OLS(back_x,t) / OLS(front_x,t)",
                "id": "equal_speed_ratio",
                "input_unit": "pixel, second",
                "output_unit": "dimensionless",
            }],
            "uncertainty_interpretation": (
                "One-pixel standard coordinate uncertainty is propagated "
                "through both OLS slopes. The half-source-frame absolute time "
                "offset is retained on every row and cancels exactly in the "
                "speed ratio because both balls share the same frames and clock."
            ),
        },
        "scenario_template.json": {
            "apparatus_transfer": {
                "ball_diameter_cm": WPA_TRANSFER_DIAMETER_CM,
                "ball_dimension_source": "WPA pool transfer value; Cross does not publish diameter",
                "ball_mass_kg": 0.115,
                "ball_mass_source": "Cross 2016 section 2",
                "cloth": "felt stretched over a small table",
            },
            "case_id": "cross_cue_frozen_pair",
            "cue_direction": [1, 0, 0],
            "cue_elevation_degrees": 0,
            "cue_tip_offset_radius": [0, 0],
            "initial_ball_gap_cm": 0,
            "ratio_absolute_floor": 0.05,
            "ratio_uncertainty_multiplier": 2,
            "source_observed_ratio": values["ratio"],
            "source_ratio_standard_uncertainty":
                values["ratio_standard_uncertainty"],
            "stable_source_frame_window": [STABLE_FRAMES[0], STABLE_FRAMES[-1]],
            "schema_version": 1,
        },
        "expected_model_mismatches.json": {
            "failures": [],
            "schema_version": 1,
        },
        "expected_reference_limitations.json": {
            "failures": [{
                "code": "SOURCE_CUE_INPUT_UNDERSPECIFIED",
                "dataset_id": DATASET_ID,
                "rationale": (
                    "Video 4582 and the manuscript do not publish cue speed, "
                    "cue effective mass, tip stiffness, or tip friction; the "
                    "confirmation therefore tests the normalized two-ball "
                    "release-speed relation rather than absolute launch speed."
                ),
                "scope": "absolute_cue_contact_parameters",
            }, {
                "code": "POOL_DIMENSION_TRANSFER",
                "dataset_id": DATASET_ID,
                "rationale": (
                    "Cross publishes 115 g ball mass but not diameter; WPA pool "
                    "diameter is used only to report absolute extracted speeds "
                    "and cancels from the primary speed ratio."
                ),
                "scope": "absolute_video_scale",
            }],
            "schema_version": 1,
        },
        "source_access_audit.json": {
            "apparatus_statement": (
                "experiment using two billiard balls initially in contact, "
                "resting on felt cloth stretched over a small table"
            ),
            "audited_on": "2026-07-16",
            "author": "Rod Cross",
            "author_manuscript_url": AUTHOR_URL,
            "copyright_policy": (
                "Commit complete derived coordinates and numerical results; "
                "do not redistribute the MOV without an explicit grant."
            ),
            "media_filename": "Cue-Separated-4582.mov",
            "media_sha256": MEDIA_SHA256,
            "media_url": MEDIA_URL,
            "published_observation": (
                "the two balls take off with the same speed and both head off "
                "at essentially the same speed"
            ),
            "source_media_committed": False,
            "supplementary_video_identifier": "4582",
            "video_container": "QuickTime MOV / H.264, 640x480, 105 frames",
            "video_playback_frame_rate_fps": PLAYBACK_FPS,
            "video_source_frame_rate_fps": SOURCE_FPS,
        },
    }


def generated_files():
    raw = _csv_bytes(RAW_FRAME_HEADER, _raw_frame_rows())
    scalars = _csv_bytes(RAW_HEADER, _scalar_rows())
    with tempfile.TemporaryDirectory() as directory:
        scalars_path = Path(directory) / "scalars.csv"
        scalars_path.write_bytes(scalars)
        normalized = normalized_bytes(scalars_path, DATASET_ID)
    files = {
        "raw_frame_tracks.csv": raw,
        "normalized.csv": normalized,
        "scalars.csv": scalars,
    }
    files.update({name: _canonical_json(document)
                  for name, document in _documents(raw, normalized).items()})
    logical_ids = {
        "raw_frame_tracks.csv": "raw_extracted",
        "normalized.csv": "normalized",
        "scalars.csv": "scalars",
        "split.json": "split",
        "extraction.json": "extraction",
        "scenario_template.json": "scenario_template",
        "expected_model_mismatches.json": "expected_model_mismatches",
        "expected_reference_limitations.json": "expected_reference_limitations",
        "source_access_audit.json": "source_access_audit",
    }
    order = (
        "raw_frame_tracks.csv", "normalized.csv", "scalars.csv",
        "split.json", "extraction.json", "scenario_template.json",
        "expected_model_mismatches.json", "expected_reference_limitations.json",
        "source_access_audit.json",
    )
    values = extracted_scalars()
    manifest = {
        "acquisition": {
            "license_status": "publisher-supplement-no-redistribution-grant-recorded",
            "retrieved_on": "2026-07-16",
            "source_media_committed": False,
            "source_sha256": MEDIA_SHA256,
            "source_url": MEDIA_URL,
        },
        "adapter_id": "cross_2016_newtons_cradle_confirmation_v1",
        "apparatus": {
            "ball_mass_g": 115,
            "balls_initially_in_contact": True,
            "camera": "Casio EX-F1 at 600 frames/s",
            "cloth": "felt stretched over a small table",
            "cue_contact": "centered horizontal repository transfer contract",
        },
        "dataset_id": DATASET_ID,
        "dataset_version": DATASET_VERSION,
        "evidence": {
            "candidate_selection_input": False,
            "confirmation_only": True,
            "grade": "B",
            "hard_contract": "uncertainty-aware equal release speed plus frozen-contact invariants",
            "method": "complete OLS track of supplementary video 4582 stable window",
            "observed_speed_ratio": values["ratio"],
            "target_count": 1,
        },
        "extraction_review": {
            "date": "2026-07-16",
            "method": "visual frame-window and OLS residual cross-check",
            "reviewed_by": "Codex primary-source video review",
        },
        "files": [{
            "id": logical_ids[name], "path": name,
            "sha256": _sha256(files[name]),
        } for name in order],
        "schema_version": 1,
        "source": {
            "authors": ["Rod Cross"],
            "doi": DOI,
            "journal": "Physics Education",
            "pages": "51(6):065020",
            "title": "Newton's cradle in billiards and croquet",
            "year": 2016,
        },
    }
    files["manifest.json"] = _canonical_json(manifest)
    return files


def write_package(path):
    package = Path(path)
    package.mkdir(parents=True, exist_ok=True)
    files = generated_files()
    for name, content in files.items():
        (package / name).write_bytes(content)
    return files


def verify_package(path):
    package = Path(path)
    expected = generated_files()
    return [name for name, content in expected.items()
            if not (package / name).is_file()
            or (package / name).read_bytes() != content]


def verify_media(path):
    digest = "sha256:" + hashlib.sha256(Path(path).read_bytes()).hexdigest()
    if digest != MEDIA_SHA256:
        raise ValueError(
            f"Cross video hash mismatch: expected {MEDIA_SHA256}, received {digest}")
    return digest


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Generate or verify the Cross 2016 frozen-pair package")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--output", type=Path)
    mode.add_argument("--verify", type=Path)
    parser.add_argument("--source-video", type=Path)
    arguments = parser.parse_args(argv)
    if arguments.source_video is not None:
        verify_media(arguments.source_video)
    if arguments.output is not None:
        write_package(arguments.output)
        print(f"{DATASET_ID} {DATASET_VERSION} written")
        return 0
    differences = verify_package(arguments.verify)
    if differences:
        raise SystemExit("Cross package differs: " + ", ".join(differences))
    print(f"{DATASET_ID} {DATASET_VERSION} verified")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
