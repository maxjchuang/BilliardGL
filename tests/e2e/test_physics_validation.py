import json
import os
import sys
import tempfile
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT))

from tools.physics_validation.run import run_validation


with tempfile.TemporaryDirectory() as directory:
    output = Path(directory)
    exit_code = run_validation(
        executable=Path(sys.argv[1]),
        scenarios=REPO_ROOT / "tests/physics_validation/scenarios",
        known_failures=REPO_ROOT / "tests/physics_validation/known_failures.json",
        output=output,
    )
    assert exit_code == 0
    payload = json.loads((output / "report.json").read_text(encoding="utf-8"))
    assert payload["summary"] == {
        "passed": 6,
        "failed_known": 0,
        "failed_new": 0,
        "reference_limited": 0,
    }
    traces = list((output / "traces").glob("*.json"))
    assert len(traces) == 7
    assert (output / "traces/reverse_cradle_v1__permuted.json").is_file()
    markdown = (output / "report.md").read_text(encoding="utf-8")
    assert markdown.count("FAILED (KNOWN)") == 0
    assert "free_roll_v1 | PASSED" in markdown
    assert "cue_impact_v2_contract | PASSED" in markdown
    assert "profile_override_v3 | PASSED" in markdown
    assert "high_speed_tunneling_v1 | PASSED" in markdown
    assert "receding_overlap_v1 | PASSED" in markdown
    assert "reverse_cradle_v1 | PASSED" in markdown
