from __future__ import annotations
import copy,importlib.util,json,subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
SCRIPT=ROOT/'scripts/validate_m4_d5_a_stroke_observation_canonical_tip_interval_foundation_implementation.py'
RECEIPT=ROOT/'docs/rules/m4-d5-a-stroke-observation-canonical-tip-interval-foundation-implementation-receipt-v1.json'
spec=importlib.util.spec_from_file_location('validator',SCRIPT);mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
class TestD5AImplementation(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.base=json.loads(RECEIPT.read_text())
 def reject(self,mutator):
  value=copy.deepcopy(self.base);mutator(value);self.assertTrue(mod.validate(value,False))
 def test_live_validator(self):self.assertEqual(mod.validate(copy.deepcopy(self.base),True),[])
 def test_cli(self):self.assertEqual(subprocess.run([sys.executable,str(SCRIPT)],cwd=ROOT,capture_output=True,text=True).returncode,0)
 def test_rejects_status(self):self.reject(lambda v:v.__setitem__('status','M4_COMPLETE'))
 def test_rejects_gate(self):self.reject(lambda v:v['authority_bindings']['implementation_design_gate'].__setitem__('sha256','0'*64))
 def test_rejects_policy(self):self.reject(lambda v:v['authority_bindings']['root_selected_policy'].__setitem__('sha256','0'*64))
 def test_rejects_batch(self):self.reject(lambda v:v['authority_bindings']['d5_d6_feasibility'].__setitem__('batch','D5-C'))
 def test_rejects_red(self):self.reject(lambda v:v['tests_first'].__setitem__('red_observed',False))
 def test_rejects_publication_boundary(self):self.reject(lambda v:v['contract'].__setitem__('successful_physics_tick_required_before_runtime_publication',False))
 def test_rejects_classification_claim(self):self.reject(lambda v:v['contract'].__setitem__('double_hit_classified',True))
 def test_rejects_threshold(self):self.reject(lambda v:v['contract'].__setitem__('threshold_selected',True))
 def test_rejects_fixture_order(self):self.reject(lambda v:v['fixtures']['positive'].reverse())
 def test_rejects_missing_negative(self):self.reject(lambda v:v['fixtures']['negative'].pop())
 def test_rejects_source_order(self):self.reject(lambda v:v['source_bindings'].reverse())
 def test_rejects_test_order(self):self.reject(lambda v:v['test_bindings'].reverse())
 def test_rejects_cmake_change(self):self.reject(lambda v:v['build_binding'].__setitem__('modified_for_d5_a',True))
 def test_rejects_pass_count(self):self.reject(lambda v:v['cpu'].__setitem__('passed_count',18))
 def test_rejects_binary_exit(self):self.reject(lambda v:v['cpu']['binaries'][0].__setitem__('exit_code',1))
 def test_rejects_hidden_external_failure(self):self.reject(lambda v:v['cpu'].__setitem__('link_blocked_external_count',0))
 def test_rejects_gpu(self):self.reject(lambda v:v['cpu'].__setitem__('gpu',True))
 def test_rejects_d5_complete(self):self.reject(lambda v:v['completion'].__setitem__('d5_complete',True))
 def test_rejects_audit_progress(self):self.reject(lambda v:v['completion'].__setitem__('classification_or_audit_progress',True))
 def test_rejects_m4_complete(self):self.reject(lambda v:v['completion'].__setitem__('m4_complete',True))
 def test_bad_json_cli(self):
  with tempfile.TemporaryDirectory() as td:
   p=Path(td)/'bad.json';p.write_text('{');self.assertNotEqual(subprocess.run([sys.executable,str(SCRIPT),str(p)],cwd=ROOT,capture_output=True).returncode,0)
if __name__=='__main__':unittest.main()
