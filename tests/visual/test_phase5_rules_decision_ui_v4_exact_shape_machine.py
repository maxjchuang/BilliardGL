from __future__ import annotations

import copy
import json
from pathlib import Path
import tempfile
import unittest

from scripts import run_phase5_rules_decision_ui_v4_exact_shape_machine as runner
from scripts import validate_phase5_rules_decision_ui_v4_exact_shape_machine as validator


class Phase5RulesDecisionUiV4ExactShapeMachineTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.temp = tempfile.TemporaryDirectory()
        cls.first = Path(cls.temp.name) / "first.json"
        cls.second = Path(cls.temp.name) / "second.json"
        runner.run(cls.first)
        runner.run(cls.second)
        cls.value = json.loads(cls.first.read_text(encoding="utf-8"))

    @classmethod
    def tearDownClass(cls) -> None:
        cls.temp.cleanup()

    def reject(self, mutation) -> None:
        value = copy.deepcopy(self.value)
        mutation(value)
        with self.assertRaises(RuntimeError):
            validator.validate(value)

    def test_540_cases_pass_and_two_clean_runs_are_byte_identical(self) -> None:
        result = validator.validate(self.value)
        self.assertEqual(result["case_count"], 540)
        self.assertEqual(result["affected_shape_cases"], 36)
        self.assertEqual(self.first.read_bytes(), self.second.read_bytes())

    def test_three_exact_tokens_are_distinct(self) -> None:
        self.assertEqual(len(set(self.value["v4_tokens"].values())), 3)
        self.assertEqual(self.value["v4_tokens"], runner.TOKENS)

    def test_actor_action_copy_is_frozen(self) -> None:
        self.assertEqual(self.value["copy_freeze"], runner.COPY_FREEZE)
        self.reject(lambda value: value["copy_freeze"].update(
            {"confirm_en": "Confirm"}))

    def test_bare_or_open_shape_mutations_fail_closed(self) -> None:
        affected = next(index for index, case in enumerate(self.value["cases"])
                        if case["v4_exact_shape_affected"])
        mutations = (
            ("selected_closed_diamond", False),
            ("selected_check_enclosed", False),
            ("confirm_closed_rectangular_frame_count", 1),
            ("confirm_bent_enter_arrow", False),
            ("focus_joined_diagonal_stroke_count", 1),
            ("token_aliasing", True),
        )
        for field, bad in mutations:
            with self.subTest(field=field):
                self.reject(lambda value, key=field, replacement=bad:
                    value["cases"][affected]["v4_exact_shape_contract"].update(
                        {key: replacement}))

    def test_undersized_bbox_or_stroke_fails_closed(self) -> None:
        affected = next(index for index, case in enumerate(self.value["cases"])
                        if case["v4_exact_shape_affected"])
        self.reject(lambda value: value["cases"][affected][
            "v4_exact_shape_contract"]["physical_bounding_boxes_px"].update(
                {"focus": [17.99, 24.0]}))
        self.reject(lambda value: value["cases"][affected][
            "v4_exact_shape_contract"]["physical_strokes_px"].update(
                {"confirm_enter_arrow": 1.99}))

    def test_token_leakage_into_control_state_fails_closed(self) -> None:
        control = next(index for index, case in enumerate(self.value["cases"])
                       if not case["v4_exact_shape_affected"])
        self.reject(lambda value: value["cases"][control].update(
            {"v4_token_leakage": True}))

    def test_gpu_auth_product_ta_or_promotion_claim_fails(self) -> None:
        for field in self.value["v4_evidence_boundary"]:
            with self.subTest(field=field):
                self.reject(lambda value, key=field:
                    value["v4_evidence_boundary"].update({key: True}))


if __name__ == "__main__":
    unittest.main()
