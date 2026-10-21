#!/usr/bin/env python3
"""Generate the deterministic Phase-2 v10i offline-only sidewall candidate."""

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
BASELINE_ROOT = ROOT / "assets/modern/phase2-v10h"
DEFAULT_OUTPUT_ROOT = ROOT / "assets/modern/phase2-v10i"
DESIGN_GATE = ROOT / "docs/art/phase2-pocket-v10i-offline-correction-design-gate-v1.json"
EXPECTED_SHA256 = {
    "baseline_gltf": "99ddceb6771dc1a05e1a21b469940f457f229aa871368c10cd2bc40970c0e136",
    "baseline_manifest": "4534acc249d1cf505e0ee6303ea237123536226eb272f2a347d4703049ccc98a",
    "design_gate": "5a17fc98b9b4f95a937227da0f0c3104fbd12bf1456cd1af8f67ba59b73dda48",
}
SIDEWALL_MATERIAL_NAME = "MAT_pocket_liner_sidewall_v10i"
DEFAULT_BASE_NEUTRAL = 0.05625
DEFAULT_LOWER_SCALAR = 0.115
DEFAULT_UPPER_SCALAR = 0.99
DEFAULT_ROUGHNESS = 0.84


def validate_parameters(base, lower, upper, roughness):
    if any(not isinstance(v, (int, float)) or not float("-inf") < v < float("inf")
           for v in (base, lower, upper, roughness)):
        raise ValueError("v10i parameters must be finite")
    if base != 0.05625 or roughness != 0.84:
        raise ValueError("v10i base and roughness are frozen")
    if not any(abs(upper-v) <= 1e-9 for v in [0.97, 0.975, 0.98, 0.985, 0.99]):
        raise ValueError("upper endpoint is outside the v10i grid")
    if not any(abs(lower-v) <= 1e-9 for v in
               [0.105, 0.1075, 0.11, 0.1125, 0.115, 0.1175, 0.12]):
        raise ValueError("lower endpoint is outside the v10i grid")
    if not 0.85-1e-9 <= upper-lower <= 0.885+1e-9:
        raise ValueError("endpoint difference is outside [0.85,0.885]")


def require_inputs():
    paths = {"baseline_gltf": BASELINE_ROOT / ASSET_NAME,
             "baseline_manifest": BASELINE_ROOT / MANIFEST_NAME,
             "design_gate": DESIGN_GATE}
    for role, path in paths.items():
        if sha256_path(path) != EXPECTED_SHA256[role]:
            raise ValueError(role + " SHA-256 mismatch")
    gate = json.loads(DESIGN_GATE.read_text(encoding="utf-8"))
    if gate.get("status") != "APPROVED_FOR_OFFLINE_IMPLEMENTATION" or \
            gate.get("GPU_CAPTURE_AUTHORIZED") is not False or \
            gate.get("evidence_boundary", {}).get("design_gate_only") is not True:
        raise ValueError("v10i gate is invalid or authorizes GPU")


def build_document(baseline, base=DEFAULT_BASE_NEUTRAL,
                   lower=DEFAULT_LOWER_SCALAR, upper=DEFAULT_UPPER_SCALAR,
                   roughness=DEFAULT_ROUGHNESS):
    validate_parameters(base, lower, upper, roughness)
    document = copy.deepcopy(baseline)
    payload = _decode_buffer(document)
    sidewalls = [(i, m) for i, m in enumerate(document["meshes"])
                 if m.get("extras", {}).get("asset_role") == SIDEWALL_ROLE]
    if len(sidewalls) != 3:
        raise ValueError("expected three sidewall LOD meshes")
    material_indices = {m["primitives"][0]["material"] for _, m in sidewalls}
    if len(material_indices) != 1:
        raise ValueError("sidewall material slot differs across LODs")
    material_index = material_indices.pop()
    material = document["materials"][material_index]
    material["name"] = SIDEWALL_MATERIAL_NAME
    pbr = material["pbrMetallicRoughness"]
    pbr["baseColorFactor"] = [base*v for v in RGB_MULTIPLIER] + [1.0]
    pbr["roughnessFactor"] = roughness
    material["extras"] = {
        "base_color_factor_space": "linear_srgb_d65",
        "fallback_workflow": "gltf2_metallic_roughness",
        "phase2_v10i_offline_only": True,
        "texture_change": "none",
    }
    touched = []
    for mesh_index, mesh in sidewalls:
        primitive = mesh["primitives"][0]
        positions = _read_vectors(document, payload,
                                  primitive["attributes"]["POSITION"], "VEC3")
        color_index = primitive["attributes"]["COLOR_0"]
        accessor, _, offset, stride, _ = _accessor_layout(document, color_index, "VEC4")
        upper_count = lower_count = 0
        for vertex, position in enumerate(positions):
            if abs(position[1]-UPPER_Y_M) <= Y_TOLERANCE_M:
                scalar, upper_count = upper, upper_count+1
            elif abs(position[1]-LOWER_Y_M) <= Y_TOLERANCE_M:
                scalar, lower_count = lower, lower_count+1
            else:
                raise ValueError("sidewall vertex moved off the frozen rings")
            struct.pack_into("<4f", payload, offset+vertex*stride,
                             scalar, scalar, scalar, 1.0)
        if accessor["count"] != len(positions) or upper_count != lower_count:
            raise ValueError("sidewall endpoint rings are unbalanced")
        touched.append({"mesh_index": mesh_index, "mesh": mesh["name"],
                        "color_accessor": color_index, "upper_vertices": upper_count,
                        "lower_vertices": lower_count, "material_index": material_index})
    document["buffers"][0]["uri"] = "data:application/octet-stream;base64," + \
        base64.b64encode(payload).decode("ascii")
    return document, touched


