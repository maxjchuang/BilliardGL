#!/usr/bin/env python3
"""Generate the Phase-2 v10g offline-only two-ring pocket candidate.

The generator is deliberately a byte-preserving transform of the frozen v10f
glTF.  It appends one untextured sidewall material, redirects only the three
sidewall primitives to it, and overwrites only their existing COLOR_0 buffer
ranges.  No geometry, texture, renderer or image is produced.
"""

import argparse
import base64
import copy
import hashlib
import json
from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[1]
BASELINE_ROOT = ROOT / "assets/modern/phase2-v10f"
DEFAULT_OUTPUT_ROOT = ROOT / "assets/modern/phase2-v10g"
ASSET_NAME = "focused_practice_hall_modular_v1.gltf"
MANIFEST_NAME = "focused_practice_hall_modular_v1.asset.json"
DESIGN_GATE = ROOT / "docs/art/phase2-pocket-v10g-offline-correction-design-gate-v2.json"
DIAGNOSTIC = ROOT / "docs/art/phase2-pocket-v10f-gpu-preflight-v1/root-cause-diagnostic-v1.json"

EXPECTED_SHA256 = {
    "baseline_gltf": "4bd3726f569b3b04d9b5d0e9b002c268d0bd15385112b147575d3a4845a04d56",
    "baseline_manifest": "3f2cf12e2f2b5bfc34352548d8a457046fd50b4db8df2db927fbff81d7907824",
    "design_gate": "eea60f8a01bf185f052ce278fc9d26f911dce57a736f29c3c27eb9a649245ae4",
    "diagnostic": "194f0e03e40bc95e7ecdb3a982dd1f91b039d5849efd2606bfec621b863a3592",
}

SIDEWALL_ROLE = "pocket_liner_sidewall_visual_only"
SIDEWALL_MATERIAL_NAME = "MAT_pocket_liner_sidewall_v10g"
UPPER_Y_M = 0.85883975
LOWER_Y_M = 0.83597976
Y_TOLERANCE_M = 1e-6

# Lowest-energy passing candidate selected by the deterministic v10g grid.
DEFAULT_BASE_NEUTRAL = 0.055
DEFAULT_LOWER_SCALAR = 0.30
DEFAULT_UPPER_SCALAR = 0.95
DEFAULT_ROUGHNESS = 0.84
RGB_MULTIPLIER = (0.84, 0.94, 1.0)


def sha256_bytes(payload):
    return hashlib.sha256(payload).hexdigest()


def sha256_path(path):
    return sha256_bytes(path.read_bytes())


def _canonical_json(value):
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode("utf-8")


def _require_frozen_inputs(baseline_root=BASELINE_ROOT, design_gate=DESIGN_GATE,
                           diagnostic=DIAGNOSTIC):
    paths = {
        "baseline_gltf": baseline_root / ASSET_NAME,
        "baseline_manifest": baseline_root / MANIFEST_NAME,
        "design_gate": design_gate,
        "diagnostic": diagnostic,
    }
    for role, path in paths.items():
        actual = sha256_path(path)
        if actual != EXPECTED_SHA256[role]:
            raise ValueError("%s SHA-256 mismatch: %s" % (role, actual))
    gate = json.loads(design_gate.read_text(encoding="utf-8"))
    if gate.get("schema") != "phase2_pocket_v10g_offline_correction_design_gate_v2" or \
            gate.get("status") != "APPROVED_FOR_OFFLINE_IMPLEMENTATION" or \
            gate.get("GPU_CAPTURE_AUTHORIZED") is not False:
        raise ValueError("v10g v2 gate is invalid or authorizes GPU capture")
    return paths


def validate_parameters(base_neutral, lower_scalar, upper_scalar, roughness):
    values = (base_neutral, lower_scalar, upper_scalar, roughness)
    if any(not isinstance(value, (int, float)) or not float("-inf") < value < float("inf")
           for value in values):
        raise ValueError("v10g parameters must be finite numbers")
    if not 0.055 <= base_neutral <= 0.08:
        raise ValueError("base neutral scalar is outside [0.055,0.08]")
    if not 0.30 <= lower_scalar <= 0.42:
        raise ValueError("lower endpoint scalar is outside [0.30,0.42]")
    if not 0.95 <= upper_scalar <= 1.0:
        raise ValueError("upper endpoint scalar is outside [0.95,1.0]")
    if not 0.74 <= roughness <= 0.84:
        raise ValueError("roughness is outside [0.74,0.84]")
    if not 0.53 <= upper_scalar - lower_scalar <= 0.70:
        raise ValueError("endpoint difference is outside [0.53,0.70]")


def _decode_buffer(document):
    prefix = "data:application/octet-stream;base64,"
    buffers = document.get("buffers", [])
    if len(buffers) != 1 or not buffers[0].get("uri", "").startswith(prefix):
        raise ValueError("v10g requires exactly one embedded glTF buffer")
    return bytearray(base64.b64decode(buffers[0]["uri"][len(prefix):], validate=True))


