from __future__ import annotations

import hashlib
import json
import math
import tempfile
import unittest
from pathlib import Path

from scripts.generate_phase4_training_pack import (
    build_scenarios, build_scenarios_v2, canonical_physics_bytes, generate)
from scripts.calibrate_phase4_reference_replays import teaching_range_universe
from scripts.validate_phase4_training_pack import ValidationError, validate_pack, validate_scenario


ROOT = Path(__file__).resolve().parents[2]
PACK = ROOT / "assets" / "training" / "phase4" / "training_pack_v1.json"


def tree_digest(root: Path) -> str:
    digest = hashlib.sha256()
    for path in sorted(item for item in root.rglob("*.json")
                       if item.is_file() and "calibration" not in item.parts):
        digest.update(path.relative_to(root).as_posix().encode())
        digest.update(b"\0")
        digest.update(path.read_bytes())
    return digest.hexdigest()


class Phase4TrainingPackTests(unittest.TestCase):
    def assert_error(self, value: dict, code: str) -> None:
        with self.assertRaises(ValidationError) as raised:
            validate_scenario(value)
        self.assertEqual(raised.exception.code, code)

    def test_repository_pack_validates_with_all_bad_inputs(self) -> None:
        result = validate_pack(PACK)
        self.assertEqual(result["scenarios"], 12)
        self.assertEqual(result["bad_inputs_verified"], 5)

    def test_reference_generation_binds_the_shipping_build(self) -> None:
        pack = json.loads(PACK.read_text())
        entry = pack["reference_generation"]
        generation_path = PACK.parent / entry["file"]
        self.assertEqual(hashlib.sha256(generation_path.read_bytes()).hexdigest(),
                         entry["sha256"])
        generation = json.loads(generation_path.read_text())
        self.assertEqual(generation["build_type"], "Release")
        self.assertEqual(generation["compiler_flags"], "-O3 -DNDEBUG")
        self.assertEqual(generation["scenario_count"], 12)
        self.assertEqual(generation["portability"],
            "terminal_hash_verified_only_with_matching_build_configuration")

    def test_generator_is_byte_deterministic(self) -> None:
        with tempfile.TemporaryDirectory() as first, tempfile.TemporaryDirectory() as second:
            generate(Path(first))
            generate(Path(second))
            self.assertEqual(tree_digest(Path(first)), tree_digest(Path(second)))

    def test_committed_package_matches_fresh_generation(self) -> None:
        with tempfile.TemporaryDirectory() as generated:
            generate(Path(generated))
            self.assertEqual(tree_digest(Path(generated)), tree_digest(PACK.parent))

    def test_exact_category_coverage(self) -> None:
        scenarios = build_scenarios()
        actual = {category: sum(item["category"] == category for item in scenarios) for category in {
            "stun_follow_draw", "side_spin_cushion", "pocket_angle", "continuous_position", "break_cue_control"
        }}
        self.assertEqual(actual, {
            "stun_follow_draw": 3, "side_spin_cushion": 3, "pocket_angle": 2,
            "continuous_position": 2, "break_cue_control": 2,
        })

    def test_all_content_is_declared_original(self) -> None:
        for item in build_scenarios():
            self.assertEqual(item["authorship"]["asset_policy"], "original_billiardgl_content_no_third_party_assets")

    def test_spin_lessons_are_locked_behind_authoritative_capabilities(self) -> None:
        gated = [item for item in build_scenarios() if item["allowed_controls"]["tip_offset"]["enabled"]]
        self.assertEqual(len(gated), 5)
        for item in gated:
            self.assertFalse(item["runtime_gate"]["production_default_enabled"])
            self.assertGreater(len(item["runtime_gate"]["required_capabilities"]), 1)

    def test_unpromoted_cue_elevation_and_chalk_are_forbidden(self) -> None:
        for item in build_scenarios():
            forbidden = item["allowed_controls"]["explicitly_forbidden"]
            self.assertIn("cue_elevation", forbidden)
            self.assertIn("chalk", forbidden)

    def test_overlap_mutation_fails(self) -> None:
        value = build_scenarios()[0]
        value["initial_scenario"]["scenario"]["balls"][1]["position_cm"] = value["initial_scenario"]["scenario"]["balls"][0]["position_cm"]
        initial = value["initial_scenario"]["scenario"]
        canonical = canonical_physics_bytes(initial)
        value["initial_scenario"]["canonical_sha256"] = hashlib.sha256(canonical).hexdigest()
        value["reference_replay"]["binding_rule"]["scenario_sha256_must_equal"] = value["initial_scenario"]["canonical_sha256"]
        self.assert_error(value, "E_GEOMETRY_OVERLAP")

    def test_spin_gate_mutation_fails(self) -> None:
        value = build_scenarios()[1]
        value["runtime_gate"]["required_capabilities"] = ["training_scenario_v1"]
        value["runtime_gate"]["production_default_enabled"] = True
        self.assert_error(value, "E_SPIN_CAPABILITY_GATE")

    def test_nondeterministic_score_mutation_fails(self) -> None:
        value = build_scenarios()[0]
        value["scoring"]["tie_break"] = ["wall_clock"]
        self.assert_error(value, "E_SCORING_DETERMINISM")

    def test_bound_replay_without_required_hash_fails(self) -> None:
        value = build_scenarios()[0]
        del value["reference_replay"]["binding_rule"]["trace_sha256"]
        self.assert_error(value, "E_REPLAY_BINDING")

    def test_all_reference_replays_are_automatically_verified_and_bound(self) -> None:
        for item in build_scenarios():
            replay = item["reference_replay"]
            binding = replay["binding_rule"]
            self.assertEqual(replay["status"], "bound")
            self.assertEqual(binding["review_status"], "automated_verified")
            self.assertEqual(binding["physics_profile_id"],
                             "chinese_pool_interactive_120hz_v5")
            self.assertEqual(len(binding["action_log_sha256"]), 64)
            self.assertEqual(len(binding["trace_sha256"]), 64)

    def test_embedded_scenarios_bind_exact_shipping_profile(self) -> None:
        for item in build_scenarios():
            self.assertNotIn("physics_profile", item)
            initial = item["initial_scenario"]
            scenario = initial["scenario"]
            self.assertEqual(scenario["schema_version"], 12)
            self.assertEqual(scenario["boundary_mode"], "production_table")
            self.assertEqual(scenario["simulation"]["time_step_seconds"], 1.0 / 120.0)
            self.assertEqual(scenario["physics_profile"]["id"],
                             "chinese_pool_interactive_120hz_v5")
            self.assertEqual(initial["physics_profile_sha256"],
                hashlib.sha256((json.dumps(scenario["physics_profile"],
                    ensure_ascii=False, sort_keys=True,
                    separators=(",", ":")) + "\n").encode()).hexdigest())

    def test_v2_catalog_replaces_every_legacy_lesson(self) -> None:
        legacy = build_scenarios()
        revised = build_scenarios_v2()
        self.assertEqual(len(legacy), 12)
        self.assertEqual(len(revised), 13)
        self.assertEqual([item["id"] for item in revised], [
            "launch_straight_pot_intro_v2",
            "launch_straight_stop_intro_v2",
            "launch_straight_follow_intro_v2",
            "launch_straight_draw_intro_v2",
            "launch_left_english_one_rail_v2",
            "launch_right_english_one_rail_v2",
            "launch_side_english_two_rail_v2",
            "launch_corner_pocket_angle_v2",
            "launch_side_pocket_angle_v2",
            "launch_two_ball_lane_route_v2",
            "launch_three_ball_corner_route_v2",
            "launch_head_ball_break_control_v2",
            "launch_cut_break_control_v2",
        ])
        self.assertTrue(all(item["version"] == 2 for item in revised))
        self.assertEqual(legacy[0]["id"], "launch_stop_shot_center_v1")
        self.assertEqual(legacy[0]["version"], 1)

    def test_head_break_is_beginner_with_optional_ring(self) -> None:
        lesson = next(s for s in build_scenarios_v2() if s["id"] == "launch_head_ball_break_control_v2")
        validate_scenario(lesson)
        goals = lesson["success"]["all"]
        self.assertFalse(any(g["predicate"] == "ball_final_position_in_zone" for g in goals))
        self.assertEqual(next(g["minimum"] for g in goals if g["predicate"] == "distinct_object_balls_with_rail_contact_gte"), 1)
        self.assertEqual(lesson["scoring"]["optional_bonus_zone"]["ball_index"], 0)
        self.assertEqual(lesson["scoring"]["deductions"][0]["cap_points"], 20)
        power = lesson["teaching_hints"]["recommended_power"]
        self.assertGreaterEqual(power["maximum_percent"] - power["minimum_percent"], 15)

    def test_v2_beginner_geometry_and_teaching_ranges_are_explicit(self) -> None:
        pot, stop, follow, draw = build_scenarios_v2()[:4]
        for item in (pot, stop, follow, draw):
            validate_scenario(item)
            self.assertEqual(item["version"], 2)
            balls = item["initial_scenario"]["scenario"]["balls"]
            self.assertEqual([ball["index"] for ball in balls], [0, 1])
            self.assertEqual(balls[0]["position_cm"], [10.0, 89.341476, 0.0])
            self.assertEqual(balls[1]["position_cm"], [40.0, 89.341476, 0.0])
            hints = item["teaching_hints"]
            self.assertEqual(hints["target_pocket_id"], "side_px")
            self.assertEqual(hints["target_pocket_center_cm"], [63.5, 0.0])
            self.assertEqual(hints["shot_line"]["cue_ball_index"], 0)
            self.assertEqual(hints["shot_line"]["object_ball_index"], 1)
        self.assertEqual(pot["teaching_hints"]["recommended_tip"]["label"],
                         "center_ball")
        self.assertEqual(stop["teaching_hints"]["recommended_tip"]["label"],
                         "center_ball")
        self.assertEqual(follow["teaching_hints"]["recommended_tip"]["label"],
                         "high_tip")
        self.assertEqual(draw["teaching_hints"]["recommended_tip"]["label"],
                         "low_tip")
        self.assertEqual(pot["teaching_hints"]["recommended_power"], {
            "minimum_percent": 22.5, "default_percent": 27.5,
            "maximum_percent": 37.5})
        self.assertEqual(stop["teaching_hints"]["recommended_power"], {
            "minimum_percent": 22.5, "default_percent": 25.0,
            "maximum_percent": 27.5})
        self.assertEqual(stop["teaching_hints"]["cue_ball_target_zone"], {
            "center_cm": [40.0 - 2.0 * 2.8575, 0.0], "radius_cm": 4.5,
            "visible": True})
        self.assertEqual(stop["allowed_controls"]["tip_offset"], {
            "axis": "vertical", "enabled": True,
            "preferred_radius_units": [0.0, 0.0]})
        dynamic_stop = next(condition for condition in stop["success"]["all"]
                            if condition["predicate"] ==
                            "cue_ball_final_near_first_contact_point")
        self.assertEqual(dynamic_stop, {
            "predicate": "cue_ball_final_near_first_contact_point",
            "radius_cm": 4.5})
        self.assertFalse(
            pot["teaching_hints"]["cue_ball_target_zone"]["visible"])
        self.assertEqual(follow["teaching_hints"]["recommended_power"], {
            "minimum_percent": 27.5, "default_percent": 32.5,
            "maximum_percent": 35.0})
        self.assertEqual(follow["allowed_controls"]["tip_offset"], {
            "axis": "vertical", "enabled": True,
            "preferred_radius_units": [0.0, 0.25]})
        self.assertEqual(follow["teaching_hints"]["cue_ball_target_zone"], {
            "center_cm": [42.0, 0.0], "radius_cm": 5.5,
            "visible": True})
        self.assertIn({"predicate": "rail_contact_count_lte",
                       "ball_index": 0, "maximum": 0},
                      follow["success"]["all"])
        self.assertGreater(42.0 - 5.5, 40.0 - 2.0 * 2.8575)
        self.assertEqual(draw["teaching_hints"]["recommended_power"], {
            "minimum_percent": 47.5, "default_percent": 50.0,
            "maximum_percent": 50.0})
        self.assertEqual(draw["allowed_controls"]["tip_offset"], {
            "axis": "vertical", "enabled": True,
            "preferred_radius_units": [0.0, -0.5]})
        self.assertEqual(draw["teaching_hints"]["cue_ball_target_zone"], {
            "center_cm": [28.0, 0.0], "radius_cm": 5.75,
            "visible": True})
        self.assertIn({"predicate": "rail_contact_count_lte",
                       "ball_index": 0, "maximum": 0},
                      draw["success"]["all"])
        # The entire draw target is behind the theoretical cue-ball centre at
        # first contact, so a stationary cue ball at contact cannot pass it.
        self.assertLess(28.0 + 5.75, 40.0 - 2.0 * 2.8575)

    def test_teaching_range_lattice_covers_endpoints_and_center(self) -> None:
        pot, stop, follow, draw = build_scenarios_v2()[:4]
        pot_grid = list(teaching_range_universe(pot, 1000))
        stop_grid = list(teaching_range_universe(stop, 1000))
        follow_grid = list(teaching_range_universe(follow, 1000))
        draw_grid = list(teaching_range_universe(draw, 1000))
        self.assertEqual(len(pot_grid), 35)
        self.assertEqual(len(stop_grid), 15)
        self.assertEqual(sorted({item[1] for item in pot_grid}),
                         [45.0, 50.0, 55.0, 60.0, 65.0, 70.0, 75.0])
        self.assertEqual(sorted({item[1] for item in stop_grid}),
                         [45.0, 50.0, 55.0])
        self.assertEqual(len(follow_grid), 20)
        self.assertEqual(sorted({item[1] for item in follow_grid}),
                         [55.0, 60.0, 65.0, 70.0])
        self.assertEqual({(item[2], item[3]) for item in follow_grid},
                         {(0.0, 0.25)})
        self.assertEqual(len(draw_grid), 15)
        self.assertEqual(sorted({item[1] for item in draw_grid}),
                         [95.0, 97.5, 100.0])
        self.assertEqual({(item[2], item[3]) for item in draw_grid},
                         {(0.0, -0.5)})
        for item, grid in ((pot, pot_grid), (stop, stop_grid),
                           (follow, follow_grid), (draw, draw_grid)):
            center = item["teaching_hints"]["aim"]["yaw_radians"]
            tolerance = math.radians(
                item["teaching_hints"]["aim"]["tolerance_degrees"])
            self.assertAlmostEqual(min(row[0] for row in grid),
                                   center - tolerance)
            self.assertAlmostEqual(max(row[0] for row in grid),
                                   center + tolerance)

    def test_draw_calibration_proves_real_rearward_motion(self) -> None:
        report = json.loads((PACK.parent / "calibration" /
            "launch_straight_draw_intro_v2.range.json").read_text())
        replay = json.loads((PACK.parent / "replays_v2" /
            "launch_straight_draw_intro_v2.replay.json").read_text())
        tip_action = next(action for action in replay["semantic_actions"]
                          if action["type"] == "set_tip_offset")
        self.assertLess(tip_action["second"], 0.0)
        self.assertEqual(report["attempt_count"], 15)
        self.assertTrue(report["all_candidates_succeeded"])
        for attempt in report["attempts"]:
            self.assertLess(attempt["actions"]["tip_y"], 0.0)
            self.assertEqual(attempt["facts"]["rail_contacts"][0], 0)
            self.assertIn(1, attempt["facts"]["pocket_order"])
            contact = attempt["facts"]["cue_ball_position_at_first_contact_cm"]
            final = attempt["final_cue_position"]
            yaw = attempt["actions"]["yaw"]
            along_aim = ((final["x"] - contact[0]) * math.cos(yaw) +
                         (final["z"] - contact[2]) * math.sin(yaw))
            self.assertLessEqual(along_aim, -1.0)

    def test_revised_side_english_lessons_require_exact_rail_counts(self) -> None:
        left, right, two_rail = build_scenarios_v2()[4:7]
        for item, exact in ((left, 1), (right, 1), (two_rail, 2)):
            predicates = item["success"]["all"]
            self.assertIn({"predicate": "rail_contact_count_gte",
                           "ball_index": 0, "minimum": exact}, predicates)
            self.assertIn({"predicate": "rail_contact_count_lte",
                           "ball_index": 0, "maximum": exact}, predicates)
            hints = item["teaching_hints"]
            self.assertGreaterEqual(hints["aim"]["tolerance_degrees"], 0.25)
            grid = list(teaching_range_universe(item, 1000))
            self.assertEqual(len(grid), 15)
            self.assertGreaterEqual(max(row[1] for row in grid) -
                                    min(row[1] for row in grid), 5.0)
        self.assertLess(left["teaching_hints"]["recommended_tip"]
                        ["x_ball_radii"], 0.0)
        self.assertGreater(right["teaching_hints"]["recommended_tip"]
                           ["x_ball_radii"], 0.0)
        self.assertLess(two_rail["teaching_hints"]["recommended_tip"]
                        ["x_ball_radii"], 0.0)

    def test_remaining_lessons_have_at_least_one_ui_step_of_adjustment(self) -> None:
        for item in build_scenarios_v2()[4:]:
            hints = item["teaching_hints"]
            steps = hints.get("shot_steps", [hints])
            for step in steps:
                power = step["recommended_power"]
                self.assertGreaterEqual(
                    power["maximum_percent"] - power["minimum_percent"],
                    2.5, item["id"])
                self.assertGreaterEqual(
                    step["aim"]["tolerance_degrees"], 0.125,
                    item["id"])

    def test_remaining_lessons_reject_cue_scratch_history_only(self) -> None:
        revised = build_scenarios_v2()
        legacy = build_scenarios()
        for item in revised[:4] + legacy:
            predicates = [entry["predicate"]
                          for entry in item["failure"]["any"]]
            self.assertNotIn("cue_ball_pocketed_during_attempt", predicates)
            self.assertEqual(item["failure"]["precedence"], [
                "numerical_failure", "cue_ball_pocketed",
                "shot_limit_exceeded", "timeout_ticks"])
        for item in revised[4:]:
            predicates = [entry["predicate"]
                          for entry in item["failure"]["any"]]
            self.assertEqual(predicates.count(
                "cue_ball_pocketed_during_attempt"), 1)
            self.assertEqual(item["failure"]["precedence"], [
                "numerical_failure", "cue_ball_pocketed",
                "cue_ball_pocketed_during_attempt",
                "shot_limit_exceeded", "timeout_ticks"])

    def test_side_english_calibration_and_controls_are_bound(self) -> None:
        from scripts.training_remaining_spin import build_remaining_spin
        report = json.loads((PACK.parent / "calibration" /
            "remaining_spin.range.json").read_text())
        expected_specs = hashlib.sha256(json.dumps(
            build_remaining_spin(), sort_keys=True,
            separators=(",", ":")).encode()).hexdigest()
        self.assertEqual(report["specs_sha256"], expected_specs)
        self.assertEqual(report["attempt_count"], 45)
        self.assertTrue(report["all_succeeded"])
        self.assertTrue(report["center_tip_controls_distinct"])
        self.assertTrue(report["legacy_demos_exceed_named_rail_count"])
        expected = {
            "launch_left_english_one_rail_v2": 1,
            "launch_right_english_one_rail_v2": 1,
            "launch_side_english_two_rail_v2": 2,
        }
        for attempt in report["attempts"]:
            self.assertEqual(attempt["facts"]["rail_contacts"][0],
                             expected[attempt["scenario_id"]])
            self.assertNotIn(0, attempt["facts"]["pocket_order"])


if __name__ == "__main__":
    unittest.main()
