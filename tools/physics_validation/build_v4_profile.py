import argparse
import copy
import hashlib
import json
from pathlib import Path

from .build_v3_profile import (
    _artifact,
    _canonical,
    canonical_runtime_text,
    generated_include,
)


PHYSICS_SECTIONS = (
    "ball", "surface", "cue", "cushion", "table_boundary", "solver",
)


def _document_sha256(document):
    encoded = json.dumps(
        document, ensure_ascii=False, sort_keys=True, separators=(",", ":"),
        allow_nan=False,
    ).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def physics_values(profile):
    runtime = profile.get("runtime_profile", profile)
    return {section: copy.deepcopy(runtime[section])
            for section in PHYSICS_SECTIONS}


def build_v4_profile(v3_profile):
    result = copy.deepcopy(v3_profile)
    result["applicability"]["notes"] = (
        "Pre-freeze Phase 3 v4 confirmation candidate. Its physics is exactly "
        "the selected v3 runtime; Derby/Fuller has been spent, while Alciatore "
        "TP A.15 and Han remain unopened confirmation packages."
    )
    result["limitations"] = [
        limitation for limitation in result["limitations"]
        if "Derby/Fuller 1999 and Han 2005" not in limitation
    ] + [
        "Alciatore 2005 TP A.15 and Han 2005 confirmation partitions remain unopened."
    ]
    runtime = result["runtime_profile"]
    runtime["id"] = "chinese_pool_full_game_v4"
    canonical_text = canonical_runtime_text(runtime)
    result["runtime_query"] = {
        "canonical_text_sha256": hashlib.sha256(
            canonical_text.encode("utf-8")).hexdigest(),
        "formula_version": runtime["formula_version"],
        "id": runtime["id"],
    }
    return result


def _equivalence_document(v3_path, v4_path, v3, v4):
    sections = {}
    for name in PHYSICS_SECTIONS:
        v3_hash = _document_sha256(v3["runtime_profile"][name])
        v4_hash = _document_sha256(v4["runtime_profile"][name])
        sections[name] = {
            "matches": v3_hash == v4_hash,
            "v3_sha256": v3_hash,
            "v4_sha256": v4_hash,
        }
    return {
        "candidate_id": "phase3_integrated_v4",
        "formula_version": v4["runtime_profile"]["formula_version"],
        "physics_identical": (
            physics_values(v3) == physics_values(v4)
            and all(section["matches"] for section in sections.values())
        ),
        "schema_version": 1,
        "sections": sections,
        "v3_profile": {
            "id": v3["runtime_profile"]["id"],
            "path": str(v3_path),
            "sha256": hashlib.sha256(_canonical(v3).encode("utf-8")).hexdigest(),
        },
        "v4_profile": {
            "id": v4["runtime_profile"]["id"],
            "path": str(v4_path),
            "sha256": hashlib.sha256(_canonical(v4).encode("utf-8")).hexdigest(),
        },
    }


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
        ("runtime_profile_source", "src/Billiards/generated/phase3_v4_profile.inc"),
    ]
    packages = []
    contracts = []
    contract_files = (
        ("raw_extracted_data", "raw_extracted.csv"),
        ("acceptance_metrics", "normalized.csv"),
        ("supplemental_scalars", "scalars.csv"),
        ("extraction_record", "extraction.json"),
        ("confirmation_split", "split.json"),
        ("scenario_template", "scenario_template.json"),
        ("expected_model_mismatches", "expected_model_mismatches.json"),
        ("expected_reference_limitations", "expected_reference_limitations.json"),
        ("source_access_audit", "source_access_audit.json"),
    )
    for package_id in ("alciatore_2005_tp_a15", "han_2005"):
        base = f"tests/physics_validation/reference_data/{package_id}"
        packages.append(_artifact(
            root, "confirmation_package_manifest", f"{base}/manifest.json",
            package_id=package_id, partition="confirmation"))
        for role, name in contract_files:
            contracts.append(_artifact(
                root, role, f"{base}/{name}", package_id=package_id))
    contracts.append(_artifact(
        root, "physics_equivalence",
        "physics_models/promotion/phase3_v4_physics_equivalence.json"))
    return {
        "calibration_reports": [
            _artifact(root, role, path) for role, path in calibration],
        "candidate_id": "phase3_integrated_v4",
        "confirmation_packages": packages,
        "full_game_matrix": _artifact(
            root, "full_game_matrix",
            "physics_models/promotion/full_game_matrix_v4.json"),
        "metric_contracts": contracts,
        "performance_budget": _artifact(
            root, "performance_budget",
            "physics_models/promotion/full_game_performance_budget_v4.json"),
        "profile": _artifact(
            root, "profile",
            "physics_models/profiles/chinese_pool_full_game_v4.json"),
        "result_policy": (
            "No confirmation or full-game result artifact is admitted before "
            "the source revision is frozen."
        ),
        "schema_version": 4,
        "status": "pre_freeze",
    }


def write_v4_candidate(root):
    root = Path(root).resolve()
    v3_relative = Path("physics_models/profiles/chinese_pool_full_game_v3.json")
    v4_relative = Path("physics_models/profiles/chinese_pool_full_game_v4.json")
    v3 = json.loads((root / v3_relative).read_text(encoding="utf-8"))
    v4 = build_v4_profile(v3)

    profile_path = root / v4_relative
    include_path = root / "src/Billiards/generated/phase3_v4_profile.inc"
    matrix_path = root / "physics_models/promotion/full_game_matrix_v4.json"
    budget_path = root / "physics_models/promotion/full_game_performance_budget_v4.json"
    equivalence_path = root / "physics_models/promotion/phase3_v4_physics_equivalence.json"
    inventory_path = root / "physics_models/promotion/phase3_candidates_v4.json"
    profile_path.write_text(_canonical(v4), encoding="utf-8")
    include = generated_include(v4["runtime_profile"]).replace(
        "build_v3_profile", "build_v4_profile", 1)
    include_path.write_text(include, encoding="utf-8")

    matrix = json.loads((root / "physics_models/promotion/full_game_matrix_v3.json")
                        .read_text(encoding="utf-8"))
    matrix["artifact_root"] = "physics_models/candidates/phase3_integrated_v4/full_game"
    matrix["physics_profile_id"] = "chinese_pool_full_game_v4"
    matrix_path.write_text(_canonical(matrix), encoding="utf-8")
    budget = json.loads((root / "physics_models/promotion/"
                         "full_game_performance_budget_v3.json")
                        .read_text(encoding="utf-8"))
    budget_path.write_text(_canonical(budget), encoding="utf-8")

    equivalence = _equivalence_document(v3_relative, v4_relative, v3, v4)
    if not equivalence["physics_identical"]:
        raise ValueError("v4 candidate changes v3 physics values")
    equivalence_path.write_text(_canonical(equivalence), encoding="utf-8")
    inventory_path.write_text(_canonical(build_inventory(root)), encoding="utf-8")
    return v4


def main(argv=None):
    parser = argparse.ArgumentParser(description="Build Phase 3 v4 candidate")
    parser.add_argument("--root", type=Path, default=Path.cwd())
    arguments = parser.parse_args(argv)
    write_v4_candidate(arguments.root)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
