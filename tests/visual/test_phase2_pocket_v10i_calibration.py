import json,math
from pathlib import Path
import subprocess,sys,tempfile,unittest

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/"scripts"))
from calibrate_phase2_pocket_v10i import ANCHOR_G,ANCHOR_H,FU,FL,band_models,candidate_metrics,deterministic_grid
from freeze_phase2_pocket_v10i_offline import freeze

class Phase2V10iCalibrationTests(unittest.TestCase):
 def test_complete_grid_and_selection(self):
  rows,selected=deterministic_grid();self.assertEqual(35,len(rows));self.assertEqual(5,sum(i["passes_all_hard_safety_locality_spread"] for i in rows));self.assertEqual({"base_neutral_scalar":.05625,"lower_endpoint_scalar":.115,"upper_endpoint_scalar":.99,"roughness":.84},selected["parameters"]);self.assertTrue(all(selected["checks"].values()))
 def test_grid_is_deterministic_and_rejections_explained(self):
  a,sa=deterministic_grid();b,sb=deterministic_grid();self.assertEqual(a,b);self.assertEqual(sa,sb);self.assertTrue(all(i["rejection_reasons"] or i["passes_all_hard_safety_locality_spread"] for i in a))
 def test_both_models_reproduce_both_actual_anchors(self):
  for anchor in (ANCHOR_G,ANCHOR_H):
   for fraction,band in ((FU,"upper"),(FL,"lower")):
    row=band_models(anchor["B"],anchor["L"],anchor["U"],fraction,band);self.assertAlmostEqual(anchor[band],row["model_a_secant_linear"],12);self.assertAlmostEqual(anchor[band],row["model_b_log_linear_above_void"],12)
 def test_extrapolation_and_bad_grid_rejected(self):
  row=candidate_metrics(.05625,.05,.99);self.assertFalse(row["passes_all_hard_safety_locality_spread"]);self.assertTrue(any("grid" in r or "locality" in r for r in row["rejection_reasons"]))
 def test_lower_overbright_rejected_independently(self):
  row=candidate_metrics(.05625,.12,.99);self.assertFalse(row["passes_all_hard_safety_locality_spread"]);self.assertTrue(any("lower" in r for r in row["rejection_reasons"]))
 def test_report_byte_deterministic(self):
  script=ROOT/"scripts/calibrate_phase2_pocket_v10i.py"
  with tempfile.TemporaryDirectory() as d:
   a,b=Path(d)/"a.json",Path(d)/"b.json"
   for out in (a,b):subprocess.run([sys.executable,str(script),"--output",str(out)],cwd=ROOT,check=True,capture_output=True,text=True)
   self.assertEqual(a.read_bytes(),b.read_bytes());report=json.loads(a.read_text());self.assertEqual("OFFLINE_PREFLIGHT_PASS_GPU_NOT_AUTHORIZED",report["status"]);self.assertFalse(report["actual_gpu_used"])
 def test_freeze_rejects_gpu_claiming_report(self):
  report=json.loads((ROOT/"docs/art/phase2-pocket-v10i-offline-preflight-v1/phase2-pocket-v10i-offline-report-v1.json").read_text())
  with tempfile.TemporaryDirectory(dir=ROOT) as d:
   bad=Path(d)/"bad.json";report["actual_gpu_used"]=True;bad.write_text(json.dumps(report))
   with self.assertRaises(ValueError):freeze(Path(d)/"freeze",bad)

if __name__=="__main__":unittest.main()
