import argparse
import copy
import hashlib
import json
from pathlib import Path

from .build_v3_profile import (
    _artifact,
    _canonical,
    _cpp_value,
    canonical_runtime_text,
    generated_include,
)


PROFILE_ID = "chinese_pool_full_game_v5"
FORMULA_VERSION = "phase3_integrated_v5_coupled_cue_contact_v1"
CANDIDATE_ID = "phase3_integrated_v5"
ORDINARY_SECTIONS = (
    "ball", "surface", "cue", "cushion", "table_boundary", "solver",
)
FROZEN_CPP_FIELDS = (
    ("enabled", "enabled"),
    ("normal_stiffness_n_per_m32", "normalStiffnessNPerM32"),
    ("normal_dissipation_s_per_m", "normalDissipationSPerM"),
    ("tangential_stiffness_n_per_m", "tangentialStiffnessNPerM"),
    ("tangential_damping_ns_per_m", "tangentialDampingNsPerM"),
    ("microstep_seconds", "microstepSeconds"),
    ("maximum_contact_seconds", "maximumContactSeconds"),
    ("release_compression_m", "releaseCompressionM"),
    ("maximum_compression_m", "maximumCompressionM"),
    ("maximum_normal_force_n", "maximumNormalForceN"),
)


def _sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def _document_sha256(document):
    encoded = json.dumps(
        document, ensure_ascii=False, sort_keys=True, separators=(",", ":"),
        allow_nan=False).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def canonical_v5_runtime_text(runtime):
    text = canonical_runtime_text(runtime)
    frozen = runtime["frozen_cue_contact"]
    lines = ["frozen_cue_contact.enabled=1"]
    for source_name, _ in FROZEN_CPP_FIELDS[1:]:
        lines.append(
            f"frozen_cue_contact.{source_name}="
            f"{format(frozen[source_name], '.17g')}")
    return text + "\n".join(lines) + "\n"


def generated_v5_include(runtime):
    base = generated_include(runtime).replace(
        "build_v3_profile", "build_v5_profile", 1)
    lines = [base.rstrip("\n")]
    frozen = runtime["frozen_cue_contact"]
    for source_name, cpp_name in FROZEN_CPP_FIELDS:
        value = frozen[source_name]
        rendered = "true" if value is True else _cpp_value(value, False)
        lines.append(f"profile.frozenCueContact.{cpp_name} = {rendered};")
    return "\n".join(lines) + "\n"


def build_v5_profile(v4_profile, fit_document):
    result = copy.deepcopy(v4_profile)
    result["schema_version"] = max(int(result.get("schema_version", 8)), 9)
    result["applicability"]["notes"] = (
        "Pre-freeze Phase 3 v5 engineering candidate. Ordinary v4 physics is "
        "byte-for-byte retained; only the calibrated coupled cue contact for "
        "initially frozen topologies is enabled. Cross 2016 and Han 2005 "
        "remain unopened confirmation packages."
    )
    result["limitations"] = [
        limitation for limitation in result["limitations"]
        if "Alciatore 2005 TP A.15 and Han 2005" not in limitation
    ] + [
        "Cross 2016 and Han 2005 confirmation partitions remain unopened.",
        "Cross 2016 absolute cue input is underspecified; only its normalized frozen-pair speed relation is confirmatory.",
    ]
    result["parameter_sources"]["frozen_cue_contact"] = {
        "fit_artifact": (
            "physics_models/calibration/frozen_cue_contact_v1_fit.json"),
        "regression_artifact": (
            "physics_models/calibration/"
            "alciatore_frozen_contact_v5_report.json"),
        "partitions": ["spent", "spent_regression"],
    }
    runtime = result["runtime_profile"]
    runtime["id"] = PROFILE_ID
    runtime["formula_version"] = FORMULA_VERSION
    winner = fit_document["winner"]
    fixed = fit_document["fixed"]
    runtime["frozen_cue_contact"] = {
        "enabled": True,
        "normal_stiffness_n_per_m32": winner["stiffness_n_per_m32"],
        "normal_dissipation_s_per_m": winner["dissipation_s_per_m"],
        "tangential_stiffness_n_per_m":
            fixed["tangential_stiffness_n_per_m"],
        "tangential_damping_ns_per_m":
            fixed["tangential_damping_n_s_per_m"],
        "microstep_seconds": 0.0000025,
        "maximum_contact_seconds": 0.006,
        "release_compression_m": 0.00000001,
        "maximum_compression_m": 0.004,
        "maximum_normal_force_n": 10000.0,
    }
    canonical = canonical_v5_runtime_text(runtime)
    result["runtime_query"] = {
        "canonical_text_sha256": hashlib.sha256(canonical.encode()).hexdigest(),
        "formula_version": FORMULA_VERSION,
        "id": PROFILE_ID,
    }
    return result