def _accessor_layout(document, accessor_index, expected_type=None):
    accessor = document["accessors"][accessor_index]
    view = document["bufferViews"][accessor["bufferView"]]
    if accessor.get("componentType") != 5126:
        raise ValueError("v10g endpoint accessors must use float32")
    components = {"VEC3": 3, "VEC4": 4}.get(accessor.get("type"))
    if not components or expected_type and accessor.get("type") != expected_type:
        raise ValueError("unexpected accessor type")
    packed = components * 4
    return accessor, view, view.get("byteOffset", 0) + accessor.get("byteOffset", 0), \
        view.get("byteStride", packed), components


def _read_vectors(document, payload, accessor_index, expected_type):
    accessor, _, offset, stride, components = _accessor_layout(
        document, accessor_index, expected_type)
    return [struct.unpack_from("<" + "f" * components, payload, offset + i * stride)
            for i in range(accessor["count"])]


def build_document(baseline_document, base_neutral=DEFAULT_BASE_NEUTRAL,
                   lower_scalar=DEFAULT_LOWER_SCALAR,
                   upper_scalar=DEFAULT_UPPER_SCALAR,
                   roughness=DEFAULT_ROUGHNESS):
    validate_parameters(base_neutral, lower_scalar, upper_scalar, roughness)
    document = copy.deepcopy(baseline_document)
    payload = _decode_buffer(document)
    material_index = len(document["materials"])
    color = [base_neutral * multiplier for multiplier in RGB_MULTIPLIER] + [1.0]
    document["materials"].append({
        "name": SIDEWALL_MATERIAL_NAME,
        "pbrMetallicRoughness": {
            "baseColorFactor": color,
            "metallicFactor": 0.0,
            "roughnessFactor": roughness,
        },
        "doubleSided": False,
        "emissiveFactor": [0.0, 0.0, 0.0],
        "extras": {
            "base_color_factor_space": "linear_srgb_d65",
            "fallback_workflow": "gltf2_metallic_roughness",
            "phase2_v10g_offline_only": True,
            "texture_change": "none",
        },
    })

    touched = []
    for mesh_index, mesh in enumerate(document["meshes"]):
        if mesh.get("extras", {}).get("asset_role") != SIDEWALL_ROLE:
            continue
        primitive = mesh["primitives"][0]
        positions = _read_vectors(document, payload,
                                  primitive["attributes"]["POSITION"], "VEC3")
        color_accessor_index = primitive["attributes"].get("COLOR_0")
        accessor, _, offset, stride, components = _accessor_layout(
            document, color_accessor_index, "VEC4")
        if components != 4 or accessor["count"] != len(positions):
            raise ValueError("sidewall POSITION/COLOR_0 cardinality mismatch")
        upper_count = lower_count = 0
        for vertex_index, position in enumerate(positions):
            if abs(position[1] - UPPER_Y_M) <= Y_TOLERANCE_M:
                scalar = upper_scalar
                upper_count += 1
            elif abs(position[1] - LOWER_Y_M) <= Y_TOLERANCE_M:
                scalar = lower_scalar
                lower_count += 1
            else:
                raise ValueError("sidewall vertex is not on a frozen endpoint ring")
            struct.pack_into("<4f", payload, offset + vertex_index * stride,
                             scalar, scalar, scalar, 1.0)
        if not upper_count or upper_count != lower_count:
            raise ValueError("sidewall endpoint rings are not balanced")
        old_material = primitive["material"]
        primitive["material"] = material_index
        touched.append({
            "mesh_index": mesh_index,
            "mesh": mesh["name"],
            "color_accessor": color_accessor_index,
            "vertex_count": len(positions),
            "upper_vertices": upper_count,
            "lower_vertices": lower_count,
            "old_material_index": old_material,
            "new_material_index": material_index,
        })
    if len(touched) != 3:
        raise ValueError("expected exactly three sidewall LOD primitives")
    document["buffers"][0]["uri"] = "data:application/octet-stream;base64," + \
        base64.b64encode(payload).decode("ascii")
    document["buffers"][0]["byteLength"] = len(payload)
    return document, touched


