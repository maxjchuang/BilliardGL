import argparse
import copy
import hashlib
import json
import subprocess
from pathlib import Path

from .confirmation_adapters import base_scenario, scenario_ball
from .run import _execute_once_with_evidence


def _canonical_bytes(document):
    return (json.dumps(
        document, ensure_ascii=False, indent=2, sort_keys=True,
        allow_nan=False) + "\n").encode("utf-8")


def _sha256_bytes(value):
    return hashlib.sha256(value).hexdigest()


def _sha256_file(path):
    return _sha256_bytes(Path(path).read_bytes())


def _candidate_profile(root):
    profile_path = root / (
        "physics_models/profiles/chinese_pool_full_game_v5.json")
    profile = json.loads(
        profile_path.read_text(encoding="utf-8"))["runtime_profile"]
    if profile.get("id") != "chinese_pool_full_game_v5" or \
            "frozen_cue_contact" not in profile:
        raise RuntimeError("preserved v5 candidate profile is invalid")
    return profile


def _scenario(profile):
    radius = profile["ball"]["radius_cm"]
    ball = scenario_ball(
        0, [0.0, 89.34147644042969, 0.0], [10.0, 0.0, 0.0],
        radius)
    return base_scenario(
        profile,
        "confirmation_contract_fixture__free_roll",
        [ball],
        "unbounded",
        2,
        "Synthetic confirmation contract through the real automation path",
        evidence_source="fixture_confirmation",
    )


def build_contract_proof(root, executable, fixture, mutate=None):
    root = Path(root).resolve()
    executable = Path(executable).resolve()
    fixture = Path(fixture).resolve()
    fixture.relative_to(root)
    scenario = _scenario(_candidate_profile(root))
    if mutate is not None:
        mutate(scenario)
    first = _execute_once_with_evidence(executable, copy.deepcopy(scenario))
    second = _execute_once_with_evidence(executable, copy.deepcopy(scenario))
    first_trace = _canonical_bytes(list(first.frames))
    second_trace = _canonical_bytes(list(second.frames))
    if first_trace != second_trace:
        raise RuntimeError("confirmation contract trace is nondeterministic")
    first_protocol = _canonical_bytes(list(first.protocol_transcript))
    second_protocol = _canonical_bytes(list(second.protocol_transcript))
    if first_protocol != second_protocol:
        raise RuntimeError("confirmation contract protocol is nondeterministic")
    if first.return_code != 0 or second.return_code != 0:
        raise RuntimeError("confirmation contract process did not exit cleanly")
    if first.stderr or second.stderr:
        raise RuntimeError("confirmation contract process wrote stderr")
    proof = {
        "schema_version": 1,
        "dataset_id": "fixture_confirmation",
        "result": "PASSED",
        "parse_succeeded": True,
        "frames": len(first.frames),
        "executable_sha256": _sha256_file(executable),
        "fixture_manifest_sha256": _sha256_file(fixture / "manifest.json"),
        "scenario_sha256": _sha256_bytes(_canonical_bytes(scenario)),
        "first_trace_sha256": _sha256_bytes(first_trace),
        "second_trace_sha256": _sha256_bytes(second_trace),
        "protocol_transcript_sha256": _sha256_bytes(first_protocol),
        "stderr": first.stderr,
        "return_code": first.return_code,
    }
    return {"confirmation_contract_proof.json": _canonical_bytes(proof)}


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Prove confirmation through the real automation path")
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--executable", required=True, type=Path)
    parser.add_argument("--fixture", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    arguments = parser.parse_args(argv)
    files = build_contract_proof(
        arguments.root, arguments.executable, arguments.fixture)
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_bytes(files["confirmation_contract_proof.json"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