def build_manifest(baseline, gltf_bytes, touched, parameters):
    manifest = copy.deepcopy(baseline)
    manifest["asset_id"] = "focused_practice_hall_modular_v10i_offline_v1"
    manifest["status"] = "OFFLINE_CANDIDATE_GPU_NOT_AUTHORIZED"
    manifest["generator"] = "scripts/generate_phase2_modular_gltf_v10i.py"
    manifest["files"]["gltf"] = {"path": "assets/modern/phase2-v10i/"+ASSET_NAME,
                                 "sha256": "sha256:"+sha256_bytes(gltf_bytes)}
    manifest.pop("v10h_offline_contract", None)
    manifest["shared_frozen_asset_root"] = {
        "path": "assets/modern/phase2-v10h",
        "policy": "reference_only_no_images_generated_or_copied",
        "baseline_gltf_sha256": "sha256:"+EXPECTED_SHA256["baseline_gltf"],
        "baseline_manifest_sha256": "sha256:"+EXPECTED_SHA256["baseline_manifest"],
    }
    manifest["v10i_offline_contract"] = {
        "schema": "phase2_pocket_v10i_asset_contract_v1",
        "evidence_scope": "phase2_pocket_v10i_offline_only",
        "visual_review_status": "NOT_RUN", "machine_status_before_execution": "NOT_RUN",
        "actual_gpu_used": False, "gpu_capture_authorized": False,
        "ta_review_authorized": False,
        "formal_six_scene_evidence_authorized": False,
        "golden_promotion_authorized": False,
        "design_gate_sha256": "sha256:"+EXPECTED_SHA256["design_gate"],
        "baseline_gltf_sha256": "sha256:"+EXPECTED_SHA256["baseline_gltf"],
        "parameters": parameters, "touched_sidewall_primitives": touched,
        "permitted_differences": [
            "existing sidewall material name/extras only; numeric material factors frozen",
            "existing sidewall COLOR_0 accessor bytes",
            "embedded buffer URI caused only by those COLOR_0 bytes",
        ],
    }
    return manifest


def generate(output_root=DEFAULT_OUTPUT_ROOT, base=DEFAULT_BASE_NEUTRAL,
             lower=DEFAULT_LOWER_SCALAR, upper=DEFAULT_UPPER_SCALAR,
             roughness=DEFAULT_ROUGHNESS):
    require_inputs()
    baseline = json.loads((BASELINE_ROOT/ASSET_NAME).read_text(encoding="utf-8"))
    baseline_manifest = json.loads((BASELINE_ROOT/MANIFEST_NAME).read_text(encoding="utf-8"))
    document, touched = build_document(baseline, base, lower, upper, roughness)
    gltf_bytes = _canonical_json(document)
    parameters = {"base_neutral_scalar": base, "lower_endpoint_scalar": lower,
                  "upper_endpoint_scalar": upper, "roughness": roughness}
    manifest = build_manifest(baseline_manifest, gltf_bytes, touched, parameters)
    output_root.mkdir(parents=True, exist_ok=True)
    (output_root/ASSET_NAME).write_bytes(gltf_bytes)
    (output_root/MANIFEST_NAME).write_bytes(_canonical_json(manifest))
    return document, manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--lower-scalar", type=float, default=DEFAULT_LOWER_SCALAR)
    parser.add_argument("--upper-scalar", type=float, default=DEFAULT_UPPER_SCALAR)
    args = parser.parse_args()
    generate(args.output_root.resolve(), lower=args.lower_scalar, upper=args.upper_scalar)


if __name__ == "__main__":
    main()
