from __future__ import annotations
import json, tempfile, unittest
from pathlib import Path
from scripts import validate_phase2_v10i_formal_full_six_v8_source_drift_prevalidation_failure as validator

class V8SourceDriftFailureTests(unittest.TestCase):
    def test_live_receipt_passes(self): self.assertFalse(validator.validate()["marker_created"])
    def reject(self, mutate):
        value=json.loads(validator.RECEIPT.read_text()); mutate(value)
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/"r.json"; path.write_text(json.dumps(value))
            with self.assertRaises(RuntimeError): validator.validate(path)
    def test_rejects_auth_reuse(self): self.reject(lambda v:v["authorization"].update(authorization_reuse_forbidden=False))
    def test_rejects_marker_claim(self): self.reject(lambda v:v["execution_boundary"].update(consumed_marker_created=True))
    def test_rejects_process_claim(self): self.reject(lambda v:v["execution_boundary"].update(candidate_process_started=True))
    def test_rejects_drift_rewrite(self): self.reject(lambda v:v["source_drift"].update(readiness_expected_sha256=v["source_drift"]["prevalidation_live_sha256"]))
    def test_rejects_scene_promotion(self): self.reject(lambda v:v["scene_results"][0].update(status="PASS"))
    def test_rejects_formal_promotion(self): self.reject(lambda v:v["claims"].update(formal_pass=True))

if __name__ == "__main__": unittest.main()
