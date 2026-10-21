from __future__ import annotations

import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from scripts.generate_phase5_product_ux import generate, manifest, profiles
from scripts.validate_phase5_product_ux import ValidationError, validate_manifest, validate_package


ROOT = Path(__file__).resolve().parents[2]
PACKAGE = ROOT / "assets" / "modern" / "product_ux" / "product_ux_v1.json"


def json_tree_digest(root: Path) -> str:
    result = hashlib.sha256()
    for path in sorted(root.rglob("*")):
        if path.is_file():
            result.update(path.relative_to(root).as_posix().encode())
            result.update(b"\0")
            result.update(path.read_bytes())
    return result.hexdigest()


class Phase5ProductUxTests(unittest.TestCase):
    def setUp(self) -> None:
        self.value = json.loads(PACKAGE.read_text(encoding="utf-8"))

    def assert_error(self, value: dict, code: str) -> None:
        with self.assertRaises(ValidationError) as raised:
            validate_manifest(value)
        self.assertEqual(raised.exception.code, code)

    def test_repository_package_and_bad_inputs_validate(self) -> None:
        self.assertEqual(validate_package(PACKAGE), {"ai_tiers": 3, "bad_inputs": 6, "mockups": 3, "reasons": 9})

    def test_generation_is_byte_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as first, tempfile.TemporaryDirectory() as second:
            generate(Path(first)); generate(Path(second))
            self.assertEqual(json_tree_digest(Path(first)), json_tree_digest(Path(second)))

    def test_committed_package_matches_fresh_generation(self) -> None:
        with tempfile.TemporaryDirectory() as generated:
            generate(Path(generated))
            committed = PACKAGE.parent
            generated_files = {p.relative_to(Path(generated)) for p in Path(generated).rglob("*") if p.is_file()}
            for relative in generated_files:
                self.assertEqual((Path(generated) / relative).read_bytes(), (committed / relative).read_bytes(), relative)

    def test_reason_codes_match_cpp_source(self) -> None:
        source = (ROOT / "src" / "Billiards" / "rules" / "chinese_eight_ball_rules.cpp").read_text(encoding="utf-8")
        for item in self.value["reason_messages"]:
            self.assertIn(f'return "{item["code"]}";', source)
            self.assertIn(f'return "{item["text_key"]}";', source)

    def test_visible_reasons_are_capability_gated(self) -> None:
        for item in self.value["reason_messages"]:
            if item["code"] != "none":
                self.assertEqual(item["runtime_binding"]["status"], "capability_gated")

    def test_reason_never_relies_on_color_alone(self) -> None:
        for item in self.value["reason_messages"]:
            if item["code"] != "none":
                self.assertTrue(item["presentation"]["icon_and_text_required"])
                self.assertNotEqual(item["icon"]["silhouette"], "color_only")
                self.assertTrue(item["icon"]["non_color_prefix"].startswith("["))

    def test_ai_ranges_are_ordered_and_measurable(self) -> None:
        tiers = self.value["ai_difficulty"]
        self.assertLess(tiers[0]["target_success_rate"]["maximum"], tiers[1]["target_success_rate"]["minimum"])
        self.assertLess(tiers[1]["target_success_rate"]["maximum"], tiers[2]["target_success_rate"]["minimum"])
        self.assertTrue(all(item["evaluation"]["attempts"] == 600 for item in tiers))

    def test_keyboard_and_mouse_actions_have_no_default_conflicts(self) -> None:
        validate_manifest(self.value)
        devices = {binding["device"] for action in self.value["input_rebinding"]["actions"] for binding in action["defaults"]}
        self.assertEqual(devices, {"keyboard", "mouse"})

    def test_accessibility_contract_has_reflow_and_non_speech_captions(self) -> None:
        accessibility = self.value["accessibility"]
        self.assertLessEqual(accessibility["font_scale"]["reflow_required_above"], 1.15)
        self.assertTrue(accessibility["subtitles"]["non_speech_captions"])
        self.assertEqual(accessibility["assist_invariant"], "never_auto_aim_or_auto_shot")

    def test_color_only_mutation_fails(self) -> None:
        self.value["reason_messages"][1]["presentation"]["icon_and_text_required"] = False
        self.assert_error(self.value, "E_COLOR_ONLY_REASON")

    def test_binding_conflict_mutation_fails(self) -> None:
        target = next(item for item in self.value["input_rebinding"]["actions"] if item["id"] == "toggle_aim")
        target["context"] = "global"
        target["defaults"] = [{"control": "H", "device": "keyboard", "status": "runtime_existing", "trigger": "press"}]
        self.assert_error(self.value, "E_DEFAULT_BINDING_CONFLICT")

    def test_safe_area_mutation_fails(self) -> None:
        self.value["responsive_profiles"]["minimum_1280x720_96dpi"]["regions_px"]["content"][0] = 1200
        self.assert_error(self.value, "E_RESPONSIVE_SAFE_AREA")


if __name__ == "__main__":
    unittest.main()
