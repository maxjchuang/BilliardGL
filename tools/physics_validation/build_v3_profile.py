import argparse
import copy
import hashlib
import json
import struct
from pathlib import Path


def _canonical(document):
    return json.dumps(
        document, ensure_ascii=False, indent=2, sort_keys=True,
        allow_nan=False) + "\n"


def _sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def build_v3_profile(v2_profile, ball_fit, cushion_fit):
    result = copy.deepcopy(v2_profile)
    result["schema_version"] = 8
    result["applicability"]["notes"] = (
        "Pre-freeze integrated Phase 3 v3 successor selected from spent "
        "surface, ball, cushion, cue, pocket, and solver evidence. Derby and "
        "Han remain unopened confirmation packages."
    )
    result["limitations"] = [
        limitation for limitation in result["limitations"]
        if "Sudo 2002 and Derby/Fuller" not in limitation
    ] + [
        "Sudo 2002 finite cushion contact duration remains a visible structural gap.",
        "Derby/Fuller 1999 and Han 2005 confirmation partitions remain unopened.",
    ]
    result["parameter_sources"]["ball"] = {
        "fit_artifact": "physics_models/calibration/ball_collision_fit_v3.json",
        "partition": "spent",
    }
    result["parameter_sources"]["cushion"] = {
        "fit_artifact": "physics_models/calibration/cushion_fit_v3.json",
        "partition": "spent",
    }
    runtime = result["runtime_profile"]
    runtime["id"] = "chinese_pool_full_game_v3"
    runtime["formula_version"] = "full_game_integration_v3"
    ball = ball_fit["fit"]
    runtime["ball"]["normal_restitution"] = ball["normal_restitution"]
    runtime["ball"]["friction_coefficient"] = ball["friction_coefficient"]
    cushion = cushion_fit["fit"]
    runtime["cushion"]["normal_restitution"] = cushion["e_max"]
    runtime["cushion"]["restitution_intercept"] = cushion["e_intercept"]
    runtime["cushion"]["restitution_slope_per_mps"] = \
        cushion["e_slope_per_mps"]
    runtime["cushion"]["minimum_restitution"] = cushion["e_min"]
    runtime["cushion"]["maximum_restitution"] = cushion["e_max"]
    canonical_text = canonical_runtime_text(runtime)
    result["runtime_query"] = {
        "canonical_text_sha256": hashlib.sha256(
            canonical_text.encode("utf-8")).hexdigest(),
        "formula_version": runtime["formula_version"],
        "id": runtime["id"],
    }
    return result


_FIELD_MAP = {
    "ball": {
        "radius_cm": "radiusCm", "mass_kg": "massKg",
        "inertia_factor": "inertiaFactor",
        "normal_restitution": "normalRestitution",
        "friction_coefficient": "frictionCoefficient", "material": "material",
    },
    "surface": {
        "legacy_friction_acceleration_cm_s2": "legacyFrictionAccelerationCmS2",
        "sliding_friction_coefficient": "slidingFrictionCoefficient",
        "rolling_resistance_acceleration_cm_s2": "rollingResistanceAccelerationCmS2",
        "torsional_spin_deceleration_rad_s2": "torsionalSpinDecelerationRadS2",
        "slip_speed_epsilon_cm_s": "slipSpeedEpsilonCmS",
        "stop_energy_threshold_j": "stopEnergyThresholdJ", "material": "material",
    },
    "cue": {
        "effective_mass_kg": "effectiveMassKg",
        "normal_restitution": "normalRestitution",
        "chalked_friction_coefficient": "chalkedFrictionCoefficient",
        "unchalked_friction_coefficient": "unchalkedFrictionCoefficient",
        "maximum_reliable_offset_radius": "maximumReliableOffsetRadius",
        "cue_speed_per_power_unit_cm_s": "cueSpeedPerPowerUnitCmS",
    },
    "cushion": {
        "normal_restitution": "normalRestitution",
        "restitution_intercept": "restitutionIntercept",
        "restitution_slope_per_mps": "restitutionSlopePerMps",
        "minimum_restitution": "minimumRestitution",
        "maximum_restitution": "maximumRestitution",
        "friction_coefficient": "frictionCoefficient",
        "nose_height_ratio": "noseHeightRatio",
        "maximum_rigid_incident_speed_cm_s": "maximumRigidIncidentSpeedCmS",
        "material": "material",
    },
    "table_boundary": {
        "playfield_width_cm": "playfieldWidthCm",
        "playfield_length_cm": "playfieldLengthCm",
        "corner_mouth_width_cm": "cornerMouthWidthCm",
        "side_mouth_width_cm": "sideMouthWidthCm",
        "corner_throat_width_cm": "cornerThroatWidthCm",
        "side_throat_width_cm": "sideThroatWidthCm",
        "jaw_radius_cm": "jawRadiusCm", "throat_depth_cm": "throatDepthCm",
        "capture_depth_cm": "captureDepthCm", "geometry_id": "geometryId",
        "material": "material",
    },
    "solver": {
        "time_step_seconds": "timeStepSeconds",
        "maximum_events_per_tick": "maximumEventsPerTick",
        "toi_tolerance_seconds": "toiToleranceSeconds",
        "maximum_island_size": "maximumIslandSize",
        "velocity_iterations": "velocityIterations",
        "position_iterations": "positionIterations",
        "penetration_slop_cm": "penetrationSlopCm",
        "maximum_penetration_cm": "maximumPenetrationCm",
        "residual_tolerance_cm_s": "residualToleranceCmS",
        "passive_energy_tolerance_j": "passiveEnergyToleranceJ",
    },
}