def _ordinary_equivalence(root, v4_path, v5_path, v4, v5):
    sections = {}
    for name in ORDINARY_SECTIONS:
        old_hash = _document_sha256(v4["runtime_profile"][name])
        new_hash = _document_sha256(v5["runtime_profile"][name])
        sections[name] = {
            "matches": old_hash == new_hash,
            "v4_sha256": old_hash,
            "v5_sha256": new_hash,
        }
    baseline_path = Path(
        "physics_models/regression/phase3_v4_ordinary_shot_baseline.json")
    return {
        "baseline": {
            "path": str(baseline_path),
            "sha256": _sha256(root / baseline_path),
        },
        "candidate_id": CANDIDATE_ID,
        "formula_version": FORMULA_VERSION,
        "ordinary_physics_identical": all(
            item["matches"] for item in sections.values()),
        "schema_version": 1,
        "sections": sections,
        "v4_profile": {
            "id": v4["runtime_profile"]["id"],
            "path": str(v4_path),
            "sha256": _sha256(root / v4_path),
        },
        "v5_profile": {
            "id": v5["runtime_profile"]["id"],
            "path": str(v5_path),
            "sha256": _sha256(root / v5_path),
        },
    }


def build_inventory(root):
    root = Path(root)
    calibration_paths = (
        ("surface_fit_inputs", "physics_models/calibration/surface_fit_v2_inputs.csv"),
        ("surface_fit_report", "physics_models/calibration/surface_fit_v2.json"),
        ("surface_fit_residuals", "physics_models/calibration/surface_fit_v2_residuals.csv"),
        ("ball_fit_inputs", "physics_models/calibration/ball_collision_fit_v3_inputs.csv"),
        ("ball_fit_report", "physics_models/calibration/ball_collision_fit_v3.json"),
        ("ball_fit_residuals", "physics_models/calibration/ball_collision_fit_v3_residuals.csv"),
        ("cushion_fit_inputs", "physics_models/calibration/cushion_fit_v3_inputs.csv"),
        ("cushion_fit_report", "physics_models/calibration/cushion_fit_v3.json"),
        ("cushion_fit_residuals", "physics_models/calibration/cushion_fit_v3_residuals.csv"),
        ("frozen_fit_inputs", "physics_models/calibration/frozen_cue_contact_v1_inputs.csv"),
        ("frozen_fit_report", "physics_models/calibration/frozen_cue_contact_v1_fit.json"),
        ("frozen_fit_residuals", "physics_models/calibration/frozen_cue_contact_v1_residuals.csv"),
        ("frozen_fit_sensitivity", "physics_models/calibration/frozen_cue_contact_v1_sensitivity.csv"),
        ("alciatore_regression_inputs", "physics_models/calibration/alciatore_frozen_contact_v5_inputs.csv"),
        ("alciatore_regression_report", "physics_models/calibration/alciatore_frozen_contact_v5_report.json"),
        ("alciatore_regression_residuals", "physics_models/calibration/alciatore_frozen_contact_v5_residuals.csv"),
        ("alciatore_regression_sensitivity", "physics_models/calibration/alciatore_frozen_contact_v5_sensitivity.csv"),
        ("runtime_profile_source", "src/Billiards/generated/phase3_v5_profile.inc"),
    )
    confirmation_packages = [
        _artifact(root, "confirmation_package_manifest",
                  f"tests/physics_validation/reference_data/{package}/manifest.json",
                  package_id=package, partition="confirmation")
        for package in ("cross_2016_newtons_cradle", "han_2005")
    ]
    contracts = []
    for package in ("cross_2016_newtons_cradle", "han_2005"):
        base = f"tests/physics_validation/reference_data/{package}"
        manifest = json.loads((root / base / "manifest.json").read_text())
        for item in manifest["files"]:
            contracts.append(_artifact(
                root, f"confirmation_{item['id']}",
                f"{base}/{item['path']}", package_id=package))
    contracts.extend((
        _artifact(root, "calibration_source_manifest",
                  "tests/physics_validation/reference_data/shimamura_2006_cue_contact/manifest.json",
                  package_id="shimamura_2006_cue_contact"),
        _artifact(root, "regression_source_manifest",
                  "tests/physics_validation/reference_data/alciatore_2005_tp_a15/manifest.json",
                  package_id="alciatore_2005_tp_a15"),
        _artifact(root, "ordinary_equivalence_baseline",
                  "physics_models/regression/phase3_v4_ordinary_shot_baseline.json"),
        _artifact(root, "ordinary_equivalence",
                  "physics_models/promotion/phase3_v5_ordinary_equivalence.json"),
        _artifact(root, "automation_protocol_schema",
                  "src/Billiards/automation_protocol.cpp"),
        _artifact(root, "scenario_parser_schema",
                  "src/Billiards/physics_scenario.cpp"),
        _artifact(root, "lifecycle_registry",
                  "tests/physics_validation/validation_data_status.json"),
    ))
    return {
        "calibration_reports": [
            _artifact(root, role, path) for role, path in calibration_paths],
        "candidate_id": CANDIDATE_ID,
        "confirmation_packages": confirmation_packages,
        "full_game_matrix": _artifact(
            root, "full_game_matrix",
            "physics_models/promotion/full_game_matrix_v5.json"),
        "metric_contracts": contracts,
        "performance_budget": _artifact(
            root, "performance_budget",
            "physics_models/promotion/full_game_performance_budget_v5.json"),
        "profile": _artifact(
            root, "profile",
            "physics_models/profiles/chinese_pool_full_game_v5.json"),
        "result_policy": (
            "No Cross 2016 or Han 2005 candidate prediction, residual, receipt, "
            "or confirmation result is admitted before the clean freeze."
        ),
        "schema_version": 5,
        "status": "pre_freeze",
    }


