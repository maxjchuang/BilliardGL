#!/usr/bin/env python3
"""Generate the deterministic Phase-2 v10h offline-only sidewall candidate."""

import argparse
import base64
import copy
import json
from pathlib import Path
import struct

from generate_phase2_modular_gltf_v10g import (
    ASSET_NAME, MANIFEST_NAME, RGB_MULTIPLIER, SIDEWALL_ROLE,
    UPPER_Y_M, LOWER_Y_M, Y_TOLERANCE_M, _canonical_json, _decode_buffer,
    _read_vectors, _accessor_layout, sha256_bytes, sha256_path,
)


ROOT = Path(__file__).resolve().parents[1]
BASELINE_ROOT = ROOT / "assets/modern/phase2-v10g"
DEFAULT_OUTPUT_ROOT = ROOT / "assets/modern/phase2-v10h"
DESIGN_GATE = ROOT / "docs/art/phase2-pocket-v10h-offline-correction-design-gate-v1.json"
EXPECTED_SHA256 = {
    "baseline_gltf": "a72c5ab4dd477fe3a6d85e2ced1421af93bb768ff613a411dabb0a02def6226f",
    "baseline_manifest": "27387e5c06606ab90cb91e8620868b02cc8f8417c5ce6db7618512b290f11e4b",
    "design_gate": "fd6c2d501a54dce74c2ceb1117b2364099465010e880781c8049f9cd6a269d08",
}
SIDEWALL_MATERIAL_NAME = "MAT_pocket_liner_sidewall_v10h"
DEFAULT_BASE_NEUTRAL = 0.05625
DEFAULT_LOWER_SCALAR = 0.055
DEFAULT_UPPER_SCALAR = 0.99
DEFAULT_ROUGHNESS = 0.84


def validate_parameters(base_neutral, lower_scalar, upper_scalar, roughness):
    values = (base_neutral, lower_scalar, upper_scalar, roughness)
    if any(not isinstance(v, (int, float)) or not float("-inf") < v < float("inf")
           for v in values):
        raise ValueError("v10h parameters must be finite")
    grids = (
        (base_neutral, [0.0525, 0.05375, 0.055, 0.05625, 0.0575], "base"),
        (upper_scalar, [0.98, 0.985, 0.99, 0.995, 1.0], "upper"),
        (lower_scalar, [0.04 + 0.005 * i for i in range(9)], "lower"),
    )
    for value, grid, name in grids:
        if not any(abs(value - item) <= 1e-9 for item in grid):
            raise ValueError("%s scalar is outside the declared v10h grid" % name)
    if roughness != 0.84:
        raise ValueError("v10h roughness must remain 0.84")
    if not 0.90 - 1e-9 <= upper_scalar - lower_scalar <= 0.96 + 1e-9:
        raise ValueError("v10h endpoint difference is outside [0.90,0.96]")


def require_inputs(baseline_root=BASELINE_ROOT, design_gate=DESIGN_GATE):
    paths = {"baseline_gltf": baseline_root / ASSET_NAME,
             "baseline_manifest": baseline_root / MANIFEST_NAME,
             "design_gate": design_gate}
    for role, path in paths.items():
        if sha256_path(path) != EXPECTED_SHA256[role]:
            raise ValueError("%s SHA-256 mismatch" % role)
    gate = json.loads(design_gate.read_text(encoding="utf-8"))
    if gate.get("status") != "APPROVED_FOR_OFFLINE_IMPLEMENTATION" or \
            gate.get("GPU_CAPTURE_AUTHORIZED") is not False:
        raise ValueError("v10h gate is invalid or authorizes GPU")
    return paths


def build_document(baseline, base_neutral=DEFAULT_BASE_NEUTRAL,
                   lower_scalar=DEFAULT_LOWER_SCALAR,
                   upper_scalar=DEFAULT_UPPER_SCALAR,
                   roughness=DEFAULT_ROUGHNESS):
    validate_parameters(base_neutral, lower_scalar, upper_scalar, roughness)
    document = copy.deepcopy(baseline)
    payload = _decode_buffer(document)
    sidewalls = [(i, m) for i, m in enumerate(document["meshes"])
                 if m.get("extras", {}).get("asset_role") == SIDEWALL_ROLE]
    if len(sidewalls) != 3:
        raise ValueError("expected exactly three sidewall LOD meshes")
    material_indices = {m["primitives"][0]["material"] for _, m in sidewalls}
    if len(material_indices) != 1:
        raise ValueError("v10g sidewalls do not share one material")
    material_index = material_indices.pop()
    material = document["materials"][material_index]
    material["name"] = SIDEWALL_MATERIAL_NAME
    pbr = material["pbrMetallicRoughness"]
    pbr["baseColorFactor"] = [base_neutral * v for v in RGB_MULTIPLIER] + [1.0]
    pbr["roughnessFactor"] = roughness
    material["extras"] = {
        "base_color_factor_space": "linear_srgb_d65",
        "fallback_workflow": "gltf2_metallic_roughness",
        "phase2_v10h_offline_only": True,
        "texture_change": "none",
    }
    touched = []
    for mesh_index, mesh in sidewalls:
        primitive = mesh["primitives"][0]
        positions = _read_vectors(document, payload,
                                  primitive["attributes"]["POSITION"], "VEC3")
        accessor_index = primitive["attributes"]["COLOR_0"]
        accessor, _, offset, stride, _ = _accessor_layout(
            document, accessor_index, "VEC4")
        upper_count = lower_count = 0
        for vertex, position in enumerate(positions):
            if abs(position[1] - UPPER_Y_M) <= Y_TOLERANCE_M:
                scalar, upper_count = upper_scalar, upper_count + 1
            elif abs(position[1] - LOWER_Y_M) <= Y_TOLERANCE_M:
                scalar, lower_count = lower_scalar, lower_count + 1
            else:
                raise ValueError("sidewall vertex moved off frozen endpoint rings")
            struct.pack_into("<4f", payload, offset + vertex * stride,
                             scalar, scalar, scalar, 1.0)
        if accessor["count"] != len(positions) or upper_count != lower_count:
            raise ValueError("unbalanced sidewall endpoint rings")
        touched.append({"mesh_index": mesh_index, "mesh": mesh["name"],
                        "color_accessor": accessor_index,
                        "upper_vertices": upper_count, "lower_vertices": lower_count,
                        "material_index": material_index})
    document["buffers"][0]["uri"] = "data:application/octet-stream;base64," + \
        base64.b64encode(payload).decode("ascii")
    return document, touched