def _cpp_value(value, float_suffix=True):
    if isinstance(value, str):
        return json.dumps(value)
    if isinstance(value, int):
        return str(value)
    rendered = format(value, ".17g")
    if "." not in rendered and "e" not in rendered:
        rendered += ".0"
    return rendered + ("f" if float_suffix else "")


def _runtime_text_value(section, source_name, value):
    if isinstance(value, (str, int)):
        return str(value)
    if not (section == "solver" and
            source_name == "passive_energy_tolerance_j"):
        value = struct.unpack("f", struct.pack("f", value))[0]
    return format(value, ".9g")


def canonical_runtime_text(runtime):
    lines = [
        f"id={runtime['id']}",
        f"formula_version={runtime['formula_version']}",
    ]
    for section, fields in _FIELD_MAP.items():
        for source_name in fields:
            value = _runtime_text_value(
                section, source_name, runtime[section][source_name])
            lines.append(f"{section}.{source_name}={value}")
    return "\n".join(lines) + "\n"


def generated_include(runtime):
    lines = [
        "// Generated by tools.physics_validation.build_v3_profile; do not edit.",
        f"profile.id = {_cpp_value(runtime['id'])};",
        f"profile.formulaVersion = {_cpp_value(runtime['formula_version'])};",
    ]
    cpp_sections = {"table_boundary": "tableBoundary", **{
        key: key for key in ("ball", "surface", "cue", "cushion", "solver")}}
    for section, fields in _FIELD_MAP.items():
        for source_name, cpp_name in fields.items():
            lines.append(
                f"profile.{cpp_sections[section]}.{cpp_name} = "
                f"{_cpp_value(runtime[section][source_name], not (
                    section == 'solver' and
                    source_name == 'passive_energy_tolerance_j'))};")
    return "\n".join(lines) + "\n"


def _artifact(root, role, path, **extra):
    return {"role": role, "path": path, "sha256": _sha256(root / path), **extra}


