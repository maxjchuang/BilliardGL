#!/usr/bin/env python3
"""Static validator for BilliardGL training_scenario_v1 content."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from collections import Counter
from pathlib import Path


BALL_RADIUS_CM = 2.8575
MIN_CENTER_DISTANCE_CM = 2.0 * BALL_RADIUS_CM
X_LIMIT_CM = 63.5 - BALL_RADIUS_CM
Z_LIMIT_CM = 127.0 - BALL_RADIUS_CM
EXPECTED_CATEGORIES = {
    "stun_follow_draw": 3,
    "side_spin_cushion": 3,
    "pocket_angle": 2,
    "continuous_position": 2,
    "break_cue_control": 2,
}
EXPECTED_CATEGORIES_V2 = {**EXPECTED_CATEGORIES, "stun_follow_draw": 4}
POCKET_IDS = {
    "corner_nx_nz": 0, "corner_px_nz": 1,
    "corner_nx_pz": 2, "corner_px_pz": 3,
    "side_nx": 4, "side_px": 5,
}
SUCCESS_PREDICATES = {
    "ball_final_position_in_zone", "ball_not_pocketed", "ball_pocketed",
    "cue_ball_final_near_first_contact_point",
    "distinct_object_balls_with_rail_contact_gte", "first_contact_ball",
    "ordered_ball_pockets", "rail_contact_count_gte", "shot_count_lte",
    "rail_contact_count_lte", "ball_pocketed_in_pocket",
}
FAILURE_PREDICATES = {
    "cue_ball_pocketed", "cue_ball_pocketed_during_attempt",
    "numerical_failure", "shot_limit_exceeded", "timeout_ticks",
}


class ValidationError(Exception):
    def __init__(self, code: str, message: str):
        super().__init__(message)
        self.code = code


def fail(code: str, message: str) -> None:
    raise ValidationError(code, message)


def canonical_bytes(value: object) -> bytes:
    return (json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n").encode()


def canonical_physics_bytes(value: object) -> bytes:
    def encode(item: object, parent: str = "") -> str:
        if item is None:
            return "null"
        if isinstance(item, bool):
            return "true" if item else "false"
        if isinstance(item, str):
            return json.dumps(item, ensure_ascii=False, separators=(",", ":"))
        if isinstance(item, (int, float)):
            if parent in {"index", "schema_version", "ticks"}:
                return str(int(item))
            if float(item) == 12500000.0:
                return "1.25e+07"
            number = repr(float(item))
            return number if any(marker in number for marker in ".eE") else number + ".0"
        if isinstance(item, list):
            return "[" + ",".join(encode(child, parent) for child in item) + "]"
        if isinstance(item, dict):
            return "{" + ",".join(
                json.dumps(key, ensure_ascii=False) + ":" + encode(item[key], key)
                for key in sorted(item)) + "}"
        raise TypeError(type(item))
    return (encode(value) + "\n").encode()


def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def require(condition: bool, code: str, message: str) -> None:
    if not condition:
        fail(code, message)


def finite_tree(value: object) -> bool:
    if isinstance(value, float):
        return math.isfinite(value)
    if isinstance(value, dict):
        return all(finite_tree(item) for item in value.values())
    if isinstance(value, list):
        return all(finite_tree(item) for item in value)
    return True


def validate_initial_scenario(wrapper: dict, owner_id: str) -> None:
    require(wrapper.get("binding") == "embedded_canonical_physics_scenario_v1", "E_INITIAL_BINDING", owner_id)
    initial = wrapper.get("scenario")
    require(isinstance(initial, dict), "E_INITIAL_SCENARIO", owner_id)
    require(initial.get("schema_version") == 12, "E_INITIAL_SCHEMA", owner_id)
    require("physics_profile" in initial, "E_INITIAL_PROFILE", owner_id)
    require(initial.get("boundary_mode") == "production_table", "E_INITIAL_BOUNDARY", owner_id)
    require(initial.get("simulation") == {"ticks": 1, "time_step_seconds": 1.0 / 120.0}, "E_INITIAL_SIMULATION", owner_id)
    require(initial.get("expectations") == [{"metric": "finite_state", "operator": "eq", "value": True}], "E_INITIAL_EXPECTATION", owner_id)
    require(wrapper.get("canonical_sha256") == sha256_bytes(canonical_physics_bytes(initial)), "E_INITIAL_HASH", owner_id)
    profile = initial.get("physics_profile")
    require(isinstance(profile, dict) and
            profile.get("id") == "chinese_pool_interactive_120hz_v5" and
            profile.get("solver", {}).get("time_step_seconds") == 1.0 / 120.0,
            "E_INITIAL_PROFILE", owner_id)
    require(wrapper.get("physics_profile_sha256") ==
            sha256_bytes(canonical_bytes(profile)), "E_INITIAL_PROFILE_HASH", owner_id)
    balls = initial.get("balls")
    require(isinstance(balls, list) and balls, "E_INITIAL_BALLS", owner_id)
    seen: set[int] = set()
    positions: list[tuple[int, list[float]]] = []
    for item in balls:
        require(isinstance(item, dict), "E_INITIAL_BALL", owner_id)
        index = item.get("index")
        require(isinstance(index, int) and 0 <= index <= 15 and index not in seen, "E_BALL_INDEX", owner_id)
        seen.add(index)
        position = item.get("position_cm")
        require(isinstance(position, list) and len(position) == 3 and all(isinstance(v, (int, float)) for v in position), "E_BALL_POSITION", owner_id)
        require(abs(position[0]) <= X_LIMIT_CM and abs(position[2]) <= Z_LIMIT_CM, "E_GEOMETRY_BOUNDS", owner_id)
        require(item.get("velocity_cm_s") == [0.0, 0.0, 0.0] and item.get("angular_velocity_rad_s") == [0.0, 0.0, 0.0], "E_INITIAL_MOTION", owner_id)
        require(item.get("pocketed") is False, "E_INITIAL_POCKETED", owner_id)
        positions.append((index, position))
    require(0 in seen, "E_CUE_BALL_MISSING", owner_id)
    for offset, (first_index, first) in enumerate(positions):
        for second_index, second in positions[offset + 1:]:
            distance = math.hypot(first[0] - second[0], first[2] - second[2])
            require(distance + 1e-6 >= MIN_CENTER_DISTANCE_CM, "E_GEOMETRY_OVERLAP", f"{owner_id}: balls {first_index}/{second_index}")


def validate_predicates(container: object, allowed: set[str], code: str, owner_id: str) -> None:
    require(isinstance(container, list) and container, code, owner_id)
    for item in container:
        require(isinstance(item, dict) and item.get("predicate") in allowed, "E_PREDICATE_VOCABULARY", owner_id)
        if item.get("predicate") == "cue_ball_final_near_first_contact_point":
            require(isinstance(item.get("radius_cm"), (int, float)) and
                    item["radius_cm"] > 0.0,
                    "E_PREDICATE_PARAMETERS", owner_id)
        if item.get("predicate") == "rail_contact_count_lte":
            require(isinstance(item.get("ball_index"), int) and
                    0 <= item["ball_index"] < 16 and
                    isinstance(item.get("maximum"), int) and
                    item["maximum"] >= 0,
                    "E_PREDICATE_PARAMETERS", owner_id)
        if item.get("predicate") == "ball_pocketed_in_pocket":
            require(isinstance(item.get("ball_index"), int) and
                    0 <= item["ball_index"] < 16 and
                    isinstance(item.get("pocket_id"), int) and
                    0 <= item["pocket_id"] < 6,
                    "E_PREDICATE_PARAMETERS", owner_id)


def validate_scenario(value: object) -> None:
    require(isinstance(value, dict), "E_ROOT", "scenario must be an object")
    item: dict = value
    owner_id = str(item.get("id", "<missing-id>"))
    require(item.get("schema") == "training_scenario_v1" and item.get("schema_version") == 1, "E_SCHEMA", owner_id)
    content_version = item.get("version")
    require(content_version in {1, 2} and isinstance(item.get("id"), str) and
            item["id"].endswith(f"_v{content_version}"), "E_ID", owner_id)
    require(item.get("category") in EXPECTED_CATEGORIES, "E_CATEGORY", owner_id)
    locale = item.get("locale", {})
    require(all(isinstance(locale.get(key), str) and locale[key].strip() for key in ("title_zh_cn", "title_en", "description_zh_cn", "description_en")), "E_LOCALE", owner_id)
    require(isinstance(item.get("tags"), list) and len(item["tags"]) >= 4 and len(set(item["tags"])) == len(item["tags"]), "E_TAGS", owner_id)
    require(item.get("authorship", {}).get("asset_policy") == "original_billiardgl_content_no_third_party_assets", "E_AUTHORSHIP", owner_id)
    require(finite_tree(item), "E_NONFINITE", owner_id)
    validate_initial_scenario(item.get("initial_scenario", {}), owner_id)

    controls = item.get("allowed_controls", {})
    require(controls.get("explicitly_forbidden") == ["cue_elevation", "chalk", "auto_shot", "auto_aim"], "E_FORBIDDEN_CONTROLS", owner_id)
    require(set(controls.get("actions", [])) == {"aim_yaw", "shot_power", "camera_cycle", "reset_attempt"}, "E_ALLOWED_CONTROLS", owner_id)
    tip = controls.get("tip_offset", {})
    axis = tip.get("axis")
    require(axis in {"none", "vertical", "horizontal"}, "E_TIP_AXIS", owner_id)
    gate = item.get("runtime_gate", {})
    required_caps = gate.get("required_capabilities", [])
    require("training_scenario_v1" in required_caps and gate.get("on_missing") == "visible_locked_no_score", "E_RUNTIME_GATE", owner_id)
    if axis == "vertical":
        require("authoritative_longitudinal_tip_offset_v1" in required_caps and gate.get("production_default_enabled") is False, "E_SPIN_CAPABILITY_GATE", owner_id)
    elif axis == "horizontal":
        require("authoritative_horizontal_tip_offset_v1" in required_caps and gate.get("production_default_enabled") is False, "E_SPIN_CAPABILITY_GATE", owner_id)
    else:
        require(tip.get("enabled") is False and tip.get("preferred_radius_units") == [0.0, 0.0], "E_TIP_DEFAULT", owner_id)

    require("success" in item, "E_REQUIRED_SUCCESS", owner_id)
    require("failure" in item, "E_REQUIRED_FAILURE", owner_id)
    validate_predicates(item["success"].get("all"), SUCCESS_PREDICATES, "E_REQUIRED_SUCCESS", owner_id)
    validate_predicates(item["failure"].get("any"), FAILURE_PREDICATES, "E_REQUIRED_FAILURE", owner_id)
    require(item["success"].get("evaluation") == "attempt_end_after_balls_stopped", "E_SUCCESS_EVALUATION", owner_id)
    failure_predicates = [entry.get("predicate") for entry in item["failure"].get("any", [])]
    expected_failure_precedence = ["numerical_failure", "cue_ball_pocketed"]
    if "cue_ball_pocketed_during_attempt" in failure_predicates:
        expected_failure_precedence.append("cue_ball_pocketed_during_attempt")
    expected_failure_precedence.extend(["shot_limit_exceeded", "timeout_ticks"])
    require(item["failure"].get("precedence") == expected_failure_precedence, "E_FAILURE_PRECEDENCE", owner_id)

    reset = item.get("reset", {})
    require(reset.get("mode") == "atomic_reload_embedded_initial_scenario" and reset.get("attempt_seed") == 0 and reset.get("reset_tick_to") == 0, "E_RESET", owner_id)
    scoring = item.get("scoring", {})
    require(scoring.get("deterministic") is True, "E_SCORING_DETERMINISM", owner_id)
    require(scoring.get("rounding") == "nearest_integer_half_away_from_zero", "E_SCORING_DETERMINISM", owner_id)
    require(scoring.get("tie_break") == ["higher_score", "lower_attempt_ticks", "lexicographic_trace_sha256"], "E_SCORING_DETERMINISM", owner_id)
    require(scoring.get("minimum") == 0 and scoring.get("maximum") == 100 and scoring.get("base_points") == 100, "E_SCORING_RANGE", owner_id)
    require(all(rule.get("quantization", 0) > 0 and rule.get("cap_points", -1) >= 0 for rule in scoring.get("deductions", [])), "E_SCORING_DETERMINISM", owner_id)
    if any(rule.get("metric") == "cue_ball_bonus_zone_missed" for rule in scoring.get("deductions", [])):
        bonus = scoring.get("optional_bonus_zone", {})
        require(bonus.get("predicate") == "ball_final_position_in_zone" and
                bonus.get("ball_index") == 0 and len(bonus.get("center_cm", [])) == 2 and
                all(isinstance(n, (int, float)) and math.isfinite(n) for n in bonus["center_cm"]) and
                isinstance(bonus.get("radius_cm"), (int, float)) and
                math.isfinite(bonus["radius_cm"]) and bonus["radius_cm"] > 0,
                "E_SCORING_BONUS_ZONE", owner_id)

    replay = item.get("reference_replay", {})
    bind = replay.get("binding_rule", {})
    needed = {"replay_id", "scenario_sha256", "physics_profile_id", "action_log_sha256", "trace_sha256", "review_status_automated_verified"}
    replay_file = replay.get("file")
    require(replay.get("status") == "bound" and
            str(replay.get("id", "")).startswith("training-replay://phase4/") and
            isinstance(replay_file, str) and
            Path(replay_file).parts[0] in {"replays", "replays_v2"} and
            not Path(replay_file).is_absolute() and ".." not in Path(replay_file).parts,
            "E_REPLAY_BINDING", owner_id)
    require(set(bind.get("activation_requires", [])) == needed, "E_REPLAY_BINDING", owner_id)
    require(bind.get("scenario_sha256_must_equal") == item["initial_scenario"]["canonical_sha256"], "E_REPLAY_BINDING", owner_id)
    require(bind.get("physics_profile_id") == "chinese_pool_interactive_120hz_v5" and
            bind.get("review_status") == "automated_verified" and
            isinstance(bind.get("action_log_sha256"), str) and
            len(bind["action_log_sha256"]) == 64 and
            isinstance(bind.get("trace_sha256"), str) and
            len(bind["trace_sha256"]) == 64,
            "E_REPLAY_BINDING", owner_id)
    require("physics_profile" not in item and "production_profile" not in item, "E_PRODUCTION_PROFILE_OVERRIDE", owner_id)
    if content_version == 2:
        hints = item.get("teaching_hints", {})
        def validate_hints(value: object) -> None:
            require(isinstance(value, dict), "E_TEACHING_HINTS", owner_id)
            power = value.get("recommended_power", {})
            tip_hint = value.get("recommended_tip", {})
            aim = value.get("aim", {})
            target_zone = value.get("cue_ball_target_zone", {})
            pocket_enabled = value.get("target_pocket_enabled", True)
            pocket_id = value.get("target_pocket_id")
            goal = value.get("goal_en", "")
            require(0.0 <= power.get("minimum_percent", -1.0) <=
                    power.get("default_percent", -1.0) <=
                    power.get("maximum_percent", -1.0) <= 100.0 and
                    tip_hint.get("label") in {"center_ball", "high_tip", "low_tip",
                                                    "left_english", "right_english"} and
                    all(isinstance(tip_hint.get(key), (int, float)) for key in
                        ("x_ball_radii", "y_ball_radii")) and
                    isinstance(aim.get("yaw_radians"), (int, float)) and
                    0.0 < aim.get("tolerance_degrees", 0.0) <= 10.0 and
                    isinstance(pocket_enabled, bool) and
                    ((pocket_enabled and pocket_id in POCKET_IDS) or
                     (not pocket_enabled and pocket_id in {"", "none"})) and
                    isinstance(target_zone.get("center_cm"), list) and
                    len(target_zone["center_cm"]) == 2 and
                    isinstance(target_zone.get("visible"), bool) and
                    target_zone.get("radius_cm", 0.0) > 0.0 and
                    (not goal or (isinstance(goal, str) and len(goal) <= 65)),
                    "E_TEACHING_HINTS", owner_id)
        validate_hints(hints)
        steps = hints.get("shot_steps")
        if steps is not None:
            require(isinstance(steps, list) and len(steps) ==
                    item["scoring"]["par_shots"], "E_TEACHING_HINTS", owner_id)
            for step in steps:
                validate_hints(step)

        # A visible named pocket and a strict scoring pocket must refer to the
        # same canonical opening; the hint cannot advertise a different goal.
        hint_values = steps if steps is not None else [hints]
        strict = [predicate for predicate in item["success"]["all"]
                  if predicate["predicate"] == "ball_pocketed_in_pocket"]
        if strict:
            for hint in hint_values:
                if not hint.get("target_pocket_enabled", True):
                    continue
                object_index = hint.get("shot_line", {}).get("object_ball_index")
                require(any(predicate.get("ball_index") == object_index and
                            predicate.get("pocket_id") ==
                            POCKET_IDS[hint["target_pocket_id"]]
                            for predicate in strict),
                        "E_TEACHING_HINTS", owner_id)


def load_json(path: Path) -> object:
    return json.loads(path.read_text(encoding="utf-8"))


def validate_pack(pack_path: Path, check_bad_inputs: bool = True) -> dict:
    pack = load_json(pack_path)
    require(isinstance(pack, dict) and pack.get("schema") == "training_pack_v1" and pack.get("schema_version") == 1, "E_PACK_SCHEMA", str(pack_path))
    root = pack_path.parent
    is_v2 = pack.get("id") == "billiardgl_phase4_launch13_v2"
    expected_count = 13 if is_v2 else 12
    expected_categories = EXPECTED_CATEGORIES_V2 if is_v2 else EXPECTED_CATEGORIES
    expected_generation_file = (
        "reference_generation_v2.json" if is_v2 else "reference_generation.json")
    generation_entry = pack.get("reference_generation", {})
    generation_file = generation_entry.get("file")
    require(isinstance(generation_file, str) and
            generation_file == expected_generation_file,
            "E_REFERENCE_GENERATION", str(pack_path))
    generation_path = root / generation_file
    require(generation_path.is_file() and
            generation_entry.get("sha256") ==
            hashlib.sha256(generation_path.read_bytes()).hexdigest(),
            "E_REFERENCE_GENERATION", str(pack_path))
    generation = load_json(generation_path)
    require(isinstance(generation, dict) and
            generation.get("schema") == "training_reference_generation_v1" and
            generation.get("schema_version") == 1 and
            generation.get("build_type") == "Release" and
            generation.get("compiler_flags") == "-O3 -DNDEBUG" and
            generation.get("scenario_count") == expected_count and
            generation.get("method") ==
                "fresh_runtime_real_actions_two_pass_recorder" and
            generation.get("portability") ==
                "terminal_hash_verified_only_with_matching_build_configuration" and
            all(isinstance(generation.get(key), str) and
                len(generation[key]) == 64
                for key in ("binary_sha256", "audit_binary_sha256")),
            "E_REFERENCE_GENERATION", str(generation_path))
    entries = pack.get("scenarios", [])
    require(isinstance(entries, list) and len(entries) == expected_count, "E_PACK_COUNT", str(pack_path))
    ids: list[str] = []
    categories: Counter[str] = Counter()
    scenarios: list[dict] = []
    for entry in entries:
        path = root / entry["file"]
        payload = path.read_bytes()
        require(sha256_bytes(payload) == entry.get("sha256"), "E_FILE_HASH", str(path))
        value = json.loads(payload)
        validate_scenario(value)
        require(value["id"] == entry.get("id") and value["category"] == entry.get("category"), "E_PACK_ENTRY", str(path))
        replay_entry = entry.get("reference_replay", {})
        replay_relative = value["reference_replay"]["file"]
        require(replay_entry.get("file") == replay_relative,
                "E_REPLAY_BINDING", str(path))
        replay_path = root / replay_relative
        replay_payload = replay_path.read_bytes()
        require(sha256_bytes(replay_payload) == replay_entry.get("sha256"),
                "E_FILE_HASH", str(replay_path))
        replay_document = json.loads(replay_payload)
        binding = value["reference_replay"]["binding_rule"]
        timeout = next(condition["maximum"] for condition in
            value["failure"]["any"] if condition["predicate"] == "timeout_ticks")
        require(replay_document.get("format") == "billiardgl_replay_v1" and
                replay_document.get("format_version") == 1 and
                replay_document.get("content_sha256") == entry.get("sha256") and
                replay_document.get("physics_profile_id") ==
                    binding["physics_profile_id"] and
                replay_document.get("final_summary", {}).get(
                    "authority_state_schema") == "d6_authority_v1" and
                replay_document.get("final_summary", {}).get("tick", timeout + 1) <= timeout,
                "E_REPLAY_BINDING", str(replay_path))
        ids.append(value["id"]); categories[value["category"]] += 1; scenarios.append(value)
    require(len(ids) == len(set(ids)), "E_DUPLICATE_ID", str(pack_path))
    require(dict(categories) == expected_categories, "E_CATEGORY_COVERAGE", str(pack_path))

    matrix_ref = pack.get("coverage_matrix", {})
    matrix_path = root / matrix_ref.get("file", "")
    matrix_payload = matrix_path.read_bytes()
    require(sha256_bytes(matrix_payload) == matrix_ref.get("sha256"), "E_FILE_HASH", str(matrix_path))
    matrix = json.loads(matrix_payload)
    require(matrix.get("scenario_count") == expected_count and
            matrix.get("category_counts") == expected_categories,
            "E_COVERAGE_MATRIX", str(matrix_path))
    expected_gated = sorted(item["id"] for item in scenarios if not item["runtime_gate"]["production_default_enabled"])
    require(matrix.get("gated_scenarios") == expected_gated, "E_COVERAGE_MATRIX", str(matrix_path))

    verified_bad = 0
    if check_bad_inputs:
        for entry in pack.get("bad_input_fixtures", []):
            path = root / entry["file"]
            payload = path.read_bytes()
            require(sha256_bytes(payload) == entry.get("sha256"), "E_FILE_HASH", str(path))
            fixture = json.loads(payload)
            require(fixture.get("expected_error_code") == entry.get("expected_error_code"), "E_BAD_FIXTURE", str(path))
            try:
                validate_scenario(fixture.get("scenario"))
            except ValidationError as error:
                require(error.code == entry["expected_error_code"], "E_BAD_FIXTURE", f"{path}: expected {entry['expected_error_code']}, got {error.code}")
                verified_bad += 1
            else:
                fail("E_BAD_FIXTURE", f"{path}: invalid fixture unexpectedly passed")
    return {"bad_inputs_verified": verified_bad, "categories": dict(categories), "scenarios": len(scenarios)}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("path", nargs="?", type=Path, default=Path("assets/training/phase4/training_pack_v1.json"))
    parser.add_argument("--skip-bad-inputs", action="store_true")
    args = parser.parse_args()
    try:
        result = validate_pack(args.path.resolve(), not args.skip_bad_inputs)
    except (ValidationError, KeyError, OSError, json.JSONDecodeError) as error:
        code = error.code if isinstance(error, ValidationError) else "E_IO_OR_JSON"
        print(f"FAIL {code}: {error}")
        return 1
    print(f"PASS training_scenario_v1: scenarios={result['scenarios']} bad_inputs={result['bad_inputs_verified']} categories={result['categories']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
