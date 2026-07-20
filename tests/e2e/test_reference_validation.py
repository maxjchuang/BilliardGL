import csv
import hashlib
import json
import shutil
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT))

from tools.physics_validation.reference_run import run_reference_validation


executable = Path(sys.argv[1]).resolve()
output = Path(sys.argv[2]).resolve()
package = REPO_ROOT / "tests/physics_validation/fixtures/reference_package_v1"

if output.exists():
    shutil.rmtree(output)
output.mkdir(parents=True)

exit_code = run_reference_validation(executable, package, output)
assert exit_code == 0

payload = json.loads(
    (output / "reference_report.json").read_text(encoding="utf-8"))
assert payload["partitions"]["CALIBRATION"]["summary"]["points"] == 1
assert payload["partitions"]["HOLDOUT"]["summary"]["points"] == 1
assert payload["partitions"]["CALIBRATION"]["summary"]["passed"] == 1
assert payload["partitions"]["HOLDOUT"]["summary"]["passed"] == 1

digest = hashlib.sha256(executable.read_bytes()).hexdigest()
assert payload["metadata"]["build_id"] == "sha256:" + digest
manifest = json.loads((package / "manifest.json").read_text(encoding="utf-8"))
assert payload["metadata"]["package_hashes"] == {
    item["id"]: item["sha256"] for item in manifest["files"]}

assert len(list((output / "traces").glob("*.json"))) == 2
assert len(list((output / "provenance").glob("*.json"))) == 2
with (output / "reference_points.csv").open(
        "r", encoding="utf-8", newline="") as source:
    rows = list(csv.DictReader(source))
assert {row["point_id"] for row in rows} == {
    "stop_distance_cal_01", "stop_distance_holdout_01"}
assert "## CALIBRATION" in (output / "reference_report.md").read_text(encoding="utf-8")
assert "## HOLDOUT" in (output / "reference_report.md").read_text(encoding="utf-8")