def build_inventory(root):
    root = Path(root)
    calibration = [
        ("surface_fit_inputs", "physics_models/calibration/surface_fit_v2_inputs.csv"),
        ("surface_fit_report", "physics_models/calibration/surface_fit_v2.json"),
        ("surface_fit_residuals", "physics_models/calibration/surface_fit_v2_residuals.csv"),
        ("ball_fit_inputs", "physics_models/calibration/ball_collision_fit_v3_inputs.csv"),
        ("ball_fit_report", "physics_models/calibration/ball_collision_fit_v3.json"),
        ("ball_fit_residuals", "physics_models/calibration/ball_collision_fit_v3_residuals.csv"),
        ("cushion_fit_inputs", "physics_models/calibration/cushion_fit_v3_inputs.csv"),
        ("cushion_fit_report", "physics_models/calibration/cushion_fit_v3.json"),
        ("cushion_fit_residuals", "physics_models/calibration/cushion_fit_v3_residuals.csv"),
        ("structural_residuals", "physics_models/calibration/sudo_2002_structural_residuals.csv"),
        ("runtime_profile_source", "src/Billiards/generated/phase3_v3_profile.inc"),
    ]
    packages = []
    contracts = []
    contract_files = (
        ("acceptance_metrics", "normalized.csv"),
        ("confirmation_split", "split.json"),
        ("supplemental_scalars", "scalars.csv"),
        ("scenario_template", "scenario_template.json"),
        ("expected_model_mismatches", "expected_model_mismatches.json"),
        ("expected_reference_limitations", "expected_reference_limitations.json"),
        ("source_access_audit", "source_access_audit.json"),
    )
    for package_id in ("derby_fuller_1999", "han_2005"):
        base = f"tests/physics_validation/reference_data/{package_id}"
        packages.append(_artifact(
            root, "confirmation_package_manifest", f"{base}/manifest.json",
            package_id=package_id, partition="confirmation"))
        for role, name in contract_files:
            contracts.append(_artifact(
                root, role, f"{base}/{name}", package_id=package_id))
    return {
        "calibration_reports": [
            _artifact(root, role, path) for role, path in calibration],
        "candidate_id": "phase3_integrated_v3",
        "confirmation_packages": packages,
        "full_game_matrix": _artifact(
            root, "full_game_matrix",
            "physics_models/promotion/full_game_matrix_v3.json"),
        "metric_contracts": contracts,
        "performance_budget": _artifact(
            root, "performance_budget",
            "physics_models/promotion/full_game_performance_budget_v3.json"),
        "profile": _artifact(
            root, "profile",
            "physics_models/profiles/chinese_pool_full_game_v3.json"),
        "result_policy": (
            "No confirmation or full-game result artifact is admitted before "
            "the source revision is frozen."
        ),
        "schema_version": 3,
        "status": "pre_freeze",
    }


def write_v3_candidate(root):
    root = Path(root).resolve()
    v2_profile = json.loads((root / "physics_models/profiles/"
                             "chinese_pool_full_game_v2.json").read_text())
    ball_fit = json.loads((root / "physics_models/calibration/"
                           "ball_collision_fit_v3.json").read_text())
    cushion_fit = json.loads((root / "physics_models/calibration/"
                              "cushion_fit_v3.json").read_text())
    profile = build_v3_profile(v2_profile, ball_fit, cushion_fit)
    profile_path = root / "physics_models/profiles/chinese_pool_full_game_v3.json"
    include_path = root / "src/Billiards/generated/phase3_v3_profile.inc"
    matrix_path = root / "physics_models/promotion/full_game_matrix_v3.json"
    budget_path = root / "physics_models/promotion/full_game_performance_budget_v3.json"
    profile_path.parent.mkdir(parents=True, exist_ok=True)
    include_path.parent.mkdir(parents=True, exist_ok=True)
    profile_path.write_text(_canonical(profile), encoding="utf-8")
    include_path.write_text(
        generated_include(profile["runtime_profile"]), encoding="utf-8")
    matrix = json.loads((root / "physics_models/promotion/"
                         "full_game_matrix_v2.json").read_text())
    matrix["artifact_root"] = "physics_models/candidates/phase3_integrated_v3/full_game"
    matrix["physics_profile_id"] = "chinese_pool_full_game_v3"
    matrix_path.write_text(_canonical(matrix), encoding="utf-8")
    budget = json.loads((root / "physics_models/promotion/"
                         "full_game_performance_budget_v2.json").read_text())
    budget_path.write_text(_canonical(budget), encoding="utf-8")
    inventory = build_inventory(root)
    inventory_path = root / "physics_models/promotion/phase3_candidates_v3.json"
    inventory_path.write_text(_canonical(inventory), encoding="utf-8")
    return profile


def main(argv=None):
    parser = argparse.ArgumentParser(description="Build Phase 3 v3 candidate")
    parser.add_argument("--root", type=Path, default=Path.cwd())
    arguments = parser.parse_args(argv)
    write_v3_candidate(arguments.root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