def write_v5_candidate(root):
    root = Path(root).resolve()
    v4_relative = Path("physics_models/profiles/chinese_pool_full_game_v4.json")
    v5_relative = Path("physics_models/profiles/chinese_pool_full_game_v5.json")
    fit_relative = Path("physics_models/calibration/frozen_cue_contact_v1_fit.json")
    v4 = json.loads((root / v4_relative).read_text(encoding="utf-8"))
    fit = json.loads((root / fit_relative).read_text(encoding="utf-8"))
    v5 = build_v5_profile(v4, fit)

    profile_path = root / v5_relative
    include_path = root / "src/Billiards/generated/phase3_v5_profile.inc"
    matrix_path = root / "physics_models/promotion/full_game_matrix_v5.json"
    budget_path = root / "physics_models/promotion/full_game_performance_budget_v5.json"
    equivalence_path = root / "physics_models/promotion/phase3_v5_ordinary_equivalence.json"
    inventory_path = root / "physics_models/promotion/phase3_candidates_v5.json"
    profile_path.write_text(_canonical(v5), encoding="utf-8")
    include_path.write_text(
        generated_v5_include(v5["runtime_profile"]), encoding="utf-8")

    matrix = json.loads((root /
        "physics_models/promotion/full_game_matrix_v4.json").read_text())
    matrix["artifact_root"] = (
        "physics_models/candidates/phase3_integrated_v5/full_game")
    matrix["physics_profile_id"] = PROFILE_ID
    matrix_path.write_text(_canonical(matrix), encoding="utf-8")
    budget_path.write_bytes((root /
        "physics_models/promotion/full_game_performance_budget_v4.json"
    ).read_bytes())

    equivalence = _ordinary_equivalence(
        root, v4_relative, v5_relative, v4, v5)
    if not equivalence["ordinary_physics_identical"]:
        raise ValueError("v5 candidate changes ordinary v4 physics values")
    equivalence_path.write_text(_canonical(equivalence), encoding="utf-8")
    inventory_path.write_text(_canonical(build_inventory(root)), encoding="utf-8")
    return v5


def main(argv=None):
    parser = argparse.ArgumentParser(description="Build Phase 3 v5 candidate")
    parser.add_argument("--root", type=Path, default=Path.cwd())
    arguments = parser.parse_args(argv)
    write_v5_candidate(arguments.root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
