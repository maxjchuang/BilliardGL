from __future__ import annotations
import copy,importlib.util,json,subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
SCRIPT=ROOT/'scripts/validate_m4_d5_b_cue_object_frozen_cushion_policy_independent_telemetry_implementation.py'
RECEIPT=ROOT/'docs/rules/m4-d5-b-cue-object-frozen-cushion-policy-independent-telemetry-implementation-receipt-v1.json'
spec=importlib.util.spec_from_file_location('validator',SCRIPT);mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
class TestD5BImplementation(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.base=json.loads(RECEIPT.read_text())
 def reject(self,mutator):
  value=copy.deepcopy(self.base);mutator(value);self.assertTrue(mod.validate(value,False))
 def test_live_validator(self):self.assertEqual(mod.validate(copy.deepcopy(self.base),True),[])
 def test_cli(self):self.assertEqual(subprocess.run([sys.executable,str(SCRIPT)],cwd=ROOT,capture_output=True,text=True).returncode,0)
 def test_rejects_status(self):self.reject(lambda v:v.__setitem__('status','M4_COMPLETE'))
 def test_rejects_gate(self):self.reject(lambda v:v['authority_bindings']['implementation_design_gate'].__setitem__('sha256','0'*64))
 def test_rejects_d5a(self):self.reject(lambda v:v['authority_bindings']['d5_a_receipt'].__setitem__('sha256','0'*64))
 def test_rejects_batch(self):self.reject(lambda v:v['authority_bindings']['d5_d6_feasibility'].__setitem__('batch','D5-C'))
 def test_rejects_red(self):self.reject(lambda v:v['tests_first'].__setitem__('red_observed',False))
 def test_rejects_equal_classification(self):self.reject(lambda v:v['contract'].__setitem__('raw_equal_not_classified',False))
 def test_rejects_unsigned_binding(self):self.reject(lambda v:v['contract'].__setitem__('signed_committed_consumed_frozen_binding_only',False))
 def test_rejects_foul_claim(self):self.reject(lambda v:v['contract'].__setitem__('frozen_cushion_legal_or_foul_classified',True))
 def test_rejects_threshold(self):self.reject(lambda v:v['contract'].__setitem__('threshold_or_tolerance_selected',True))
 def test_rejects_positive_order(self):self.reject(lambda v:v['fixtures']['positive'].reverse())
 def test_rejects_negative_missing(self):self.reject(lambda v:v['fixtures']['negative'].pop())
 def test_rejects_source_order(self):self.reject(lambda v:v['source_bindings'].reverse())
 def test_rejects_test_order(self):self.reject(lambda v:v['test_bindings'].reverse())
 def test_rejects_forbidden_drift_binding(self):self.reject(lambda v:v['forbidden_live_bindings'].pop())
 def test_rejects_cmake_change(self):self.reject(lambda v:v['build_binding'].__setitem__('modified_for_d5_b',True))
 def test_rejects_pass_count(self):self.reject(lambda v:v['cpu'].__setitem__('passed_count',17))
 def test_rejects_binary_order(self):self.reject(lambda v:v['cpu']['binaries'].reverse())
 def test_rejects_binary_failure(self):self.reject(lambda v:v['cpu']['binaries'][0].__setitem__('exit_code',1))
 def test_rejects_gpu(self):self.reject(lambda v:v['cpu'].__setitem__('gpu',True))
 def test_rejects_d5_complete(self):self.reject(lambda v:v['completion'].__setitem__('d5_complete',True))
 def test_rejects_audit_progress(self):self.reject(lambda v:v['completion'].__setitem__('classification_or_audit_progress',True))
 def test_rejects_rules_change(self):self.reject(lambda v:v['completion'].__setitem__('rules_shotfacts_automation_ui_renderer_modified',True))
 def test_rejects_m4_complete(self):self.reject(lambda v:v['completion'].__setitem__('m4_complete',True))
 def test_bad_json_cli(self):
  with tempfile.TemporaryDirectory() as td:
   p=Path(td)/'bad.json';p.write_text('{');self.assertNotEqual(subprocess.run([sys.executable,str(SCRIPT),str(p)],cwd=ROOT,capture_output=True).returncode,0)
if __name__=='__main__':unittest.main()
