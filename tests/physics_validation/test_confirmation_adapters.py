import json
import unittest
from pathlib import Path

from tools.physics_validation.confirmation_adapters import (
    ConfirmationAdapter,
    ConfirmationEvaluation,
    base_scenario,
    confirmation_adapter,
    register_confirmation_adapter,
)


ROOT = Path(__file__).resolve().parents[2]
PROFILE = json.loads((
    ROOT / "physics_models/profiles/chinese_pool_full_game_v3.json"
).read_text(encoding="utf-8"))["runtime_profile"]
BALLS = [{
    "angular_velocity_rad_s": [0.0, 0.0, 0.0],
    "index": 0,
    "pocketed": False,
    "position_cm": [0.0, 89.34147644042969, 0.0],
    "velocity_cm_s": [0.0, 0.0, 0.0],
}]


def _scenarios(profile, package):
    return {}


def _evaluate(traces, profile, package):
    return ConfirmationEvaluation((), {})


class ConfirmationAdapterTests(unittest.TestCase):
    def test_base_scenario_has_real_expectations(self):
        scenario = base_scenario(
            PROFILE, "fixture", BALLS, "unbounded", 2,
            "contract fixture")
        self.assertEqual(scenario["expectations"], [
            {"metric": "finite_state", "operator": "eq", "value": True},
            {
                "metric": "nonincreasing_translational_energy",
                "operator": "eq",
                "value": True,
            },
        ])

    def test_base_scenario_selects_schema_12_only_for_frozen_contact(self):
        profile = json.loads(json.dumps(PROFILE))
        profile["frozen_cue_contact"] = {"normal_stiffness_n_per_m32": 1.0}
        self.assertEqual(base_scenario(
            PROFILE, "v4", BALLS, "unbounded", 2, "v4 fixture"
        )["schema_version"], 11)
        self.assertEqual(base_scenario(
            profile, "v5", BALLS, "unbounded", 2, "v5 fixture"
        )["schema_version"], 12)

    def test_unknown_adapter_fails_closed(self):
        with self.assertRaisesRegex(
                ValueError, "unsupported confirmation package: unknown"):
            confirmation_adapter("unknown")

    def test_duplicate_adapter_registration_fails(self):
        adapter = ConfirmationAdapter(
            "fixture_unique_adapter", _scenarios, _evaluate)
        register_confirmation_adapter(adapter)
        with self.assertRaisesRegex(
                ValueError, "duplicate confirmation adapter"):
            register_confirmation_adapter(adapter)


if __name__ == "__main__":
    unittest.main()
