#!/usr/bin/env python3
"""Fail-closed structural and decoded-accessor validator for Phase-2 v10h."""

import argparse
import copy
import json
from pathlib import Path

from generate_phase2_modular_gltf_v10h import (
    ASSET_NAME, BASELINE_ROOT, DEFAULT_OUTPUT_ROOT, DESIGN_GATE, EXPECTED_SHA256,
    LOWER_Y_M, MANIFEST_NAME, RGB_MULTIPLIER, SIDEWALL_MATERIAL_NAME,
    SIDEWALL_ROLE, UPPER_Y_M, Y_TOLERANCE_M, _decode_buffer, sha256_path,
    validate_parameters,
)
from validate_phase2_modular_gltf_v10g import (
    EXPECTED_TOPOLOGY, _accessor_byte_offsets, _canonical, _decode_accessor,
    _mesh_by_role, _topology, validate as validate_v10g,
)


def validate(document, manifest, asset_root=DEFAULT_OUTPUT_ROOT,
             baseline_root=BASELINE_ROOT, verify_hashes=True):
    errors = []
    baseline_document = json.loads((baseline_root / ASSET_NAME).read_text(encoding="utf-8"))
    baseline_manifest = json.loads((baseline_root / MANIFEST_NAME).read_text(encoding="utf-8"))
    baseline_payload = bytes(_decode_buffer(baseline_document))
    payload = bytes(_decode_buffer(document))
    base_errors, _ = validate_v10g(baseline_document, baseline_manifest,
                                   baseline_root, verify_hashes=True)
    if base_errors:
        errors.append("frozen v10g baseline validator failed: " + "; ".join(base_errors))

    contract = manifest.get("v10h_offline_contract", {})
    authorizations = {
        "actual_gpu_used": contract.get("actual_gpu_used"),
        "gpu_capture_authorized": contract.get("gpu_capture_authorized"),
        "ta_review_authorized": contract.get("ta_review_authorized"),
        "formal_six_scene_evidence_authorized":
            contract.get("formal_six_scene_evidence_authorized"),
        "golden_promotion_authorized": contract.get("golden_promotion_authorized"),
    }
    if contract.get("evidence_scope") != "phase2_pocket_v10h_offline_only" or \
            contract.get("visual_review_status") != "NOT_RUN" or \
            contract.get("machine_status_before_execution") != "NOT_RUN" or \
            any(value is not False for value in authorizations.values()):
        errors.append("v10h metadata or authorization boundary is invalid")
    if contract.get("design_gate_sha256") != "sha256:" + EXPECTED_SHA256["design_gate"] or \
            contract.get("baseline_gltf_sha256") != "sha256:" + EXPECTED_SHA256["baseline_gltf"]:
        errors.append("v10h frozen input binding differs")
    serialized = _canonical(manifest)
    for token in ("PENDING_TA", "PENDING_ROOT_AND_TA", "phase2_pocket_v10c_single_scene_non_formal"):
        if token in serialized:
            errors.append("v10h manifest contains stale review token " + token)

    baseline_sidewalls = dict(_mesh_by_role(baseline_document, SIDEWALL_ROLE))
    sidewalls = _mesh_by_role(document, SIDEWALL_ROLE)
    if len(sidewalls) != 3:
        errors.append("expected exactly three frozen sidewall LOD meshes")
    material_indices = {mesh["primitives"][0].get("material") for _, mesh in sidewalls}
    if len(material_indices) != 1:
        errors.append("sidewalls no longer share the frozen v10g material slot")
        material_index = -1
        material = {}
    else:
        material_index = next(iter(material_indices))
        material = document["materials"][material_index]
    parameters = contract.get("parameters", {})
    pbr = material.get("pbrMetallicRoughness", {})
    color = pbr.get("baseColorFactor", [])
    base = color[2] if len(color) == 4 else -1
    try:
        validate_parameters(base, parameters["lower_endpoint_scalar"],
                            parameters["upper_endpoint_scalar"],
                            pbr.get("roughnessFactor"))
    except (KeyError, TypeError, ValueError) as error:
        errors.append("v10h parameter gate failed: " + str(error))
    expected_color = [base * value for value in RGB_MULTIPLIER] + [1.0]
    if material.get("name") != SIDEWALL_MATERIAL_NAME or len(color) != 4 or \
            any(abs(a - b) > 1e-9 for a, b in zip(color, expected_color)) or \
            pbr.get("metallicFactor") != 0.0 or pbr.get("roughnessFactor") != 0.84 or \
            material.get("emissiveFactor") != [0.0, 0.0, 0.0] or \
            any(key.endswith("Texture") for key in pbr):
        errors.append("v10h sidewall material violates neutral untextured contract")

    restored = copy.deepcopy(document)
    if 0 <= material_index < len(restored.get("materials", [])):
        restored["materials"][material_index] = copy.deepcopy(
            baseline_document["materials"][material_index])
    allowed_offsets = set()
    color_accessors = set()
    topology_report = {}
    ring_counts = {}
    for mesh_index, mesh in sidewalls:
        baseline_mesh = baseline_sidewalls.get(mesh_index)
        if baseline_mesh is None:
            errors.append("sidewall mesh indices changed")
            continue
        primitive = mesh["primitives"][0]
        if primitive.get("material") != baseline_mesh["primitives"][0].get("material"):
            errors.append("sidewall material slot changed from v10g")
        color_index = primitive["attributes"].get("COLOR_0")
        color_accessors.add(color_index)
        allowed_offsets.update(_accessor_byte_offsets(document, color_index))
        lod = int(mesh.get("extras", {}).get("lod", -1))
        positions, indices, edges, cross = _topology(document, payload, mesh)
        observed = {"vertices": len(positions), "indices": len(indices),
                    "edges": len(edges), "cross_edges": len(cross)}
        topology_report[str(lod)] = observed
        if observed != EXPECTED_TOPOLOGY.get(lod):
            errors.append("sidewall topology changed for LOD%d" % lod)
        colors = _decode_accessor(document, payload, color_index)
        upper_count = lower_count = 0
        for position, rgba in zip(positions, colors):
            if abs(position[1] - UPPER_Y_M) <= Y_TOLERANCE_M:
                expected = parameters.get("upper_endpoint_scalar")
                upper_count += 1
            elif abs(position[1] - LOWER_Y_M) <= Y_TOLERANCE_M:
                expected = parameters.get("lower_endpoint_scalar")
                lower_count += 1
            else:
                expected = None
            if expected is None or max(rgba[:3]) - min(rgba[:3]) > 1e-6 or \
                    abs(rgba[0] - expected) > 1e-6 or abs(rgba[3] - 1.0) > 1e-6:
                errors.append("sidewall LOD%d COLOR_0 endpoint mismatch" % lod)
                break
        ring_counts[str(lod)] = {"upper": upper_count, "lower": lower_count}
        if upper_count != lower_count or not upper_count:
            errors.append("sidewall LOD%d endpoint rings are unbalanced" % lod)

    restored["buffers"][0] = copy.deepcopy(baseline_document["buffers"][0])
    if _canonical(restored) != _canonical(baseline_document):
        errors.append("glTF JSON changed outside permitted sidewall material metadata/scalar")
    if len(payload) != len(baseline_payload):
        changed_offsets = []
        errors.append("embedded buffer byte length changed")
    else:
        changed_offsets = [i for i, values in enumerate(zip(baseline_payload, payload))
                           if values[0] != values[1]]
        if not changed_offsets or any(i not in allowed_offsets for i in changed_offsets):
            errors.append("embedded bytes changed outside sidewall COLOR_0")

    decoded_exact = []
    decoded_changed = []
    if len(document.get("accessors", [])) != len(baseline_document.get("accessors", [])):
        errors.append("accessor count changed")
    else:
        for index in range(len(document["accessors"])):
            before = _decode_accessor(baseline_document, baseline_payload, index)
            after = _decode_accessor(document, payload, index)
            if before == after:
                decoded_exact.append(index)
            else:
                decoded_changed.append(index)
                if index not in color_accessors:
                    errors.append("decoded non-COLOR accessor %d changed" % index)

    if manifest.get("files", {}).get("gltf", {}).get("path") != \
            "assets/modern/phase2-v10h/" + ASSET_NAME:
        errors.append("manifest glTF path is not the v10h root")
    if verify_hashes:
        gltf_path = asset_root / ASSET_NAME
        if manifest.get("files", {}).get("gltf", {}).get("sha256") != \
                "sha256:" + sha256_path(gltf_path):
            errors.append("manifest glTF SHA mismatch")
        if sha256_path(DESIGN_GATE) != EXPECTED_SHA256["design_gate"]:
            errors.append("design gate SHA changed")

    baseline_images = baseline_manifest.get("files", {}).get("images", {})
    candidate_images = manifest.get("files", {}).get("images", {})
    if candidate_images != baseline_images:
        errors.append("shared image bindings changed")
    report = {
        "schema": "phase2_pocket_v10h_decoded_diff_v1",
        "errors": errors,
        "authorizations": authorizations,
        "material_index_frozen": material_index,
        "sidewall_color_accessors": sorted(color_accessors),
        "changed_buffer_byte_count": len(changed_offsets),
        "changed_bytes_subset_of_allowed_COLOR_0":
            bool(changed_offsets) and all(i in allowed_offsets for i in changed_offsets),
        "decoded_accessors_exact": len(decoded_exact),
        "decoded_accessors_changed": decoded_changed,
        "decoded_non_COLOR_accessors_exact":
            all(i in color_accessors for i in decoded_changed),
        "topology": topology_report,
        "ring_counts": ring_counts,
        "scene_json_exact_after_permitted_restore":
            _canonical(restored) == _canonical(baseline_document),
        "shared_image_bindings_exact": candidate_images == baseline_images,
        "checks": {
            "validator_pass": not errors,
            "geometry_indices_non_COLOR_exact":
                all(i in color_accessors for i in decoded_changed),
            "only_sidewall_COLOR_0_buffer_bytes_changed":
                bool(changed_offsets) and all(i in allowed_offsets for i in changed_offsets),
            "camera_lighting_renderer_physics_roi_frozen_by_full_json_restore":
                _canonical(restored) == _canonical(baseline_document),
            "actual_gpu_used_false": authorizations["actual_gpu_used"] is False,
        },
    }
    return errors, report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--asset-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    root = args.asset_root.resolve()
    document = json.loads((root / ASSET_NAME).read_text(encoding="utf-8"))
    manifest = json.loads((root / MANIFEST_NAME).read_text(encoding="utf-8"))
    errors, report = validate(document, manifest, root)
    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))
    elif errors:
        for error in errors:
            print("ERROR:", error)
    else:
        print("Phase-2 v10h structural validation PASS")
    raise SystemExit(1 if errors else 0)


if __name__ == "__main__":
    main()