def build_manifest(baseline, gltf_bytes, touched, parameters):
    manifest = copy.deepcopy(baseline)
    manifest["asset_id"] = "focused_practice_hall_modular_v10h_offline_v1"
    manifest["status"] = "OFFLINE_CANDIDATE_GPU_NOT_AUTHORIZED"
    manifest["generator"] = "scripts/generate_phase2_modular_gltf_v10h.py"
    manifest["files"]["gltf"] = {
        "path": "assets/modern/phase2-v10h/" + ASSET_NAME,
        "sha256": "sha256:" + sha256_bytes(gltf_bytes),
    }
    manifest.pop("v10g_offline_contract", None)
    manifest["shared_frozen_asset_root"] = {
        "path": "assets/modern/phase2-v10g",
        "policy": "reference_only_no_images_generated_or_copied",
        "baseline_gltf_sha256": "sha256:" + EXPECTED_SHA256["baseline_gltf"],
        "baseline_manifest_sha256": "sha256:" + EXPECTED_SHA256["baseline_manifest"],
    }
    manifest["v10h_offline_contract"] = {
        "schema": "phase2_pocket_v10h_asset_contract_v1",
        "evidence_scope": "phase2_pocket_v10h_offline_only",
        "visual_review_status": "NOT_RUN",
        "machine_status_before_execution": "NOT_RUN",
        "actual_gpu_used": False,
        "gpu_capture_authorized": False,
        "ta_review_authorized": False,
        "formal_six_scene_evidence_authorized": False,
        "golden_promotion_authorized": False,
        "design_gate_sha256": "sha256:" + EXPECTED_SHA256["design_gate"],
        "baseline_gltf_sha256": "sha256:" + EXPECTED_SHA256["baseline_gltf"],
        "parameters": parameters,
        "touched_sidewall_primitives": touched,
        "permitted_differences": [
            "existing sidewall material name/baseColorFactor/extras",
            "sidewall COLOR_0 accessor bytes",
            "embedded buffer URI caused only by those COLOR_0 bytes",
        ],
    }
    return manifest


def generate(output_root=DEFAULT_OUTPUT_ROOT, base_neutral=DEFAULT_BASE_NEUTRAL,
             lower_scalar=DEFAULT_LOWER_SCALAR, upper_scalar=DEFAULT_UPPER_SCALAR,
             roughness=DEFAULT_ROUGHNESS):
    require_inputs()
    baseline_doc = json.loads((BASELINE_ROOT / ASSET_NAME).read_text(encoding="utf-8"))
    baseline_manifest = json.loads((BASELINE_ROOT / MANIFEST_NAME).read_text(encoding="utf-8"))
    document, touched = build_document(baseline_doc, base_neutral, lower_scalar,
                                       upper_scalar, roughness)
    gltf_bytes = _canonical_json(document)
    parameters = {"base_neutral_scalar": base_neutral,
                  "lower_endpoint_scalar": lower_scalar,
                  "upper_endpoint_scalar": upper_scalar, "roughness": roughness}
    manifest = build_manifest(baseline_manifest, gltf_bytes, touched, parameters)
    output_root.mkdir(parents=True, exist_ok=True)
    (output_root / ASSET_NAME).write_bytes(gltf_bytes)
    (output_root / MANIFEST_NAME).write_bytes(_canonical_json(manifest))
    return document, manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--base-neutral", type=float, default=DEFAULT_BASE_NEUTRAL)
    parser.add_argument("--lower-scalar", type=float, default=DEFAULT_LOWER_SCALAR)
    parser.add_argument("--upper-scalar", type=float, default=DEFAULT_UPPER_SCALAR)
    args = parser.parse_args()
    generate(args.output_root.resolve(), args.base_neutral,
             args.lower_scalar, args.upper_scalar)


if __name__ == "__main__":
    main()
