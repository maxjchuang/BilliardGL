#!/usr/bin/env python3
"""Fail-closed structural validator for the Phase-2 v10g offline asset."""

import argparse
import base64
import copy
import hashlib
import json
from pathlib import Path
import struct

from generate_phase2_modular_gltf_v10g import (
    ASSET_NAME, BASELINE_ROOT, DEFAULT_OUTPUT_ROOT, DESIGN_GATE, DIAGNOSTIC,
    EXPECTED_SHA256, LOWER_Y_M, MANIFEST_NAME, RGB_MULTIPLIER,
    SIDEWALL_MATERIAL_NAME, SIDEWALL_ROLE, UPPER_Y_M, Y_TOLERANCE_M,
    _decode_buffer, _read_vectors, sha256_path, validate_parameters,
)
from validate_phase2_modular_gltf_v10f import validate as validate_v10f


ROOT = Path(__file__).resolve().parents[1]
EXPECTED_TOPOLOGY = {
    0: {"vertices": 576, "indices": 864, "edges": 720, "cross_edges": 432},
    1: {"vertices": 384, "indices": 576, "edges": 480, "cross_edges": 288},
    2: {"vertices": 240, "indices": 360, "edges": 300, "cross_edges": 180},
}


def _sha256_bytes(payload):
    return hashlib.sha256(payload).hexdigest()


def _decode_accessor(document, payload, index):
    accessor = document["accessors"][index]
    view = document["bufferViews"][accessor["bufferView"]]
    code = {5126: "f", 5125: "I", 5123: "H"}.get(accessor["componentType"])
    count = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}.get(accessor["type"])
    if not code or not count:
        raise ValueError("unsupported accessor")
    size = struct.calcsize("<" + code) * count
    stride = view.get("byteStride", size)
    offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    return [struct.unpack_from("<" + code * count, payload, offset + item * stride)
            for item in range(accessor["count"])]


def _accessor_byte_offsets(document, accessor_index):
    accessor = document["accessors"][accessor_index]
    view = document["bufferViews"][accessor["bufferView"]]
    components = {"VEC4": 4}.get(accessor["type"])
    if accessor["componentType"] != 5126 or components != 4:
        raise ValueError("COLOR_0 must be float32 VEC4")
    packed = components * 4
    stride = view.get("byteStride", packed)
    offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    result = set()
    for item in range(accessor["count"]):
        result.update(range(offset + item * stride, offset + item * stride + packed))
    return result


def _canonical(value):
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def _mesh_by_role(document, role):
    return [(index, mesh) for index, mesh in enumerate(document["meshes"])
            if mesh.get("extras", {}).get("asset_role") == role]


def _topology(document, payload, mesh):
    primitive = mesh["primitives"][0]
    positions = _decode_accessor(document, payload,
                                 primitive["attributes"]["POSITION"])
    indices = [item[0] for item in _decode_accessor(document, payload,
                                                    primitive["indices"])]
    edges = set()
    for offset in range(0, len(indices), 3):
        a, b, c = indices[offset:offset + 3]
        edges.update((tuple(sorted((a, b))), tuple(sorted((b, c))),
                      tuple(sorted((c, a)))))
    cross = [edge for edge in edges
             if abs(positions[edge[0]][1] - positions[edge[1]][1]) > Y_TOLERANCE_M]
    return positions, indices, edges, cross


