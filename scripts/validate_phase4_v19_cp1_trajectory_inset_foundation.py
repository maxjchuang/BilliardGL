#!/usr/bin/env python3
"""Fail-closed validator for the Phase4 V19 CP1 inset foundation receipt."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_RECEIPT = ROOT / "docs/evidence/phase4-v19-cp1-trajectory-inset-foundation-implementation-v1/receipt.json"

EXPECTED_BINDINGS = {
    "v19_original_gate": ("docs/art/phase4-v19-dedicated-trajectory-inset-minimap-product-decision-and-implementation-design-gate-v1.json", "c7d86d9dcdd4891fac9dd9cbfffe3931561b5098db1df5a4aaec9b50e79af4c7"),
    "v19_orientation_gate": ("docs/art/phase4-v19-landscape-orientation-layout-interpretation-correction-gate-v1.json", "cffc93f29bb24f7f9378dee3c17fffd1da1c139745eec17bdb380cf6bda04d13"),
    "v19_dash_event_gate": ("docs/art/phase4-v19-route-wide-dash-event-semantic-interpretation-correction-gate-v1.json", "de1d3dd818050d42e03025080939002af82395fd42ea9a22d3413aab0efe3441"),
    "v19_endpoint_overlap_addendum": ("docs/art/phase4-v19-endpoint-event-overlap-ta-acceptance-interpretation-addendum-v1.json", "5d78711e5bd7f6200727db3c60cf8e102fc462aefefc4aa329fff5e52349279c"),
    "inset_model_header": ("src/Billiards/training/trajectory_inset_model.h", "3a6e0550d1f85b9168528454b74c5768c5d6c9b9ba6db0dd8fd4fdac1c66a8c6"),
    "inset_model_source": ("src/Billiards/training/trajectory_inset_model.cpp", "58a9a3dfe922c235314884d36bd8cea47df8cfe400adcf5a4a716e64e093e2b4"),
    "inset_renderer_header": ("src/Billiards/training/trajectory_inset_renderer.h", "2e7191c1b260dc257a07f5868d0dc1a9dc0012e69696229f3fd0118c02af3947"),
    "inset_renderer_source": ("src/Billiards/training/trajectory_inset_renderer.cpp", "9969c1736fb89329e25dfa1d0252f18e5475d18a9027ddebf740cab4e8804201"),
    "cp1_cpu_test_source": ("tests/phase4_v19_trajectory_inset_cpu_tests.cpp", "56c1c8e21bcd87b00cc11cdb5e21012a2acb0b4c3dcd4e08b98b9dfb254eef6b"),
    "world_trajectory_renderer_source": ("src/Billiards/training/trajectory_overlay_renderer.cpp", "0c108c506641ab4828290a1a4c7b6b5ebbd51ab4fc7c12845bde0f5deb18a840"),
    "world_trajectory_renderer_header": ("src/Billiards/training/trajectory_overlay_renderer.h", "9805e6a5f56c17d7645200576505425cddf0d642970e5dc5cec186e003bd564e"),
    "world_trajectory_model_header": ("src/Billiards/training/trajectory_overlay_model.h", "176049fc73e2558e3ea96ed1e2cc4fe67c11c1f771b19752b2a9cf32f8d24869"),
    "deterministic_hash_source": ("src/Billiards/deterministic_hash.cpp", "254552ed3f64f80e52baa7cabeda4cad52160e4d6fed9e55a4ff94aa791f4410"),
    "deterministic_hash_header": ("src/Billiards/deterministic_hash.h", "7a4f0f5d3bf895fb7b1260d0ad4656af78da9604fdd40b8de635643bd9a6ea6c"),
    "automation_json_source": ("src/Billiards/automation_json.cpp", "ab0dcde0aa6934d1909923cf7b48583975639453ab07ff2452c63232c37d29d1"),
    "automation_json_header": ("src/Billiards/automation_json.h", "229b540419df1f8525c4f366a393b64667ab64a6d59024c96ba9b1ee9528b0ef"),
    "cmake_cp1_untouched": ("CMakeLists.txt", "f41563c35224dd809f64acedba6455355e70082697c41305c2d1b05426f71d41"),
    "modern_renderer_cpp_cp2_cp3_untouched": ("src/Billiards/render/modern/modern_renderer.cpp", "b00cc6b370122275a42f106967f935dbb34bd5d3f81f2a750ca9ba6e503af191"),
    "modern_renderer_h_cp2_cp3_untouched": ("src/Billiards/render/modern/modern_renderer.h", "936de5b77c9e3485f2e51f10e9de26c59c8ec94674865e0b50ae125725183c9d"),
    "product_shell_layout_cpp_cp2_untouched": ("src/Billiards/render/ui/product_shell_layout.cpp", "1cd65911f93f72f1fb4adf353c401047832b2b46b24372cf14fb1ebdbfa250fc"),
    "product_shell_layout_h_cp2_untouched": ("src/Billiards/render/ui/product_shell_layout.h", "5a9b8836a133baf42c788b92bcd2a3f212081995a7eb2af3c100f78667a7312e"),
    "product_shell_draw_list_cpp_cp2_untouched": ("src/Billiards/render/ui/product_shell_draw_list.cpp", "c0ce953eb5f8c72368540dd2128ca5f60304371812213894d947adf1f8b4f07b"),
    "product_shell_draw_list_h_cp2_untouched": ("src/Billiards/render/ui/product_shell_draw_list.h", "daf31b57263a81b0e041be890ead7fed1e44b95d2dc0336b7e6c9017ed79da2c"),
    "cpu_test_stdout": ("docs/evidence/phase4-v19-cp1-trajectory-inset-foundation-implementation-v1/cpu-test.stdout", "ec13f4c1d0638e75ca653ab1b36336239947317acd46cf8d546e2b96121e846b"),
    "receipt_validator": ("scripts/validate_phase4_v19_cp1_trajectory_inset_foundation.py", None),
    "receipt_mutation_tests": ("tests/test_phase4_v19_cp1_trajectory_inset_foundation.py", "de93cc48d556b3f7f1d4378c7e4d4257861ac273af5378c96377aec9d5c8c2cf"),
}

SOURCE_ROLES = ["inset_model_header", "inset_model_source", "inset_renderer_header", "inset_renderer_source", "cp1_cpu_test_source"]
EXPECTED_AGGREGATE = "375b48d42643eb48a01d86907dbcfd4abfb067bf2eeba1a3ee8250be6d482a9b"
EXPECTED_STDOUT = "phase4_v19_trajectory_inset_cpu_tests: PASS cases=96 camera=4 palette=2 samples=1/2/4 layouts=4\n"
EXPECTED_INVENTORY = [
    {"profileId":"1920x1080_ui100","sourceGroupDenominator":8,"semanticMemberDenominator":72,"probeDenominator":29,"dashEventCount":17,"routeSpanCount":1,"orderedParentSourceDigest":"c6818f25d3da0db33e8fb39a7f0901337be0b858cd6acfba9d93f6d12d1ac186","orderedProbeDigest":"0fe2720d3e580ef5c720984011a3836b15493b019a892e6feb1458863c5efc17","orderedDashEventDigest":"ae1b95a3ca21f3986441721b8378a3d4460d45de700da1fd73fcbd2da6452254","orderedDashContributorDigest":"ab91990a175d41ce9db37a0aac24f7b3406e122f58f4a471d47ae1be751eee4b","orderedSourceRouteIntervalDigest":"4a95ae275292661a2ae4b16037cea1da20b9839e818fc2f0fd1986ace4e0f3aa"},
    {"profileId":"1920x1080_ui135","sourceGroupDenominator":8,"semanticMemberDenominator":90,"probeDenominator":35,"dashEventCount":23,"routeSpanCount":1,"orderedParentSourceDigest":"c6818f25d3da0db33e8fb39a7f0901337be0b858cd6acfba9d93f6d12d1ac186","orderedProbeDigest":"00c2c0055e3aef5e7d85f5eddd2620abf808410b3b049792b26cfc8658d949ec","orderedDashEventDigest":"16d190923fee2896a9a68690ba1b7d94ff293c9c45b3823b251901f55213a65c","orderedDashContributorDigest":"4372f48b8cd588625e4f6a545fcbf07ba86043eb68a1b58ace21b94556a2c49e","orderedSourceRouteIntervalDigest":"26d619f7bc4f2e35884508273a8a76895cf50db1fef97d67e457d88c76a112e7"},
    {"profileId":"1280x720_ui100","sourceGroupDenominator":8,"semanticMemberDenominator":54,"probeDenominator":23,"dashEventCount":11,"routeSpanCount":1,"orderedParentSourceDigest":"c6818f25d3da0db33e8fb39a7f0901337be0b858cd6acfba9d93f6d12d1ac186","orderedProbeDigest":"78dcf2aa7f4cf259dbbd6ce6517fb0ed50d42cf3e7060a809d3f8943a3b327b0","orderedDashEventDigest":"852636e235f01a60bdf148efe89e7eabc3ad7609af76323d4a82b982508c479c","orderedDashContributorDigest":"9e38595b53554dc12837b67f0e7cd7d201e95448c1bd4d21b437827e7fd280cf","orderedSourceRouteIntervalDigest":"610365e6deeacb0dd83d1c928d698ccf0dce50856cbe8413a54c233f588e64ce"},
    {"profileId":"1280x720_ui135","sourceGroupDenominator":8,"semanticMemberDenominator":69,"probeDenominator":28,"dashEventCount":16,"routeSpanCount":1,"orderedParentSourceDigest":"c6818f25d3da0db33e8fb39a7f0901337be0b858cd6acfba9d93f6d12d1ac186","orderedProbeDigest":"86bbf305a7f3b170e76445443c13c117d0c65ad0125f04eb9903af99ba492707","orderedDashEventDigest":"3ba15c995c63ff46b44df044e197bdd0b194fa86c3b273d687ada1775c4f305b","orderedDashContributorDigest":"d028e45f97a9b1c888e09f750ab6e6d8dc7e9bd9fcde22448cf46d2de053ab3e","orderedSourceRouteIntervalDigest":"064302def27dbfc76ea763f10965c5defacc358d67e5cb63052af5d4bdffa70b"},
]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_aggregate(bindings: dict[str, tuple[str, str]]) -> str:
    digest = hashlib.sha256()
    for role in SOURCE_ROLES:
        rel = bindings[role][0]
        data = (ROOT / rel).read_bytes()
        encoded = rel.encode()
        digest.update(len(encoded).to_bytes(8, "big"))
        digest.update(encoded)
        digest.update(len(data).to_bytes(8, "big"))
        digest.update(data)
    return digest.hexdigest()


def validate(receipt_path: Path, *, replay: bool = True) -> list[str]:
    errors: list[str] = []
    try:
        receipt = json.loads(receipt_path.read_text())
    except Exception as exc:
        return [f"receipt_parse_failed:{exc}"]
    if receipt.get("schema") != "billiardgl.phase4.v19.cp1_trajectory_inset_foundation_implementation_receipt.v1":
        errors.append("schema_invalid")
    actual = {}
    for row in receipt.get("frozen_bindings", []):
        role = row.get("role")
        if role in actual:
            errors.append(f"duplicate_binding:{role}")
        actual[role] = (row.get("path"), row.get("sha256"))
    if set(actual) != set(EXPECTED_BINDINGS):
        errors.append("binding_roles_invalid")
    for role, expected in EXPECTED_BINDINGS.items():
        if actual.get(role) is None or actual[role][0] != expected[0] or (
            expected[1] is not None and actual[role][1] != expected[1]):
            errors.append(f"binding_identity_invalid:{role}")
            continue
        path = ROOT / expected[0]
        receipt_sha = actual[role][1]
        if not path.is_file() or sha256(path) != receipt_sha:
            errors.append(f"binding_bytes_invalid:{role}")
    aggregate = receipt.get("source_aggregate", {})
    if aggregate.get("ordered_roles") != SOURCE_ROLES or aggregate.get("sha256") != EXPECTED_AGGREGATE:
        errors.append("source_aggregate_receipt_invalid")
    if source_aggregate(EXPECTED_BINDINGS) != EXPECTED_AGGREGATE:
        errors.append("source_aggregate_current_invalid")
    replay_contract = receipt.get("compile_replay", {})
    expected_args = ["-std=c++17", "-O2", "-Isrc/Billiards", "tests/phase4_v19_trajectory_inset_cpu_tests.cpp", "src/Billiards/training/trajectory_inset_model.cpp", "src/Billiards/training/trajectory_inset_renderer.cpp", "src/Billiards/training/trajectory_overlay_renderer.cpp", "src/Billiards/deterministic_hash.cpp", "src/Billiards/automation_json.cpp"]
    if replay_contract.get("compiler") != "/usr/bin/clang++" or replay_contract.get("arguments") != expected_args or replay_contract.get("expected_exit_code") != 0 or replay_contract.get("gpu_or_window_context_created") is not False:
        errors.append("compile_contract_invalid")
    verified = receipt.get("verified_contract", {})
    required_verified = {
        "exact_case_count": 96,
        "route_wide_event_membership": True,
        "exact_ieee754_contributor_digest": True,
        "exact_source_route_interval_digest": True,
        "ui135_arc0_arc1_residual_px": 0.7779527559055168,
        "gap_only_source_preserved": True,
        "visibility_discontinuity_resets_route_span": True,
        "bounded_overlap_later_endpoint_ownership": True,
        "standard_cvd_topology_ownership_probe_parity": True,
        "repeated_raster_byte_identical": True,
    }
    for key, value in required_verified.items():
        if verified.get(key) != value:
            errors.append(f"verified_contract_invalid:{key}")
    if verified.get("predicted_endpoint_bands_l1_px") != [[4.5, 3.5], [3.5, 2.5]] or verified.get("actual_capsule_tangent_by_normal_px") != [7.0, 2.5] or verified.get("actual_capsule_offsets_px") != [-7.75, 7.75] or verified.get("edge_tail_interval_px") != [550.0, 552.0]:
        errors.append("endpoint_contract_invalid")
    auth = receipt.get("authorization_boundary", {})
    for key in ("cp1_accepted", "cp2_started", "cp3_started", "cmake_modified_by_cp1", "gpu_run", "readiness_created", "authorization_created", "billiards_executed", "product_or_machine_pass_claimed"):
        if auth.get(key) is not False:
            errors.append(f"false_authority:{key}")
    if receipt.get("canonical_layout_inventory") != EXPECTED_INVENTORY:
        errors.append("canonical_layout_inventory_invalid")
    if replay and not errors:
        with tempfile.TemporaryDirectory(prefix="phase4-v19-cp1-") as temp:
            binary = Path(temp) / "phase4_v19_cp1"
            command = [replay_contract["compiler"], *replay_contract["arguments"], "-o", str(binary)]
            compile_result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
            if compile_result.returncode != 0:
                errors.append("fresh_compile_failed:" + compile_result.stderr[-500:])
            else:
                run_result = subprocess.run([str(binary)], cwd=ROOT, text=True, capture_output=True)
                if run_result.returncode != 0 or run_result.stdout != EXPECTED_STDOUT or run_result.stderr:
                    errors.append("fresh_replay_failed")
                inventory_result = subprocess.run([str(binary), "--emit-inventory-json"], cwd=ROOT, text=True, capture_output=True)
                try:
                    inventory = json.loads(inventory_result.stdout)
                except Exception:
                    inventory = {}
                if inventory_result.returncode != 0 or inventory_result.stderr or inventory.get("schema") != "billiardgl.phase4.v19.cp1_layout_inventory.v1" or inventory.get("layouts") != EXPECTED_INVENTORY:
                    errors.append("fresh_inventory_replay_failed")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--receipt", type=Path, default=DEFAULT_RECEIPT)
    parser.add_argument("--no-replay", action="store_true")
    args = parser.parse_args()
    receipt = args.receipt if args.receipt.is_absolute() else ROOT / args.receipt
    errors = validate(receipt, replay=not args.no_replay)
    print(json.dumps({"valid": not errors, "errors": errors}, sort_keys=True))
    return 0 if not errors else 1


if __name__ == "__main__":
    raise SystemExit(main())