def build_manifest(baseline_manifest, gltf_bytes, touched, parameters):
    manifest = copy.deepcopy(baseline_manifest)
    manifest["asset_id"] = "focused_practice_hall_modular_v10g_offline_v2"
    manifest["status"] = "OFFLINE_CANDIDATE_GPU_NOT_AUTHORIZED"
    manifest["generator"] = "scripts/generate_phase2_modular_gltf_v10g.py"
    # The v10f manifest's legacy PENDING_TA token is provenance-defective for
    # this offline-only package.  Preserve its geometry values but explicitly
    # reset review semantics instead of copying that status forward.
    manifest["pocket_v10b_contract"]["manual_visual_status"] = "NOT_RUN"
    manifest["pocket_v10b_contract"]["status"] = \
        "FROZEN_GEOMETRY_BASELINE_NO_REVIEW_CLAIM"
    manifest["files"]["gltf"] = {
        "path": "assets/modern/phase2-v10g/" + ASSET_NAME,
        "sha256": "sha256:" + sha256_bytes(gltf_bytes),
    }
    manifest["budgets"]["materials_total"] = \
        baseline_manifest["budgets"]["materials_total"] + 1
    slots = manifest["budgets"]["functional_material_slots"]
    slots["table"] = baseline_manifest["budgets"]["functional_material_slots"]["table"] + 1
    manifest["shared_frozen_asset_root"] = {
        "path": "assets/modern/phase2-v10f",
        "policy": "reference_only_no_v10g_images_generated_or_copied",
        "baseline_manifest_sha256": "sha256:" + EXPECTED_SHA256["baseline_manifest"],
        "all_non_gltf_file_bindings_inherited_byte_identical": True,
    }
    manifest["v10g_offline_contract"] = {
        "schema": "phase2_pocket_v10g_asset_contract_v2",
        "evidence_scope": "phase2_pocket_v10g_offline_only",
        "visual_review_status": "NOT_RUN",
        "machine_status_before_execution": "NOT_RUN",
        "actual_gpu_used": False,
        "gpu_capture_authorized": False,
        "ta_review_authorized": False,
        "formal_six_scene_evidence_authorized": False,
        "design_gate_sha256": "sha256:" + EXPECTED_SHA256["design_gate"],
        "diagnostic_sha256": "sha256:" + EXPECTED_SHA256["diagnostic"],
        "solution": "A_TWO_RING_ENDPOINT_GPU_INTERPOLATION",
        "parameters": parameters,
        "touched_sidewall_primitives": touched,
        "permitted_gltf_differences": [
            "append MAT_pocket_liner_sidewall_v10g",
            "sidewall primitive material indices",
            "sidewall COLOR_0 accessor bytes",
            "embedded buffer data URI caused only by those COLOR_0 bytes",
        ],
    }
    return manifest


def generate(output_root=DEFAULT_OUTPUT_ROOT, baseline_root=BASELINE_ROOT,
             design_gate=DESIGN_GATE, diagnostic=DIAGNOSTIC,
             base_neutral=DEFAULT_BASE_NEUTRAL, lower_scalar=DEFAULT_LOWER_SCALAR,
             upper_scalar=DEFAULT_UPPER_SCALAR, roughness=DEFAULT_ROUGHNESS):
    paths = _require_frozen_inputs(baseline_root, design_gate, diagnostic)
    baseline_document = json.loads(paths["baseline_gltf"].read_text(encoding="utf-8"))
    baseline_manifest = json.loads(paths["baseline_manifest"].read_text(encoding="utf-8"))
    document, touched = build_document(baseline_document, base_neutral, lower_scalar,
                                       upper_scalar, roughness)
    gltf_bytes = _canonical_json(document)
    parameters = {
        "base_neutral_scalar": base_neutral,
        "base_color_factor_linear_rgba":
            [base_neutral * value for value in RGB_MULTIPLIER] + [1.0],
        "lower_endpoint_scalar": lower_scalar,
        "upper_endpoint_scalar": upper_scalar,
        "roughness": roughness,
        "metallic": 0.0,
        "emissiveFactor": [0.0, 0.0, 0.0],
    }
    manifest = build_manifest(baseline_manifest, gltf_bytes, touched, parameters)
    manifest_bytes = _canonical_json(manifest)
    output_root.mkdir(parents=True, exist_ok=True)
    if any(path.suffix.lower() in (".png", ".ppm") for path in output_root.iterdir()):
        raise ValueError("v10g output root must not contain images")
    (output_root / ASSET_NAME).write_bytes(gltf_bytes)
    (output_root / MANIFEST_NAME).write_bytes(manifest_bytes)
    return {
        "gltf_sha256": sha256_bytes(gltf_bytes),
        "manifest_sha256": sha256_bytes(manifest_bytes),
        "touched": touched,
        "parameters": parameters,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--baseline-root", type=Path, default=BASELINE_ROOT)
    parser.add_argument("--design-gate", type=Path, default=DESIGN_GATE)
    parser.add_argument("--diagnostic", type=Path, default=DIAGNOSTIC)
    parser.add_argument("--base-neutral", type=float, default=DEFAULT_BASE_NEUTRAL)
    parser.add_argument("--lower-scalar", type=float, default=DEFAULT_LOWER_SCALAR)
    parser.add_argument("--upper-scalar", type=float, default=DEFAULT_UPPER_SCALAR)
    parser.add_argument("--roughness", type=float, default=DEFAULT_ROUGHNESS)
    args = parser.parse_args()
    result = generate(args.output_root.resolve(), args.baseline_root.resolve(),
                      args.design_gate.resolve(), args.diagnostic.resolve(),
                      args.base_neutral, args.lower_scalar, args.upper_scalar,
                      args.roughness)
    print("phase2 v10g offline asset generated: gltf_sha256=%s manifest_sha256=%s" %
          (result["gltf_sha256"], result["manifest_sha256"]))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, IndexError, struct.error,
            json.JSONDecodeError) as error:
        print("phase2 v10g generation failed: " + str(error))
        raise SystemExit(2)