def validate(document, manifest, asset_root=DEFAULT_OUTPUT_ROOT,
             baseline_root=BASELINE_ROOT, verify_hashes=True):
    errors = []
    baseline_document = json.loads((baseline_root / ASSET_NAME).read_text(encoding="utf-8"))
    baseline_manifest = json.loads((baseline_root / MANIFEST_NAME).read_text(encoding="utf-8"))
    baseline_payload = bytes(_decode_buffer(baseline_document))
    payload = bytes(_decode_buffer(document))

    baseline_errors, baseline_report = validate_v10f(
        baseline_document, baseline_manifest, baseline_root, verify_hashes=True)
    if baseline_errors:
        errors.append("frozen v10f baseline validator failed: " + "; ".join(baseline_errors))

    contract = manifest.get("v10g_offline_contract", {})
    if contract.get("evidence_scope") != "phase2_pocket_v10g_offline_only" or \
            contract.get("visual_review_status") != "NOT_RUN" or \
            contract.get("machine_status_before_execution") != "NOT_RUN" or \
            contract.get("actual_gpu_used") is not False or \
            contract.get("gpu_capture_authorized") is not False or \
            contract.get("ta_review_authorized") is not False:
        errors.append("v10g offline metadata or authorization boundary is invalid")
    forbidden_tokens = ("phase2_pocket_v10c_single_scene_non_formal", "PENDING_TA",
                        "PENDING_ROOT_AND_TA")
    serialized_manifest = _canonical(manifest)
    if any(token in serialized_manifest for token in forbidden_tokens):
        errors.append("v10g manifest inherited stale v10f/v10c review metadata")
    if contract.get("design_gate_sha256") != "sha256:" + EXPECTED_SHA256["design_gate"] or \
            contract.get("diagnostic_sha256") != "sha256:" + EXPECTED_SHA256["diagnostic"]:
        errors.append("v10g gate or diagnostic binding differs from the frozen SHA")

    if len(document.get("materials", [])) != len(baseline_document["materials"]) + 1:
        errors.append("v10g must append exactly one sidewall material")
        side_material = None
    else:
        side_material = document["materials"][-1]
    if side_material is not None:
        pbr = side_material.get("pbrMetallicRoughness", {})
        color = pbr.get("baseColorFactor", [])
        base = color[2] if len(color) == 4 else -1.0
        try:
            validate_parameters(base, contract["parameters"]["lower_endpoint_scalar"],
                                contract["parameters"]["upper_endpoint_scalar"],
                                pbr.get("roughnessFactor"))
        except (ValueError, KeyError, TypeError) as error:
            errors.append("sidewall material/endpoint parameter gate failed: " + str(error))
        expected_color = [base * multiplier for multiplier in RGB_MULTIPLIER] + [1.0]
        if side_material.get("name") != SIDEWALL_MATERIAL_NAME or \
                len(color) != 4 or any(abs(a - b) > 1e-9 for a, b in zip(color, expected_color)) or \
                pbr.get("metallicFactor") != 0.0 or \
                side_material.get("emissiveFactor") != [0.0, 0.0, 0.0] or \
                any(key in side_material for key in ("normalTexture", "occlusionTexture")) or \
                any(key.endswith("Texture") for key in pbr):
            errors.append("MAT_pocket_liner_sidewall_v10g violates its untextured neutral contract")

    # Restore the four permitted glTF differences and require exact JSON identity.
    restored = copy.deepcopy(document)
    if len(restored.get("materials", [])) == len(baseline_document["materials"]) + 1:
        restored["materials"] = restored["materials"][:-1]
    baseline_sidewalls = dict(_mesh_by_role(baseline_document, SIDEWALL_ROLE))
    candidate_sidewalls = _mesh_by_role(document, SIDEWALL_ROLE)
    if len(candidate_sidewalls) != 3:
        errors.append("v10g must contain exactly three sidewall LOD primitives")
    allowed_offsets = set()
    topology_report = {}
    material_index = len(baseline_document["materials"])
    lower_expected = contract.get("parameters", {}).get("lower_endpoint_scalar")
    upper_expected = contract.get("parameters", {}).get("upper_endpoint_scalar")
    for mesh_index, mesh in candidate_sidewalls:
        primitive = mesh["primitives"][0]
        baseline_mesh = baseline_sidewalls.get(mesh_index)
        if baseline_mesh is None:
            errors.append("sidewall mesh indices differ from v10f")
            continue
        if primitive.get("material") != material_index:
            errors.append("sidewall primitive is not bound to the appended material")
        restored["meshes"][mesh_index]["primitives"][0]["material"] = \
            baseline_mesh["primitives"][0]["material"]
        color_index = primitive["attributes"].get("COLOR_0")
        allowed_offsets.update(_accessor_byte_offsets(document, color_index))
        lod = int(mesh.get("extras", {}).get("lod", -1))
        positions, indices, edges, cross = _topology(document, payload, mesh)
        expected = EXPECTED_TOPOLOGY.get(lod, {})
        observed = {"vertices": len(positions), "indices": len(indices),
                    "edges": len(edges), "cross_edges": len(cross)}
        topology_report[str(lod)] = observed
        if observed != expected:
            errors.append("sidewall LOD%d topology differs from v10f/v2" % lod)
        ys = sorted(set(round(item[1], 8) for item in positions))
        if ys != [round(LOWER_Y_M, 8), round(UPPER_Y_M, 8)]:
            errors.append("sidewall LOD%d does not retain the two frozen Y rings" % lod)
        colors = _decode_accessor(document, payload, color_index)
        for position, color in zip(positions, colors):
            expected_scalar = upper_expected if abs(position[1] - UPPER_Y_M) <= Y_TOLERANCE_M \
                else lower_expected if abs(position[1] - LOWER_Y_M) <= Y_TOLERANCE_M else None
            if expected_scalar is None or len(color) != 4 or \
                    max(color[:3]) - min(color[:3]) > 1e-6 or \
                    abs(color[0] - expected_scalar) > 1e-6 or abs(color[3] - 1.0) > 1e-6:
                errors.append("sidewall LOD%d endpoint COLOR_0 is invalid" % lod)
                break
        # Coincident duplicated vertices must carry exactly matching endpoints.
        coincident = {}
        for position, color in zip(positions, colors):
            key = tuple(round(value, 7) for value in position)
            if key in coincident and abs(coincident[key] - color[0]) > 1e-6:
                errors.append("sidewall LOD%d geometric seam scalar mismatch" % lod)
                break
            coincident[key] = color[0]

    restored["buffers"][0]["uri"] = baseline_document["buffers"][0]["uri"]
    restored["buffers"][0]["byteLength"] = baseline_document["buffers"][0]["byteLength"]
    if _canonical(restored) != _canonical(baseline_document):
        errors.append("glTF JSON differs outside appended material and sidewall material indices")
    if len(payload) != len(baseline_payload):
        errors.append("embedded geometry buffer byte length differs from v10f")
        changed_offsets = []
    else:
        changed_offsets = [index for index, (before, after) in
                           enumerate(zip(baseline_payload, payload)) if before != after]
        if not changed_offsets or any(index not in allowed_offsets for index in changed_offsets):
            errors.append("embedded buffer differs outside sidewall COLOR_0 accessor bytes")

    # Every decoded accessor except the sidewall COLOR_0 accessors is exact.
    color_accessors = {mesh["primitives"][0]["attributes"]["COLOR_0"]
                       for _, mesh in candidate_sidewalls}
    exact_accessors = 0
    for index in range(len(document["accessors"])):
        if index in color_accessors:
            continue
        if _decode_accessor(document, payload, index) != \
                _decode_accessor(baseline_document, baseline_payload, index):
            errors.append("decoded accessor %d differs outside sidewall COLOR_0" % index)
        else:
            exact_accessors += 1

    image_files = [path for path in asset_root.iterdir()
                   if path.suffix.lower() in (".png", ".ppm")]
    if image_files:
        errors.append("v10g asset root contains forbidden generated/copied images")
    shared = manifest.get("shared_frozen_asset_root", {})
    if shared.get("path") != "assets/modern/phase2-v10f" or \
            shared.get("baseline_manifest_sha256") != \
            "sha256:" + EXPECTED_SHA256["baseline_manifest"]:
        errors.append("shared frozen v10f asset-root binding is invalid")
    candidate_files = copy.deepcopy(manifest.get("files", {}))
    candidate_files["gltf"] = copy.deepcopy(baseline_manifest["files"]["gltf"])
    if _canonical(candidate_files) != _canonical(baseline_manifest.get("files", {})):
        errors.append("shared non-glTF asset bindings differ from v10f manifest")

    def sha_entries(value):
        if isinstance(value, dict) and isinstance(value.get("path"), str) and \
                isinstance(value.get("sha256"), str):
            yield value
        elif isinstance(value, dict):
            for child in value.values():
                yield from sha_entries(child)

    shared_hashes_checked = 0
    for entry in sha_entries(baseline_manifest.get("files", {})):
        if Path(entry["path"]).name == ASSET_NAME:
            continue
        path = baseline_root / Path(entry["path"]).name
        if not path.is_file():
            errors.append("shared v10f asset is missing: " + path.name)
            continue
        actual = "sha256:" + _sha256_bytes(path.read_bytes())
        if actual != entry["sha256"]:
            errors.append("shared v10f asset SHA mismatch: " + path.name)
        shared_hashes_checked += 1

    if verify_hashes:
        gltf_entry = manifest.get("files", {}).get("gltf", {})
        if gltf_entry.get("sha256") != "sha256:" + sha256_path(asset_root / ASSET_NAME):
            errors.append("v10g glTF manifest hash mismatch")
        if sha256_path(DESIGN_GATE) != EXPECTED_SHA256["design_gate"] or \
                sha256_path(DIAGNOSTIC) != EXPECTED_SHA256["diagnostic"]:
            errors.append("frozen design-gate or diagnostic SHA changed")

    report = {
        "status": "VALID" if not errors else "INVALID",
        "baseline_validator": baseline_report,
        "topology": topology_report,
        "sidewall_color_accessors": sorted(color_accessors),
        "decoded_non_color_accessors_exact": exact_accessors,
        "changed_buffer_byte_count": len(changed_offsets),
        "changed_buffer_offset_min": min(changed_offsets) if changed_offsets else None,
        "changed_buffer_offset_max": max(changed_offsets) if changed_offsets else None,
        "all_changed_buffer_bytes_within_sidewall_color_accessors":
            bool(changed_offsets) and all(index in allowed_offsets for index in changed_offsets),
        "shared_asset_files_checked": shared_hashes_checked,
        "v10g_image_file_count": len(image_files),
        "material_count": len(document.get("materials", [])),
        "errors": errors,
    }
    return errors, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--asset-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--baseline-root", type=Path, default=BASELINE_ROOT)
    args = parser.parse_args()
    asset_root = args.asset_root.resolve()
    document = json.loads((asset_root / ASSET_NAME).read_text(encoding="utf-8"))
    manifest = json.loads((asset_root / MANIFEST_NAME).read_text(encoding="utf-8"))
    errors, report = validate(document, manifest, asset_root,
                              args.baseline_root.resolve(), verify_hashes=True)
    if errors:
        for error in errors:
            print("ERROR: " + error)
        return 1
    print("phase2 v10g offline glTF valid: materials=%d changed_color_bytes=%d "
          "topology=%s images=%d" %
          (report["material_count"], report["changed_buffer_byte_count"],
           json.dumps(report["topology"], sort_keys=True),
           report["v10g_image_file_count"]))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, IndexError, struct.error,
            json.JSONDecodeError) as error:
        print("phase2 v10g validation failed: " + str(error))
        raise SystemExit(2)
