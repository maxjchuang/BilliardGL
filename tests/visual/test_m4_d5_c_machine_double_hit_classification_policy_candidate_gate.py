from __future__ import annotations
import copy,importlib.util,json,subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
SCRIPT=ROOT/'scripts/validate_m4_d5_c_machine_double_hit_classification_policy_candidate_gate.py'
GATE=ROOT/'docs/rules/m4-d5-c-machine-double-hit-classification-policy-candidate-gate-v1.json'
spec=importlib.util.spec_from_file_location('validator',SCRIPT);mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
class TestD5CPolicyCandidateGate(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.base=json.loads(GATE.read_text())
 def reject(self,mutator):
  value=copy.deepcopy(self.base);mutator(value);self.assertTrue(mod.validate(value,False))
 def test_live_validator(self):self.assertEqual(mod.validate(copy.deepcopy(self.base),True),[])
 def test_cli(self):self.assertEqual(subprocess.run([sys.executable,str(SCRIPT)],cwd=ROOT,capture_output=True,text=True).returncode,0)
 def test_rejects_status(self):self.reject(lambda v:v.__setitem__('status','ROOT_SELECTED'))
 def test_rejects_pdf(self):self.reject(lambda v:v['official_source'].__setitem__('sha256','0'*64))
 def test_rejects_numeric_official_claim(self):self.reject(lambda v:v['official_source']['not_numerically_determined'].pop())
 def test_rejects_d5a(self):self.reject(lambda v:v['authority_bindings']['d5_a_receipt'].__setitem__('sha256','0'*64))
 def test_rejects_d5b(self):self.reject(lambda v:v['authority_bindings']['d5_b_receipt'].__setitem__('sha256','0'*64))
 def test_rejects_batch(self):self.reject(lambda v:v['authority_bindings']['d5_d6_feasibility'].__setitem__('batch','D5-D'))
 def test_rejects_prior_selection_claim(self):self.reject(lambda v:v['authority_bindings']['root_selected_policy_bundle'].__setitem__('does_not_select_d5_c_numeric_inputs',False))
 def test_rejects_missing_telemetry_gap(self):self.reject(lambda v:v['current_capability'].__setitem__('calibrated_light_graze_metric',True))
 def test_rejects_foul_on_incomplete(self):self.reject(lambda v:v['shared_machine_contract'].__setitem__('incomplete_evidence','DOUBLE_HIT_FOUL'))
 def test_rejects_output_missing_review(self):self.reject(lambda v:v['shared_machine_contract']['outputs'].remove('REVIEW_REQUIRED'))
 def test_rejects_gap_bound(self):self.reject(lambda v:v['bounded_policy_space']['very_close_surface_gap_cm'].__setitem__('maximum',5.0))
 def test_rejects_graze_bound(self):self.reject(lambda v:v['bounded_policy_space']['light_graze_normal_impulse_ratio'].__setitem__('maximum',1.0))
 def test_rejects_tie_bound(self):self.reject(lambda v:v['bounded_policy_space']['tip_separation_tie_tolerance_seconds'].__setitem__('maximum',0.01))
 def test_rejects_recommended_id(self):self.reject(lambda v:v.__setitem__('recommended_bundle_id',v['policy_bundles'][1]['id']))
 def test_rejects_recommendation_as_selection(self):self.reject(lambda v:v.__setitem__('recommendation_is_not_selection',False))
 def test_rejects_selected_bundle(self):self.reject(lambda v:v['policy_bundles'][0].__setitem__('selection_status','ROOT_SELECTED'))
 def test_rejects_mixed_bundle_constants(self):self.reject(lambda v:v['policy_bundles'][1]['constants'].__setitem__('light_graze_normal_impulse_ratio',0.1))
 def test_rejects_bundle_order(self):self.reject(lambda v:v['policy_bundles'].reverse())
 def test_rejects_duplicate_bundle(self):self.reject(lambda v:v['policy_bundles'][2].__setitem__('id',v['policy_bundles'][1]['id']))
 def test_rejects_gate_selects_one(self):self.reject(lambda v:v['mutual_exclusion'].__setitem__('this_gate_selects_none',False))
 def test_rejects_no_additional_gate(self):self.reject(lambda v:v['future_implementation_boundary'].__setitem__('bundle_b_or_c_additional_design_gate_required',False))
 def test_rejects_allowlist(self):self.reject(lambda v:v['future_implementation_boundary']['candidate_production_allowlist'].pop())
 def test_rejects_forbidden(self):self.reject(lambda v:v['future_implementation_boundary']['forbidden'].pop())
 def test_rejects_live_order(self):self.reject(lambda v:v['live_no_change_bindings'].reverse())
 def test_rejects_policy_selected_claim(self):self.reject(lambda v:v['claims'].__setitem__('policy_selected',True))
 def test_rejects_implementation_claim(self):self.reject(lambda v:v['claims'].__setitem__('machine_double_hit_classification_implemented',True))
 def test_rejects_audit_claim(self):self.reject(lambda v:v['claims'].__setitem__('audit_or_m4_progress',True))
 def test_rejects_gpu(self):self.reject(lambda v:v['claims'].__setitem__('gpu_readiness_or_authorization',True))
 def test_bad_json_cli(self):
  with tempfile.TemporaryDirectory() as td:
   p=Path(td)/'bad.json';p.write_text('{');self.assertNotEqual(subprocess.run([sys.executable,str(SCRIPT),str(p)],cwd=ROOT,capture_output=True).returncode,0)
if __name__=='__main__':unittest.main()
