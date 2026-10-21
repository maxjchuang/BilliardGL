#!/usr/bin/env python3
"""Fail-closed decoded/raw diff validator for the Phase-2 v10i asset."""

import argparse
import copy
import json
from pathlib import Path

from generate_phase2_modular_gltf_v10i import (
    ASSET_NAME, BASELINE_ROOT, DEFAULT_OUTPUT_ROOT, DESIGN_GATE, EXPECTED_SHA256,
    LOWER_Y_M, MANIFEST_NAME, SIDEWALL_MATERIAL_NAME, SIDEWALL_ROLE,
    UPPER_Y_M, Y_TOLERANCE_M, _decode_buffer, sha256_path, validate_parameters,
)
from validate_phase2_modular_gltf_v10g import (
    EXPECTED_TOPOLOGY, _accessor_byte_offsets, _canonical, _decode_accessor,
    _mesh_by_role, _topology,
)
from validate_phase2_modular_gltf_v10h import validate as validate_v10h


def validate(document, manifest, asset_root=DEFAULT_OUTPUT_ROOT,
             baseline_root=BASELINE_ROOT, verify_hashes=True):
    errors = []
    baseline = json.loads((baseline_root/ASSET_NAME).read_text(encoding="utf-8"))
    baseline_manifest = json.loads((baseline_root/MANIFEST_NAME).read_text(encoding="utf-8"))
    baseline_payload = bytes(_decode_buffer(baseline)); payload = bytes(_decode_buffer(document))
    base_errors, _ = validate_v10h(baseline, baseline_manifest, baseline_root)
    if base_errors:
        errors.append("frozen v10h baseline failed: "+"; ".join(base_errors))
    contract = manifest.get("v10i_offline_contract", {})
    authorizations = {key: contract.get(key) for key in (
        "actual_gpu_used", "gpu_capture_authorized", "ta_review_authorized",
        "formal_six_scene_evidence_authorized", "golden_promotion_authorized")}
    if contract.get("evidence_scope") != "phase2_pocket_v10i_offline_only" or \
            contract.get("visual_review_status") != "NOT_RUN" or \
            contract.get("machine_status_before_execution") != "NOT_RUN" or \
            any(value is not False for value in authorizations.values()):
        errors.append("v10i metadata or authorization boundary is invalid")
    if contract.get("design_gate_sha256") != "sha256:"+EXPECTED_SHA256["design_gate"] or \
            contract.get("baseline_gltf_sha256") != "sha256:"+EXPECTED_SHA256["baseline_gltf"]:
        errors.append("v10i frozen input binding differs")
    if any(token in _canonical(manifest) for token in
           ("PENDING_TA", "PENDING_ROOT_AND_TA", "phase2_pocket_v10h_single_scene_non_formal")):
        errors.append("v10i manifest contains stale review/failure metadata")

    base_sidewalls = dict(_mesh_by_role(baseline, SIDEWALL_ROLE))
    sidewalls = _mesh_by_role(document, SIDEWALL_ROLE)
    material_indices = {mesh["primitives"][0].get("material") for _, mesh in sidewalls}
    if len(sidewalls) != 3 or len(material_indices) != 1:
        errors.append("v10i sidewall mesh/material-slot count differs")
        material_index, material = -1, {}
    else:
        material_index = next(iter(material_indices)); material = document["materials"][material_index]
    parameters = contract.get("parameters", {})
    pbr = material.get("pbrMetallicRoughness", {})
    color = pbr.get("baseColorFactor", [])
    try:
        validate_parameters(color[2], parameters["lower_endpoint_scalar"],
                            parameters["upper_endpoint_scalar"], pbr.get("roughnessFactor"))
    except (IndexError, KeyError, TypeError, ValueError) as error:
        errors.append("v10i parameter gate failed: "+str(error))
    if material_index >= 0:
        baseline_material = baseline["materials"][material_index]
        restored_material = copy.deepcopy(material)
        restored_material["name"] = baseline_material["name"]
        restored_material["extras"] = copy.deepcopy(baseline_material.get("extras"))
        if _canonical(restored_material) != _canonical(baseline_material) or \
                material.get("name") != SIDEWALL_MATERIAL_NAME:
            errors.append("sidewall numeric material factors changed outside metadata")

    restored = copy.deepcopy(document)
    if material_index >= 0:
        restored["materials"][material_index] = copy.deepcopy(baseline["materials"][material_index])
    allowed_offsets, color_accessors = set(), set()
    topology, ring_counts = {}, {}
    lower_expected = parameters.get("lower_endpoint_scalar")
    upper_expected = parameters.get("upper_endpoint_scalar")
    for mesh_index, mesh in sidewalls:
        base_mesh = base_sidewalls.get(mesh_index)
        if base_mesh is None:
            errors.append("sidewall mesh index changed"); continue
        primitive = mesh["primitives"][0]
        if primitive.get("material") != base_mesh["primitives"][0].get("material"):
            errors.append("sidewall material slot changed")
        color_index = primitive["attributes"]["COLOR_0"]
        color_accessors.add(color_index); allowed_offsets.update(_accessor_byte_offsets(document, color_index))
        lod = int(mesh.get("extras", {}).get("lod", -1))
        positions, indices, edges, cross = _topology(document, payload, mesh)
        observed = {"vertices":len(positions),"indices":len(indices),
                    "edges":len(edges),"cross_edges":len(cross)}
        topology[str(lod)] = observed
        if observed != EXPECTED_TOPOLOGY.get(lod):
            errors.append("sidewall topology changed for LOD%d"%lod)
        colors = _decode_accessor(document, payload, color_index)
        upper_count = lower_count = 0
        for position, rgba in zip(positions, colors):
            if abs(position[1]-UPPER_Y_M) <= Y_TOLERANCE_M:
                expected, upper_count = upper_expected, upper_count+1
            elif abs(position[1]-LOWER_Y_M) <= Y_TOLERANCE_M:
                expected, lower_count = lower_expected, lower_count+1
            else: expected = None
            if expected is None or max(rgba[:3])-min(rgba[:3]) > 1e-6 or \
                    abs(rgba[0]-expected)>1e-6 or abs(rgba[3]-1)>1e-6:
                errors.append("sidewall LOD%d COLOR_0 mismatch"%lod); break
        ring_counts[str(lod)] = {"upper":upper_count,"lower":lower_count}
        if upper_count != lower_count or not upper_count:
            errors.append("sidewall LOD%d rings unbalanced"%lod)
    restored["buffers"][0] = copy.deepcopy(baseline["buffers"][0])
    scene_exact = _canonical(restored) == _canonical(baseline)
    if not scene_exact:
        errors.append("glTF JSON changed outside permitted material metadata/COLOR_0 URI")
    if len(payload) != len(baseline_payload):
        changed_offsets=[]; errors.append("embedded buffer length changed")
    else:
        changed_offsets=[i for i,(a,b) in enumerate(zip(baseline_payload,payload)) if a!=b]
        if not changed_offsets or any(i not in allowed_offsets for i in changed_offsets):
            errors.append("raw bytes changed outside sidewall COLOR_0")
    decoded_exact, decoded_changed = [], []
    if len(document["accessors"]) != len(baseline["accessors"]):
        errors.append("accessor count changed")
    else:
        for index in range(len(document["accessors"])):
            if _decode_accessor(document,payload,index) == _decode_accessor(baseline,baseline_payload,index):
                decoded_exact.append(index)
            else:
                decoded_changed.append(index)
                if index not in color_accessors:
                    errors.append("decoded non-COLOR accessor %d changed"%index)
    candidate_images=manifest.get("files",{}).get("images",{})
    baseline_images=baseline_manifest.get("files",{}).get("images",{})
    if candidate_images != baseline_images:
        errors.append("shared image bindings changed")
    if verify_hashes:
        if manifest.get("files",{}).get("gltf",{}).get("sha256") != \
                "sha256:"+sha256_path(asset_root/ASSET_NAME):
            errors.append("manifest glTF SHA mismatch")
        if sha256_path(DESIGN_GATE) != EXPECTED_SHA256["design_gate"]:
            errors.append("design gate SHA changed")
    report = {
        "schema":"phase2_pocket_v10i_decoded_raw_diff_v1", "errors":errors,
        "authorizations":authorizations, "material_index_frozen":material_index,
        "sidewall_color_accessors":sorted(color_accessors),
        "changed_buffer_byte_count":len(changed_offsets),
        "changed_bytes_subset_of_allowed_COLOR_0":bool(changed_offsets) and
            all(i in allowed_offsets for i in changed_offsets),
        "decoded_accessors_exact":len(decoded_exact),
        "decoded_accessors_changed":decoded_changed,
        "decoded_non_COLOR_accessors_exact":all(i in color_accessors for i in decoded_changed),
        "scene_json_exact_after_permitted_restore":scene_exact,
        "shared_image_bindings_exact":candidate_images==baseline_images,
        "topology":topology,"ring_counts":ring_counts,
        "checks":{
            "validator_pass":not errors,
            "only_sidewall_COLOR_0_raw_bytes_changed":bool(changed_offsets) and all(i in allowed_offsets for i in changed_offsets),
            "geometry_indices_non_COLOR_exact":all(i in color_accessors for i in decoded_changed),
            "camera_light_renderer_physics_roi_exact_by_full_restore":scene_exact,
            "material_numeric_factors_frozen": material_index >= 0 and _canonical(restored["materials"][material_index]) == _canonical(baseline["materials"][material_index]),
            "actual_gpu_used_false":authorizations.get("actual_gpu_used") is False,
        }}
    return errors, report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--asset-root",type=Path,default=DEFAULT_OUTPUT_ROOT)
    parser.add_argument("--json",action="store_true"); args=parser.parse_args(); root=args.asset_root.resolve()
    document=json.loads((root/ASSET_NAME).read_text()); manifest=json.loads((root/MANIFEST_NAME).read_text())
    errors,report=validate(document,manifest,root)
    print(json.dumps(report,indent=2,sort_keys=True) if args.json else
          "Phase-2 v10i structural validation PASS" if not errors else "\n".join("ERROR: "+e for e in errors))
    raise SystemExit(1 if errors else 0)


if __name__=="__main__": main()
