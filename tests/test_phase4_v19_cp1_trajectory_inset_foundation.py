#!/usr/bin/env python3
"""Receipt-level mutations for V19 CP1 trajectory inset foundation."""

from __future__ import annotations

import copy
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VALIDATOR_PATH = ROOT / "scripts/validate_phase4_v19_cp1_trajectory_inset_foundation.py"
SPEC = importlib.util.spec_from_file_location("v19_cp1_validator", VALIDATOR_PATH)
VALIDATOR = importlib.util.module_from_spec(SPEC)
assert SPEC.loader
SPEC.loader.exec_module(VALIDATOR)
RECEIPT = ROOT / "docs/evidence/phase4-v19-cp1-trajectory-inset-foundation-implementation-v1/receipt.json"


class Cp1ReceiptTests(unittest.TestCase):
    def setUp(self) -> None:
        self.value = json.loads(RECEIPT.read_text())

    def reject(self, mutation) -> None:
        value = copy.deepcopy(self.value)
        mutation(value)
        with tempfile.TemporaryDirectory(prefix="v19-cp1-mutation-") as temp:
            path = Path(temp) / "receipt.json"
            path.write_text(json.dumps(value))
            self.assertTrue(VALIDATOR.validate(path, replay=False))

    def test_canonical_passes_without_replay(self) -> None:
        self.assertEqual(VALIDATOR.validate(RECEIPT, replay=False), [])

    def test_fail_closed_mutations(self) -> None:
        mutations = [
            lambda v: v.update(schema="wrong"),
            lambda v: v["frozen_bindings"].pop(),
            lambda v: v["frozen_bindings"].append(copy.deepcopy(v["frozen_bindings"][0])),
            lambda v: v["frozen_bindings"][0].update(path="docs/art/phase4-v19-landscape-orientation-layout-interpretation-correction-gate-v1.json", sha256="cffc93f29bb24f7f9378dee3c17fffd1da1c139745eec17bdb380cf6bda04d13"),
            lambda v: v["source_aggregate"].update(sha256="0" * 64),
            lambda v: v["source_aggregate"]["ordered_roles"].reverse(),
            lambda v: v["compile_replay"].update(compiler="clang++"),
            lambda v: v["compile_replay"]["arguments"].pop(),
            lambda v: v["compile_replay"].update(gpu_or_window_context_created=True),
            lambda v: v["verified_contract"].update(exact_case_count=95),
            lambda v: v["verified_contract"].update(route_wide_event_membership=False),
            lambda v: v["verified_contract"].update(exact_ieee754_contributor_digest=False),
            lambda v: v["verified_contract"].update(exact_source_route_interval_digest=False),
            lambda v: v["verified_contract"].update(ui135_arc0_arc1_residual_px=0.8),
            lambda v: v["verified_contract"].update(gap_only_source_preserved=False),
            lambda v: v["verified_contract"].update(visibility_discontinuity_resets_route_span=False),
            lambda v: v["verified_contract"].update(predicted_endpoint_bands_l1_px=[[4.5, 3.5], [3.5, 2.4]]),
            lambda v: v["verified_contract"].update(actual_capsule_tangent_by_normal_px=[7.0, 3.0]),
            lambda v: v["verified_contract"].update(actual_capsule_offsets_px=[-7.75, 7.5]),
            lambda v: v["verified_contract"].update(edge_tail_interval_px=[550.0, 551.5]),
            lambda v: v["verified_contract"].update(bounded_overlap_later_endpoint_ownership=False),
            lambda v: v["verified_contract"].update(standard_cvd_topology_ownership_probe_parity=False),
            lambda v: v["verified_contract"].update(repeated_raster_byte_identical=False),
            lambda v: v["authorization_boundary"].update(cp1_accepted=True),
            lambda v: v["authorization_boundary"].update(cp2_started=True),
            lambda v: v["authorization_boundary"].update(cp3_started=True),
            lambda v: v["authorization_boundary"].update(cmake_modified_by_cp1=True),
            lambda v: v["authorization_boundary"].update(gpu_run=True),
            lambda v: v["authorization_boundary"].update(readiness_created=True),
            lambda v: v["authorization_boundary"].update(authorization_created=True),
            lambda v: v["authorization_boundary"].update(billiards_executed=True),
            lambda v: v["authorization_boundary"].update(product_or_machine_pass_claimed=True),
        ]
        inventory_fields = ["sourceGroupDenominator", "semanticMemberDenominator",
            "probeDenominator", "dashEventCount", "routeSpanCount"]
        digest_fields = ["orderedParentSourceDigest", "orderedProbeDigest",
            "orderedDashEventDigest", "orderedDashContributorDigest",
            "orderedSourceRouteIntervalDigest"]
        for row in range(4):
            for field in inventory_fields:
                mutations.append(lambda v, r=row, f=field:
                    v["canonical_layout_inventory"][r].__setitem__(f,
                        v["canonical_layout_inventory"][r][f] + 1))
            for field in digest_fields:
                mutations.append(lambda v, r=row, f=field:
                    v["canonical_layout_inventory"][r].__setitem__(f, "0" * 64))
        for index, mutation in enumerate(mutations):
            with self.subTest(index=index):
                self.reject(mutation)


if __name__ == "__main__":
    unittest.main()
